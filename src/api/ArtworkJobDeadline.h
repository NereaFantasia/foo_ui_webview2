#pragma once
// ============================================================
// ArtworkJobDeadline — deferred artwork 请求的期限（deadline）与取消原语
// ============================================================
//
// 为什么需要：artwork.* 的读图段跑在 fb2k::inCpuWorkerThread 上，慢 I/O
// （网络盘、云占位符水合、异常大的内嵌图）可能远超过调用方愿意等待的时间。
// 页面侧的 invoke 有 30s 超时兜底，超时后页面已放弃这次请求，但 worker 仍会
// 继续占着一个 CPU worker 线程直到 I/O 自己返回 —— 既浪费线程，也让后续请求
// 排队。这里给每次请求一个**上限时间**：到期仍未回包时，由本层先回一包
// 超时错误（available:false + code），同时 abort 掉 SDK 侧可中断的取图调用，
// worker 随异常退出、线程归还线程池。
//
// 两层结构：
//   · DeadlineScheduler：全进程一个后台线程，按最早到期时间唤醒（不轮询）。
//     只做"时间到了通知"，不碰任何 fb2k 对象；到期回调必须自己保持轻量，
//     并且允许从任意线程投递回包（DeferredResponder 本身就是跨线程安全的）。
//   · JobGate：每请求一个，用一次 CAS 决定"worker 结果"与"到期超时"谁负责
//     回包，另一侧静默丢弃。DeferredResponder 内还有一道同语义的 CAS，这里
//     先闸一次是为了：超时后 worker 晚到的结果不再触碰信封/log，让"只回一次"
//     的判定点停在业务层，重复响应日志只反映真正的框架级重复。
//
// 生命周期（审查 SEC-03 修复）：调度器单例**故意不析构**（见 Instance 注释），
// 停机走显式 Shutdown（initquit::on_quit 调用）：置停机标志、丢弃未到期回调、
// 唤醒并 join。这样既避免 Windows Loader Lock 下的静态析构 join 死锁，也避免
// 迟到 worker 调 gate->Finish 时访问已析构单例的 UAF —— 单例对象与互斥量
// 始终有效，Shutdown 之后 Schedule 返回 0（不再设期限）、Cancel 变 no-op。
//
// 纯标准库（无 foobar2000 SDK 依赖）：tests 工程可直接包含并测试，
// 见 tests/test_artwork_job_deadline.cpp。
// ============================================================

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace artwork_async {

// ============================================================
// DeadlineScheduler — 单后台线程的到期调度器
// ============================================================
//
// 语义：
//   · Schedule(delay, cb) 在 delay 之后最多调用 cb 一次；返回取消令牌。
//     停机后返回 0（表示"没有设期限"）。
//   · Cancel(token) 收回尚未触发的回调（已触发的为 no-op）；取消队首时唤醒
//     调度线程重算最早到期时间。
//   · 到期回调在调度线程上执行，必须自行 try/catch（本类也会兜一层）。
//   · 单例由 Shutdown 显式停机；析构函数只停本实例的线程（供测试的局部实例），
//     单例故意 leak 不参与静态析构。
class DeadlineScheduler {
public:
    using Callback = std::function<void()>;

    static DeadlineScheduler& Instance();

    // 显式停机（在 initquit::on_quit 调用）：幂等；返回后调度线程已退出，
    // 未到期回调被丢弃（不执行）。从未用过调度器时不会为了停机而创建线程。
    // 禁止在静态析构里调用 —— 那正是 Loader Lock 死锁面。
    static void Shutdown();

    // 无锁快速查询：进程级停机后新建的 JobGate 不再设期限。
    static bool IsStopping() noexcept { return s_stopping.load(std::memory_order_acquire); }

    explicit DeadlineScheduler(bool marksProcessStopped = false)
        : marksProcessStopped_(marksProcessStopped), thread_(&DeadlineScheduler::Run, this) {}
    ~DeadlineScheduler() { Stop(); }

    DeadlineScheduler(const DeadlineScheduler&) = delete;
    DeadlineScheduler& operator=(const DeadlineScheduler&) = delete;

    // 停本实例的线程（不置进程级标记；单例停机走 static Shutdown）。
    void Stop();

    // 返回取消令牌（非 0）；delay <= 0 时下一轮立即触发。停机后返回 0。
    std::uint64_t Schedule(std::chrono::milliseconds delay, Callback callback) {
        const auto due = std::chrono::steady_clock::now() + delay;
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) return 0;
        const std::uint64_t token = nextToken_++;
        entries_.emplace(token, Entry{token, due, std::move(callback)});
        cv_.notify_all();
        return token;
    }

    void Cancel(std::uint64_t token) {
        if (token == 0) return;
        bool erased = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            erased = entries_.erase(token) != 0;
        }
        // 被取消的可能是队首：唤醒调度线程重算 wait_until，避免虚等到原到期时间。
        if (erased) cv_.notify_all();
    }

private:
    struct Entry {
        std::uint64_t token;
        std::chrono::steady_clock::time_point due;
        Callback callback;
    };

    void Run();

    std::mutex mutex_;
    std::condition_variable cv_;
    std::map<std::uint64_t, Entry> entries_;
    bool stopping_ = false;
    bool marksProcessStopped_ = false;
    std::uint64_t nextToken_ = 1;
    std::thread thread_;

    // 进程级停机标记：JobGate/Shutdown 无锁读取。inline static：全进程一份。
    inline static std::atomic<bool> s_stopping{false};
    inline static std::atomic<bool> s_created{false};
};

inline void DeadlineScheduler::Run() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stopping_) {
        if (entries_.empty()) {
            cv_.wait(lock);
            continue;
        }

        const auto earliest = std::min_element(
            entries_.begin(), entries_.end(),
            [](const auto& left, const auto& right) { return left.second.due < right.second.due; });
        // 被新任务/取消/停机提前唤醒时返回 no_timeout → 回到循环重算；只有真正
        // 到期才 timeout。取消队首后不重算会让线程虚等旧期限（SEC-04）。
        if (cv_.wait_until(lock, earliest->second.due) != std::cv_status::timeout) {
            continue;
        }

        const auto now = std::chrono::steady_clock::now();
        std::vector<Entry> expired;
        for (auto it = entries_.begin(); it != entries_.end();) {
            if (it->second.due <= now) {
                expired.push_back(std::move(it->second));
                it = entries_.erase(it);
            } else {
                ++it;
            }
        }
        if (expired.empty()) continue;

        // 回调会经 DeferredResponder 投递回主线程；在本类的锁之外执行，
        // 避免回调里再 Schedule/Cancel 时自锁。
        lock.unlock();
        for (auto& entry : expired) {
            if (!entry.callback) continue;
            try {
                entry.callback();
            } catch (...) {
                // 到期回包本身失败不改变"只回一次"语义：worker 侧拿不到闸门，
                // 页面侧落到 30s 超时兜底（比在调度线程上传播异常好）。
            }
        }
        lock.lock();
    }
}

inline void DeadlineScheduler::Stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
        if (marksProcessStopped_) s_stopping.store(true, std::memory_order_release);
        entries_.clear();  // 丢弃未到期回调（不执行）：停机时没有可回包的对象
    }
    cv_.notify_all();
    if (thread_.joinable()) {
        // 只在显式停机路径（on_quit / 局部实例析构）join；单例故意 leak，
        // 静态析构阶段不会走到这里（那时才可能持有 Windows Loader Lock）。
        thread_.join();
    }
}

inline DeadlineScheduler& DeadlineScheduler::Instance() {
    // 故意 leak：组件卸载/进程退出时不跑静态析构，从而不 join、不留 UAF 窗口。
    // 停机由 DeadlineScheduler::Shutdown()（initquit::on_quit）显式完成；
    // 迟到的 worker 仍能安全调用 Cancel（对象与互斥量一直有效）。
    static DeadlineScheduler* const instance = []() {
        s_created.store(true, std::memory_order_release);
        return new DeadlineScheduler(/*marksProcessStopped=*/true);
    }();
    return *instance;
}

inline void DeadlineScheduler::Shutdown() {
    // 从未用过调度器时不要为停机去创建线程。
    if (!s_created.load(std::memory_order_acquire)) return;
    Instance().Stop();
}

// ============================================================
// JobGate — 每请求一个的"谁回包"闸门
// ============================================================
//
// 用法（见 src/api/ArtworkApi.cpp 的 RunArtworkOnWorker）：
//   auto gate = JobGate::Create(deadlineMs, onDeadline);
//   fb2k::inCpuWorkerThread([gate, ...] {
//       ... 取图 ...
//       gate->Finish([&] { responder.SendJson(result); });   // worker 赢则回结果
//   });
// onDeadline 由调度线程执行；它应当 abort SDK 调用并回超时信封。
// 两侧的 CAS 恰好一个成功，另一侧 Finish 直接返回、不产生第二包。
class JobGate {
public:
    using Respond = std::function<void()>;

    static std::shared_ptr<JobGate> Create(std::chrono::milliseconds deadline, Respond onDeadline) {
        auto gate = std::shared_ptr<JobGate>(new JobGate());
        gate->onDeadline_ = std::move(onDeadline);
        const std::uint64_t token = DeadlineScheduler::Instance().Schedule(
            deadline, [gate]() { gate->FireDeadline(); });
        gate->deadlineToken_.store(token);
        return gate;
    }

    // worker 完成时调用；恰好一个调用者（worker 或到期回调）的 respond 会被执行。
    void Finish(const Respond& respond) {
        bool expected = false;
        if (!claimed_.compare_exchange_strong(expected, true)) {
            return;  // 到期侧已回包，worker 结果按设计丢弃
        }
        // 停机后调度器可能已停止；IsStopping 是无锁静态标记，避免无谓触达调度队列。
        const std::uint64_t token = deadlineToken_.load();
        if (token != 0 && !DeadlineScheduler::IsStopping()) {
            DeadlineScheduler::Instance().Cancel(token);
        }
        if (respond) respond();
    }

    bool ClaimedByDeadline() const { return claimedByDeadline_.load(); }

private:
    JobGate() = default;

    void FireDeadline() {
        bool expected = false;
        if (!claimed_.compare_exchange_strong(expected, true)) {
            return;  // worker 已先回包
        }
        claimedByDeadline_.store(true);
        if (onDeadline_) onDeadline_();
    }

    std::atomic<bool> claimed_{false};
    std::atomic<bool> claimedByDeadline_{false};
    std::atomic<std::uint64_t> deadlineToken_{0};
    Respond onDeadline_;
};

}  // namespace artwork_async

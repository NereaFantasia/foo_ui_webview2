#include "pch.h"
#include "media/MediaFile.h"
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

namespace media {
namespace {

class IoQueue {
public:
    struct Task { std::function<void()> run; std::function<void()> cancelled; };
    IoQueue() {
        try {
            for (unsigned i = 0; i < 2; ++i) threads_.emplace_back([this] { Run(); });
        } catch (...) {
            Stop();
            throw;
        }
    }
    ~IoQueue() { Stop(); }
    bool Enqueue(Task task) {
        std::lock_guard lock(mutex_);
        if (stopping_ || tasks_.size() >= 32) return false;
        tasks_.push_back(std::move(task));
        wake_.notify_one();
        return true;
    }
    void Stop() {
        // 主线程先接管尚未开始的任务，运行中的任务通过 ioStopping 和同步 IO 取消退出。
        std::deque<Task> abandoned;
        {
            std::lock_guard lock(mutex_);
            stopping_ = true;
            detail::ioStopping.store(true, std::memory_order_release);
            abandoned.swap(tasks_);
        }
        for (auto& task : abandoned) if (task.cancelled) {
            try { task.cancelled(); } catch (...) {
                // 单个取消通知失败不能跳过其余任务和线程的退出清理。
            }
        }
        wake_.notify_all();
        for (auto& thread : threads_) {
            if (!thread.joinable()) continue;
            // 取消请求由驱动处理，不能把此循环的轮询间隔当作退出时间上限。
            while (WaitForSingleObject(thread.native_handle(), 20) == WAIT_TIMEOUT)
                CancelSynchronousIo(thread.native_handle());
            thread.join();
        }
    }

private:
    void Run() {
        for (;;) {
            Task task;
            {
                std::unique_lock lock(mutex_);
                wake_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
                if (stopping_) return;
                task = std::move(tasks_.front());
                tasks_.pop_front();
            }
            try { task.run(); }
            catch (...) {
                // 失败通知投递时也可能分配失败；cancelled 可能操作 COM，不能改在 worker 执行。
                try { if (task.cancelled) fb2k::inMainThread(std::move(task.cancelled)); }
                catch (...) {
                    // 无法投递时保留主线程清理责任，不能在 worker 上释放 COM 请求。
                }
            }
        }
    }
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Task> tasks_;
    std::vector<std::thread> threads_;
    bool stopping_ = false;
};

std::unique_ptr<IoQueue>& Queue() { static std::unique_ptr<IoQueue> queue; return queue; }
class Shutdown : public initquit {
public:
    void on_init() override {
        // IO 线程按首次请求延迟创建；此服务只登记退出清理。
    }
    void on_quit() override { StopMediaIo(); }
};
static initquit_factory_t<Shutdown> shutdown;

}  // namespace

bool QueueMediaIo(std::function<void()> task, std::function<void()> cancelled) {
    if (detail::ioStopping.load(std::memory_order_acquire)) return false;
    auto& queue = Queue();
    if (!queue) queue = std::make_unique<IoQueue>();
    return queue->Enqueue({std::move(task), std::move(cancelled)});
}
void StopMediaIo() {
    detail::ioStopping.store(true, std::memory_order_release);
    if (Queue()) Queue()->Stop();
}

}  // namespace media

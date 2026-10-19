// test_artwork_job_deadline.cpp — deferred artwork 的期限/取消原语（JobGate）契约测试
//
// 判据来源：fix/async-bridge 二期「worker 超时/取消」——
//   1. 到期且 worker 未回包时，到期回调恰好执行一次；
//   2. worker 先回包时，到期回调不再执行（计时器被收回）；
//   3. 到期已回包时，worker 晚到的 Finish 不执行（结果按设计丢弃）；
//   4. worker 与到期两侧竞争时，整场只有一个回包（恰好一次）；
//   5. 每个 JobGate 独立，互不串包。
//
// 施测层是 docs/... 里的 header-only 原语（纯标准库）；ArtworkApi.cpp 的接线
// （5s 期限 + abort + DeferredResponder）在测试台探针里另验。
#include "pch.h"
#include "api/ArtworkJobDeadline.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

namespace {

using namespace std::chrono_literals;

// 轮询等待谓词为真，避免固定 sleep 带来的 flaky。
void WaitFor(const std::function<bool()>& predicate, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) return;
        std::this_thread::sleep_for(1ms);
    }
}

}  // namespace

TEST(ArtworkJobDeadlineTest, DeadlineFiresOnceWhenWorkerNeverFinishes) {
    std::atomic<int> deadlineRuns{0};
    {
        auto gate = artwork_async::JobGate::Create(20ms, [&deadlineRuns]() { ++deadlineRuns; });
        WaitFor([&deadlineRuns]() { return deadlineRuns.load() == 1; }, 2000ms);
    }
    // 再等一段时间，确认不会重复触发。
    std::this_thread::sleep_for(60ms);
    EXPECT_EQ(deadlineRuns.load(), 1);
}

TEST(ArtworkJobDeadlineTest, WorkerFinishBeforeDeadlineCancelsTimeout) {
    std::atomic<int> deadlineRuns{0};
    std::atomic<int> workerRuns{0};
    auto gate = artwork_async::JobGate::Create(2000ms, [&deadlineRuns]() { ++deadlineRuns; });

    gate->Finish([&workerRuns]() { ++workerRuns; });

    EXPECT_EQ(workerRuns.load(), 1);
    EXPECT_FALSE(gate->ClaimedByDeadline());
    // 期限远未到；即便到了也被 Cancel 收回 —— 这里只等一小段，确认没有立即误触发。
    std::this_thread::sleep_for(60ms);
    EXPECT_EQ(deadlineRuns.load(), 0);
}

TEST(ArtworkJobDeadlineTest, LateWorkerFinishAfterDeadlineIsDropped) {
    std::atomic<int> deadlineRuns{0};
    std::atomic<int> workerRuns{0};
    auto gate = artwork_async::JobGate::Create(10ms, [&deadlineRuns]() { ++deadlineRuns; });

    WaitFor([&deadlineRuns]() { return deadlineRuns.load() == 1; }, 2000ms);
    ASSERT_EQ(deadlineRuns.load(), 1);
    EXPECT_TRUE(gate->ClaimedByDeadline());

    gate->Finish([&workerRuns]() { ++workerRuns; });
    EXPECT_EQ(workerRuns.load(), 0);  // 晚到的结果被闸门丢弃，不产生第二包
}

TEST(ArtworkJobDeadlineTest, DeadlineBeforeWorkerFinishIsTheOnlyResponse) {
    std::atomic<int> responses{0};
    auto gate = artwork_async::JobGate::Create(
        10ms, [&responses]() { ++responses; });  // 到期侧回包

    WaitFor([&responses]() { return responses.load() == 1; }, 2000ms);
    gate->Finish([&responses]() { ++responses; });  // worker 侧晚到
    EXPECT_EQ(responses.load(), 1);
}

TEST(ArtworkJobDeadlineTest, FinishIsSingleWinnerUnderContention) {
    // 100 轮：worker 线程立刻 Finish，同时 0ms 期限在调度线程上竞争。
    for (int round = 0; round < 100; ++round) {
        std::atomic<int> responses{0};
        auto gate = artwork_async::JobGate::Create(0ms, [&responses]() { ++responses; });

        std::thread worker([gate, &responses]() { gate->Finish([&responses]() { ++responses; }); });
        worker.join();

        // 让可能已排入调度线程的到期回调跑完（若它赢了这一轮）。
        std::this_thread::sleep_for(2ms);
        ASSERT_EQ(responses.load(), 1) << "round " << round;
    }
}

TEST(ArtworkJobDeadlineTest, IndependentGatesDoNotCrossRespond) {
    std::atomic<int> firstDeadline{0};
    std::atomic<int> secondDeadline{0};
    auto first = artwork_async::JobGate::Create(10ms, [&firstDeadline]() { ++firstDeadline; });
    auto second = artwork_async::JobGate::Create(2000ms, [&secondDeadline]() { ++secondDeadline; });

    WaitFor([&firstDeadline]() { return firstDeadline.load() == 1; }, 2000ms);

    std::atomic<int> secondWorker{0};
    second->Finish([&secondWorker]() { ++secondWorker; });
    secondDeadline.store(0);  // Finish 已收回第二只的计时器

    std::this_thread::sleep_for(40ms);
    EXPECT_EQ(firstDeadline.load(), 1);
    EXPECT_EQ(secondWorker.load(), 1);
    EXPECT_EQ(secondDeadline.load(), 0);
    EXPECT_TRUE(first->ClaimedByDeadline());
    EXPECT_FALSE(second->ClaimedByDeadline());
}

TEST(ArtworkJobDeadlineTest, ZeroDeadlineStillAllowsWorkerToWinTheGate) {
    // 期限为 0 时到期回调可能在 Create 返回前后任意时刻触发；Finish 与它竞争，
    // 但只有一个会穿过 CAS。这里验证"创建后立刻 Finish"不会两边都回包。
    for (int round = 0; round < 20; ++round) {
        std::atomic<int> responses{0};
        auto gate = artwork_async::JobGate::Create(0ms, [&responses]() { ++responses; });
        gate->Finish([&responses]() { ++responses; });
        std::this_thread::sleep_for(2ms);
        ASSERT_EQ(responses.load(), 1) << "round " << round;
    }
}

// ============================================================
// DeadlineScheduler 生命周期（审查 SEC-03 / SEC-04）
// ============================================================
// 用局部实例测试（不打进程级停机标记），避免影响同一进程里 JobGate 的用例。
// 单例的显式停机在 initquit::on_quit 走 Shutdown()，组件内不便单测。

TEST(DeadlineSchedulerLifecycle, StopDropsPendingDeadlineAndSchedulesAreRejected) {
    artwork_async::DeadlineScheduler scheduler;
    std::atomic<int> fired{0};

    ASSERT_NE(scheduler.Schedule(20ms, [&fired]() { ++fired; }), 0u);
    WaitFor([&fired]() { return fired.load() == 1; }, 2000ms);
    ASSERT_EQ(fired.load(), 1);

    ASSERT_NE(scheduler.Schedule(60000ms, [&fired]() { ++fired; }), 0u);
    scheduler.Stop();  // 丢弃未到期回调并 join 线程
    std::this_thread::sleep_for(60ms);
    EXPECT_EQ(fired.load(), 1);  // 未到期回调没有执行
    EXPECT_EQ(scheduler.Schedule(10ms, [&fired]() { ++fired; }), 0u);  // 停机后不再设期限
    scheduler.Stop();  // 幂等
}

TEST(DeadlineSchedulerLifecycle, StopReturnsPromptlyWithLongPendingDeadline) {
    artwork_async::DeadlineScheduler scheduler;
    scheduler.Schedule(60000ms, []() {});

    const auto started = std::chrono::steady_clock::now();
    scheduler.Stop();
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - started)
                               .count();
    // 停机要立刻唤醒，而不是等 60s 期限（Shutdown 路径不允许挂死）。
    EXPECT_LT(elapsedMs, 1000) << "Stop took " << elapsedMs << "ms";
}

TEST(DeadlineSchedulerLifecycle, CancelReArmsNextDeadline) {
    artwork_async::DeadlineScheduler scheduler;
    std::atomic<int> cancelledFired{0};
    std::atomic<int> nextFired{0};

    const std::uint64_t head = scheduler.Schedule(30000ms, [&cancelledFired]() { ++cancelledFired; });
    ASSERT_NE(head, 0u);
    scheduler.Cancel(head);
    ASSERT_NE(scheduler.Schedule(20ms, [&nextFired]() { ++nextFired; }), 0u);

    WaitFor([&nextFired]() { return nextFired.load() == 1; }, 2000ms);
    EXPECT_EQ(nextFired.load(), 1);
    EXPECT_EQ(cancelledFired.load(), 0);
}

TEST(DeadlineSchedulerLifecycle, CancelledDeadlineNeverFires) {
    artwork_async::DeadlineScheduler scheduler;
    std::atomic<int> fired{0};

    const std::uint64_t token = scheduler.Schedule(30ms, [&fired]() { ++fired; });
    ASSERT_NE(token, 0u);
    scheduler.Cancel(token);

    std::this_thread::sleep_for(120ms);
    EXPECT_EQ(fired.load(), 0);
}

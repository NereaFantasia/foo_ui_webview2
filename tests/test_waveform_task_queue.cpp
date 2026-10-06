// test_waveform_task_queue.cpp - 整轨波形解码任务的合并、并发、取消与归属决策
// （docs/audio-visualization/SPEC.md D7、§4.5、§4.6、§10 A2.6 / A2.8）。
//
// 解码与发事件在宿主里；这里只断言队列给出的决策：启动哪个任务、中止哪个、
// 写不写缓存、哪些 taskId 收到哪种终态。
#include "pch.h"
#include "../src/api/WaveformTaskQueue.h"

#include <string>
#include <vector>

using fb2k_waveform::CallerIdentity;
using fb2k_waveform::CancelVerdict;
using fb2k_waveform::JobOutcome;
using fb2k_waveform::JobState;
using fb2k_waveform::JudgeCancel;
using fb2k_waveform::Waiter;
using fb2k_waveform::WaveformTaskQueue;

namespace {

const CallerIdentity kPageA{0x1000, "main"};
const CallerIdentity kPageB{0x2000, "popup-1"};

Waiter MakeWaiter(const std::string& taskId, const CallerIdentity& owner = kPageA) {
    Waiter w;
    w.taskId = taskId;
    w.path = "E:\\a.flac";
    w.owner = owner;
    return w;
}

std::vector<std::string> TaskIds(const std::vector<Waiter>& waiters) {
    std::vector<std::string> ids;
    for (const Waiter& w : waiters) ids.push_back(w.taskId);
    return ids;
}

}  // namespace

// 同键请求并入同一任务、各留各的 taskId；完成时每个等待者都在通知名单里，并写缓存。
TEST(WaveformTaskQueue, SameKeyMergesAndEveryWaiterIsNotified) {
    WaveformTaskQueue q;
    const auto first = q.Submit("k", MakeWaiter("t1"));
    EXPECT_FALSE(first.merged);
    ASSERT_EQ(first.start, std::vector<std::uint64_t>{first.jobId});

    const auto second = q.Submit("k", MakeWaiter("t2", kPageB));
    EXPECT_TRUE(second.merged);
    EXPECT_EQ(second.jobId, first.jobId);
    EXPECT_TRUE(second.start.empty());
    EXPECT_EQ(q.WaiterCount(first.jobId), 2u);

    const auto done = q.Finish(first.jobId, JobOutcome::Succeeded);
    EXPECT_TRUE(done.writeCache);
    EXPECT_EQ(TaskIds(done.notify), (std::vector<std::string>{"t1", "t2"}));
    EXPECT_FALSE(q.HasJob(first.jobId));
}

// 排队中的任务同样接受并入。
TEST(WaveformTaskQueue, QueuedJobAcceptsWaiters) {
    WaveformTaskQueue q(1);
    q.Submit("a", MakeWaiter("t1"));
    const auto queued = q.Submit("b", MakeWaiter("t2"));
    EXPECT_EQ(q.StateOf(queued.jobId), JobState::Queued);
    const auto joined = q.Submit("b", MakeWaiter("t3"));
    EXPECT_TRUE(joined.merged);
    EXPECT_EQ(joined.jobId, queued.jobId);
    EXPECT_EQ(q.WaiterCount(queued.jobId), 2u);
}

// A2.6：6 个不同曲目，前两个立即开始，其余按提交顺序在槽位空出时开始，任一时刻至多 2 个。
TEST(WaveformTaskQueue, AtMostTwoDecodeAndTheRestStartInSubmissionOrder) {
    WaveformTaskQueue q;
    std::vector<std::uint64_t> jobs;
    std::vector<std::uint64_t> started;
    for (int i = 0; i < 6; ++i) {
        const auto r = q.Submit("k" + std::to_string(i), MakeWaiter("t" + std::to_string(i)));
        jobs.push_back(r.jobId);
        started.insert(started.end(), r.start.begin(), r.start.end());
        EXPECT_LE(q.ActiveCount(), 2u);
    }
    EXPECT_EQ(started, (std::vector<std::uint64_t>{jobs[0], jobs[1]}));
    EXPECT_EQ(q.QueuedCount(), 4u);

    // 第二个先完成：空出的槽给排在最前的第三个，而不是后到的。
    auto r = q.Finish(jobs[1], JobOutcome::Succeeded);
    EXPECT_EQ(r.start, std::vector<std::uint64_t>{jobs[2]});
    r = q.Finish(jobs[0], JobOutcome::Succeeded);
    EXPECT_EQ(r.start, std::vector<std::uint64_t>{jobs[3]});
    r = q.Finish(jobs[2], JobOutcome::Failed);
    EXPECT_EQ(r.start, std::vector<std::uint64_t>{jobs[4]});
    r = q.Finish(jobs[3], JobOutcome::Succeeded);
    EXPECT_EQ(r.start, std::vector<std::uint64_t>{jobs[5]});
    EXPECT_LE(q.ActiveCount(), 2u);
    EXPECT_EQ(q.QueuedCount(), 0u);
}

// 失败时每个等待者各得一条失败通知，不写缓存。
TEST(WaveformTaskQueue, FailureFansOutToEveryWaiterWithoutCaching) {
    WaveformTaskQueue q;
    const auto job = q.Submit("k", MakeWaiter("t1")).jobId;
    q.Submit("k", MakeWaiter("t2"));
    q.Submit("k", MakeWaiter("t3"));
    const auto r = q.Finish(job, JobOutcome::Failed);
    EXPECT_FALSE(r.writeCache);
    EXPECT_EQ(TaskIds(r.notify), (std::vector<std::string>{"t1", "t2", "t3"}));
}

// 取消一个等待者：它本身交给宿主发 CANCELLED；任务仍有等待者时照常解码，完成后只通知剩下的。
TEST(WaveformTaskQueue, CancellingOneWaiterLeavesTheJobRunningForTheOthers) {
    WaveformTaskQueue q;
    const auto job = q.Submit("k", MakeWaiter("t1")).jobId;
    q.Submit("k", MakeWaiter("t2"));

    const auto c = q.Cancel("t1", kPageA);
    EXPECT_TRUE(c.cancelled);
    EXPECT_EQ(c.verdict, CancelVerdict::Owner);
    EXPECT_EQ(c.waiter.taskId, "t1");
    EXPECT_TRUE(c.abort.empty());
    EXPECT_EQ(q.StateOf(job), JobState::Decoding);

    const auto r = q.Finish(job, JobOutcome::Succeeded);
    EXPECT_EQ(TaskIds(r.notify), std::vector<std::string>{"t2"});
    // 已结束的 taskId 再取消：false。
    EXPECT_FALSE(q.Cancel("t2", kPageA).cancelled);
    EXPECT_FALSE(q.Cancel("nope", kPageA).cancelled);
}

// 排队中的任务等待者全部取消：直接移出队列，不中止、不占槽。
TEST(WaveformTaskQueue, CancellingTheLastWaiterOfAQueuedJobRemovesIt) {
    WaveformTaskQueue q(1);
    q.Submit("a", MakeWaiter("t1"));
    const auto queued = q.Submit("b", MakeWaiter("t2")).jobId;
    const auto c = q.Cancel("t2", kPageA);
    EXPECT_TRUE(c.cancelled);
    EXPECT_TRUE(c.abort.empty());
    EXPECT_EQ(c.discarded, std::vector<std::uint64_t>{queued});
    EXPECT_FALSE(q.HasJob(queued));
    EXPECT_EQ(q.QueuedCount(), 0u);
}

// 解码中的任务等待者全部取消：给出中止；中止中的任务不接受并入，同键新请求另起任务，
// 而且中止中的任务仍占槽，直到它的 worker 返回。
TEST(WaveformTaskQueue, AbortingJobKeepsItsSlotAndRefusesNewWaiters) {
    WaveformTaskQueue q;
    const auto a = q.Submit("a", MakeWaiter("t1")).jobId;
    const auto b = q.Submit("b", MakeWaiter("t2")).jobId;

    const auto c = q.Cancel("t1", kPageA);
    EXPECT_EQ(c.abort, std::vector<std::uint64_t>{a});
    EXPECT_EQ(q.StateOf(a), JobState::Aborting);

    const auto again = q.Submit("a", MakeWaiter("t3"));
    EXPECT_FALSE(again.merged);
    EXPECT_NE(again.jobId, a);
    EXPECT_TRUE(again.start.empty()) << "two slots are still held: one aborting, one decoding";
    EXPECT_EQ(q.ActiveCount(), 2u);

    const auto r = q.Finish(a, JobOutcome::Aborted);
    EXPECT_TRUE(r.notify.empty());
    EXPECT_FALSE(r.writeCache);
    EXPECT_EQ(r.start, std::vector<std::uint64_t>{again.jobId});

    // 旧任务收尾不能摘掉同键新任务的并入入口：之后的同键请求仍并入新任务。
    const auto joined = q.Submit("a", MakeWaiter("t4"));
    EXPECT_TRUE(joined.merged);
    EXPECT_EQ(joined.jobId, again.jobId);
    (void)b;
}

// 中止结局而任务仍有等待者（中止不是由最后一个取消触发的）：每个等待者都在通知名单里，
// 宿主给它们发 CANCELLED，不写缓存。
TEST(WaveformTaskQueue, AbortedJobStillNotifiesItsWaiters) {
    WaveformTaskQueue q;
    const auto job = q.Submit("k", MakeWaiter("t1")).jobId;
    q.Submit("k", MakeWaiter("t2"));
    const auto r = q.Finish(job, JobOutcome::Aborted);
    EXPECT_FALSE(r.writeCache);
    EXPECT_EQ(TaskIds(r.notify), (std::vector<std::string>{"t1", "t2"}));
}

// 全部取消后解码已越过最后一次中止检查而完成：写缓存、不发任何事件。
TEST(WaveformTaskQueue, JobThatFinishesAfterEveryoneLeftIsCachedSilently) {
    WaveformTaskQueue q;
    const auto job = q.Submit("k", MakeWaiter("t1")).jobId;
    q.Cancel("t1", kPageA);
    const auto r = q.Finish(job, JobOutcome::Succeeded);
    EXPECT_TRUE(r.writeCache);
    EXPECT_TRUE(r.notify.empty());
}

// 归属：hwnd 相等认、windowId 相等认、登记值两项都空照认；其余拒绝，任务照常完成。
TEST(WaveformTaskQueue, CancelHonoursOwnership) {
    EXPECT_EQ(JudgeCancel({0x10, "w"}, {0x10, "other"}), CancelVerdict::Owner);
    EXPECT_EQ(JudgeCancel({0x10, "w"}, {0x99, "w"}), CancelVerdict::Owner);
    EXPECT_EQ(JudgeCancel({0, ""}, {0x99, "x"}), CancelVerdict::Unattributed);
    EXPECT_EQ(JudgeCancel({0x10, "w"}, {0x99, "x"}), CancelVerdict::Denied);
    // 登记值只有 windowId 时，取消方 windowId 为空不算相等。
    EXPECT_EQ(JudgeCancel({0, "w"}, {0x10, ""}), CancelVerdict::Denied);
    // 登记值只有 hwnd 时，取消方 hwnd 为 0 不算相等。
    EXPECT_EQ(JudgeCancel({0x10, ""}, {0, ""}), CancelVerdict::Denied);

    WaveformTaskQueue q;
    const auto job = q.Submit("k", MakeWaiter("t1", kPageA)).jobId;
    const auto denied = q.Cancel("t1", kPageB);
    EXPECT_FALSE(denied.cancelled);
    EXPECT_EQ(denied.verdict, CancelVerdict::Denied);
    EXPECT_EQ(q.StateOf(job), JobState::Decoding);
    const auto r = q.Finish(job, JobOutcome::Succeeded);
    EXPECT_EQ(TaskIds(r.notify), std::vector<std::string>{"t1"});

    const auto open = q.Submit("k2", MakeWaiter("t2", CallerIdentity{})).jobId;
    const auto unattributed = q.Cancel("t2", kPageB);
    EXPECT_TRUE(unattributed.cancelled);
    EXPECT_EQ(unattributed.verdict, CancelVerdict::Unattributed);
    EXPECT_EQ(q.StateOf(open), JobState::Aborting);
}

// 弹窗关闭：只移除该窗口的等待者、不发事件；任务没人等了就中止或出队。
TEST(WaveformTaskQueue, DroppingAWindowRemovesOnlyItsWaiters) {
    WaveformTaskQueue q(1);
    const auto shared = q.Submit("a", MakeWaiter("t1", kPageB)).jobId;
    q.Submit("a", MakeWaiter("t2", kPageA));
    const auto own = q.Submit("b", MakeWaiter("t3", kPageB)).jobId;

    const auto r = q.DropWindow("popup-1");
    EXPECT_EQ(r.removedWaiters, 2u);
    EXPECT_TRUE(r.abort.empty());
    EXPECT_EQ(r.discarded, std::vector<std::uint64_t>{own});
    EXPECT_EQ(q.WaiterCount(shared), 1u);
    EXPECT_FALSE(q.HasJob(own));
    EXPECT_TRUE(q.DropWindow("").abort.empty());

    const auto alone = q.DropWindow("main");
    EXPECT_EQ(alone.abort, std::vector<std::uint64_t>{shared});
}

// 退出：丢弃排队任务、中止解码中的任务、不留等待者，此后不再启动任何任务。
TEST(WaveformTaskQueue, DropAllAbortsDecodingAndDiscardsTheQueue) {
    WaveformTaskQueue q;
    const auto a = q.Submit("a", MakeWaiter("t1")).jobId;
    const auto b = q.Submit("b", MakeWaiter("t2")).jobId;
    const auto c = q.Submit("c", MakeWaiter("t3")).jobId;

    const auto r = q.DropAll();
    EXPECT_EQ(r.abort, (std::vector<std::uint64_t>{a, b}));
    EXPECT_EQ(r.discarded, std::vector<std::uint64_t>{c});
    EXPECT_EQ(r.removedWaiters, 3u);
    EXPECT_FALSE(q.HasJob(c));

    const auto fa = q.Finish(a, JobOutcome::Aborted);
    EXPECT_TRUE(fa.notify.empty());
    EXPECT_TRUE(fa.start.empty());
    EXPECT_TRUE(q.Submit("d", MakeWaiter("t4")).start.empty());
}

// Finish 只认已启动的任务：排队中的与不存在的都得空结果，排队中的任务原样留着。
TEST(WaveformTaskQueue, FinishIgnoresUnknownAndQueuedJobs) {
    WaveformTaskQueue q(1);
    q.Submit("a", MakeWaiter("t1"));
    const auto queued = q.Submit("b", MakeWaiter("t2")).jobId;
    const auto r = q.Finish(queued, JobOutcome::Succeeded);
    EXPECT_FALSE(r.writeCache);
    EXPECT_TRUE(r.notify.empty());
    EXPECT_TRUE(q.HasJob(queued));
    EXPECT_TRUE(q.Finish(9999, JobOutcome::Succeeded).notify.empty());
}

// PCM 离线解码用并发 1 的实例（docs/audio-pcm/SPEC.md D7）：不同曲目严格一个接一个按提交顺序开始，
// 中止中的任务仍占着唯一的槽，后面的要等它的 worker 返回。
TEST(WaveformTaskQueue, WithOneSlotJobsRunStrictlyOneAfterAnother) {
    WaveformTaskQueue q(1);
    const auto a = q.Submit("a", MakeWaiter("t1"));
    const auto b = q.Submit("b", MakeWaiter("t2"));
    const auto c = q.Submit("c", MakeWaiter("t3"));
    EXPECT_EQ(a.start, std::vector<std::uint64_t>{a.jobId});
    EXPECT_TRUE(b.start.empty());
    EXPECT_TRUE(c.start.empty());
    EXPECT_EQ(q.ActiveCount(), 1u);
    EXPECT_EQ(q.QueuedCount(), 2u);

    auto r = q.Finish(a.jobId, JobOutcome::Succeeded);
    EXPECT_EQ(r.start, std::vector<std::uint64_t>{b.jobId});

    const auto cancel = q.Cancel("t2", kPageA);
    EXPECT_EQ(cancel.abort, std::vector<std::uint64_t>{b.jobId});
    EXPECT_EQ(q.StateOf(c.jobId), JobState::Queued) << "the aborting job still holds the only slot";

    r = q.Finish(b.jobId, JobOutcome::Aborted);
    EXPECT_EQ(r.start, std::vector<std::uint64_t>{c.jobId});
    EXPECT_EQ(q.ActiveCount(), 1u);
    EXPECT_EQ(q.QueuedCount(), 0u);
}

// test_spectrum_schedule.cpp - 频谱订阅逐拍调度的纯值计算
// （docs/audio-visualization/SPEC.md D1 / D5 / D12 / D13、§5.1、§10 A1.3 / A1.7 / A1.9 / A3.5）。
//
// 读播放状态、判外部前台限流、取频谱都在宿主里；这里只喂进它们的结果，
// 测每拍的状态推进、到期判定、配置组去重与投递决策。
#include "pch.h"
#include "../src/api/SpectrumSchedule.h"

#include <chrono>

using fb2k_spectrum::BeatGate;
using fb2k_spectrum::Clock;
using fb2k_spectrum::EffectiveFftSize;
using fb2k_spectrum::NextBeat;
using fb2k_spectrum::FrameDecision;
using fb2k_spectrum::MakeScheduleState;
using fb2k_spectrum::PlanTick;
using fb2k_spectrum::PlaybackState;
using fb2k_spectrum::ScheduleEntry;
using fb2k_spectrum::ScheduleParams;
using fb2k_spectrum::ScheduleState;
using fb2k_spectrum::SpectrumScale;
using fb2k_spectrum::TickInput;
using fb2k_spectrum::TickPlan;

namespace {

// 时间轴从 1 s 起算，避开与「下一拍就到期」标记（时钟纪元）重合。
Clock::time_point At(int64_t ms) {
    return Clock::time_point{} + std::chrono::seconds(1) + std::chrono::milliseconds(ms);
}

constexpr auto kInterval30 = std::chrono::microseconds(33333);

struct Sub {
    ScheduleParams params;
    ScheduleState state;
    bool visible = true;

    ScheduleEntry Entry() { return {&params, &state, visible}; }
};

Sub MakeSub(const char* id, int fps, int bands = 16, int fftSize = 1024, bool backgroundThrottle = true,
            PlaybackState initial = PlaybackState::Playing) {
    Sub sub;
    sub.params.subscriptionId = id;
    sub.params.fps = fps;
    sub.params.bands = bands;
    sub.params.fftSize = fftSize;
    sub.params.backgroundThrottle = backgroundThrottle;
    sub.state = MakeScheduleState(initial);
    return sub;
}

TickInput Tick(int64_t ms, PlaybackState playback = PlaybackState::Playing, bool throttled = false,
               bool skipFft = false) {
    TickInput in;
    in.now = At(ms);
    in.playback = playback;
    in.throttled = throttled;
    in.skipFft = skipFft;
    in.maxFftSize = 65536;
    return in;
}

const FrameDecision* FindFrame(const TickPlan& plan, const std::string& id) {
    for (const FrameDecision& f : plan.frames) {
        if (f.subscriptionId == id) return &f;
    }
    return nullptr;
}

size_t CountFrames(const TickPlan& plan, const std::string& id) {
    size_t n = 0;
    for (const FrameDecision& f : plan.frames) {
        if (f.subscriptionId == id) ++n;
    }
    return n;
}

}  // namespace

// 自动提升沿用宿主原规则：64 带起至少 8192 点，32 带起至少 4096 点，且不超上限。
TEST(SpectrumSchedule, EffectiveFftSizeRaisesWithBandCount) {
    EXPECT_EQ(EffectiveFftSize(1024, 16, 65536), 1024);
    EXPECT_EQ(EffectiveFftSize(1024, 32, 65536), 4096);
    EXPECT_EQ(EffectiveFftSize(1024, 64, 65536), 8192);
    EXPECT_EQ(EffectiveFftSize(16384, 256, 65536), 16384);
    EXPECT_EQ(EffectiveFftSize(1024, 256, 4096), 4096);
    EXPECT_EQ(EffectiveFftSize(65536, 8, 65536), 65536);
}

// 首拍每个可见的播放中订阅都出帧；实际点数、频带数、scale 相同的只算一个配置组。
TEST(SpectrumSchedule, FirstTickEmitsAndSharesOneGroup) {
    Sub a = MakeSub("a", 30, 16, 1024);
    Sub b = MakeSub("b", 60, 16, 1024);
    Sub c = MakeSub("c", 30, 64, 2048);
    const TickPlan plan = PlanTick(Tick(0), {a.Entry(), b.Entry(), c.Entry()});

    ASSERT_EQ(plan.groups.size(), 2u);
    ASSERT_EQ(plan.frames.size(), 3u);
    const FrameDecision* fa = FindFrame(plan, "a");
    const FrameDecision* fb = FindFrame(plan, "b");
    const FrameDecision* fc = FindFrame(plan, "c");
    ASSERT_TRUE(fa && fb && fc);
    EXPECT_EQ(fa->group, fb->group);
    EXPECT_NE(fa->group, fc->group);
    EXPECT_EQ(plan.groups[fc->group].fftSize, 8192);
    EXPECT_EQ(plan.groups[fc->group].bands, 64);
    for (const FrameDecision& f : plan.frames) {
        EXPECT_FALSE(f.silent);
        EXPECT_TRUE(f.deliver);
        EXPECT_EQ(f.state, PlaybackState::Playing);
    }
}

// 点数与频带数都相同、只有 scale 不同的两个订阅分成两个配置组：
// 合成一组的话 'db' 订阅会收到 'weighted' 算出的 0 到 1 的值。
TEST(SpectrumSchedule, ScaleSplitsComputeGroups) {
    Sub w = MakeSub("w", 30, 48, 8192);
    Sub d = MakeSub("d", 30, 48, 8192);
    d.params.scale = SpectrumScale::Db;
    const TickPlan plan = PlanTick(Tick(0), {w.Entry(), d.Entry()});

    ASSERT_EQ(plan.groups.size(), 2u);
    const FrameDecision* fw = FindFrame(plan, "w");
    const FrameDecision* fd = FindFrame(plan, "d");
    ASSERT_TRUE(fw && fd);
    ASSERT_NE(fw->group, fd->group);
    EXPECT_EQ(plan.groups[fw->group].scale, SpectrumScale::Weighted);
    EXPECT_EQ(plan.groups[fd->group].scale, SpectrumScale::Db);
}

// A1.3：30 fps 与 60 fps 的订阅在 31 ms 的节拍下各按自己的间隔到期。
// 30 fps 的一秒内不超过 31 帧、不少于 27 帧；60 fps 的每拍都到期。
TEST(SpectrumSchedule, DeadlinesFollowEachSubscriptionsFps) {
    Sub a = MakeSub("a", 30);
    Sub b = MakeSub("b", 60);
    size_t framesA = 0, framesB = 0, ticks = 0;
    for (int64_t t = 0; t < 1000; t += 31) {
        const TickPlan plan = PlanTick(Tick(t), {a.Entry(), b.Entry()});
        framesA += CountFrames(plan, "a");
        framesB += CountFrames(plan, "b");
        ++ticks;
    }
    EXPECT_LE(framesA, 31u);
    EXPECT_GE(framesA, 27u);
    EXPECT_EQ(framesB, ticks);
}

// 准时到期时截止时刻按上一个截止时刻推进（相位锁定），不按本拍时刻。
TEST(SpectrumSchedule, OnTimeDeadlineAdvancesFromPreviousDeadline) {
    Sub a = MakeSub("a", 30);
    PlanTick(Tick(0), {a.Entry()});
    EXPECT_EQ(a.state.nextDue, At(0) + kInterval30);
    const TickPlan plan = PlanTick(Tick(40), {a.Entry()});
    EXPECT_EQ(CountFrames(plan, "a"), 1u);
    EXPECT_EQ(a.state.nextDue, At(0) + kInterval30 + kInterval30);
}

// 落后超过一帧就重置为本拍时刻加一帧，不补帧。
TEST(SpectrumSchedule, FallingBehindResetsWithoutCatchUp) {
    Sub a = MakeSub("a", 30);
    PlanTick(Tick(0), {a.Entry()});
    const TickPlan plan = PlanTick(Tick(500), {a.Entry()});
    EXPECT_EQ(CountFrames(plan, "a"), 1u);
    EXPECT_EQ(a.state.nextDue, At(500) + kInterval30);
    const TickPlan next = PlanTick(Tick(510), {a.Entry()});
    EXPECT_EQ(CountFrames(next, "a"), 0u);
}

// D5 六种转移：进入暂停或停止的四种各发一帧静音；进入播放的两种不发静音、当拍出频谱帧。
TEST(SpectrumSchedule, SixTransitionsProduceSilenceOnlyWhenLeavingPlayback) {
    Sub a = MakeSub("a", 30);
    PlanTick(Tick(0), {a.Entry()});

    auto expectSilence = [&](int64_t ms, PlaybackState to) {
        const TickPlan plan = PlanTick(Tick(ms, to), {a.Entry()});
        ASSERT_EQ(plan.frames.size(), 1u) << "ms=" << ms;
        EXPECT_TRUE(plan.frames[0].silent);
        EXPECT_EQ(plan.frames[0].state, to);
        EXPECT_TRUE(plan.frames[0].deliver);
        EXPECT_TRUE(plan.groups.empty());
        EXPECT_EQ(a.state.lastState, to);
    };
    auto expectSpectrumOnly = [&](int64_t ms) {
        const TickPlan plan = PlanTick(Tick(ms, PlaybackState::Playing), {a.Entry()});
        ASSERT_EQ(plan.frames.size(), 1u) << "ms=" << ms;
        EXPECT_FALSE(plan.frames[0].silent);
        EXPECT_EQ(plan.frames[0].state, PlaybackState::Playing);
        EXPECT_EQ(plan.groups.size(), 1u);
    };

    expectSilence(50, PlaybackState::Paused);        // 播放 → 暂停
    EXPECT_TRUE(PlanTick(Tick(100, PlaybackState::Paused), {a.Entry()}).frames.empty());
    expectSilence(150, PlaybackState::Stopped);      // 暂停 → 停止
    expectSilence(200, PlaybackState::Paused);       // 停止 → 暂停
    expectSpectrumOnly(250);                          // 暂停 → 播放
    expectSilence(300, PlaybackState::Stopped);      // 播放 → 停止
    expectSpectrumOnly(350);                          // 停止 → 播放
}

// 登记时的状态是初始状态，不算一次进入：暂停中新建的订阅不收静音帧。
TEST(SpectrumSchedule, RegistrationStateIsNotATransition) {
    Sub paused = MakeSub("p", 30, 16, 1024, true, PlaybackState::Paused);
    EXPECT_TRUE(PlanTick(Tick(0, PlaybackState::Paused), {paused.Entry()}).frames.empty());
    const TickPlan resumed = PlanTick(Tick(50, PlaybackState::Playing), {paused.Entry()});
    ASSERT_EQ(resumed.frames.size(), 1u);
    EXPECT_FALSE(resumed.frames[0].silent);

    Sub stopped = MakeSub("s", 30, 16, 1024, true, PlaybackState::Stopped);
    EXPECT_TRUE(PlanTick(Tick(0, PlaybackState::Stopped), {stopped.Entry()}).frames.empty());
}

// 非播放态不出频谱帧、不建配置组，截止时刻保持不动。
TEST(SpectrumSchedule, NoSpectrumWhileNotPlaying) {
    Sub a = MakeSub("a", 30);
    PlanTick(Tick(0), {a.Entry()});
    PlanTick(Tick(50, PlaybackState::Paused), {a.Entry()});
    const Clock::time_point held = a.state.nextDue;
    for (int64_t t = 100; t < 600; t += 50) {
        const TickPlan plan = PlanTick(Tick(t, t < 350 ? PlaybackState::Paused : PlaybackState::Stopped), {a.Entry()});
        EXPECT_TRUE(plan.groups.empty()) << "t=" << t;
        for (const FrameDecision& f : plan.frames) EXPECT_TRUE(f.silent) << "t=" << t;
    }
    EXPECT_EQ(a.state.nextDue, held);
}

// A1.7：隐藏时静音帧只丢投递、状态照样推进；恢复可见后仍在暂停，不补发。
TEST(SpectrumSchedule, HiddenDropsDeliveryAndNeverResends) {
    Sub a = MakeSub("a", 30);
    PlanTick(Tick(0), {a.Entry()});

    a.visible = false;
    const TickPlan hiddenPause = PlanTick(Tick(50, PlaybackState::Paused), {a.Entry()});
    ASSERT_EQ(hiddenPause.frames.size(), 1u);
    EXPECT_TRUE(hiddenPause.frames[0].silent);
    EXPECT_FALSE(hiddenPause.frames[0].deliver);
    EXPECT_EQ(a.state.lastState, PlaybackState::Paused);

    a.visible = true;
    EXPECT_TRUE(PlanTick(Tick(100, PlaybackState::Paused), {a.Entry()}).frames.empty());
}

// 隐藏的播放中订阅：不建配置组、决策标为不投递，但截止时刻照常推进，恢复可见时不会积压。
TEST(SpectrumSchedule, HiddenSpectrumFrameCreatesNoGroupButAdvancesDeadline) {
    Sub a = MakeSub("a", 30);
    a.visible = false;
    const TickPlan plan = PlanTick(Tick(0), {a.Entry()});
    ASSERT_EQ(plan.frames.size(), 1u);
    EXPECT_FALSE(plan.frames[0].deliver);
    EXPECT_EQ(plan.frames[0].group, -1);
    EXPECT_TRUE(plan.groups.empty());
    EXPECT_EQ(a.state.nextDue, At(0) + kInterval30);

    a.visible = true;
    EXPECT_TRUE(PlanTick(Tick(10), {a.Entry()}).frames.empty());
}

// A1.7：限流或跳帧的拍里状态照样推进；静音帧不受限流与跳帧影响。
TEST(SpectrumSchedule, ThrottleAndSkipDoNotStallStateMachine) {
    Sub a = MakeSub("a", 30);
    PlanTick(Tick(0), {a.Entry()});

    const TickPlan pause = PlanTick(Tick(50, PlaybackState::Paused, true, true), {a.Entry()});
    ASSERT_EQ(pause.frames.size(), 1u);
    EXPECT_TRUE(pause.frames[0].silent);
    EXPECT_TRUE(pause.frames[0].deliver);
    EXPECT_EQ(a.state.lastState, PlaybackState::Paused);

    const TickPlan resumeThrottled = PlanTick(Tick(100, PlaybackState::Playing, true, false), {a.Entry()});
    EXPECT_TRUE(resumeThrottled.frames.empty());
    EXPECT_EQ(a.state.lastState, PlaybackState::Playing);

    const TickPlan next = PlanTick(Tick(150), {a.Entry()});
    ASSERT_EQ(next.frames.size(), 1u);
    EXPECT_FALSE(next.frames[0].silent);
}

// 被限流或跳帧扣下的帧不消耗截止时刻：下一个放行的拍立刻出帧。
TEST(SpectrumSchedule, WithheldFrameStaysDue) {
    Sub a = MakeSub("a", 30);
    PlanTick(Tick(0), {a.Entry()});
    const Clock::time_point due = a.state.nextDue;

    EXPECT_TRUE(PlanTick(Tick(47, PlaybackState::Playing, true), {a.Entry()}).frames.empty());
    EXPECT_EQ(a.state.nextDue, due);
    EXPECT_TRUE(PlanTick(Tick(60, PlaybackState::Playing, false, true), {a.Entry()}).frames.empty());
    EXPECT_EQ(a.state.nextDue, due);

    EXPECT_EQ(CountFrames(PlanTick(Tick(94), {a.Entry()}), "a"), 1u);
}

// backgroundThrottle 为 false 的订阅不受限流。
TEST(SpectrumSchedule, UnthrottledSubscriptionIgnoresVerdict) {
    Sub f = MakeSub("f", 30, 16, 1024, false);
    const TickPlan plan = PlanTick(Tick(0, PlaybackState::Playing, true), {f.Entry()});
    EXPECT_EQ(CountFrames(plan, "f"), 1u);
}

// A1.9：限流判定每拍一次、各订阅共用。两个 true 订阅在 47 ms 节拍、83 ms 放行门限下
// 各得 8–12 帧/秒且帧数相同（旧实现放行即写入，第二个订阅永远被拦）；false 订阅每拍都出帧。
TEST(SpectrumSchedule, SharedThrottleVerdictServesEveryThrottledSubscription) {
    Sub t1 = MakeSub("t1", 30);
    Sub t2 = MakeSub("t2", 30);
    Sub f = MakeSub("f", 30, 16, 1024, false);

    size_t framesT1 = 0, framesT2 = 0, framesF = 0, ticks = 0;
    int64_t lastPass = -1000;
    for (int64_t t = 0; t < 1000; t += 47) {
        const bool pass = (t - lastPass) >= 83;
        if (pass) lastPass = t;
        const TickPlan plan = PlanTick(Tick(t, PlaybackState::Playing, !pass), {t1.Entry(), t2.Entry(), f.Entry()});
        framesT1 += CountFrames(plan, "t1");
        framesT2 += CountFrames(plan, "t2");
        framesF += CountFrames(plan, "f");
        ++ticks;
    }
    EXPECT_EQ(framesT1, framesT2);
    EXPECT_GE(framesT1, 8u);
    EXPECT_LE(framesT1, 12u);
    EXPECT_EQ(framesF, ticks);
}

// 只差频率范围的两个订阅各算各的；范围相同的仍共用一组。
TEST(SpectrumSchedule, FrequencyRangeSplitsComputeGroups) {
    Sub full = MakeSub("full", 30, 48, 8192);
    Sub narrow = MakeSub("narrow", 30, 48, 8192);
    narrow.params.minFrequency = 500.0;
    narrow.params.maxFrequency = 2000.0;
    Sub narrowToo = MakeSub("narrowToo", 30, 48, 8192);
    narrowToo.params.minFrequency = 500.0;
    narrowToo.params.maxFrequency = 2000.0;
    const TickPlan plan = PlanTick(Tick(0), {full.Entry(), narrow.Entry(), narrowToo.Entry()});

    ASSERT_EQ(plan.groups.size(), 2u);
    const FrameDecision* ff = FindFrame(plan, "full");
    const FrameDecision* fn = FindFrame(plan, "narrow");
    const FrameDecision* fn2 = FindFrame(plan, "narrowToo");
    ASSERT_TRUE(ff && fn && fn2);
    EXPECT_NE(ff->group, fn->group);
    EXPECT_EQ(fn->group, fn2->group);
    EXPECT_EQ(plan.groups[ff->group].minFrequency, 20.0);
    EXPECT_EQ(plan.groups[ff->group].maxFrequency, 0.0);
    EXPECT_EQ(plan.groups[fn->group].minFrequency, 500.0);
    EXPECT_EQ(plan.groups[fn->group].maxFrequency, 2000.0);
}

// ---- 批 3：到期容差、走拍与合并闸 ----

namespace {

constexpr auto kBeat60 = std::chrono::nanoseconds(1000000000 / 60);

TickInput TickAt(Clock::time_point now, Clock::duration halfBeat) {
    TickInput in;
    in.now = now;
    in.playback = PlaybackState::Playing;
    in.maxFftSize = 65536;
    in.halfBeat = halfBeat;
    return in;
}

// 60 Hz 拍，每拍带 0、−2、+1、−1、+2 ms 循环的确定性抖动，模拟 10 秒，返回该订阅的帧数。
size_t CountFramesOverTenSeconds(int fps, Clock::duration halfBeat) {
    Sub sub = MakeSub("s", fps);
    size_t frames = 0;
    for (int64_t k = 0; k < 600; ++k) {
        const auto jitter = std::chrono::milliseconds((k * 3 + 2) % 5 - 2);
        const Clock::time_point now = At(0) + k * kBeat60 + jitter;
        frames += CountFrames(PlanTick(TickAt(now, halfBeat), {sub.Entry()}), "s");
    }
    return frames;
}

}  // namespace

// A3.5：容差不抬高长期帧率。拍长等于订阅间隔时，严格比较会让早到的拍推迟一整拍、
// 落后超过一帧再重置，丢帧；半拍容差下 30 / 45 / 60 fps 各得 300 / 450 / 600 帧。
TEST(SpectrumSchedule, HalfBeatToleranceKeepsLongTermRate) {
    const Clock::duration halfBeat = kBeat60 / 2;
    EXPECT_NEAR(static_cast<double>(CountFramesOverTenSeconds(30, halfBeat)), 300.0, 1.0);
    EXPECT_NEAR(static_cast<double>(CountFramesOverTenSeconds(45, halfBeat)), 450.0, 1.0);
    EXPECT_NEAR(static_cast<double>(CountFramesOverTenSeconds(60, halfBeat)), 600.0, 1.0);
    EXPECT_LT(CountFramesOverTenSeconds(60, Clock::duration::zero()), 590u);
}

// A3.5：恰好早半拍时出帧（含等号），再早 1 µs 不出。
TEST(SpectrumSchedule, HalfBeatToleranceBoundaryIsInclusive) {
    const Clock::duration halfBeat = kBeat60 / 2;

    Sub onEdge = MakeSub("e", 60);
    PlanTick(TickAt(At(0), halfBeat), {onEdge.Entry()});
    const Clock::time_point due = onEdge.state.nextDue;
    EXPECT_EQ(CountFrames(PlanTick(TickAt(due - halfBeat, halfBeat), {onEdge.Entry()}), "e"), 1u);

    Sub early = MakeSub("x", 60);
    PlanTick(TickAt(At(0), halfBeat), {early.Entry()});
    const Clock::time_point dueEarly = early.state.nextDue;
    const TickPlan plan =
        PlanTick(TickAt(dueEarly - halfBeat - std::chrono::microseconds(1), halfBeat), {early.Entry()});
    EXPECT_EQ(CountFrames(plan, "x"), 0u);
    EXPECT_EQ(early.state.nextDue, dueEarly);
}

// A3.5：halfBeat 为 0 时就是严格比较：截止时刻前 1 ns 不出，恰到截止时刻出。
TEST(SpectrumSchedule, ZeroHalfBeatIsStrictComparison) {
    Sub a = MakeSub("a", 60);
    PlanTick(TickAt(At(0), Clock::duration::zero()), {a.Entry()});
    const Clock::time_point due = a.state.nextDue;
    EXPECT_EQ(CountFrames(PlanTick(TickAt(due - std::chrono::nanoseconds(1), Clock::duration::zero()),
                                   {a.Entry()}),
                          "a"),
              0u);
    EXPECT_EQ(CountFrames(PlanTick(TickAt(due, Clock::duration::zero()), {a.Entry()}), "a"), 1u);
}

// A3.5：NextBeat 正常时从上一个截止时刻推进一拍；落后恰好一拍仍按截止时刻推进，
// 落后超过一拍时从当前时刻推进一拍、不补拍。
TEST(SpectrumSchedule, NextBeatAdvancesFromDeadlineAndResetsWhenBehind) {
    const Clock::time_point deadline = At(100);
    EXPECT_EQ(NextBeat(deadline, kBeat60, deadline + std::chrono::milliseconds(3)), deadline + kBeat60);
    EXPECT_EQ(NextBeat(deadline, kBeat60, deadline - std::chrono::milliseconds(3)), deadline + kBeat60);
    EXPECT_EQ(NextBeat(deadline, kBeat60, deadline + kBeat60), deadline + kBeat60);
    const Clock::time_point late = deadline + kBeat60 + std::chrono::nanoseconds(1);
    EXPECT_EQ(NextBeat(deadline, kBeat60, late), late + kBeat60);
}

// A3.5：合并闸。已投递的拍没执行前再到拍不投；执行清闸后可再投。
// 停—起序列：投递后线程停了、回调从未执行，新线程 Reset 后照常能投。
TEST(SpectrumSchedule, BeatGateCoalescesAndSurvivesThreadRestart) {
    BeatGate gate;
    EXPECT_FALSE(gate.IsArmed());
    EXPECT_TRUE(gate.TryArm());
    EXPECT_FALSE(gate.TryArm());
    EXPECT_TRUE(gate.IsArmed());
    gate.Disarm();
    EXPECT_TRUE(gate.TryArm());

    gate.Reset();
    EXPECT_TRUE(gate.TryArm());
    EXPECT_FALSE(gate.TryArm());
}

// ---- 原始频点（docs/audio-visualization/SPECTRUM_BINS_SPEC.md B4 / B7、§8 A-B5） ----

using fb2k_spectrum::SpectrumChannels;
using fb2k_spectrum::SpectrumOutput;

namespace {

Sub MakeBinsSub(const char* id, int fftSize, int bands = 48,
                SpectrumChannels channels = SpectrumChannels::Mix) {
    Sub sub = MakeSub(id, 30, bands, fftSize);
    sub.params.output = SpectrumOutput::Bins;
    sub.params.scale = SpectrumScale::Db;
    sub.params.channels = channels;
    return sub;
}

}  // namespace

// bins 按请求的点数算，带数再多也不提升；上限照样截。频带输出的规则不变。
TEST(SpectrumSchedule, BinsKeepTheRequestedFftSize) {
    EXPECT_EQ(EffectiveFftSize(1024, 256, SpectrumOutput::Bins, 65536), 1024);
    EXPECT_EQ(EffectiveFftSize(4096, 1024, SpectrumOutput::Bins, 65536), 4096);
    EXPECT_EQ(EffectiveFftSize(65536, 8, SpectrumOutput::Bins, 16384), 16384);
    EXPECT_EQ(EffectiveFftSize(4096, 1024, SpectrumOutput::Bands, 65536), 8192);
    EXPECT_EQ(EffectiveFftSize(1024, 32, SpectrumOutput::Bands, 65536), 4096);

    Sub bins = MakeBinsSub("bins", 4096, 1024);
    const TickPlan plan = PlanTick(Tick(0), {bins.Entry()});
    ASSERT_EQ(plan.groups.size(), 1u);
    EXPECT_EQ(plan.groups[0].fftSize, 4096);
    EXPECT_EQ(plan.groups[0].output, SpectrumOutput::Bins);
    EXPECT_EQ(plan.groups[0].bands, 0);
    EXPECT_EQ(plan.groups[0].scale, SpectrumScale::Db);
}

// 只差 bands 的两个 bins 订阅算一组：bands 不参与 bins 的计算。
TEST(SpectrumSchedule, BinsGroupIgnoresBands) {
    Sub a = MakeBinsSub("a", 8192, 48);
    Sub b = MakeBinsSub("b", 8192, 1024);
    const TickPlan plan = PlanTick(Tick(0), {a.Entry(), b.Entry()});
    ASSERT_EQ(plan.groups.size(), 1u);
    const FrameDecision* fa = FindFrame(plan, "a");
    const FrameDecision* fb = FindFrame(plan, "b");
    ASSERT_TRUE(fa && fb);
    EXPECT_EQ(fa->group, fb->group);
}

// 点数相同的 bins 与频带订阅分两组；只差 channels 的 bins 订阅也分两组。
TEST(SpectrumSchedule, BinsSplitFromBandsAndByChannels) {
    Sub bands = MakeSub("bands", 30, 48, 8192);
    bands.params.scale = SpectrumScale::Db;
    Sub mix = MakeBinsSub("mix", 8192);
    Sub stereo = MakeBinsSub("stereo", 8192, 48, SpectrumChannels::Stereo);
    const TickPlan plan = PlanTick(Tick(0), {bands.Entry(), mix.Entry(), stereo.Entry()});

    ASSERT_EQ(plan.groups.size(), 3u);
    const FrameDecision* fBands = FindFrame(plan, "bands");
    const FrameDecision* fMix = FindFrame(plan, "mix");
    const FrameDecision* fStereo = FindFrame(plan, "stereo");
    ASSERT_TRUE(fBands && fMix && fStereo);
    EXPECT_EQ(plan.groups[fBands->group].output, SpectrumOutput::Bands);
    EXPECT_EQ(plan.groups[fBands->group].fftSize, 8192);
    EXPECT_EQ(plan.groups[fMix->group].channels, SpectrumChannels::Mix);
    EXPECT_EQ(plan.groups[fStereo->group].channels, SpectrumChannels::Stereo);
}

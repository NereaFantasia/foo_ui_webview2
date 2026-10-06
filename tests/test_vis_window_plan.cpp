// test_vis_window_plan.cpp - 可视化窗口的取数规划
// （docs/audio-timing/SPEC.md T8、§5.1 窗口规划、§10 A-V7）。
//
// 窗口是截至可视化时钟 now 的最近 N 个样本：时钟走过一整窗之后恰好一段，超过 1 s 就拆成
// 首尾相接的几段；时钟还没走满一窗时从 0 取到 now、缺的在前面补零；时钟为 0 时全是零。
#include "pch.h"
#include "../src/utils/VisWindowPlan.h"

#include <cmath>
#include <limits>

using vis_window::Plan;
using vis_window::PlanWindow;

namespace {

constexpr double kMaxRequestSeconds = 1.0;
constexpr unsigned kRates[] = {44100, 48000};
constexpr uint64_t kWindows[] = {4096, 8192, 16384, 32768, 65536};

uint64_t FetchedSamples(const Plan& plan) {
    uint64_t total = 0;
    for (const auto& segment : plan.segments) total += segment.count;
    return total;
}

// 与 PlanWindow 无关的写法算出的 now 对应的样本位置，用来核对最后一段止于 now。
uint64_t EndSample(double now, unsigned rate) {
    return static_cast<uint64_t>(std::llround(now * rate));
}

// 所有情形共同的约束：各段非空、首尾相接、单段不超过 1 s，取回的样本数加补零等于窗口长度，
// 有段时最后一段止于 now、第一段从 max(0, now − 窗长) 起。
void ExpectWellFormed(const Plan& plan, double now, uint64_t window, unsigned rate) {
    SCOPED_TRACE(::testing::Message() << "now=" << now << " window=" << window << " rate=" << rate);
    EXPECT_EQ(FetchedSamples(plan) + plan.zeroPad, window);
    for (size_t i = 0; i < plan.segments.size(); ++i) {
        EXPECT_GT(plan.segments[i].count, 0u);
        EXPECT_LE(plan.segments[i].count, static_cast<uint64_t>(rate * kMaxRequestSeconds));
        if (i > 0) {
            EXPECT_EQ(plan.segments[i].start, plan.segments[i - 1].start + plan.segments[i - 1].count);
        }
    }
    if (!plan.segments.empty()) {
        const uint64_t end = EndSample(now, rate);
        EXPECT_EQ(plan.segments.back().start + plan.segments.back().count, end);
        EXPECT_EQ(plan.segments.front().start, end > window ? end - window : 0u);
    }
}

}  // namespace

// 时钟走过一整窗、窗长不到 1 s：恰好一段 [end − N, end)，不补零。
TEST(VisWindowPlan, FullWindowUnderOneSecondIsOneSegment) {
    const Plan plan = PlanWindow(10.0, 16384, 44100, kMaxRequestSeconds);
    ASSERT_EQ(plan.segments.size(), 1u);
    EXPECT_EQ(plan.segments[0].start, 441000u - 16384u);
    EXPECT_EQ(plan.segments[0].count, 16384u);
    EXPECT_EQ(plan.zeroPad, 0u);
}

// 65536 点在 44.1 与 48 kHz 下超过 1 s：拆成两段等长的请求，首尾相接、总长 N，止于 now。
// 48 kHz 下正是临时构建验证过的两段各 32768 个样本（0.68 s）。
TEST(VisWindowPlan, WindowOverOneSecondSplitsIntoEqualContiguousSegments) {
    const Plan at48 = PlanWindow(30.0, 65536, 48000, kMaxRequestSeconds);
    ASSERT_EQ(at48.segments.size(), 2u);
    EXPECT_EQ(at48.segments[0].start, 1440000u - 65536u);
    EXPECT_EQ(at48.segments[0].count, 32768u);
    EXPECT_EQ(at48.segments[1].start, 1440000u - 32768u);
    EXPECT_EQ(at48.segments[1].count, 32768u);
    EXPECT_EQ(at48.zeroPad, 0u);

    const Plan at44 = PlanWindow(30.0, 65536, 44100, kMaxRequestSeconds);
    ASSERT_EQ(at44.segments.size(), 2u);
    ExpectWellFormed(at44, 30.0, 65536, 44100);
}

// 除不尽时多出的样本给最早的段：8 kHz 下 65536 点要 9 段，65536 = 9 × 7281 + 7。
TEST(VisWindowPlan, RemainderGoesToEarliestSegments) {
    const Plan plan = PlanWindow(60.0, 65536, 8000, kMaxRequestSeconds);
    ASSERT_EQ(plan.segments.size(), 9u);
    for (size_t i = 0; i < plan.segments.size(); ++i) {
        EXPECT_EQ(plan.segments[i].count, i < 7 ? 7282u : 7281u) << "segment " << i;
    }
    ExpectWellFormed(plan, 60.0, 65536, 8000);
}

// 时钟还没走满一窗：从 0 取到 now，补零数 = N − 取回的样本数。
TEST(VisWindowPlan, BeforeClockCoversWindowFetchesFromZeroAndPadsFront) {
    const Plan plan = PlanWindow(0.25, 16384, 44100, kMaxRequestSeconds);
    ASSERT_EQ(plan.segments.size(), 1u);
    EXPECT_EQ(plan.segments[0].start, 0u);
    EXPECT_EQ(plan.segments[0].count, 11025u);
    EXPECT_EQ(plan.zeroPad, 16384u - 11025u);
}

// 没走满一窗、但已走过的部分本身超过 1 s：仍从 0 起，按 1 s 的上限拆开，前面补零。
TEST(VisWindowPlan, PartialWindowOverOneSecondStillSplits) {
    const Plan plan = PlanWindow(1.2, 65536, 44100, kMaxRequestSeconds);
    ASSERT_EQ(plan.segments.size(), 2u);
    EXPECT_EQ(plan.segments[0].start, 0u);
    EXPECT_EQ(plan.zeroPad, 65536u - 52920u);
    ExpectWellFormed(plan, 1.2, 65536, 44100);
}

// 时钟停在 0（复位后约 200 ms）：不取数，整窗是零。
TEST(VisWindowPlan, ClockAtZeroIsAllZeros) {
    for (const unsigned rate : kRates) {
        for (const uint64_t window : kWindows) {
            const Plan plan = PlanWindow(0.0, window, rate, kMaxRequestSeconds);
            EXPECT_TRUE(plan.segments.empty());
            EXPECT_EQ(plan.zeroPad, window);
        }
    }
}

// 两种采样率、4096 至 65536 点，时钟从刚离开 0、窗口中段、恰好一窗到远超一窗，都满足共同约束。
TEST(VisWindowPlan, EveryRateAndWindowSizeIsWellFormed) {
    for (const unsigned rate : kRates) {
        for (const uint64_t window : kWindows) {
            const double windowSeconds = static_cast<double>(window) / rate;
            for (const double now : {1.0 / rate, windowSeconds / 2, windowSeconds, windowSeconds + 0.0001, 3.7, 600.0}) {
                const Plan plan = PlanWindow(now, window, rate, kMaxRequestSeconds);
                ExpectWellFormed(plan, now, window, rate);
                const uint64_t end = EndSample(now, rate);
                EXPECT_EQ(plan.zeroPad, end >= window ? 0u : window - end);
            }
        }
    }
}

// now 按最近的样本取整：48 kHz 下 1.00001 s 是第 48000.48 个样本，窗口止于 48000。
TEST(VisWindowPlan, NowRoundsToNearestSample) {
    const Plan plan = PlanWindow(1.00001, 4096, 48000, kMaxRequestSeconds);
    ASSERT_EQ(plan.segments.size(), 1u);
    EXPECT_EQ(plan.segments[0].start + plan.segments[0].count, 48000u);
}

// 采样率或窗口为 0、now 为负或 NaN：整窗是零，不产生请求；上限不大于 0 时不拆。
TEST(VisWindowPlan, DegenerateInputs) {
    EXPECT_TRUE(PlanWindow(5.0, 4096, 0, kMaxRequestSeconds).segments.empty());
    EXPECT_EQ(PlanWindow(5.0, 4096, 0, kMaxRequestSeconds).zeroPad, 4096u);
    EXPECT_TRUE(PlanWindow(5.0, 0, 44100, kMaxRequestSeconds).segments.empty());
    EXPECT_EQ(PlanWindow(5.0, 0, 44100, kMaxRequestSeconds).zeroPad, 0u);
    EXPECT_TRUE(PlanWindow(-1.0, 4096, 44100, kMaxRequestSeconds).segments.empty());
    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_TRUE(PlanWindow(nan, 4096, 44100, kMaxRequestSeconds).segments.empty());
    EXPECT_EQ(PlanWindow(nan, 4096, 44100, kMaxRequestSeconds).zeroPad, 4096u);

    const Plan unsplit = PlanWindow(30.0, 65536, 44100, 0.0);
    ASSERT_EQ(unsplit.segments.size(), 1u);
    EXPECT_EQ(unsplit.segments[0].count, 65536u);
}

namespace {

constexpr size_t kGuard = 16;
constexpr size_t kMaxShift = 4;

// 两个不成整数比的正弦相加，挪几个样本都不会与自己重合
std::vector<float> Signal(size_t frames, unsigned channels) {
    std::vector<float> out(frames * channels);
    for (size_t n = 0; n < frames; ++n) {
        for (unsigned c = 0; c < channels; ++c) {
            out[n * channels + c] = static_cast<float>(std::sin(0.1 * n + c) + 0.3 * std::sin(0.37 * n));
        }
    }
    return out;
}

// 已拼好的是 signal[0, seam)，后一段本应从 seam - guard 起取 guard + count 帧，实际起点偏了 offset 帧
size_t SeamFor(const std::vector<float>& signal, unsigned channels, size_t seam, long offset) {
    const size_t start = static_cast<size_t>(static_cast<long>(seam - kGuard) + offset);
    const size_t pieceFrames = kGuard + 64;
    return vis_window::FindSeam(signal.data(), seam, signal.data() + start * channels, pieceFrames, channels,
                                kGuard, kMaxShift);
}

}  // namespace

// 后一段起点准确时接缝就在 guard；起点早一帧、晚一帧时接缝跟着挪，接出来的第一帧正是 seam。
TEST(VisWindowSeam, FollowsTheActualStartOfTheNextPiece) {
    for (const unsigned channels : {1u, 2u}) {
        const std::vector<float> signal = Signal(400, channels);
        EXPECT_EQ(SeamFor(signal, channels, 200, 0), kGuard);
        EXPECT_EQ(SeamFor(signal, channels, 200, -1), kGuard + 1);
        EXPECT_EQ(SeamFor(signal, channels, 200, 1), kGuard - 1);
        EXPECT_EQ(SeamFor(signal, channels, 200, -static_cast<long>(kMaxShift)), kGuard + kMaxShift);
        EXPECT_EQ(SeamFor(signal, channels, 200, static_cast<long>(kMaxShift)), kGuard - kMaxShift);
    }
}

// 数字静音处处一样，取 guard；多取的帧不比挪动范围多、声道数为 0 或还没拼好任何帧时比不了，也取 guard。
TEST(VisWindowSeam, FallsBackToGuardWhenItCannotCompare) {
    const std::vector<float> silence(200, 0.0f);
    EXPECT_EQ(vis_window::FindSeam(silence.data(), 100, silence.data() + 90, 80, 1, kGuard, kMaxShift), kGuard);
    const std::vector<float> signal = Signal(400, 1);
    EXPECT_EQ(vis_window::FindSeam(signal.data(), 200, signal.data() + 197, 60, 1, 4, kMaxShift), 4u);
    EXPECT_EQ(vis_window::FindSeam(signal.data(), 200, signal.data() + 184, 60, 0, kGuard, kMaxShift), kGuard);
    EXPECT_EQ(vis_window::FindSeam(signal.data(), 0, signal.data(), 60, 1, kGuard, kMaxShift), kGuard);
}

// 周期为 3 帧的纯音：挪一整个周期的位置也差不多一样好，不论取中哪个，接出来的样本都与真正的接续相同。
TEST(VisWindowSeam, PeriodicToneContinuesWithTheSameSamples) {
    std::vector<float> tone(400);
    for (size_t n = 0; n < tone.size(); ++n) tone[n] = static_cast<float>(std::sin(2.0 * 3.14159265358979 * n / 3.0));
    const size_t seam = 200;
    for (const long offset : {-2L, -1L, 0L, 1L, 2L}) {
        const size_t start = static_cast<size_t>(static_cast<long>(seam - kGuard) + offset);
        const size_t k = vis_window::FindSeam(tone.data(), seam, tone.data() + start, kGuard + 64, 1, kGuard, kMaxShift);
        for (size_t j = 0; j < 8; ++j) {
            EXPECT_NEAR(tone[start + k + j], tone[seam + j], 1e-5) << "offset=" << offset << " j=" << j;
        }
    }
}

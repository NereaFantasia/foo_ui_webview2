// test_taskbar_progress_policy.cpp - 播放状态到任务栏进度条的映射
//
// 直接链接生产头 taskbar_progress::Decide，不在测试内重写被测逻辑。
// 断言依据是 docs/preferences/DESIGN.md 第 11.3 节：播放 → normal 按已播比例，
// 暂停 → paused，停止或时长未知 → noprogress。
#include "pch.h"
#include "../src/window/TaskbarProgressPolicy.h"

using namespace taskbar_progress;

TEST(TaskbarProgressPolicy, PlayingMapsToNormalWithPlayedRatio) {
    const Decision d = Decide("playing", 30.0, 120.0);
    EXPECT_EQ(d.mode, Mode::Normal);
    EXPECT_EQ(d.completed, 250u);
}

TEST(TaskbarProgressPolicy, PausedKeepsRatioInPausedMode) {
    const Decision d = Decide("paused", 90.0, 120.0);
    EXPECT_EQ(d.mode, Mode::Paused);
    EXPECT_EQ(d.completed, 750u);
}

TEST(TaskbarProgressPolicy, StoppedOrUnknownStateHidesProgress) {
    EXPECT_EQ(Decide("stopped", 30.0, 120.0), Decision{});
    EXPECT_EQ(Decide("", 30.0, 120.0), Decision{});
    EXPECT_EQ(Decide("loading", 30.0, 120.0), Decision{});
}

TEST(TaskbarProgressPolicy, UnknownLengthHidesProgressEvenWhilePlaying) {
    EXPECT_EQ(Decide("playing", 30.0, 0.0), Decision{});
    EXPECT_EQ(Decide("playing", 30.0, -1.0), Decision{});
    EXPECT_EQ(Decide("paused", 30.0, 0.0), Decision{});
}

TEST(TaskbarProgressPolicy, RatioIsClampedAndRounded) {
    // 位置略超时长（解码器尾部）钳到满格，不回绕到 0。
    EXPECT_EQ(Decide("playing", 121.0, 120.0).completed, 1000u);
    EXPECT_EQ(Decide("playing", -5.0, 120.0).completed, 0u);
    // 千分位四舍五入：1/3 → 333，2/3 → 667。
    EXPECT_EQ(Decide("playing", 1.0, 3.0).completed, 333u);
    EXPECT_EQ(Decide("playing", 2.0, 3.0).completed, 667u);
    EXPECT_EQ(Decide("playing", 0.0, 120.0).completed, 0u);
    EXPECT_EQ(Decide("playing", 120.0, 120.0).completed, 1000u);
}

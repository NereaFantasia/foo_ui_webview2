// test_jit_native_observation.cpp - 原生接管的观察版本
//
// 直接链接生产头 core/JitNativeObservation.h；队列与列表计数读共享的 content_version::Counters，
// 回调接入在 SDK 一侧，不在这里测。
#include "pch.h"
#include "core/JitNativeObservation.h"

using jit_native::Observer;
using jit_native::Settings;

TEST(JitNativeObservation, SameSettingValueNotificationsDoNotInvalidate) {
    content_version::Counters content;
    Observer observer(content);
    observer.SettingsRead(Settings{});  // 第一次读取只建立基线
    const auto stamp = observer.Capture({});
    observer.SettingsRead(Settings{});
    EXPECT_TRUE(observer.Unchanged(stamp));
    observer.SettingsRead(Settings{0, true, false, false});
    EXPECT_FALSE(observer.Unchanged(stamp));
}

// 改了又改回：中间那次变化已经计入，不能被复原洗掉。
TEST(JitNativeObservation, SettingChangedAndRevertedStillInvalidates) {
    content_version::Counters content;
    Observer observer(content);
    observer.SettingsRead(Settings{});
    const auto stamp = observer.Capture({});
    observer.SettingsRead(Settings{1, false, false, false});
    observer.SettingsRead(Settings{});
    EXPECT_FALSE(observer.Unchanged(stamp));
}

TEST(JitNativeObservation, UnreadableSettingsCountAsAChange) {
    content_version::Counters content;
    Observer observer(content);
    observer.SettingsRead(Settings{});
    const auto stamp = observer.Capture({});
    observer.SettingsRead(std::nullopt);
    EXPECT_FALSE(observer.Unchanged(stamp));
    EXPECT_FALSE(observer.LastSettings());
}

// 等量替换：行数不变也要失效；版本按 GUID 计，不关心的列表变化不影响快照。
TEST(JitNativeObservation, ListContentChangesInvalidateOnlyWatchedLists) {
    content_version::Counters content;
    Observer observer(content);
    const auto stamp = observer.Capture({"source", "manual"});
    content.ListChanged("other");
    EXPECT_TRUE(observer.Unchanged(stamp));
    content.ListChanged("manual");
    EXPECT_FALSE(observer.Unchanged(stamp));
}

TEST(JitNativeObservation, PlaybackAndQueueFactsInvalidate) {
    content_version::Counters content;
    Observer observer(content);
    auto stamp = observer.Capture({});
    observer.Stopped(false);  // 非用户停止本身不计；它总伴随起播或换曲
    EXPECT_TRUE(observer.Unchanged(stamp));
    observer.Stopped(true);
    EXPECT_FALSE(observer.Unchanged(stamp));
    stamp = observer.Capture({});
    content.QueueChanged();
    EXPECT_FALSE(observer.Unchanged(stamp));
    stamp = observer.Capture({});
    observer.Starting();
    EXPECT_FALSE(observer.Unchanged(stamp));
    stamp = observer.Capture({});
    observer.TrackStarted();
    EXPECT_FALSE(observer.Unchanged(stamp));
}

TEST(JitNativeObservation, OnlyDefaultOrderWithAllFollowingOffIsSupported) {
    EXPECT_TRUE(Settings{}.Supported());
    EXPECT_FALSE((Settings{1, false, false, false}.Supported()));
    EXPECT_FALSE((Settings{0, true, false, false}.Supported()));
    EXPECT_FALSE((Settings{0, false, true, false}.Supported()));
    EXPECT_FALSE((Settings{0, false, false, true}.Supported()));
}

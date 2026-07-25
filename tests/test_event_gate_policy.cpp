#include "pch.h"
#include "webview/EventGatePolicy.h"

using event_gate::Action;
using event_gate::Classify;
using event_gate::LatestBuffer;

// ============================================
// 事件分类契约
// ============================================

TEST(EventGatePolicyTest, ControlPlaneEventsPassThrough) {
    // 前缀直通
    EXPECT_EQ(Classify("menu:show"), Action::Pass);
    EXPECT_EQ(Classify("menu:__submenuOpened"), Action::Pass);
    EXPECT_EQ(Classify("jitQueue:needNext"), Action::Pass);
    EXPECT_EQ(Classify("jitQueue:trackChanged"), Action::Pass);
    EXPECT_EQ(Classify("tray:click"), Action::Pass);
    EXPECT_EQ(Classify("tray:beforeContextMenu"), Action::Pass);
    EXPECT_EQ(Classify("port:message"), Action::Pass);
    // 精确名直通
    EXPECT_EQ(Classify("taskbar:buttonClicked"), Action::Pass);
    EXPECT_EQ(Classify("keyboard:hotkey"), Action::Pass);
    EXPECT_EQ(Classify("app:beforeQuit"), Action::Pass);
    EXPECT_EQ(Classify("window:beforeClose"), Action::Pass);
    EXPECT_EQ(Classify("webview:processFailed"), Action::Pass);
    EXPECT_EQ(Classify("window:message"), Action::Pass);
    EXPECT_EQ(Classify("state:changed"), Action::Pass);
    EXPECT_EQ(Classify("state:deleted"), Action::Pass);
    EXPECT_EQ(Classify("ui:menuItemClicked"), Action::Pass);
}

TEST(EventGatePolicyTest, RegenerableStreamsDrop) {
    EXPECT_EQ(Classify("audio:spectrum"), Action::Drop);
    EXPECT_EQ(Classify("playback:time"), Action::Drop);
    EXPECT_EQ(Classify("playback:timeHighRes"), Action::Drop);
    EXPECT_EQ(Classify("window:hoverStateChanged"), Action::Drop);
    EXPECT_EQ(Classify("cursor:hiddenChanged"), Action::Drop);
}

TEST(EventGatePolicyTest, SnapshotAndNotifyEventsBufferLatest) {
    EXPECT_EQ(Classify("playback:stateChanged"), Action::Latest);
    EXPECT_EQ(Classify("playback:trackChanged"), Action::Latest);
    EXPECT_EQ(Classify("playback:volumeChanged"), Action::Latest);
    EXPECT_EQ(Classify("playlist:itemsAdded"), Action::Latest);
    EXPECT_EQ(Classify("playlist:activated"), Action::Latest);
    EXPECT_EQ(Classify("library:itemsRemoved"), Action::Latest);
    EXPECT_EQ(Classify("metadb:changed"), Action::Latest);
    EXPECT_EQ(Classify("window:stateChanged"), Action::Latest);
    EXPECT_EQ(Classify("window:activated"), Action::Latest);
    EXPECT_EQ(Classify("window:dpiChanged"), Action::Latest);
    EXPECT_EQ(Classify("system:themeChanged"), Action::Latest);
}

TEST(EventGatePolicyTest, UnknownEventsDefaultToLatest) {
    // 未知事件名兜底：有界（每名一槽）且无损（恢复时重放）
    EXPECT_EQ(Classify("future:someNewEvent"), Action::Latest);
    EXPECT_EQ(Classify(""), Action::Latest);
    // 相近但不等于直通前缀的名字不得误判
    EXPECT_EQ(Classify("menus:notMenuPrefix"), Action::Latest);
    EXPECT_EQ(Classify("trays:notTrayPrefix"), Action::Latest);
}

// ============================================
// LatestBuffer 语义（设计 §6.2 重放顺序契约）
// ============================================

TEST(EventGateLatestBufferTest, KeepsOnlyLatestPayloadPerEvent) {
    LatestBuffer buffer;
    buffer.Put("playback:stateChanged", L"{\"v\":1}");
    buffer.Put("playback:stateChanged", L"{\"v\":2}");
    buffer.Put("playback:stateChanged", L"{\"v\":3}");

    ASSERT_EQ(buffer.Size(), 1u);
    auto entries = buffer.Flush();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].event, "playback:stateChanged");
    EXPECT_EQ(entries[0].payload, L"{\"v\":3}");
}

TEST(EventGateLatestBufferTest, ReplayOrderFollowsLastArrival) {
    LatestBuffer buffer;
    buffer.Put("playback:trackChanged", L"track-old");
    buffer.Put("playback:stateChanged", L"state");
    buffer.Put("playlist:itemsAdded", L"items");
    // trackChanged 再次到达 → 移到队尾（最后到达顺序）
    buffer.Put("playback:trackChanged", L"track-new");

    auto entries = buffer.Flush();
    ASSERT_EQ(entries.size(), 3u);
    EXPECT_EQ(entries[0].event, "playback:stateChanged");
    EXPECT_EQ(entries[1].event, "playlist:itemsAdded");
    EXPECT_EQ(entries[2].event, "playback:trackChanged");
    EXPECT_EQ(entries[2].payload, L"track-new");
}

TEST(EventGateLatestBufferTest, FlushEmptiesBuffer) {
    LatestBuffer buffer;
    EXPECT_TRUE(buffer.Empty());

    buffer.Put("a:b", L"1");
    buffer.Put("c:d", L"2");
    EXPECT_FALSE(buffer.Empty());

    auto first = buffer.Flush();
    EXPECT_EQ(first.size(), 2u);
    EXPECT_TRUE(buffer.Empty());

    auto second = buffer.Flush();
    EXPECT_TRUE(second.empty());
}

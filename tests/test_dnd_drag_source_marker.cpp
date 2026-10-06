// test_dnd_drag_source_marker.cpp - where a drag came from.
//
// The marker is a hint read from bytes any process can write, so the decoder
// must reject anything that is not exactly its own layout, and a marker from
// another process must never read as this window's own drag.
#include "pch.h"
#include "../src/webview/dnd/DragSession.h"
#include "../src/webview/dnd/DragSourceMarker.h"

#include <string>
#include <vector>

using fb2k_dnd::ClassifyDragSource;
using fb2k_dnd::DecodeDragSourceMarker;
using fb2k_dnd::DragSource;
using fb2k_dnd::DragSourceMarker;
using fb2k_dnd::DragSourceToWire;
using fb2k_dnd::EncodeDragSourceMarker;

namespace {

DragSourceMarker MakeMarker(uint32_t pid, uint64_t window) {
    DragSourceMarker marker;
    marker.processId = pid;
    marker.window = window;
    return marker;
}

}  // namespace

// --- Encoding ---

TEST(DragSourceMarker, RoundTripsProcessAndWindow) {
    const auto bytes = EncodeDragSourceMarker(MakeMarker(4242, 0x0000'7FF6'1234'5678ull));
    const auto decoded = DecodeDragSourceMarker(bytes.data(), bytes.size());
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->processId, 4242u);
    EXPECT_EQ(decoded->window, 0x0000'7FF6'1234'5678ull);
}

TEST(DragSourceMarker, AcceptsTrailingBytes) {
    // GlobalSize may report more than was allocated.
    auto bytes = EncodeDragSourceMarker(MakeMarker(7, 9));
    bytes.resize(bytes.size() + 8, 0xCC);
    const auto decoded = DecodeDragSourceMarker(bytes.data(), bytes.size());
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->processId, 7u);
    EXPECT_EQ(decoded->window, 9u);
}

TEST(DragSourceMarker, RejectsShortBlock) {
    const auto bytes = EncodeDragSourceMarker(MakeMarker(7, 9));
    EXPECT_FALSE(DecodeDragSourceMarker(bytes.data(), bytes.size() - 1).has_value());
    EXPECT_FALSE(DecodeDragSourceMarker(nullptr, bytes.size()).has_value());
}

TEST(DragSourceMarker, RejectsForeignTag) {
    auto bytes = EncodeDragSourceMarker(MakeMarker(7, 9));
    bytes[0] = 'x';
    EXPECT_FALSE(DecodeDragSourceMarker(bytes.data(), bytes.size()).has_value());
}

TEST(DragSourceMarker, RejectsUnknownVersion) {
    auto bytes = EncodeDragSourceMarker(MakeMarker(7, 9));
    bytes[4] = 2;
    EXPECT_FALSE(DecodeDragSourceMarker(bytes.data(), bytes.size()).has_value());
}

// --- Classification ---

TEST(DragSourceMarker, SameProcessSameWindowIsSelf) {
    EXPECT_EQ(ClassifyDragSource(MakeMarker(10, 20), 10, 20), DragSource::Self);
}

TEST(DragSourceMarker, SameProcessOtherWindowIsOtherWindow) {
    EXPECT_EQ(ClassifyDragSource(MakeMarker(10, 21), 10, 20), DragSource::OtherWindow);
}

TEST(DragSourceMarker, OtherProcessIsExternalEvenWithTheSameWindowValue) {
    // Another foobar2000 process may reuse an HWND value; only the process id
    // says whether the window is ours.
    EXPECT_EQ(ClassifyDragSource(MakeMarker(11, 20), 10, 20), DragSource::External);
}

TEST(DragSourceMarker, NoMarkerIsExternal) {
    EXPECT_EQ(ClassifyDragSource(std::nullopt, 10, 20), DragSource::External);
}

TEST(DragSourceMarker, WireNamesMatchTheDeclaration) {
    EXPECT_EQ(std::string(DragSourceToWire(DragSource::Self)), "self");
    EXPECT_EQ(std::string(DragSourceToWire(DragSource::OtherWindow)), "other-window");
    EXPECT_EQ(std::string(DragSourceToWire(DragSource::External)), "external");
}

// --- Session retention ---

TEST(DragSourceMarker, SessionKeepsTheSourceUntilItExpires) {
    fb2k_dnd::DragSessionStore store;
    const std::string id = store.BeginSession({L"C:\\a.flac"}, true, 1000, {}, DragSource::Self);
    store.EndSession(id, 2000);
    const auto* session = store.Query(id, 3000);
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(session->source, DragSource::Self);
}

TEST(DragSourceMarker, SessionDefaultsToExternal) {
    fb2k_dnd::DragSessionStore store;
    const std::string id = store.BeginSession({L"C:\\a.flac"}, true, 1000);
    const auto* session = store.Query(id, 1000);
    ASSERT_NE(session, nullptr);
    EXPECT_EQ(session->source, DragSource::External);
}

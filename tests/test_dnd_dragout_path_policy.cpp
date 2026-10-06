// test_dnd_dragout_path_policy.cpp - planning page paths into a CF_HDROP list.
#include "pch.h"
#include "../src/webview/dnd/DragOutPathPolicy.h"

using fb2k_dnd::BuildDragOutPlan;
using fb2k_dnd::DragOutReject;

namespace {
// Stands in for the SDK resolver: strips file://, rewrites archive:// to its
// container, rejects anything else carrying a scheme.
bool FakeResolve(const std::wstring& in, std::wstring& out) {
    if (in.rfind(L"file://", 0) == 0) { out = in.substr(7); return true; }
    if (in.rfind(L"archive://", 0) == 0) {
        const size_t pipe = in.find(L'|');
        out = in.substr(10, pipe == std::wstring::npos ? pipe : pipe - 10);
        return true;
    }
    if (in.find(L"://") != std::wstring::npos) return false;
    out = in;
    return true;
}
}  // namespace

TEST(DragOutPathPolicy, PlainLocalPathsPassThrough) {
    const auto plan = BuildDragOutPlan({L"C:\\a.flac", L"D:\\b.mp3"}, FakeResolve);
    ASSERT_TRUE(plan.ok);
    ASSERT_EQ(plan.nativePaths.size(), 2u);
    EXPECT_EQ(plan.nativePaths[0], L"C:\\a.flac");
    EXPECT_EQ(plan.nativePaths[1], L"D:\\b.mp3");
}

TEST(DragOutPathPolicy, FileSchemeIsStripped) {
    const auto plan = BuildDragOutPlan({L"file://C:\\a.flac"}, FakeResolve);
    ASSERT_TRUE(plan.ok);
    ASSERT_EQ(plan.nativePaths.size(), 1u);
    EXPECT_EQ(plan.nativePaths[0], L"C:\\a.flac");
}

TEST(DragOutPathPolicy, ArchiveEntriesCollapseToTheirContainer) {
    // Two tracks inside one zip are one physical file, so one HDROP entry.
    const auto plan = BuildDragOutPlan(
        {L"archive://C:\\m\\album.zip|/01.flac", L"archive://C:\\m\\album.zip|/02.flac"},
        FakeResolve);
    ASSERT_TRUE(plan.ok);
    ASSERT_EQ(plan.nativePaths.size(), 1u);
    EXPECT_EQ(plan.nativePaths[0], L"C:\\m\\album.zip");
}

TEST(DragOutPathPolicy, StreamsAndProtocolsAreRejected) {
    for (const wchar_t* bad : {L"http://x.test/a.mp3", L"cdda://1", L"tone://440"}) {
        const auto plan = BuildDragOutPlan({bad}, FakeResolve);
        EXPECT_FALSE(plan.ok) << "should reject";
        EXPECT_EQ(plan.reason, DragOutReject::NoNativePath);
        EXPECT_EQ(plan.rejectedIndex, 0u);
        EXPECT_TRUE(plan.nativePaths.empty());
    }
}

TEST(DragOutPathPolicy, RejectionIsFailFastAndReportsTheIndex) {
    const auto plan = BuildDragOutPlan(
        {L"C:\\ok.flac", L"http://x.test/a.mp3", L"C:\\also-ok.flac"}, FakeResolve);
    EXPECT_FALSE(plan.ok);
    EXPECT_EQ(plan.rejectedIndex, 1u);
    EXPECT_TRUE(plan.nativePaths.empty());
}

TEST(DragOutPathPolicy, DuplicatesCollapseCaseInsensitivelyKeepingFirstOrder) {
    const auto plan = BuildDragOutPlan(
        {L"C:\\M\\A.flac", L"C:\\m\\a.FLAC", L"C:\\M\\B.flac"}, FakeResolve);
    ASSERT_TRUE(plan.ok);
    ASSERT_EQ(plan.nativePaths.size(), 2u);
    EXPECT_EQ(plan.nativePaths[0], L"C:\\M\\A.flac");  // first spelling wins
    EXPECT_EQ(plan.nativePaths[1], L"C:\\M\\B.flac");
}

TEST(DragOutPathPolicy, EmptyListRejected) {
    const auto plan = BuildDragOutPlan({}, FakeResolve);
    EXPECT_FALSE(plan.ok);
    EXPECT_EQ(plan.reason, DragOutReject::EmptyList);
}

TEST(DragOutPathPolicy, SubsongSuffixIsStrippedToTheContainer) {
    // "|subsong:N" is the API-wide way to name one track inside a cue sheet or
    // a multi-track file. Only the container exists on disk, so that is what
    // the HDROP entry must name; a '|' would make it an invalid filename.
    const auto plan = BuildDragOutPlan({L"C:\\m\\album.flac|subsong:7"}, FakeResolve);
    ASSERT_TRUE(plan.ok);
    ASSERT_EQ(plan.nativePaths.size(), 1u);
    EXPECT_EQ(plan.nativePaths[0], L"C:\\m\\album.flac");
}

TEST(DragOutPathPolicy, SubsongsOfOneContainerCollapseToOneEntry) {
    const auto plan = BuildDragOutPlan(
        {L"C:\\m\\album.cue|subsong:1", L"C:\\m\\album.cue|subsong:2", L"file://C:\\m\\ALBUM.CUE|subsong:3"},
        FakeResolve);
    ASSERT_TRUE(plan.ok);
    ASSERT_EQ(plan.nativePaths.size(), 1u);
    EXPECT_EQ(plan.nativePaths[0], L"C:\\m\\album.cue");
}

TEST(DragOutPathPolicy, SubsongSuffixInsideArchivePathIsStrippedBeforeResolving) {
    // The suffix sits after the archive's inner entry; the container still wins.
    const auto plan = BuildDragOutPlan({L"archive://C:\\m\\album.zip|/01.flac|subsong:2"}, FakeResolve);
    ASSERT_TRUE(plan.ok);
    ASSERT_EQ(plan.nativePaths.size(), 1u);
    EXPECT_EQ(plan.nativePaths[0], L"C:\\m\\album.zip");
}

TEST(DragOutPathPolicy, PipeWithoutSubsongMarkerIsLeftAlone) {
    // Only the "|subsong:" marker is a suffix; any other '|' belongs to the
    // caller's string and reaches the resolver unchanged.
    std::wstring seen;
    const auto plan = BuildDragOutPlan({L"C:\\m\\a|b.flac"},
                                       [&](const std::wstring& in, std::wstring& out) {
                                           seen = in; out = in; return true;
                                       });
    ASSERT_TRUE(plan.ok);
    EXPECT_EQ(seen, L"C:\\m\\a|b.flac");
}

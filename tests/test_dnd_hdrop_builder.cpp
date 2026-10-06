// test_dnd_hdrop_builder.cpp - CF_HDROP / DROPFILES construction for dragging out.
//
// The most valuable case here is the round trip: whatever this builder emits is
// fed back through the parsing side the drop path already uses, so the two
// halves of the same wire format are pinned against each other rather than
// against a hand-written copy of the layout.
#include "pch.h"
#include "../src/webview/dnd/HdropBuilder.h"
#include "../src/webview/dnd/HdropReader.h"

#include <cstring>

using fb2k_dnd::BuildDropFilesBlock;
using fb2k_dnd::ParseDropFilesBlob;

namespace {

const DROPFILES& Header(const std::vector<unsigned char>& blob) {
    return *reinterpret_cast<const DROPFILES*>(blob.data());
}

// The wide character region, which starts at the offset the header advertises.
const wchar_t* Chars(const std::vector<unsigned char>& blob) {
    return reinterpret_cast<const wchar_t*>(blob.data() + Header(blob).pFiles);
}

// Count of wchar_t between pFiles and the end of the buffer.
size_t CharCount(const std::vector<unsigned char>& blob) {
    return (blob.size() - Header(blob).pFiles) / sizeof(wchar_t);
}

}  // namespace

TEST(HdropBuilder, EmptyListYieldsEmptyBuffer) {
    // An HDROP carrying no files is not a drag payload; the caller must not be
    // handed a buffer it would then have to check for meaning.
    EXPECT_TRUE(BuildDropFilesBlock({}).empty());
}

TEST(HdropBuilder, HeaderDescribesAWideListWithNoDropPoint) {
    const auto blob = BuildDropFilesBlock({L"C:\\a.flac"});
    ASSERT_GE(blob.size(), sizeof(DROPFILES));
    const DROPFILES& df = Header(blob);
    EXPECT_EQ(df.pFiles, sizeof(DROPFILES));
    EXPECT_EQ(df.fWide, TRUE);   // paths below are wchar_t, not char
    EXPECT_EQ(df.fNC, FALSE);
    EXPECT_EQ(df.pt.x, 0);
    EXPECT_EQ(df.pt.y, 0);
}

TEST(HdropBuilder, SinglePathIsNulTerminatedThenListTerminated) {
    const auto blob = BuildDropFilesBlock({L"C:\\a.flac"});
    // "C:\a.flac" is 9 characters, plus its own NUL, plus the list terminator.
    ASSERT_EQ(CharCount(blob), 11u);
    EXPECT_EQ(std::wstring(Chars(blob)), L"C:\\a.flac");
    EXPECT_EQ(Chars(blob)[9], L'\0');
    EXPECT_EQ(Chars(blob)[10], L'\0');
}

TEST(HdropBuilder, MultiplePathsAreConcatenatedInOrder) {
    const auto blob = BuildDropFilesBlock({L"C:\\1.mp3", L"C:\\2.mp3"});
    // Two 8-character paths, each with a NUL, plus the list terminator.
    ASSERT_EQ(CharCount(blob), 19u);
    EXPECT_EQ(std::wstring(Chars(blob)), L"C:\\1.mp3");
    EXPECT_EQ(std::wstring(Chars(blob) + 9), L"C:\\2.mp3");
    EXPECT_EQ(Chars(blob)[18], L'\0');
}

TEST(HdropBuilder, RoundTripsThroughTheParsingSide) {
    const std::vector<std::wstring> paths = {
        L"C:\\1.mp3", L"D:\\music\\album\\02 - track.flac", L"E:\\x.ogg"};
    const auto blob = BuildDropFilesBlock(paths);
    EXPECT_EQ(ParseDropFilesBlob(blob.data(), blob.size()), paths);
}

TEST(HdropBuilder, RoundTripsASinglePath) {
    const std::vector<std::wstring> paths = {L"C:\\music\\a.mp3"};
    const auto blob = BuildDropFilesBlock(paths);
    EXPECT_EQ(ParseDropFilesBlob(blob.data(), blob.size()), paths);
}

TEST(HdropBuilder, RoundTripsNonAsciiPaths) {
    // The library in this repo has such paths, and a wide list is the whole
    // reason fWide is set.
    const std::vector<std::wstring> paths = {
        L"E:\\OST\\千年幸福論\\01.mp3", L"E:\\音楽\\テスト.flac"};
    const auto blob = BuildDropFilesBlock(paths);
    EXPECT_EQ(ParseDropFilesBlob(blob.data(), blob.size()), paths);
}

TEST(HdropBuilder, RoundTripsAPathBeyondMaxPath) {
    std::wstring longPath = L"C:\\";
    longPath += std::wstring(400, L'x');
    longPath += L"\\deep.flac";
    ASSERT_GT(longPath.size(), static_cast<size_t>(MAX_PATH));

    const std::vector<std::wstring> paths = {longPath};
    const auto blob = BuildDropFilesBlock(paths);
    EXPECT_EQ(ParseDropFilesBlob(blob.data(), blob.size()), paths);
}

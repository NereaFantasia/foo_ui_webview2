// test_canonical_handle.cpp - fb2k_paths::CreateCanonicalHandleWith 与
// CreateTrackHandleWith 调用契约
//
// 生产版 SubsongUtils::CreateCanonicalHandle、CreateTrackHandle 注入的是
// filesystem::g_get_canonical_path 与 metadb::handle_create，二者在单测里都不可用；
// 这里用 fake 验证调用顺序、规范化结果的传递、subsong 原样透传，以及曲目路径的
// "|subsong:N" 后缀先拆掉再规范化。测试直接调用共享模板，但不验证各生产调用点
// 是否使用该模板，也不验证 SDK 的实际路径规范化行为。
#include "pch.h"
#include <string>
#include <string_view>
#include <vector>

#include "utils/CanonicalHandle.h"

namespace {

struct CallLog {
    std::vector<std::string> canonicalizeInputs;
    std::vector<std::string> handleCreatePaths;
    std::vector<uint32_t> handleCreateSubsongs;
    std::vector<std::string> order;
};

// 模拟 g_get_canonical_path：把裸盘符路径变成 fb2k 的 file:// 逻辑路径。
std::string FakeCanonicalize(CallLog& log, const char* in) {
    log.canonicalizeInputs.emplace_back(in);
    log.order.emplace_back("canonicalize");
    std::string s(in);
    if (s.rfind("file://", 0) != 0) s = "file://" + s;
    return s;
}

// 模拟 handle_create：返回一个可比较的"身份"字符串，方便断言两条路径是否同一。
std::string FakeHandleCreate(CallLog& log, const char* canonical, uint32_t subsong) {
    log.handleCreatePaths.emplace_back(canonical);
    log.handleCreateSubsongs.push_back(subsong);
    log.order.emplace_back("handle_create");
    return std::string(canonical) + "#" + std::to_string(subsong);
}

std::string CreateVia(CallLog& log, const char* path, uint32_t subsong) {
    return fb2k_paths::CreateCanonicalHandleWith(
        path, subsong,
        [&log](const char* in) { return FakeCanonicalize(log, in); },
        [&log](const char* canonical, uint32_t s) { return FakeHandleCreate(log, canonical, s); });
}

} // namespace

TEST(CanonicalHandle, CanonicalizeRunsBeforeHandleCreate) {
    CallLog log;
    CreateVia(log, "D:\\Music\\a.flac", 0);
    ASSERT_EQ(log.order.size(), 2u);
    EXPECT_EQ(log.order[0], "canonicalize");
    EXPECT_EQ(log.order[1], "handle_create");
}

TEST(CanonicalHandle, HandleCreateReceivesCanonicalFormNotRawInput) {
    CallLog log;
    CreateVia(log, "D:\\Music\\a.flac", 0);
    ASSERT_EQ(log.canonicalizeInputs.size(), 1u);
    EXPECT_EQ(log.canonicalizeInputs[0], "D:\\Music\\a.flac");
    ASSERT_EQ(log.handleCreatePaths.size(), 1u);
    EXPECT_EQ(log.handleCreatePaths[0], "file://D:\\Music\\a.flac");
}

TEST(CanonicalHandle, SubsongPassesThroughUnchanged) {
    CallLog log;
    CreateVia(log, "D:\\Music\\album.cue", 7);
    ASSERT_EQ(log.handleCreateSubsongs.size(), 1u);
    EXPECT_EQ(log.handleCreateSubsongs[0], 7u);
}

TEST(CanonicalHandle, ReturnsHandleCreateResult) {
    CallLog log;
    EXPECT_EQ(CreateVia(log, "D:\\Music\\a.flac", 2), "file://D:\\Music\\a.flac#2");
}

// 同一文件的裸路径与已规范化路径经 fake 规范化后应得到相同的 handle 身份，
// 验证模板向建 handle 的函数传入规范化结果，而不是直接传入调用方的原文。
TEST(CanonicalHandle, RawAndCanonicalSpellingsCollapseToOneIdentity) {
    CallLog log;
    const std::string viaRaw = CreateVia(log, "D:\\Music\\a.flac", 0);
    const std::string viaCanonical = CreateVia(log, "file://D:\\Music\\a.flac", 0);
    EXPECT_EQ(viaRaw, viaCanonical);
}

TEST(CanonicalHandle, DifferentSubsongsOfOneFileStayDistinct) {
    CallLog log;
    EXPECT_NE(CreateVia(log, "D:\\Music\\album.cue", 0), CreateVia(log, "D:\\Music\\album.cue", 1));
}

// 带输出参数的重载：调用方拿到的规范化结果就是交给建 handle 函数的那一个，
// 规范化只做一次。
TEST(CanonicalHandle, CanonicalOutIsWhatHandleCreateReceived) {
    CallLog log;
    std::string canonical;
    const std::string identity = fb2k_paths::CreateCanonicalHandleWith(
        "D:\\Music\\album.cue", 4,
        [&log](const char* in) { return FakeCanonicalize(log, in); },
        [&log](const char* c, uint32_t s) { return FakeHandleCreate(log, c, s); },
        canonical);
    EXPECT_EQ(canonical, "file://D:\\Music\\album.cue");
    ASSERT_EQ(log.handleCreatePaths.size(), 1u);
    EXPECT_EQ(log.handleCreatePaths[0], canonical);
    EXPECT_EQ(log.canonicalizeInputs.size(), 1u);
    EXPECT_EQ(identity, "file://D:\\Music\\album.cue#4");
}

namespace {

std::string CreateTrackVia(CallLog& log, std::string_view trackPath, fb2k_paths::SubsongPath& parts) {
    return fb2k_paths::CreateTrackHandleWith(
        trackPath,
        [&log](const char* in) { return FakeCanonicalize(log, in); },
        [&log](const char* canonical, uint32_t s) { return FakeHandleCreate(log, canonical, s); },
        parts);
}

std::string CreateTrackVia(CallLog& log, std::string_view trackPath) {
    fb2k_paths::SubsongPath parts;
    return CreateTrackVia(log, trackPath, parts);
}

} // namespace

// 后缀留给规范化会被当成文件名的一部分；规范化只能看到标记之前的路径，
// 序号交给建 handle 的函数。
TEST(CanonicalHandle, TrackPathSuffixIsStrippedBeforeCanonicalize) {
    CallLog log;
    fb2k_paths::SubsongPath parts;
    const std::string identity = CreateTrackVia(log, "D:\\Music\\album.cue|subsong:3", parts);
    ASSERT_EQ(log.canonicalizeInputs.size(), 1u);
    EXPECT_EQ(log.canonicalizeInputs[0], "D:\\Music\\album.cue");
    ASSERT_EQ(log.handleCreateSubsongs.size(), 1u);
    EXPECT_EQ(log.handleCreateSubsongs[0], 3u);
    EXPECT_EQ(identity, "file://D:\\Music\\album.cue#3");
    EXPECT_TRUE(parts.hasSuffix);
    EXPECT_TRUE(parts.suffixValid);
}

TEST(CanonicalHandle, TrackPathWithoutSuffixUsesSubsongZero) {
    CallLog log;
    fb2k_paths::SubsongPath parts;
    EXPECT_EQ(CreateTrackVia(log, "D:\\Music\\a.flac", parts), "file://D:\\Music\\a.flac#0");
    ASSERT_EQ(log.canonicalizeInputs.size(), 1u);
    EXPECT_EQ(log.canonicalizeInputs[0], "D:\\Music\\a.flac");
    EXPECT_FALSE(parts.hasSuffix);
}

// 读不出序号时路径照样截断、序号为 0；parts 报出这种情况，供生产包装记日志。
TEST(CanonicalHandle, UnreadableTrackSuffixStillStripsPathAndUsesZero) {
    CallLog log;
    fb2k_paths::SubsongPath parts;
    EXPECT_EQ(CreateTrackVia(log, "D:\\Music\\album.cue|subsong:abc", parts),
              "file://D:\\Music\\album.cue#0");
    ASSERT_EQ(log.canonicalizeInputs.size(), 1u);
    EXPECT_EQ(log.canonicalizeInputs[0], "D:\\Music\\album.cue");
    EXPECT_TRUE(parts.hasSuffix);
    EXPECT_FALSE(parts.suffixValid);
}

TEST(CanonicalHandle, TrackPathKeepsItsProtocolPrefix) {
    CallLog log;
    EXPECT_EQ(CreateTrackVia(log, "file://D:\\Music\\album.cue|subsong:2"),
              "file://D:\\Music\\album.cue#2");
    ASSERT_EQ(log.canonicalizeInputs.size(), 1u);
    EXPECT_EQ(log.canonicalizeInputs[0], "file://D:\\Music\\album.cue");
}

// 同一张 CUE 的各首曲目得到各自的 handle；显式的 "|subsong:0" 与不带后缀是同一首。
TEST(CanonicalHandle, TracksOfOneSheetGetTheirOwnHandles) {
    CallLog log;
    EXPECT_NE(CreateTrackVia(log, "D:\\Music\\album.cue|subsong:1"),
              CreateTrackVia(log, "D:\\Music\\album.cue|subsong:2"));
    EXPECT_EQ(CreateTrackVia(log, "D:\\Music\\album.cue|subsong:0"),
              CreateTrackVia(log, "D:\\Music\\album.cue"));
}

// 序号按 unsigned 读：超出 int 但在 32 位无符号范围内的序号照样交给建 handle 的函数，
// 不因 int 溢出被当成读不出。
TEST(CanonicalHandle, TrackIndexBeyondIntRangeReachesHandleCreate) {
    CallLog log;
    fb2k_paths::SubsongPath parts;
    EXPECT_EQ(CreateTrackVia(log, "D:\\Music\\album.cue|subsong:3000000000", parts),
              "file://D:\\Music\\album.cue#3000000000");
    ASSERT_EQ(log.canonicalizeInputs.size(), 1u);
    EXPECT_EQ(log.canonicalizeInputs[0], "D:\\Music\\album.cue");
    EXPECT_TRUE(parts.suffixValid);
}

// 连 32 位无符号也放不下的序号算读不出：后缀仍不进规范化，序号为 0。
TEST(CanonicalHandle, OverflowingTrackIndexStillStripsPathAndUsesZero) {
    CallLog log;
    fb2k_paths::SubsongPath parts;
    EXPECT_EQ(CreateTrackVia(log, "D:\\Music\\album.cue|subsong:99999999999", parts),
              "file://D:\\Music\\album.cue#0");
    ASSERT_EQ(log.canonicalizeInputs.size(), 1u);
    EXPECT_EQ(log.canonicalizeInputs[0], "D:\\Music\\album.cue");
    EXPECT_TRUE(parts.hasSuffix);
    EXPECT_FALSE(parts.suffixValid);
}

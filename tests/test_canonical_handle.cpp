// test_canonical_handle.cpp - fb2k_paths::CreateCanonicalHandleWith 调用契约
//
// 生产版 SubsongUtils::CreateCanonicalHandle 注入的是 filesystem::g_get_canonical_path
// 与 metadb::handle_create，二者在单测里都不可用；这里用 fake 验证调用顺序、
// 规范化结果的传递及 subsong 原样透传。测试直接调用共享模板，但不验证
// 各生产调用点是否使用该模板，也不验证 SDK 的实际路径规范化行为。
#include "pch.h"
#include <string>
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

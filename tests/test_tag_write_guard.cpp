// test_tag_write_guard.cpp - WAV 标签写入的前置检查
#include "pch.h"
#include "compat/fb2k_types.h"  // console:: stub: ApiResult.h pulls in ErrorEnvelope.h
#include "../src/api/TagWriteGuard.h"

#include <string>
#include <utility>
#include <vector>

namespace {

// 只实现 FirstNonAsciiField 用到的 meta 接口
struct FakeInfo {
    std::vector<std::pair<std::string, std::vector<std::string>>> fields;

    size_t meta_get_count() const { return fields.size(); }
    const char* meta_enum_name(size_t i) const { return fields[i].first.c_str(); }
    size_t meta_enum_value_count(size_t i) const { return fields[i].second.size(); }
    const char* meta_enum_value(size_t i, size_t j) const { return fields[i].second[j].c_str(); }
};

} // namespace

TEST(TagWriteGuard, FoldsNonAsciiMatchesWavExtensionOnly) {
    using api::tag_guard::FoldsNonAscii;
    EXPECT_TRUE(FoldsNonAscii("E:\\Music\\a.wav"));
    EXPECT_TRUE(FoldsNonAscii("file://E:\\Music\\a.WAV"));
    EXPECT_TRUE(FoldsNonAscii("file-relative://..\\music\\a.Wav"));
    EXPECT_FALSE(FoldsNonAscii("E:\\Music\\a.aiff"));
    EXPECT_FALSE(FoldsNonAscii("E:\\Music\\a.flac"));
    EXPECT_FALSE(FoldsNonAscii("E:\\Music\\a.wavpack"));
    EXPECT_FALSE(FoldsNonAscii("E:\\Music\\a.wv"));
    EXPECT_FALSE(FoldsNonAscii("E:\\Music\\wav"));
    EXPECT_FALSE(FoldsNonAscii("E:\\a.wav\\track"));
    EXPECT_FALSE(FoldsNonAscii(""));
}

TEST(TagWriteGuard, HasNonAsciiSeesUtf8AndLatin1Bytes) {
    using api::tag_guard::HasNonAscii;
    EXPECT_FALSE(HasNonAscii(""));
    EXPECT_FALSE(HasNonAscii("Eyes On You (Remix) 01"));
    EXPECT_TRUE(HasNonAscii("\xE6\x98\x8E\xE6\x97\xA5"));  // UTF-8 CJK
    EXPECT_TRUE(HasNonAscii("\xC3\xB7"));                    // 宿主把 GBK 字节按 Latin-1 读入后的形态
}

TEST(TagWriteGuard, FirstNonAsciiFieldNamesTheOffendingTag) {
    using api::tag_guard::FirstNonAsciiField;
    EXPECT_FALSE(FirstNonAsciiField(FakeInfo{}).has_value());

    FakeInfo ascii{{{"TITLE", {"Secret Keepers"}}, {"ARTIST", {"A", "B"}}}};
    EXPECT_FALSE(FirstNonAsciiField(ascii).has_value());

    FakeInfo mixed{{{"TITLE", {"ok"}}, {"ARTIST", {"A", "\xE5\xBC\xA0"}}, {"ALBUM", {"\xE5\x9B\xBD"}}}};
    EXPECT_EQ(FirstNonAsciiField(mixed), std::optional<std::string>("ARTIST"));
}

TEST(TagWriteGuard, RefusalCarriesCodeAndReason) {
    const api::Failure failure = api::tag_guard::Refusal("E:\\a.wav", "TITLE");
    EXPECT_EQ(failure.code, std::string(ApiErrorCode::NOT_SUPPORTED));
    const nlohmann::json envelope = api::results::FailureToJson(failure);
    EXPECT_FALSE(envelope["success"].get<bool>());
    EXPECT_EQ(envelope["path"], "E:\\a.wav");
    EXPECT_EQ(envelope["details"]["reason"], "wavInfoNonAscii");
    EXPECT_EQ(envelope["details"]["field"], "TITLE");
}

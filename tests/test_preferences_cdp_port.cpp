// test_preferences_cdp_port.cpp - CDP 端口的文本解析、范围校验与开发者子页字段表中的端口字段
//
// 直接链接生产头 prefs_cdp::* 与 prefs_fields::developer::Table()，不在测试内重写被测逻辑。
// 断言依据是 docs/preferences/DESIGN.md 第 11.5 节：端口 1024–65535、默认 9222、改动后重启生效。
#include "pch.h"
#include "../src/core/PreferencesCdpPort.h"
#include "../src/core/PreferencesFields.h"

using namespace prefs_cdp;

// ============================================
// 范围
// ============================================

TEST(PreferencesCdpPort, ConstantsMatchDesign) {
    EXPECT_EQ(kMinPort, 1024);
    EXPECT_EQ(kMaxPort, 65535);
    EXPECT_EQ(kDefaultPort, 9222);
    EXPECT_FALSE(IsValidPort(kInvalidPort));
}

TEST(PreferencesCdpPort, ValidPortIsInclusiveRange) {
    EXPECT_TRUE(IsValidPort(1024));
    EXPECT_TRUE(IsValidPort(9222));
    EXPECT_TRUE(IsValidPort(65535));
    EXPECT_FALSE(IsValidPort(1023));
    EXPECT_FALSE(IsValidPort(65536));
    EXPECT_FALSE(IsValidPort(0));
    EXPECT_FALSE(IsValidPort(-1));
}

// ============================================
// 文本解析
// ============================================

TEST(PreferencesCdpPort, ParsesPlainDecimalWithSurroundingWhitespace) {
    EXPECT_EQ(ParsePort("9222"), 9222);
    EXPECT_EQ(ParsePort(" 9222 "), 9222);
    EXPECT_EQ(ParsePort("\t1024\r\n"), 1024);
    EXPECT_EQ(ParsePort("65535"), 65535);
    EXPECT_EQ(ParsePort("09222"), 9222);   // 前导零只是数字
}

TEST(PreferencesCdpPort, RejectsNonDigitsAndEmpty) {
    EXPECT_EQ(ParsePort(""), kInvalidPort);
    EXPECT_EQ(ParsePort("   "), kInvalidPort);
    EXPECT_EQ(ParsePort("+9222"), kInvalidPort);
    EXPECT_EQ(ParsePort("-9222"), kInvalidPort);
    EXPECT_EQ(ParsePort("9222.0"), kInvalidPort);
    EXPECT_EQ(ParsePort("92 22"), kInvalidPort);
    EXPECT_EQ(ParsePort("0x2406"), kInvalidPort);
    EXPECT_EQ(ParsePort("port"), kInvalidPort);
}

TEST(PreferencesCdpPort, RejectsOutOfRangeAndOverlongNumbers) {
    EXPECT_EQ(ParsePort("0"), kInvalidPort);
    EXPECT_EQ(ParsePort("80"), kInvalidPort);
    EXPECT_EQ(ParsePort("1023"), kInvalidPort);
    EXPECT_EQ(ParsePort("65536"), kInvalidPort);
    EXPECT_EQ(ParsePort("70000"), kInvalidPort);
    EXPECT_EQ(ParsePort("123456"), kInvalidPort);        // 六位：不做溢出算术
    EXPECT_EQ(ParsePort("99999999999999999999"), kInvalidPort);
}

// ============================================
// 清洗存储值
// ============================================

TEST(PreferencesCdpPort, SanitizeKeepsValidAndFallsBackToDefault) {
    EXPECT_EQ(SanitizePort(9222), 9222);
    EXPECT_EQ(SanitizePort(1024), 1024);
    EXPECT_EQ(SanitizePort(65535), 65535);
    EXPECT_EQ(SanitizePort(0), kDefaultPort);
    EXPECT_EQ(SanitizePort(80), kDefaultPort);
    EXPECT_EQ(SanitizePort(70000), kDefaultPort);
    EXPECT_EQ(SanitizePort(-1), kDefaultPort);
}

// ============================================
// 开发者子页字段表里的端口字段
// ============================================

namespace developer = prefs_fields::developer;

TEST(PreferencesDeveloperFields, CdpPortOutsideRangeIsOutOfRangeAndBlocksApply) {
    const prefs_draft::FieldTable fields = developer::Table();
    prefs_draft::Snapshot draft = prefs_draft::Defaults(fields);

    draft.SetInt(developer::CdpPort, 9333);
    EXPECT_TRUE(prefs_draft::Validate(fields, draft).ok());

    for (int bad : {kInvalidPort, 80, 1023, 65536}) {
        draft.SetInt(developer::CdpPort, bad);
        const prefs_draft::ValidationResult r = prefs_draft::Validate(fields, draft);
        EXPECT_EQ(r.error, prefs_draft::ValidationError::OutOfRange) << bad;
        EXPECT_EQ(r.field, static_cast<size_t>(developer::CdpPort)) << bad;
    }
}

TEST(PreferencesDeveloperFields, RestartOnlyWhenDevToolsOrCdpLeaveStartupValue) {
    const prefs_draft::FieldTable fields = developer::Table();
    const prefs_draft::Snapshot startup = prefs_draft::Defaults(fields);
    prefs_draft::Snapshot draft = startup;

    draft.SetBool(developer::UseDevServer, true);
    draft.SetString(developer::DevServerUrl, "http://localhost:5173");
    EXPECT_FALSE(prefs_draft::NeedsRestart(fields, startup, draft));

    draft.SetInt(developer::CdpPort, 9333);
    EXPECT_TRUE(prefs_draft::NeedsRestart(fields, startup, draft));
    draft.SetInt(developer::CdpPort, kDefaultPort);
    EXPECT_FALSE(prefs_draft::NeedsRestart(fields, startup, draft));

    draft.SetBool(developer::CdpRemote, true);
    EXPECT_TRUE(prefs_draft::NeedsRestart(fields, startup, draft));
    draft.SetBool(developer::CdpRemote, false);
    draft.SetBool(developer::DevTools, true);
    EXPECT_TRUE(prefs_draft::NeedsRestart(fields, startup, draft));
}

TEST(PreferencesDeveloperFields, InvalidPortDraftIsDirtyAndConflictOnlyForEditedField) {
    const prefs_draft::FieldTable fields = developer::Table();
    const prefs_draft::Snapshot initial = prefs_draft::Defaults(fields);
    prefs_draft::Snapshot draft = initial;
    // 输入框半途的文字解析失败：草稿变脏，Apply 会被 Validate 拦下，不会写配置。
    draft.SetInt(developer::CdpPort, kInvalidPort);
    EXPECT_TRUE(prefs_draft::IsDirty(initial, draft));
    EXPECT_FALSE(prefs_draft::Validate(fields, draft).ok());

    draft.SetInt(developer::CdpPort, 9333);
    prefs_draft::Snapshot current = initial;
    current.SetInt(developer::CdpPort, 9444);      // 别处改成了第三个值
    current.SetBool(developer::DevTools, true);    // 未编辑字段不参与
    const std::vector<size_t> conflicts = prefs_draft::DetectConflicts(initial, draft, current);
    ASSERT_EQ(conflicts.size(), 1u);
    EXPECT_EQ(conflicts[0], static_cast<size_t>(developer::CdpPort));
}

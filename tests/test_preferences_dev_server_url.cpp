// test_preferences_dev_server_url.cpp - 开发服务器 URL 校验与开发者子页字段表
//
// 直接链接生产头 prefs_devserver::* 与 prefs_fields::developer::Table()，不在测试内重写被测逻辑。
// 断言依据是 docs/preferences/DESIGN.md 第 11.5 节：URL 须 http(s):// 起头且带主机名，否则拒绝；
// 空串表示不使用开发服务器，放行。CDP 端口的解析与校验在 test_preferences_cdp_port.cpp。
#include "pch.h"
#include "../src/prefs/PreferencesCdpPort.h"
#include "../src/prefs/PreferencesDevServerUrl.h"
#include "../src/prefs/PreferencesFields.h"

using namespace prefs_devserver;

// ============================================
// 空白处理
// ============================================

TEST(PreferencesDevServerUrl, TrimStripsOnlyLeadingAndTrailingWhitespace) {
    EXPECT_EQ(Trim("  http://localhost:5173 \t"), "http://localhost:5173");
    EXPECT_EQ(Trim("   "), "");
    EXPECT_EQ(Trim("a b"), "a b");
}

// ============================================
// 通过
// ============================================

TEST(PreferencesDevServerUrl, AcceptsCommonDevServerAddresses) {
    EXPECT_EQ(CheckUrl(""), UrlError::None);
    EXPECT_EQ(CheckUrl("http://localhost:5173"), UrlError::None);
    EXPECT_EQ(CheckUrl("http://localhost:5173/"), UrlError::None);
    EXPECT_EQ(CheckUrl("https://127.0.0.1:5174"), UrlError::None);
    EXPECT_EQ(CheckUrl("HTTP://LOCALHOST:5180"), UrlError::None);
    EXPECT_EQ(CheckUrl("http://localhost"), UrlError::None);
    EXPECT_EQ(CheckUrl("http://dev.example.test:3000/app/?hmr=1"), UrlError::None);
    EXPECT_EQ(CheckUrl("http://[::1]:5173"), UrlError::None);
    EXPECT_EQ(CheckUrl("http://[::1]"), UrlError::None);
}

// ============================================
// 拒绝
// ============================================

TEST(PreferencesDevServerUrl, RejectsNonHttpSchemes) {
    EXPECT_EQ(CheckUrl("localhost:5173"), UrlError::Scheme);
    EXPECT_EQ(CheckUrl("ftp://localhost:5173"), UrlError::Scheme);
    EXPECT_EQ(CheckUrl("file:///E:/site/index.html"), UrlError::Scheme);
    EXPECT_EQ(CheckUrl("//localhost:5173"), UrlError::Scheme);
}

TEST(PreferencesDevServerUrl, RejectsMissingOrMalformedHost) {
    EXPECT_EQ(CheckUrl("http://"), UrlError::Host);
    EXPECT_EQ(CheckUrl("http:///index.html"), UrlError::Host);
    EXPECT_EQ(CheckUrl("http://:5173"), UrlError::Host);
    EXPECT_EQ(CheckUrl("http://user@host:5173"), UrlError::Host);
    EXPECT_EQ(CheckUrl("http://[]:5173"), UrlError::Host);
    EXPECT_EQ(CheckUrl("http://[::1]5173"), UrlError::Host);
    EXPECT_EQ(CheckUrl("http://::1:5173"), UrlError::Host);  // IPv6 必须带方括号
}

TEST(PreferencesDevServerUrl, RejectsBadPorts) {
    EXPECT_EQ(CheckUrl("http://localhost:"), UrlError::Port);
    EXPECT_EQ(CheckUrl("http://localhost:0"), UrlError::Port);
    EXPECT_EQ(CheckUrl("http://localhost:65536"), UrlError::Port);
    EXPECT_EQ(CheckUrl("http://localhost:51a3"), UrlError::Port);
    EXPECT_EQ(CheckUrl("http://localhost:123456"), UrlError::Port);
    EXPECT_EQ(CheckUrl("http://[::1]:"), UrlError::Port);
    EXPECT_EQ(CheckUrl("http://localhost:65535"), UrlError::None);
    EXPECT_EQ(CheckUrl("http://localhost:1"), UrlError::None);
}

TEST(PreferencesDevServerUrl, RejectsEmbeddedWhitespaceBeforeAnythingElse) {
    EXPECT_EQ(CheckUrl("http://local host:5173"), UrlError::Whitespace);
    EXPECT_EQ(CheckUrl("http://localhost:5173 "), UrlError::Whitespace);  // 调用方应先 Trim
    EXPECT_EQ(CheckUrl("ht tp://localhost"), UrlError::Whitespace);
}

// 中文输入法常把冒号、空格打成全角；它们不是 isspace 也不是 ':'，此前会被当成主机名的一部分放行，
// 加载时才静默回退本地模板。authority 里任何非 ASCII 字节都在主机阶段拒绝；路径里的非 ASCII 仍放行。
TEST(PreferencesDevServerUrl, RejectsNonAsciiInHostOrPort) {
    EXPECT_EQ(CheckUrl("http://localhost" "\xEF\xBC\x9A" "5173"), UrlError::NonAscii);   // 全角冒号 U+FF1A
    EXPECT_EQ(CheckUrl("http://local" "\xE3\x80\x80" "host:5173"), UrlError::NonAscii);  // 全角空格 U+3000
    EXPECT_EQ(CheckUrl("http://localhost:" "\xEF\xBC\x95\xEF\xBC\x91"), UrlError::NonAscii);  // 全角数字 ５１
    EXPECT_EQ(CheckUrl("http://" "\xE6\x9C\xAC\xE5\x9C\xB0" ":5173"), UrlError::NonAscii);  // 中文主机名
    EXPECT_EQ(CheckUrl("http" "\xEF\xBC\x9A" "//localhost:5173"), UrlError::Scheme);       // 协议后的全角冒号仍是协议错误
    EXPECT_EQ(CheckUrl("http://localhost:5173/" "\xE8\xB7\xAF\xE5\xBE\x84"), UrlError::None);  // 路径放行
}

// ============================================
// 开发者子页字段表
// ============================================

TEST(PreferencesDeveloperFields, DefaultsAreOffAndEmpty) {
    namespace developer = prefs_fields::developer;
    const prefs_draft::FieldTable fields = developer::Table();
    const prefs_draft::Snapshot d = prefs_draft::Defaults(fields);
    ASSERT_EQ(d.size(), static_cast<size_t>(developer::Count));
    EXPECT_FALSE(d.GetBool(developer::DevTools));
    EXPECT_FALSE(d.GetBool(developer::UseDevServer));
    EXPECT_EQ(d.GetString(developer::DevServerUrl), "");
    EXPECT_FALSE(d.GetBool(developer::CdpRemote));
    EXPECT_EQ(d.GetInt(developer::CdpPort), prefs_cdp::kDefaultPort);
    EXPECT_TRUE(prefs_draft::Validate(fields, d).ok());
    // 开发服务器两项在加载前端时读取，不要求重启；开发者工具与 CDP 两项在启动期读取，要求重启。
    for (size_t i = 0; i < fields.size(); ++i) {
        const bool restart = (i == developer::DevTools || i == developer::CdpRemote || i == developer::CdpPort);
        EXPECT_EQ(fields[i].affectsRestart, restart) << fields[i].key;
    }
}

TEST(PreferencesDeveloperFields, UrlFieldRejectsMalformedAndAllowsEmptyWithSwitchOn) {
    const prefs_draft::FieldTable fields = prefs_fields::developer::Table();
    prefs_draft::Snapshot draft = prefs_draft::Defaults(fields);
    draft.SetBool(prefs_fields::developer::UseDevServer, true);
    EXPECT_TRUE(prefs_draft::Validate(fields, draft).ok());

    draft.SetString(prefs_fields::developer::DevServerUrl, "localhost:5173");
    const prefs_draft::ValidationResult r = prefs_draft::Validate(fields, draft);
    EXPECT_EQ(r.error, prefs_draft::ValidationError::Invalid);
    EXPECT_EQ(r.field, static_cast<size_t>(prefs_fields::developer::DevServerUrl));

    draft.SetString(prefs_fields::developer::DevServerUrl, "http://localhost:5173");
    EXPECT_TRUE(prefs_draft::Validate(fields, draft).ok());
}

TEST(PreferencesDeveloperFields, UrlEditIsDetectedAsDirtyAndConflict) {
    const prefs_draft::FieldTable fields = prefs_fields::developer::Table();
    prefs_draft::Snapshot initial = prefs_draft::Defaults(fields);
    initial.SetString(prefs_fields::developer::DevServerUrl, "http://localhost:5173");
    prefs_draft::Snapshot draft = initial;
    draft.SetString(prefs_fields::developer::DevServerUrl, "http://localhost:5180");
    EXPECT_TRUE(prefs_draft::IsDirty(initial, draft));

    prefs_draft::Snapshot current = initial;
    current.SetString(prefs_fields::developer::DevServerUrl, "http://127.0.0.1:4000");  // 别处改成了第三个值
    const std::vector<size_t> conflicts = prefs_draft::DetectConflicts(initial, draft, current);
    ASSERT_EQ(conflicts.size(), 1u);
    EXPECT_EQ(conflicts[0], static_cast<size_t>(prefs_fields::developer::DevServerUrl));
}

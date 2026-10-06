// test_origin_policy.cpp - origin parsing and the bridge trust rule.
#include "pch.h"
#include "../src/webview/OriginPolicy.h"

using origin_policy::DevServerOrigin;
using origin_policy::IsTrustedPage;
using origin_policy::OriginOf;

TEST(OriginPolicyOriginOf, KeepsSchemeHostAndPort) {
    EXPECT_EQ(OriginOf(L"https://foo-ui-webview2.local/index.html?windowId=popup_1"),
              L"https://foo-ui-webview2.local");
    EXPECT_EQ(OriginOf(L"http://localhost:5190/"), L"http://localhost:5190");
    EXPECT_EQ(OriginOf(L"http://[::1]:5173/app#top"), L"http://[::1]:5173");
    EXPECT_EQ(OriginOf(L"https://example.com"), L"https://example.com");
}

TEST(OriginPolicyOriginOf, NormalisesCaseAndDefaultPorts) {
    EXPECT_EQ(OriginOf(L"HTTPS://Foo-UI-WebView2.LOCAL/"), L"https://foo-ui-webview2.local");
    EXPECT_EQ(OriginOf(L"https://example.com:443/"), L"https://example.com");
    EXPECT_EQ(OriginOf(L"http://example.com:80/"), L"http://example.com");
    EXPECT_EQ(OriginOf(L"http://example.com:/"), L"http://example.com");
    EXPECT_EQ(OriginOf(L"http://localhost:05190/"), L"http://localhost:5190");
}

TEST(OriginPolicyOriginOf, BackslashEndsTheHost) {
    EXPECT_EQ(OriginOf(L"https://example.com\\foo-ui-webview2.local"), L"https://example.com");
}

// The page's real host follows the "@"; whatever precedes it is chosen by the page.
TEST(OriginPolicyOriginOf, RefusesUserInfo) {
    EXPECT_EQ(OriginOf(L"https://foo-ui-webview2.local:1@example.com/"), L"");
    EXPECT_EQ(OriginOf(L"http://127.0.0.1:x@127.0.0.2:62834/userinfo.html"), L"");
    EXPECT_EQ(OriginOf(L"https://user@foo-ui-webview2.local/"), L"");
}

TEST(OriginPolicyOriginOf, RefusesOtherSchemesAndBadAuthorities) {
    EXPECT_EQ(OriginOf(L"file:///C:/theme/index.html"), L"");
    EXPECT_EQ(OriginOf(L"data:text/html,<p>x</p>"), L"");
    EXPECT_EQ(OriginOf(L"about:blank"), L"");
    EXPECT_EQ(OriginOf(L"edge://settings"), L"");
    EXPECT_EQ(OriginOf(L"https://"), L"");
    EXPECT_EQ(OriginOf(L"https:///path"), L"");
    EXPECT_EQ(OriginOf(L"http://localhost:0/"), L"");
    EXPECT_EQ(OriginOf(L"http://localhost:65536/"), L"");
    EXPECT_EQ(OriginOf(L"http://localhost:80a/"), L"");
    EXPECT_EQ(OriginOf(L"http://a:b:80/"), L"");
    EXPECT_EQ(OriginOf(L"http://[::1/"), L"");
    EXPECT_EQ(OriginOf(L"http://[::1]x/"), L"");
    EXPECT_EQ(OriginOf(L""), L"");
}

TEST(OriginPolicyDevServerOrigin, OnlyWhenSwitchedOn) {
    EXPECT_EQ(DevServerOrigin(true, "http://localhost:5190/"), L"http://localhost:5190");
    EXPECT_EQ(DevServerOrigin(false, "http://localhost:5190/"), L"");
    EXPECT_EQ(DevServerOrigin(true, ""), L"");
    EXPECT_EQ(DevServerOrigin(true, "not a url"), L"");
}

TEST(OriginPolicyIsTrustedPage, BundledOriginAndInlinePages) {
    const std::vector<std::wstring> none;
    EXPECT_TRUE(IsTrustedPage(L"https://foo-ui-webview2.local/index.html", none));
    EXPECT_TRUE(IsTrustedPage(L"about:blank", none));
    EXPECT_FALSE(IsTrustedPage(L"about:blank#x", none));
}

TEST(OriginPolicyIsTrustedPage, RegisteredOriginsMatchExactly) {
    const std::vector<std::wstring> registered = {L"http://localhost:5190", L"https://staging.example.com"};
    EXPECT_TRUE(IsTrustedPage(L"http://localhost:5190/src/main.ts", registered));
    EXPECT_TRUE(IsTrustedPage(L"https://staging.example.com/theme/", registered));
    EXPECT_FALSE(IsTrustedPage(L"http://localhost:5191/", registered));
    EXPECT_FALSE(IsTrustedPage(L"http://127.0.0.1:5190/", registered));
    EXPECT_FALSE(IsTrustedPage(L"https://staging.example.com.attacker.test/", registered));
    EXPECT_FALSE(IsTrustedPage(L"http://staging.example.com/", registered));
}

// The two forms that passed the old prefix comparison.
TEST(OriginPolicyIsTrustedPage, UserInfoCannotBorrowATrustedPrefix) {
    const std::vector<std::wstring> registered = {L"http://localhost:5190"};
    EXPECT_FALSE(IsTrustedPage(L"https://foo-ui-webview2.local:1@attacker.test/", registered));
    EXPECT_FALSE(IsTrustedPage(L"http://localhost:5190:x@attacker.test/", registered));
}

TEST(OriginPolicyIsTrustedPage, NothingElseByDefault) {
    const std::vector<std::wstring> none;
    EXPECT_FALSE(IsTrustedPage(L"http://localhost:5190/", none));
    EXPECT_FALSE(IsTrustedPage(L"file:///C:/theme/index.html", none));
    EXPECT_FALSE(IsTrustedPage(L"https://app.local/", none));
    EXPECT_FALSE(IsTrustedPage(L"https://fb2k.local/", none));
    EXPECT_FALSE(IsTrustedPage(L"", none));
}

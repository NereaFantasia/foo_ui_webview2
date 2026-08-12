// test_dnd_origin_policy.cpp - origin gating for the path side channel.
#include "pch.h"
#include "../src/webview/dnd/DndOriginPolicy.h"

using fb2k_dnd::AllowsPaths;

// Dev server off unless a case says otherwise.
constexpr bool kNoDevServer = false;
constexpr bool kDevServer = true;

TEST(DndOriginPolicy, BuiltinVirtualHostsAllowed) {
    EXPECT_TRUE(AllowsPaths(L"https://foo-ui-webview2.local", kNoDevServer));
    EXPECT_TRUE(AllowsPaths(L"https://app.local", kNoDevServer));
    EXPECT_TRUE(AllowsPaths(L"https://fb2k.local", kNoDevServer));
}

TEST(DndOriginPolicy, ThirdPartyOriginsDenied) {
    EXPECT_FALSE(AllowsPaths(L"https://example.com", kNoDevServer));
    EXPECT_FALSE(AllowsPaths(L"http://evil.test", kNoDevServer));
    EXPECT_FALSE(AllowsPaths(L"https://cdn.jsdelivr.net", kNoDevServer));
}

TEST(DndOriginPolicy, LookalikeHostsDenied) {
    // Exact match, not prefix match: a hostile host must not pass by merely
    // starting with a trusted name.
    EXPECT_FALSE(AllowsPaths(L"https://foo-ui-webview2.local.evil.com", kNoDevServer));
    EXPECT_FALSE(AllowsPaths(L"https://app.local.attacker.test", kNoDevServer));
}

TEST(DndOriginPolicy, TrailingPathDenied) {
    // Callers must normalise first; an un-normalised value must not slip through.
    EXPECT_FALSE(AllowsPaths(L"https://app.local/index.html", kNoDevServer));
}

TEST(DndOriginPolicy, EmptyOriginDenied) {
    EXPECT_FALSE(AllowsPaths(L"", kNoDevServer));
}

TEST(DndOriginPolicy, AboutBlankDenied) {
    // about:blank is fine for invoke transport but has no meaningful origin,
    // so it must not receive paths.
    EXPECT_FALSE(AllowsPaths(L"about:blank", kNoDevServer));
}

TEST(DndOriginPolicy, DevServerOriginOnlyWhenDevModeEnabled) {
    EXPECT_FALSE(AllowsPaths(L"http://localhost:5174", kNoDevServer));
    EXPECT_TRUE(AllowsPaths(L"http://localhost:5174", kDevServer));
}

TEST(DndOriginPolicy, DevModeDoesNotOpenArbitraryOrigins) {
    // Enabling the dev server must not turn into a blanket allow.
    EXPECT_FALSE(AllowsPaths(L"https://example.com", kDevServer));
    EXPECT_FALSE(AllowsPaths(L"http://evil.test:5174", kDevServer));
}

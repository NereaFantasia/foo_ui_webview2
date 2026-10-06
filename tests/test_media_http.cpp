#include "pch.h"
#include "media/MediaHttp.h"
#include <gtest/gtest.h>

TEST(MediaHttp, RequiresTheIssuingOriginAndRejectsOpaqueOrUntrustedFrames) {
    EXPECT_TRUE(media::AuthorizeRequestOrigin(L"https://app.local", L"https://app.local", L""));
    EXPECT_FALSE(media::AuthorizeRequestOrigin(L"https://app.local", L"null", L"https://app.local"));
    EXPECT_FALSE(media::AuthorizeRequestOrigin(L"https://app.local", L"https://evil.example", L"https://app.local"));
    EXPECT_FALSE(media::AuthorizeRequestOrigin(L"about:blank", L"null", L""));
    EXPECT_FALSE(media::AuthorizeRequestOrigin(L"https://app.local", L"", L"https://app.local"));
    EXPECT_TRUE(media::AuthorizeRequestOrigin(L"https://foo-ui-webview2.local", L"", L"https://foo-ui-webview2.local"));
}
TEST(MediaHttp, PreflightOnlyGrantsReadMethodsAndRange) {
    EXPECT_TRUE(media::ValidPreflight(L"GET", L" Range "));
    EXPECT_TRUE(media::ValidPreflight(L"HEAD", L""));
    EXPECT_FALSE(media::ValidPreflight(L"POST", L"range"));
    EXPECT_FALSE(media::ValidPreflight(L"GET", L"range, authorization"));
}

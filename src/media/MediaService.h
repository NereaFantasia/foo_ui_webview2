#pragma once
#include "media/MediaTokens.h"

namespace media {
// 令牌表不加锁，仅在 WebView 所属主线程访问；后台读取使用令牌副本。
MediaTokens& Tokens();
// 权限判断可能查询 foobar2000 媒体库，只能在主线程调用。
bool CanRead(const std::wstring& path);
std::string Utf8(const std::wstring& text);
std::wstring Wide(const std::string& text);
constexpr const wchar_t* kUrlPrefix = L"https://foo-ui-webview2.local/fb2k-media/";
}

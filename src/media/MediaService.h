#pragma once
#include "media/MediaTokens.h"

namespace media {
// 令牌表不加锁，仅在 WebView 所属主线程访问；后台读取使用令牌副本。
MediaTokens& Tokens();
// 权限判断可能查询 foobar2000 媒体库，只能在主线程调用。
bool CanRead(const std::wstring& path);
std::string Utf8(const std::wstring& text);
std::wstring Wide(const std::string& text);
// 主机名只靠 WebResourceRequested 应答，不能是做过 SetVirtualHostNameToFolderMapping 的主机：
// 映射主机上的请求不进事件，页面只会得到 ERR_FILE_NOT_FOUND。.invalid 不会被解析，拦截失效时请求也不会出网。
// 封面路由的主机名在 ArtworkRequestParser.cpp，两处须保持一致。
constexpr const wchar_t* kUrlPrefix = L"https://fb2k-routes.invalid/fb2k-media/";
}

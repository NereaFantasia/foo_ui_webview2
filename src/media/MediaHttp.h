#pragma once
#include <string>
#include <string_view>

namespace media {
// Origin:null 不能证明请求来自获发令牌的文档，必须拒绝。
// 同源媒体请求可能不带 Origin；仅固定虚拟来源且 Referer 同源时接受这一形式。
inline bool AuthorizeRequestOrigin(std::wstring_view ownerOrigin, std::wstring_view origin,
                                   std::wstring_view refererOrigin) {
    if (ownerOrigin.empty() || (!ownerOrigin.starts_with(L"http://") && !ownerOrigin.starts_with(L"https://"))) return false;
    if (!origin.empty()) return origin == ownerOrigin;
    return ownerOrigin == L"https://foo-ui-webview2.local" && refererOrigin == ownerOrigin;
}
inline bool ValidPreflight(std::wstring_view method, std::wstring headers) {
    if (method != L"GET" && method != L"HEAD") return false;
    for (auto& c : headers) if (c >= L'A' && c <= L'Z') c += L'a' - L'A';
    const auto first = headers.find_first_not_of(L" \t");
    if (first == std::wstring::npos) return true;
    const auto last = headers.find_last_not_of(L" \t");
    return headers.substr(first, last - first + 1) == L"range";
}
}

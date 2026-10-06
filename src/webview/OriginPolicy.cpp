// OriginPolicy.cpp
#include "pch.h"
#include "webview/OriginPolicy.h"

#include <algorithm>
#include <format>

namespace origin_policy {
namespace {

std::wstring LowerAscii(std::wstring_view text) {
    std::wstring out(text);
    for (wchar_t& c : out) {
        if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
    }
    return out;
}

// 十进制 1..65535。前导零按数值重写，与浏览器序列化后的形态一致。
bool ParsePort(std::wstring_view text, unsigned& port) {
    if (text.empty() || text.size() > 5) return false;
    unsigned value = 0;
    for (wchar_t c : text) {
        if (c < L'0' || c > L'9') return false;
        value = value * 10 + static_cast<unsigned>(c - L'0');
    }
    if (value < 1 || value > 65535) return false;
    port = value;
    return true;
}

}  // namespace

std::wstring OriginOf(std::wstring_view url) {
    const size_t schemeEnd = url.find(L"://");
    if (schemeEnd == std::wstring_view::npos) return {};
    const std::wstring scheme = LowerAscii(url.substr(0, schemeEnd));
    unsigned defaultPort = 0;
    if (scheme == L"https") {
        defaultPort = 443;
    } else if (scheme == L"http") {
        defaultPort = 80;
    } else {
        return {};
    }

    // 反斜杠在 http(s) 地址里与斜杠等价，同样结束主机段。
    const std::wstring_view rest = url.substr(schemeEnd + 3);
    const std::wstring_view authority = rest.substr(0, rest.find_first_of(L"/\\?#"));
    // 带用户信息的地址一律不认：真正的主机在 @ 之后，前面那段可以写成任何受信任的来源。
    if (authority.empty() || authority.find(L'@') != std::wstring_view::npos) return {};

    std::wstring_view host = authority;
    std::wstring_view portText;
    if (authority.front() == L'[') {
        const size_t close = authority.find(L']');
        if (close == std::wstring_view::npos) return {};
        host = authority.substr(0, close + 1);
        const std::wstring_view after = authority.substr(close + 1);
        if (!after.empty()) {
            if (after.front() != L':') return {};
            portText = after.substr(1);
        }
    } else {
        const size_t colon = authority.find(L':');
        if (colon != std::wstring_view::npos) {
            host = authority.substr(0, colon);
            portText = authority.substr(colon + 1);
        }
    }
    if (host.empty() || host == L"[]") return {};

    std::wstring origin = scheme + L"://" + LowerAscii(host);
    // 冒号后为空等同省略端口。
    if (!portText.empty()) {
        unsigned port = 0;
        if (!ParsePort(portText, port)) return {};
        if (port != defaultPort) origin += std::format(L":{}", port);
    }
    return origin;
}

std::wstring DevServerOrigin(bool useDevServer, const std::string& urlUtf8) {
    if (!useDevServer || urlUtf8.empty()) return {};
    const auto size = static_cast<int>(urlUtf8.size());
    const int length = MultiByteToWideChar(CP_UTF8, 0, urlUtf8.data(), size, nullptr, 0);
    if (length <= 0) return {};
    std::wstring wide(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, urlUtf8.data(), size, wide.data(), length);
    return OriginOf(wide);
}

bool IsTrustedPage(std::wstring_view url, const std::vector<std::wstring>& registered) {
    if (url == kInlinePageSource) return true;
    const std::wstring origin = OriginOf(url);
    if (origin.empty()) return false;
    if (origin == kBundledOrigin) return true;
    return std::ranges::find(registered, origin) != registered.end();
}

}  // namespace origin_policy

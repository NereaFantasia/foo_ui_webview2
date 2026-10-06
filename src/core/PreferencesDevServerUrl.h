#pragma once

// ============================================
// PreferencesDevServerUrl.h - 开发服务器 URL 的纯校验逻辑
//
// 开发者子页在 Apply 前用这里的规则拒绝写不进去也导航不了的地址；本头文件与 HWND、
// 网络和 foobar2000 SDK 都无关，让 GoogleTest 直接覆盖真实生产符号。
//
// 规则：
//   - 空串合法：开关打开而 URL 为空时加载路径回退本地模板，不算配置错误；
//   - 非空时须以 http:// 或 https:// 起头（大小写不敏感），主机名非空，
//     端口若给出须在 1–65535，整串不含空白；路径、查询串放行，由开发服务器自己解释。
// 主机与端口（authority）只收 ASCII：全角冒号、全角空格、中日韩字符都拒绝——它们不能规范化为
// origin，放进去只会在加载时静默回退本地模板。主机名此外只查明显非法的字符（引号、反斜杠、
// '@'），不做 DNS 或 IP 字面量校验。
// ============================================

#include <algorithm>
#include <cctype>
#include <string>

namespace prefs_devserver {

enum class UrlError {
    None,
    Scheme,      // 不是 http:// 或 https:// 起头
    Host,        // 主机名为空或含非法字符
    Port,        // 端口不是 1–65535 的十进制数
    Whitespace,  // 含空白字符
    NonAscii,    // 主机或端口部分含非 ASCII 字节（全角冒号、全角空格、中日韩字符等）
};

// 去掉首尾 ASCII 空白。
inline std::string Trim(const std::string& input) {
    const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    const auto begin = std::find_if_not(input.begin(), input.end(), isSpace);
    const auto end = std::find_if_not(input.rbegin(), input.rend(), isSpace).base();
    return begin < end ? std::string(begin, end) : std::string();
}

inline bool StartsWithIgnoreCase(const std::string& text, const char* prefix) {
    size_t i = 0;
    for (; prefix[i] != '\0'; ++i) {
        if (i >= text.size()) return false;
        if (std::tolower(static_cast<unsigned char>(text[i])) != std::tolower(static_cast<unsigned char>(prefix[i]))) {
            return false;
        }
    }
    return true;
}

// 按「空白 → 协议 → 主机（先查非 ASCII）→ 端口」的顺序给出第一处不通过的原因。url 应已经 Trim。
inline UrlError CheckUrl(const std::string& url) {
    if (url.empty()) return UrlError::None;
    for (char c : url) {
        if (std::isspace(static_cast<unsigned char>(c))) return UrlError::Whitespace;
    }

    size_t schemeEnd = 0;
    if (StartsWithIgnoreCase(url, "https://")) schemeEnd = 8;
    else if (StartsWithIgnoreCase(url, "http://")) schemeEnd = 7;
    else return UrlError::Scheme;

    // authority 到第一个 '/'、'?' 或 '#' 为止。
    const size_t authorityEnd = url.find_first_of("/?#", schemeEnd);
    const std::string authority = url.substr(schemeEnd, authorityEnd == std::string::npos ? std::string::npos
                                                                                          : authorityEnd - schemeEnd);
    if (authority.empty()) return UrlError::Host;
    // UTF-8 里非 ASCII 字符的每个字节都 >= 0x80；控制字符同样不可能出现在合法的 authority 里。
    for (char c : authority) {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte >= 0x80 || byte < 0x20) return UrlError::NonAscii;
    }

    // IPv6 字面量带方括号，端口分隔符是 ']' 之后的那个冒号。
    size_t hostEnd = authority.size();
    std::string port;
    if (authority[0] == '[') {
        const size_t close = authority.find(']');
        if (close == std::string::npos || close == 1) return UrlError::Host;
        hostEnd = close + 1;
        if (hostEnd < authority.size()) {
            if (authority[hostEnd] != ':') return UrlError::Host;
            port = authority.substr(hostEnd + 1);
        }
    } else {
        const size_t colon = authority.rfind(':');
        if (colon != std::string::npos) {
            hostEnd = colon;
            port = authority.substr(colon + 1);
        }
        if (hostEnd == 0) return UrlError::Host;
        for (size_t i = 0; i < hostEnd; ++i) {
            const char c = authority[i];
            if (c == '"' || c == '\'' || c == '\\' || c == '@' || c == ':') return UrlError::Host;
        }
    }

    if (hostEnd < authority.size() || authority.back() == ':') {
        // 写了冒号就必须跟一个合法端口。
        if (port.empty() || port.size() > 5) return UrlError::Port;
        int value = 0;
        for (char c : port) {
            if (!std::isdigit(static_cast<unsigned char>(c))) return UrlError::Port;
            value = value * 10 + (c - '0');
        }
        if (value < 1 || value > 65535) return UrlError::Port;
    }
    return UrlError::None;
}

}  // namespace prefs_devserver

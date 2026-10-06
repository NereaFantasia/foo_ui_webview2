#pragma once

// ============================================
// PreferencesCdpPort.h - CDP 远程调试端口的解析与范围校验
//
// Developer 子页把端口存成整数，输入框给的是文本。本头文件负责文本 → 整数、范围校验与
// 存储值清洗，与 HWND、WebView2 和 foobar2000 SDK 都无关，让 GoogleTest 直接覆盖。
//
// 范围 1024–65535、默认 9222；1024 以下是特权端口，
// 组件不以管理员身份运行，绑不上。
// ============================================

#include <cctype>
#include <string>

namespace prefs_cdp {

inline constexpr int kMinPort = 1024;
inline constexpr int kMaxPort = 65535;
inline constexpr int kDefaultPort = 9222;
// ParsePort 解析失败时的返回值；不在合法范围内，Validate 会把它判为越界。
inline constexpr int kInvalidPort = 0;

inline bool IsValidPort(int port) {
    return port >= kMinPort && port <= kMaxPort;
}

// 只接受由十进制数字组成、首尾可带空白的文本；空串、带符号、带小数点或超过 5 位都算失败。
// 数值越界（例如 80 或 70000）也返回 kInvalidPort，由调用方给出范围提示。
inline int ParsePort(const std::string& text) {
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    if (begin == end || end - begin > 5) return kInvalidPort;
    int value = 0;
    for (size_t i = begin; i < end; ++i) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (!std::isdigit(c)) return kInvalidPort;
        value = value * 10 + (c - '0');
    }
    return IsValidPort(value) ? value : kInvalidPort;
}

// 存储值可能来自旧版本或被外部改写：不在范围内就退回默认端口，越界不当成错误。
inline int SanitizePort(int port) {
    return IsValidPort(port) ? port : kDefaultPort;
}

}  // namespace prefs_cdp

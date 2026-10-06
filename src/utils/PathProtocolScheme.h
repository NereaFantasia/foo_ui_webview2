#pragma once
// ============================================
// PathProtocolScheme.h - 虚拟 / 网络协议判定的单点实现
// ============================================
//
// 本头只依赖标准库，让路径安全调用方与不链接 fb2k SDK 的测试使用同一协议判定。
//
// 判定回答的问题是: 这条拼写会不会被本机文件系统打开。会的 (盘符形态、UNC、
// 相对路径, 以及内嵌本地路径的 file:// / archive:// / unpack://) 必须进入完整的
// 路径安全流水线 (遍历检测、canonical、黑白名单); 不会的 (http、cdda、第三方输入
// 组件的自定义协议) 在流水线入口早退放行。判错方向即是绕过: 把一条本机可打开的
// 拼写判成虚拟协议, 它就跳过了全部检查。
//
// 契约: 虚拟协议 = 行首锚定的 `scheme://`, scheme 为 2 到 19 个 [A-Za-z0-9+.-]
// 字符 (大小写不敏感), 且 scheme 不是 file / archive / unpack。
//
//   - 长度下限 2 是安全关键项: `X://...` 是盘符形态, Win32 / CRT / std::filesystem
//     把它规范化为 `X:\...` 并打开本地文件。将这种拼写判为虚拟协议会跳过
//     本地路径检查；scheme 至少两字符可排除单字母盘符形态。
//   - 行首锚定: `://` 出现在别处 (`C:\a\b://c`) 不构成协议。
//   - 字符集取 RFC 3986 的 scheme 字符, 放宽首字符允许数字: foo_youtube 旧版协议
//     `3dydfy://` 以数字开头且仍存在于用户播放列表中; 其 2.x 协议 `fy+https://`
//     用到 `+`。下划线等其它字符不在集合内, 目前没有已知 fb2k 组件使用; 若出现,
//     在 IsSchemeChar 一处放宽即可。
//   - 长度上限为 19；更长的 scheme 不会由本判定直接放行。
//   - file-relative:// 不在排除列表中, 仍按虚拟协议放行。它内嵌相对 fb2k 程序目录
//     的本地路径，但本判定不调用 SDK 解析基准目录；调用方不能将此结果当作
//     已完成本地路径安全检查的证据。
//
// ============================================

#include <cwctype>
#include <string_view>

namespace path_protocol {

inline constexpr size_t kMinSchemeLength = 2;
inline constexpr size_t kMaxSchemeLength = 19;

inline bool IsSchemeChar(wchar_t c) {
    return (c >= L'a' && c <= L'z') ||
           (c >= L'A' && c <= L'Z') ||
           (c >= L'0' && c <= L'9') ||
           c == L'+' || c == L'-' || c == L'.';
}

// 返回行首 scheme 的长度; 不满足契约 (无 `://`、长度越界、含集合外字符) 返回 0。
inline size_t ParseSchemeLength(std::wstring_view path) {
    size_t n = 0;
    while (n < path.size() && n <= kMaxSchemeLength && IsSchemeChar(path[n])) {
        ++n;
    }
    if (n < kMinSchemeLength || n > kMaxSchemeLength) return 0;
    if (path.size() < n + 3) return 0;
    if (path[n] != L':' || path[n + 1] != L'/' || path[n + 2] != L'/') return 0;
    return n;
}

inline bool SchemeEqualsIgnoreCase(std::wstring_view scheme, std::wstring_view expected) {
    if (scheme.size() != expected.size()) return false;
    for (size_t i = 0; i < scheme.size(); ++i) {
        if (::towlower(scheme[i]) != expected[i]) return false;
    }
    return true;
}

// 内嵌本地路径的协议: 剥壳后仍是本机文件, 必须走完整流水线。
inline bool SchemeEmbedsLocalPath(std::wstring_view scheme) {
    return SchemeEqualsIgnoreCase(scheme, L"file") ||
           SchemeEqualsIgnoreCase(scheme, L"archive") ||
           SchemeEqualsIgnoreCase(scheme, L"unpack");
}

// 是否为不涉及本机文件系统的虚拟 / 网络协议路径。true 表示调用方可跳过本地路径
// 安全流水线; false 表示必须进入流水线 (含内嵌本地路径的 file / archive / unpack)。
inline bool IsVirtualOrNetworkProtocol(std::wstring_view path) {
    const size_t n = ParseSchemeLength(path);
    if (n == 0) return false;
    return !SchemeEmbedsLocalPath(path.substr(0, n));
}

}  // namespace path_protocol

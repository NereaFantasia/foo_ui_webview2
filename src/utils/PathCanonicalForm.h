#pragma once
// ============================================
// PathCanonicalForm.h - 路径规范化 (canonical + 8.3 展开) 的单点实现
// ============================================
//
// 从 PathSecurity.h 抽出为独立单元。抽出的理由是可测性而非分层美感:
// PathSecurity.h 依赖 foobar2000 SDK 头链, 而测试工程刻意 build against
// tests/compat/fb2k_types.h 以避开完整 SDK —— SDK 会拉入 winsock2.h, 与测试
// pch 已含的 windows.h (winsock v1) 冲突, 主项目靠 WIN32_LEAN_AND_MEAN 规避,
// 测试工程没有该定义。本函数只需 STL 与 Win32, 抽出后单元测试可直接针对真实
// 现断言, 而不必像其余五个 PathSecurity 测试那样退化为独立重实现。
//
// ============================================

#include <string>
#include <filesystem>
#include <cwctype>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace path_canonical {

// 把路径规范化到前缀匹配所用的统一形态: canonical 解析 + 系统盘 8.3 短名展开。
//
// 名单建表与 PassBasicPathSafetyChecks 共用本函数, 因为三张名单的条目必须与
// 被判定的 realPath 处于同一形态。名单存原始路径而判定用 canonical 后的路径时,
// 经 junction / 符号链接 / 已知文件夹重定向的机器上前缀匹配恒 miss, 名单静默失效
// (实测: profile 目录 junction 到非系统盘后, temp 写白名单整体失效)。
//
// 失败一律回退原值而非抛出: 名单建表在单例构造期运行, 此时个别目录可能尚不可访问,
// 回退到未解析形态只会让该条目匹配面变窄, 方向安全。
inline std::wstring ResolveToCanonicalForm(const std::wstring& input, wchar_t systemDrive) {
    if (input.empty()) {
        return input;
    }

    // 解析符号链接到真实路径 (防止符号链接绕过)
    std::wstring resolved;
    try {
        if (std::filesystem::exists(input)) {
            resolved = std::filesystem::canonical(input).wstring();
        } else {
            resolved = std::filesystem::weakly_canonical(input).wstring();
        }
    } catch (...) {
        resolved = input;
    }

    // 展开 8.3 短文件名 (防止 PROGRA~1 等绕过黑名单)
    //
    // 仅对系统盘执行: 8.3 展开的唯一消费者是黑白名单前缀匹配，而
    //   - ValidatePath 对非系统盘在盘符判断处直接放行，不查黑白名单;
    //   - ValidateMediaWriteAccess 的黑名单全是本机系统目录，非系统盘匹配不上;
    //   - 其写白名单 (profile/temp) 若在非系统盘则匹配失败，落到"非系统盘放行"，
    //     方向是更严格而非更宽松。
    //
    // 判据必须取 canonical **之后**的盘符: 按 raw 路径盘符提前跳过会让
    // D:\link -> C:\Windows\System32 的 junction 被当作非系统盘，绕过黑名单。
    const bool onSystemDrive = resolved.length() >= 2 &&
                               resolved[1] == L':' &&
                               ::towupper(resolved[0]) == systemDrive;
    if (onSystemDrive) {
        // 两段式调用: 缓冲区不足时 GetLongPathNameW 返回所需长度而不写入，
        // 旧实现只判 len < MAX_PATH 会静默跳过超长路径的展开，形成黑名单弱化点。
        wchar_t stackBuf[MAX_PATH];
        DWORD len = GetLongPathNameW(resolved.c_str(), stackBuf, MAX_PATH);
        if (len > 0 && len < MAX_PATH) {
            resolved.assign(stackBuf, len);
        } else if (len >= MAX_PATH) {
            std::wstring longPath(len, L'\0');
            DWORD written = GetLongPathNameW(resolved.c_str(), longPath.data(), len);
            if (written > 0 && written < len) {
                longPath.resize(written);
                resolved = std::move(longPath);
            }
        }
    }

    return resolved;
}

}  // namespace path_canonical

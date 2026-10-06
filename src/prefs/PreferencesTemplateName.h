#pragma once

// ============================================
// PreferencesTemplateName.h - 模板起名对话框的纯校验逻辑
//
// Manage 菜单的 Create 与 Rename 共用一个起名对话框；对话框在按下确定时按
// 这里的规则判定，没通过就留在对话框里让用户改，不落盘。本头文件与 HWND、
// 文件系统和 foobar2000 SDK 都无关，让 GoogleTest 直接覆盖真实生产符号。
//
// 规则：
//   - 名字先去掉首尾空白，剩下什么都没有就是空；
//   - 字符集沿用 prefs_draft::IsValidTemplateName（字母、数字、连字符、下划线）；
//   - 与现有模板重名按大小写不敏感判定：Windows 文件系统不区分大小写，
//     "Night" 与 "night" 落到同一个目录。
// ============================================

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "prefs/PreferencesDraft.h"

namespace prefs_template {

enum class NameError {
    None,
    Empty,         // 去掉首尾空白后没有字符
    InvalidChars,  // 含字母、数字、连字符、下划线以外的字符
    Duplicate,     // 与现有模板重名（不区分大小写）
};

// 去掉首尾的 ASCII 空白。名字内部的空白不在这里处理，留给字符集校验去拒。
inline std::string Trim(const std::string& input) {
    // unsigned char 转型：非 ASCII 字节为负值时 isspace 是未定义行为
    const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    const auto begin = std::find_if_not(input.begin(), input.end(), isSpace);
    const auto end = std::find_if_not(input.rbegin(), input.rend(), isSpace).base();
    return begin < end ? std::string(begin, end) : std::string();
}

// 合法名字只含 ASCII，按 ASCII 大小写折叠逐字节比较即可。
inline bool EqualsIgnoreCase(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        const int ca = std::tolower(static_cast<unsigned char>(a[i]));
        const int cb = std::tolower(static_cast<unsigned char>(b[i]));
        if (ca != cb) return false;
    }
    return true;
}

inline bool ContainsIgnoreCase(const std::vector<std::string>& names, const std::string& wanted) {
    return std::any_of(names.begin(), names.end(),
        [&wanted](const std::string& name) { return EqualsIgnoreCase(name, wanted); });
}

// 按“空 → 非法字符 → 重名”的顺序给出第一处不通过的原因。candidate 应已经 Trim。
// Rename 时 existing 含旧名本身，因此改成只有大小写不同的名字也按重名拒绝，
// 与 fs::rename 在不区分大小写的文件系统上的行为一致。
inline NameError CheckName(const std::string& candidate, const std::vector<std::string>& existing) {
    if (candidate.empty()) return NameError::Empty;
    if (!prefs_draft::IsValidTemplateName(candidate)) return NameError::InvalidChars;
    if (ContainsIgnoreCase(existing, candidate)) return NameError::Duplicate;
    return NameError::None;
}

}  // namespace prefs_template

/**
 * I18n.h - 国际化辅助函数
 * 
 * 提供中英双语支持，基于系统语言自动切换。
 * Header-only 实现，无需 .cpp 文件。
 * 
 * 使用方式：
 *   #include "utils/I18n.h"
 *   CreateWindowExW(0, L"STATIC", TR("Template:", "模板:"), ...);
 *   out = TRU("Description", "描述");
 */

#pragma once
#include <Windows.h>

namespace i18n {

/**
 * 检测系统是否为中文环境
 * 使用 GetUserDefaultUILanguage() 获取系统 UI 语言
 * 结果缓存以避免重复调用
 */
inline bool IsChineseLocale() {
    static int cached = -1;
    if (cached >= 0) return cached != 0;
    
    LANGID lang = GetUserDefaultUILanguage();
    cached = (PRIMARYLANGID(lang) == LANG_CHINESE) ? 1 : 0;
    return cached != 0;
}

/**
 * 获取本地化字符串（wchar_t 版本）
 * 用于 Win32 控件（CreateWindowExW、SetWindowTextW 等）
 * 
 * @param en 英文字符串
 * @param zh 中文字符串
 * @return 根据系统语言返回对应字符串
 */
inline const wchar_t* T(const wchar_t* en, const wchar_t* zh) {
    return IsChineseLocale() ? zh : en;
}

/**
 * 获取本地化字符串（UTF-8 const char* 版本）
 * 用于 fb2k SDK 接口（get_name、get_description 等）
 * 
 * @param en 英文字符串（UTF-8）
 * @param zh 中文字符串（UTF-8）
 * @return 根据系统语言返回对应字符串
 */
inline const char* TU(const char* en, const char* zh) {
    return IsChineseLocale() ? zh : en;
}

} // namespace i18n

// ============================================
// 宏简化（避免与 Windows _T 宏冲突）
// ============================================

/**
 * TR 宏 - wchar_t 版本
 * 自动添加 L 前缀
 * 用法: TR("English", "中文")
 */
#define TR(en, zh) i18n::T(L##en, L##zh)

/**
 * TRU 宏 - UTF-8 版本
 * 用法: TRU("English", "中文")
 */
#define TRU(en, zh) i18n::TU(en, zh)

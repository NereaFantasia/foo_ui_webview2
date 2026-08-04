/**
 * I18n.h - 国际化辅助函数
 *
 * 提供中英双语支持。语言判定跟随 **foobar2000 程序呈现语言**，而非操作系统语言：
 * foobar2000 没有 i18n API，汉化版是把核心自带字符串硬编码替换成中文，
 * 因此这里反向探测核心自己的字符串来推断当前呈现语言（实现见 I18n.cpp）。
 * 探测失败时回退到操作系统 UI 语言。用户也可在偏好设置里显式覆盖。
 *
 * 使用方式：
 *   #include "utils/I18n.h"
 *   CreateWindowExW(0, L"STATIC", TR("Template:", "模板:"), ...);
 *   out = TRU("Description", "描述");
 *
 * 本头文件故意不引入 foobar2000 SDK —— 它被多个头文件间接包含，
 * 探测逻辑一律留在 I18n.cpp 里。
 */

#pragma once
#include <Windows.h>

namespace i18n {

/**
 * 语言覆盖模式，对应偏好设置里的下拉项。
 * 数值会持久化到 cfg_var，禁止重排或复用既有数值。
 */
enum class LanguageOverride {
    Auto = 0,     // 跟随 foobar2000 呈现语言（探测失败则回退系统语言）
    English = 1,  // 强制英文
    Chinese = 2   // 强制中文
};

/** 语言判定的实际来源，供日志与诊断使用。 */
enum class LanguageSource {
    Unresolved = 0,  // 尚未判定（core_api 服务未就绪）
    Override,        // 用户在偏好设置里显式指定
    Fb2kProbe,       // 探测 foobar2000 核心字符串成功
    OsFallback       // 探测不可用，回退操作系统 UI 语言
};

/**
 * 当前是否应呈现中文。
 * 首次成功判定后缓存；core_api 服务尚未就绪时返回系统语言且不缓存，
 * 避免把启动早期的回退值永久钉死。
 */
bool IsChineseLocale();

/** 读取本次判定的来源，用于 console 诊断。 */
LanguageSource GetLanguageSource();

/** 读取/写入用户覆盖设置。Set 会清除缓存，后续 TR/TRU 立即改用新语言。 */
LanguageOverride GetLanguageOverride();
void SetLanguageOverride(LanguageOverride value);

/**
 * 丢弃缓存的判定结果，下次 IsChineseLocale() 重新解析。
 * 供覆盖设置变更与 initquit 预热使用。
 */
void InvalidateLanguageCache();

/**
 * 获取本地化字符串（wchar_t 版本）
 * 用于 Win32 控件（CreateWindowExW、SetWindowTextW 等）
 * 
 * @param en 英文字符串
 * @param zh 中文字符串
 * @return 根据当前呈现语言返回对应字符串
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
 * @return 根据当前呈现语言返回对应字符串
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

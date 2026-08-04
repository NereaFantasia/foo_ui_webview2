// ============================================
// I18n.cpp - 跟随 foobar2000 呈现语言的语言判定
//
// foobar2000 没有 i18n API：汉化版的做法是把核心自带的字符串硬编码替换成中文。
// 因此这里反向探测「核心自己注册的字符串」来推断程序当前呈现的语言：
// 若这些字符串已是中文，说明用户跑的是汉化版，本组件也应呈现中文。
//
// 探测源按代价从低到高排列，任一命中即停：
//   1. playlist_manager::playback_order_get_name — 最便宜，"Default" vs "默认"
//   2. 核心主菜单命令名（Preferences / Exit / About）— 最贴近"呈现语言"
//   3. Advanced Preferences 核心分支名（Tools 等）
// 全部不可用时回退操作系统 UI 语言（旧行为）。
//
// 已知局限：若某汉化版只替换了资源而未改这些代码内嵌字符串，探测会漏判。
// 偏好设置里的语言下拉即为此提供手动覆盖兜底。
// ============================================

#include "pch.h"
#include "utils/I18n.h"

#include <mutex>
#include <string>

namespace i18n {

namespace {

// 语言覆盖设置 GUID
// {A9C3F1E4-7B62-4D58-9E0A-3F5B8C1D2E70}
constexpr GUID guid_cfg_language_override =
    { 0xa9c3f1e4, 0x7b62, 0x4d58, { 0x9e, 0x0a, 0x3f, 0x5b, 0x8c, 0x1d, 0x2e, 0x70 } };

cfg_var_modern::cfg_int cfg_language_override(
    guid_cfg_language_override,
    static_cast<int>(LanguageOverride::Auto));

std::mutex g_mutex;
bool g_resolved = false;         // 判定是否已缓存
bool g_isChinese = false;        // 缓存的判定结果
LanguageSource g_source = LanguageSource::Unresolved;

/** 操作系统 UI 语言是否为中文 —— 探测不可用时的回退口径（本组件的旧行为）。 */
bool IsChineseOsUiLanguage() {
    const LANGID lang = GetUserDefaultUILanguage();
    return PRIMARYLANGID(lang) == LANG_CHINESE;
}

/**
 * 判断 UTF-8 文本是否"看起来是中文"。
 * 命中 CJK 统一汉字即认为是中文；但若同时出现日文假名，视为日文汉化补丁 ——
 * 本组件只有 zh/en 两档，日文应落到英文而非中文。
 */
bool LooksChinese(const char* utf8) {
    if (utf8 == nullptr || *utf8 == '\0') return false;

    const int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (wideLen <= 1) return false;

    std::wstring wide(static_cast<size_t>(wideLen), L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide.data(), wideLen) <= 0) {
        return false;
    }
    wide.resize(static_cast<size_t>(wideLen) - 1);  // 去掉结尾 NUL

    bool hasHan = false;
    for (const wchar_t ch : wide) {
        // 日文假名：平假名 U+3040–309F / 片假名 U+30A0–30FF
        if ((ch >= 0x3040 && ch <= 0x30FF) || (ch >= 0xFF66 && ch <= 0xFF9D)) {
            return false;
        }
        // CJK 统一汉字（含扩展 A）
        if ((ch >= 0x4E00 && ch <= 0x9FFF) || (ch >= 0x3400 && ch <= 0x4DBF)) {
            hasHan = true;
        }
    }
    return hasHan;
}

/** 探测源 1：播放顺序名。原版 "Default"/"Repeat (playlist)"，汉化版「默认」等。 */
bool ProbePlaybackOrderNames(bool& isChinese) {
    auto pm = playlist_manager::tryGet();
    if (!pm.is_valid()) return false;

    const t_size count = pm->playback_order_get_count();
    if (count == 0) return false;

    for (t_size i = 0; i < count; i++) {
        const char* name = pm->playback_order_get_name(i);
        if (name == nullptr || *name == '\0') continue;
        if (LooksChinese(name)) {
            isChinese = true;
            return true;
        }
    }

    // 拿到了非空名字但都不含汉字 —— 这是英文版的确定结论，不必再试其他源。
    isChinese = false;
    return true;
}

/**
 * 探测源 2：核心注册的主菜单命令名。
 * 只认 standard_commands 里由核心注册的槽位，避免被第三方组件的语言干扰。
 */
bool ProbeMainMenuCommandNames(bool& isChinese) {
    static const GUID probeGuids[] = {
        standard_commands::guid_main_preferences,
        standard_commands::guid_main_exit,
        standard_commands::guid_main_about,
    };

    bool sawAnyName = false;

    service_enum_t<mainmenu_commands> e;
    service_ptr_t<mainmenu_commands> ptr;
    while (e.next(ptr)) {
        const t_uint32 cmdCount = ptr->get_command_count();
        for (t_uint32 i = 0; i < cmdCount; i++) {
            GUID cmdGuid;
            try {
                cmdGuid = ptr->get_command(i);
            } catch (const std::exception&) {
                continue;
            }

            bool isProbeTarget = false;
            for (const GUID& probe : probeGuids) {
                if (cmdGuid == probe) { isProbeTarget = true; break; }
            }
            if (!isProbeTarget) continue;

            pfc::string8 name;
            try {
                ptr->get_name(i, name);
            } catch (const std::exception&) {
                continue;
            }

            if (name.is_empty()) continue;
            sawAnyName = true;
            if (LooksChinese(name.get_ptr())) {
                isChinese = true;
                return true;
            }
        }
    }

    if (sawAnyName) {
        isChinese = false;
        return true;
    }
    return false;
}

/** 探测源 3：Advanced Preferences 的核心分支名（"Tools" vs「工具」）。 */
bool ProbeAdvancedConfigBranchNames(bool& isChinese) {
    static const GUID probeGuids[] = {
        advconfig_entry::guid_branch_tools,
        advconfig_entry::guid_branch_playback,
        advconfig_entry::guid_branch_display,
    };

    bool sawAnyName = false;

    for (const GUID& guid : probeGuids) {
        service_ptr_t<advconfig_entry> entry;
        if (!advconfig_entry::g_find(entry, guid)) continue;

        pfc::string8 name;
        try {
            entry->get_name(name);
        } catch (const std::exception&) {
            continue;
        }

        if (name.is_empty()) continue;
        sawAnyName = true;
        if (LooksChinese(name.get_ptr())) {
            isChinese = true;
            return true;
        }
    }

    if (sawAnyName) {
        isChinese = false;
        return true;
    }
    return false;
}

const char* SourceName(LanguageSource source) {
    switch (source) {
        case LanguageSource::Override:    return "override";
        case LanguageSource::Fb2kProbe:   return "fb2k-probe";
        case LanguageSource::OsFallback:  return "os-fallback";
        default:                          return "unresolved";
    }
}

} // namespace

bool IsChineseLocale() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_resolved) return g_isChinese;

    // 1) 用户显式覆盖优先，且不依赖任何服务，可以立即缓存。
    const auto overrideMode = static_cast<LanguageOverride>(cfg_language_override.get());
    if (overrideMode == LanguageOverride::English || overrideMode == LanguageOverride::Chinese) {
        g_isChinese = (overrideMode == LanguageOverride::Chinese);
        g_source = LanguageSource::Override;
        g_resolved = true;
        return g_isChinese;
    }

    // 2) 服务未就绪（启动早期）：返回系统语言但**不缓存**，
    //    否则会把回退值永久钉死，之后再也探测不到程序语言。
    if (!core_api::are_services_available()) {
        return IsChineseOsUiLanguage();
    }

    // 3) 探测 foobar2000 自身字符串。
    bool probed = false;
    try {
        probed = ProbePlaybackOrderNames(g_isChinese)
              || ProbeMainMenuCommandNames(g_isChinese)
              || ProbeAdvancedConfigBranchNames(g_isChinese);
    } catch (const std::exception& ex) {
        console::printf("[WebView2 UI] i18n: language probe failed (%s), falling back to OS language",
                        ex.what());
        probed = false;
    }

    if (probed) {
        g_source = LanguageSource::Fb2kProbe;
    } else {
        g_isChinese = IsChineseOsUiLanguage();
        g_source = LanguageSource::OsFallback;
    }
    g_resolved = true;

    console::printf("[WebView2 UI] i18n: language=%s source=%s",
                    g_isChinese ? "zh" : "en", SourceName(g_source));
    return g_isChinese;
}

LanguageSource GetLanguageSource() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_source;
}

LanguageOverride GetLanguageOverride() {
    const int raw = static_cast<int>(cfg_language_override.get());
    switch (raw) {
        case static_cast<int>(LanguageOverride::English): return LanguageOverride::English;
        case static_cast<int>(LanguageOverride::Chinese): return LanguageOverride::Chinese;
        default: return LanguageOverride::Auto;
    }
}

void SetLanguageOverride(LanguageOverride value) {
    cfg_language_override.set(static_cast<int>(value));
    InvalidateLanguageCache();
}

void InvalidateLanguageCache() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_resolved = false;
    g_source = LanguageSource::Unresolved;
}

} // namespace i18n

// ============================================
// initquit: 服务就绪后立刻完成一次判定
// 避免第一个 TR/TRU 调用者恰好落在启动早期而拿到系统语言回退值。
// ============================================
namespace {

class I18nInitQuit : public initquit {
public:
    void on_init() override {
        i18n::InvalidateLanguageCache();
        (void)i18n::IsChineseLocale();
    }
};

initquit_factory_t<I18nInitQuit> g_i18n_initquit;

} // namespace

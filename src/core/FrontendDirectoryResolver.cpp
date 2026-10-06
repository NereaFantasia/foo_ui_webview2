// FrontendDirectoryResolver.cpp
#include "pch.h"
#include "core/FrontendDirectoryResolver.h"

#include "prefs/PreferencesPage.h"
#include "utils/PathExpansion.h"

namespace frontend_directory {

frontend_directory_policy::Resolution Resolve(const std::string& panelTemplateName) {
    frontend_directory_policy::Candidates candidates;

    std::wstring baseDir;
    try {
        baseDir = webview_prefs::GetWebResourcesBaseDir();
    } catch (const std::exception&) {
        // 建 webview-ui 目录失败（权限、profile 不可达）时 filesystem 会抛异常
        baseDir.clear();
    }
    if (!baseDir.empty()) {
        if (!panelTemplateName.empty()) {
            candidates.panelTemplate = baseDir + L"\\" +
                pfc::stringcvt::string_wide_from_utf8(panelTemplateName.c_str()).get_ptr();
        }
        candidates.activeTemplate = baseDir + L"\\" + webview_prefs::GetActiveTemplateName();
        candidates.defaultTemplate = baseDir + L"\\default";
    }

    // GetComponentDirectory 的结果带结尾反斜杠
    const std::wstring componentDir = PathExpansion::GetComponentDirectory();
    if (!componentDir.empty()) {
        candidates.bundled = componentDir + L"foo_ui_webview2_resources\\dist";
    }

    return frontend_directory_policy::Resolve(candidates);
}

std::string TemplateNameOf(frontend_directory_policy::Source source, const std::string& panelTemplateName) {
    using frontend_directory_policy::Source;
    switch (source) {
    case Source::PanelTemplate:
        return panelTemplateName;
    case Source::ActiveTemplate:
        return pfc::stringcvt::string_utf8_from_wide(webview_prefs::GetActiveTemplateName().c_str()).get_ptr();
    case Source::DefaultTemplate:
        return "default";
    default:
        return {};
    }
}

}  // namespace frontend_directory

// WebviewApi.cpp - webview.* API
//
// Shapes are declared in src/api/schema/webview.ts; the parameter and result
// structs come from the generated WebviewSchema.h.

#include "pch.h"
#include "api/WebviewApi.h"
#include "api/CallerContext.h"
#include "api/ErrorEnvelope.h"
#include "api/TypedApi.h"
#include "api/generated/WebviewSchema.h"
#include "core/WebViewContext.h"
#include "core/WebViewPanel.h"
#include "prefs/PreferencesPage.h"
#include "ui/BackgroundService.h"
#include "ui/UserInterface.h"
#include "window/MainWindow.h"
#include "window/PopupWindow.h"
#include "window/WindowManager.h"

namespace {

namespace wv = api::webview;
using Origin = WebViewPanel::FrontendOrigin;
using frontend_directory_policy::Source;

std::string ToUtf8(const std::wstring& text) {
    return pfc::stringcvt::string_utf8_from_wide(text.c_str()).get_ptr();
}

// 调用方自己的实例。DUI/CUI 面板以自己的 HWND 带面板指针登记在 WebViewContext；
// 主窗口、后台窗口与弹出窗口登记时不带面板指针，按顶级窗口逐个比对。
WebViewPanel* FindCallerInstance(const CallerContext& caller) {
    if (!caller.callerHwnd || !IsWindow(caller.callerHwnd)) return nullptr;
    if (WebViewPanel* panel = WebViewContext::GetInstance().GetPanelByHwnd(caller.callerHwnd)) return panel;

    const HWND topLevel = ::GetAncestor(caller.callerHwnd, GA_ROOT);
    if (auto* ui = WebViewUI::GetInstance()) {
        if (MainWindow* main = ui->GetMainWindow(); main && main->GetHwnd() == topLevel) return main;
    }
    if (MainWindow* background = background_service::GetBackgroundWindow();
        background && background->GetHwnd() == topLevel) {
        return background;
    }
    auto& manager = WindowManager::GetInstance();
    for (const std::string& id : manager.GetAllWindowIds()) {
        if (PopupWindow* popup = manager.GetPopup(id); popup && popup->GetHwnd() == topLevel) return popup;
    }
    return nullptr;
}

const char* DirectorySourceName(Source source) {
    switch (source) {
    case Source::PanelTemplate: return "panelTemplate";
    case Source::ActiveTemplate: return "activeTemplate";
    case Source::Bundled: return "componentDirectory";
    case Source::DefaultTemplate: return "defaultTemplate";
    default: return nullptr;
    }
}

api::Result<wv::GetSourceResult> WebviewGetSource(const wv::GetSourceParams&, const CallerContext& caller) {
    const WebViewPanel* instance = FindCallerInstance(caller);
    if (!instance) return api::Fail("Caller WebView not found", ApiErrorCode::NOT_FOUND);

    const Origin& origin = instance->GetFrontendOrigin();
    wv::GetSourceResult result;
    switch (origin.kind) {
    case Origin::Kind::DevServer:
        result.source = "devServer";
        result.url = ToUtf8(origin.url);
        break;
    case Origin::Kind::Url:
        result.source = "url";
        result.url = ToUtf8(origin.url);
        break;
    case Origin::Kind::Directory: {
        const char* name = DirectorySourceName(origin.directorySource);
        if (!name) return api::Fail("No page has been loaded into this WebView", ApiErrorCode::NOT_FOUND);
        result.source = name;
        result.directory = ToUtf8(origin.directory);
        if (!origin.templateName.empty()) result.templateName = origin.templateName;
        break;
    }
    case Origin::Kind::BuiltInPage:
        result.source = "builtInPage";
        break;
    default:
        return api::Fail("No page has been loaded into this WebView", ApiErrorCode::NOT_FOUND);
    }

    result.activeTemplateName = ToUtf8(webview_prefs::GetActiveTemplateName());
    try {
        result.templatesDirectory = ToUtf8(webview_prefs::GetWebResourcesBaseDir());
    } catch (const std::exception&) {
        // 建 webview-ui 目录失败时 filesystem 会抛异常；来源本身已经确定，照常报告
        result.templatesDirectory.clear();
    }
    return result;
}

}  // namespace

void RegisterWebviewApi() {
    api::RegisterApi("webview.getSource", WebviewGetSource);
}

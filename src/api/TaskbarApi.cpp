// TaskbarApi.cpp - taskbar.* API implementation
#include "pch.h"
#include "api/TaskbarApi.h"
#include "api/BridgeCore.h"
#include "window/TaskbarIntegration.h"
#include "utils/IconLoader.h"
#include "core/UserInterface.h"
#include "window/MainWindow.h"
#include "core/WebViewContext.h"

static bool IsPanelMode() {
    auto* ui = WebViewUI::GetInstance();
    if (ui && ui->GetMainWindow() && ui->GetMainWindow()->GetHwnd()) return false;
    return WebViewContext::GetInstance().GetInstanceCount() > 0;
}

static json PanelModeResponse() {
    return {{"success", false}, {"panelMode", true}};
}

// ============================================================
// Parse frontend-provided icon strings. Empty/null means default icon.
// ============================================================
static std::string ParseIconParam(const json& params, const char* key = "icon") {
    if (!params.contains(key) || params[key].is_null()) return {};
    return params[key].get<std::string>();
}

// ============================================================
// taskbar.setThumbnailButtons
// ============================================================
static json TaskbarSetThumbnailButtons(const json& params) {
    if (IsPanelMode()) return PanelModeResponse();
    if (!params.contains("buttons") || !params["buttons"].is_array())
        return {{"success", false}, {"error", "buttons array required"}};

    // Diagnostic gates: previously SetThumbnailButtons failures were surfaced as
    // a bare {success:false} with no reason, which made the frontend probe unable
    // to tell "not initialised yet" from "too many buttons" from "ThumbBar call
    // failed". Report each distinct cause explicitly.
    if (!TaskbarIntegration::GetInstance().IsInitialized()) {
        return {{"success", false},
                {"error", "taskbar not initialized yet; ITaskbarList3 is created on "
                          "the TaskbarButtonCreated message after the window is shown "
                          "on the taskbar. Retry once the main window is visible."}};
    }
    if (params["buttons"].size() > 7) {
        return {{"success", false},
                {"error", "too many thumbnail buttons; Windows allows at most 7"}};
    }

    std::vector<ThumbnailButton> buttons;
    for (const auto& b : params["buttons"]) {
        ThumbnailButton btn;
        btn.id = b.value("id", "");
        btn.icon = b.contains("icon") && !b["icon"].is_null() ? b.value("icon", "") : "";
        std::string tip = b.value("tooltip", "");
        btn.tooltip = std::wstring(tip.begin(), tip.end()); // ASCII fast path; full UTF-8 below
        if (!tip.empty()) {
            int len = MultiByteToWideChar(CP_UTF8, 0, tip.c_str(), -1, nullptr, 0);
            if (len > 0) { btn.tooltip.resize(len - 1); MultiByteToWideChar(CP_UTF8, 0, tip.c_str(), -1, btn.tooltip.data(), len); }
        }
        btn.enabled = b.value("enabled", true);
        btn.visible = b.value("visible", true);
        btn.dismissOnClick = b.value("dismissOnClick", false);
        buttons.push_back(std::move(btn));
    }

    bool ok = TaskbarIntegration::GetInstance().SetThumbnailButtons(buttons);
    if (!ok) {
        return {{"success", false},
                {"error", "ThumbBar install failed; ITaskbarList3 "
                          "ThumbBarAddButtons/ThumbBarUpdateButtons returned an error "
                          "(image list build or COM call failure)"}};
    }
    return {{"success", true}};
}

// ============================================================
// taskbar.updateButton
// ============================================================
static json TaskbarUpdateButton(const json& params) {
    if (IsPanelMode()) return PanelModeResponse();
    std::string id = params.value("id", "");
    if (id.empty()) return {{"success", false}, {"error", "id required"}};

    std::optional<bool> enabled;
    std::optional<bool> visible;
    if (params.contains("enabled") && params["enabled"].is_boolean()) enabled = params["enabled"].get<bool>();
    if (params.contains("visible") && params["visible"].is_boolean()) visible = params["visible"].get<bool>();
    std::string icon = ParseIconParam(params);
    std::wstring tooltip;
    if (params.contains("tooltip") && params["tooltip"].is_string()) {
        std::string tip = params["tooltip"].get<std::string>();
        int len = MultiByteToWideChar(CP_UTF8, 0, tip.c_str(), -1, nullptr, 0);
        if (len > 0) { tooltip.resize(len - 1); MultiByteToWideChar(CP_UTF8, 0, tip.c_str(), -1, tooltip.data(), len); }
    }

    bool ok = TaskbarIntegration::GetInstance().UpdateButton(id, enabled, visible, icon, tooltip);
    return {{"success", ok}};
}

// ============================================================
// taskbar.setProgress
// ============================================================
static json TaskbarSetProgress(const json& params) {
    if (IsPanelMode()) return PanelModeResponse();

    std::string stateStr = params.value("state", "none");
    TBPFLAG flag = TBPF_NOPROGRESS;
    if (stateStr == "indeterminate") flag = TBPF_INDETERMINATE;
    else if (stateStr == "normal")   flag = TBPF_NORMAL;
    else if (stateStr == "error")    flag = TBPF_ERROR;
    else if (stateStr == "paused")   flag = TBPF_PAUSED;

    auto& tb = TaskbarIntegration::GetInstance();
    bool ok = tb.SetProgressState(flag);
    if (ok && params.contains("value") && params["value"].is_number()) {
        double v = params["value"].get<double>();
        if (v >= 0.0 && v <= 1.0) {
            constexpr ULONGLONG kTotal = 1000;
            ok = tb.SetProgressValue(static_cast<ULONGLONG>(v * kTotal), kTotal);
        }
    }
    return {{"success", ok}};
}

// ============================================================
// taskbar.setOverlayIcon
// ============================================================
static json TaskbarSetOverlayIcon(const json& params) {
    if (IsPanelMode()) return PanelModeResponse();

    HICON hIcon = nullptr;
    std::string icon = ParseIconParam(params);
    if (!icon.empty()) {
        hIcon = IconLoader::FromBase64(icon);
    }

    std::wstring desc;
    if (params.contains("description") && params["description"].is_string()) {
        std::string d = params["description"].get<std::string>();
        int len = MultiByteToWideChar(CP_UTF8, 0, d.c_str(), -1, nullptr, 0);
        if (len > 0) { desc.resize(len - 1); MultiByteToWideChar(CP_UTF8, 0, d.c_str(), -1, desc.data(), len); }
    }

    bool ok = TaskbarIntegration::GetInstance().SetOverlayIcon(hIcon, desc.empty() ? nullptr : desc.c_str());
    return {{"success", ok}};
}

// ============================================================
// taskbar.flash
// ============================================================
static json TaskbarFlash(const json& params) {
    if (IsPanelMode()) return PanelModeResponse();
    UINT count = static_cast<UINT>(params.value("count", 3));
    DWORD interval = static_cast<DWORD>(params.value("interval", 0));
    bool ok = TaskbarIntegration::GetInstance().Flash(count, interval);
    return {{"success", ok}};
}

// ============================================================
// Registration
// ============================================================
void RegisterTaskbarApi() {
    auto& bridge = BridgeCore::GetInstance();

    // Register taskbar:buttonClicked event callback.
    TaskbarIntegration::GetInstance().SetButtonClickCallback([](const std::string& id) {
        // Broadcast: thumbnail buttons are app-global; the singleton EmitEvent would
        // only reach the "main" window (last SetWebView), dropping the event for any
        // popup/secondary window holding the handler. Mirrors playback:* delivery.
        WebViewContext::GetInstance().BroadcastEvent("taskbar:buttonClicked", {{"id", id}});
    });

    bridge.RegisterApi("taskbar.setThumbnailButtons", TaskbarSetThumbnailButtons);
    bridge.RegisterApi("taskbar.updateButton",        TaskbarUpdateButton);
    bridge.RegisterApi("taskbar.setProgress",         TaskbarSetProgress);
    bridge.RegisterApi("taskbar.setOverlayIcon",      TaskbarSetOverlayIcon);
    bridge.RegisterApi("taskbar.flash",               TaskbarFlash);
}

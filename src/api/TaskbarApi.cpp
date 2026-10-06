// TaskbarApi.cpp - taskbar.* API implementation
#include "pch.h"
#include "api/TaskbarApi.h"
#include "api/BridgeCore.h"
#include "api/EventEmit.h"
#include "api/TypedApi.h"
#include "api/generated/TaskbarSchema.h"
#include "window/TaskbarIntegration.h"
#include "utils/IconLoader.h"
#include "ui/UserInterface.h"
#include "window/MainWindow.h"
#include "core/WebViewContext.h"

static bool IsPanelMode() {
    auto* ui = WebViewUI::GetInstance();
    if (ui && ui->GetMainWindow() && ui->GetMainWindow()->GetHwnd()) return false;
    return WebViewContext::GetInstance().GetInstanceCount() > 0;
}

// ============================================================
// taskbar.setThumbnailButtons
// ============================================================
static api::Result<void> TaskbarSetThumbnailButtons(const api::taskbar::SetThumbnailButtonsParams& p) {
    if (IsPanelMode()) return api::PanelModeUnsupported("taskbar.setThumbnailButtons");

    // Each distinct cause gets its own message, so the frontend can tell "not initialised
    // yet" from "too many buttons" from "ThumbBar call failed".
    if (!TaskbarIntegration::GetInstance().IsInitialized()) {
        return api::Fail("taskbar not initialized yet; ITaskbarList3 is created on "
                         "the TaskbarButtonCreated message after the window is shown "
                         "on the taskbar. Retry once the main window is visible.",
                         ApiErrorCode::OPERATION_FAILED);
    }
    if (p.buttons.size() > 7) {
        return api::Fail("too many thumbnail buttons; Windows allows at most 7", ApiErrorCode::INVALID_PARAMS);
    }

    std::vector<ThumbnailButton> buttons;
    for (const api::taskbar::ThumbnailButton& b : p.buttons) {
        ThumbnailButton btn;
        btn.id = b.id;
        btn.icon = b.icon.value_or("");
        btn.tooltip = Utf8ToWide(b.tooltip);
        btn.enabled = b.enabled;
        btn.visible = b.visible;
        btn.dismissOnClick = b.dismissOnClick;
        buttons.push_back(std::move(btn));
    }

    bool ok = TaskbarIntegration::GetInstance().SetThumbnailButtons(buttons);
    if (!ok) {
        return api::Fail("ThumbBar install failed; ITaskbarList3 "
                         "ThumbBarAddButtons/ThumbBarUpdateButtons returned an error "
                         "(image list build or COM call failure)",
                         ApiErrorCode::OPERATION_FAILED);
    }
    return api::Ok();
}

// ============================================================
// taskbar.updateButton
// ============================================================
static api::Result<void> TaskbarUpdateButton(const api::taskbar::UpdateButtonParams& p) {
    if (IsPanelMode()) return api::PanelModeUnsupported("taskbar.updateButton");

    bool ok = TaskbarIntegration::GetInstance().UpdateButton(
        p.id, p.enabled, p.visible, p.icon.value_or(""), Utf8ToWide(p.tooltip));
    if (!ok) {
        return api::Fail("taskbar.updateButton failed: no installed thumbnail button has this id, "
                         "the toolbar is not installed yet, or ITaskbarList3 rejected the update",
                         ApiErrorCode::OPERATION_FAILED);
    }
    return api::Ok();
}

// ============================================================
// taskbar.setProgress
// ============================================================
static api::Result<void> TaskbarSetProgress(const api::taskbar::SetProgressParams& p) {
    if (IsPanelMode()) return api::PanelModeUnsupported("taskbar.setProgress");

    TBPFLAG flag = TBPF_NOPROGRESS;
    if (p.state == "indeterminate") flag = TBPF_INDETERMINATE;
    else if (p.state == "normal")   flag = TBPF_NORMAL;
    else if (p.state == "error")    flag = TBPF_ERROR;
    else if (p.state == "paused")   flag = TBPF_PAUSED;

    auto& tb = TaskbarIntegration::GetInstance();
    // 主题写了进度条就由主题接管，偏好驱动的播放进度到下次播放状态变化前不再覆盖它。
    tb.NoteThemeProgressOverride();
    bool ok = tb.SetProgressState(flag);
    if (ok && p.value) {
        constexpr ULONGLONG kTotal = 1000;  // 解析器已保证 0 <= value <= 1
        ok = tb.SetProgressValue(static_cast<ULONGLONG>(*p.value * kTotal), kTotal);
    }
    if (!ok) {
        return api::Fail("taskbar.setProgress failed: the taskbar button does not exist yet "
                         "or ITaskbarList3 rejected the call",
                         ApiErrorCode::OPERATION_FAILED);
    }
    return api::Ok();
}

// ============================================================
// taskbar.setOverlayIcon
// ============================================================
static api::Result<void> TaskbarSetOverlayIcon(const api::taskbar::SetOverlayIconParams& p) {
    if (IsPanelMode()) return api::PanelModeUnsupported("taskbar.setOverlayIcon");

    HICON hIcon = nullptr;
    if (p.icon && !p.icon->empty()) {
        hIcon = IconLoader::FromBase64(*p.icon);
    }

    std::wstring desc = Utf8ToWide(p.description);
    bool ok = TaskbarIntegration::GetInstance().SetOverlayIcon(hIcon, desc.empty() ? nullptr : desc.c_str());
    if (!ok) {
        return api::Fail("taskbar.setOverlayIcon failed: the taskbar button does not exist yet "
                         "or ITaskbarList3 rejected the call",
                         ApiErrorCode::OPERATION_FAILED);
    }
    return api::Ok();
}

// ============================================================
// taskbar.flash
// ============================================================
static api::Result<void> TaskbarFlash(const api::taskbar::FlashParams& p) {
    if (IsPanelMode()) return api::PanelModeUnsupported("taskbar.flash");
    bool ok = TaskbarIntegration::GetInstance().Flash(static_cast<UINT>(p.count), static_cast<DWORD>(p.interval));
    if (!ok) {
        return api::Fail("taskbar.flash failed: the main window's taskbar button has not been "
                         "created yet, or the main window no longer exists",
                         ApiErrorCode::OPERATION_FAILED);
    }
    return api::Ok();
}

// ============================================================
// Registration
// ============================================================
// Parameters come from src/api/schema/taskbar.ts through the generated types.
void RegisterTaskbarApi() {
    // Register taskbar:buttonClicked event callback.
    TaskbarIntegration::GetInstance().SetButtonClickCallback([](const std::string& id) {
        // Broadcast: thumbnail buttons are app-global; the singleton EmitEvent would
        // only reach the "main" window (last SetWebView), dropping the event for any
        // popup/secondary window holding the handler. Mirrors playback:* delivery.
        api::taskbar::ButtonClickedPayload payload;
        payload.id = id;
        api::emit::Broadcast<api::taskbar::events::ButtonClicked>(payload);
    });

    api::RegisterApi("taskbar.setThumbnailButtons", TaskbarSetThumbnailButtons);
    api::RegisterApi("taskbar.updateButton",        TaskbarUpdateButton);
    api::RegisterApi("taskbar.setProgress",         TaskbarSetProgress);
    api::RegisterApi("taskbar.setOverlayIcon",      TaskbarSetOverlayIcon);
    api::RegisterApi("taskbar.flash",               TaskbarFlash);
}

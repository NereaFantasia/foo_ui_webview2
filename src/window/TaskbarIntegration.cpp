// TaskbarIntegration.cpp - ITaskbarList3 integration
#include "pch.h"
#include "window/TaskbarIntegration.h"
#include "window/TaskbarTrayContracts.h"
#include "window/TrayIcon.h"
#include "utils/IconLoader.h"
#include <foobar2000/SDK/ui.h>
#include <foobar2000/SDK/playback_control.h>

#pragma comment(lib, "comctl32.lib")

TaskbarIntegration& TaskbarIntegration::GetInstance() {
    static TaskbarIntegration instance;
    return instance;
}

// Register TaskbarButtonCreated when the singleton is first accessed so that
// MainWindow::HandleMessage can identify it as soon as the message arrives.
static const UINT s_taskbarCreatedMsg =
    RegisterWindowMessage(L"TaskbarButtonCreated");

UINT TaskbarIntegration::GetTaskbarCreatedMsg() {
    return s_taskbarCreatedMsg;
}

bool TaskbarIntegration::Initialize(HWND hwnd) {
    m_hwnd = hwnd;
    m_taskbarCreatedMsg = s_taskbarCreatedMsg;

    if (m_imageList) {
        ImageList_Destroy(m_imageList);
        m_imageList = nullptr;
    }
    if (m_pTaskbarList) {
        m_pTaskbarList->Release();
        m_pTaskbarList = nullptr;
    }
    m_initialized = false;
    m_buttonsAdded = false;

    HRESULT hr = CoCreateInstance(CLSID_TaskbarList, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_pTaskbarList));
    if (FAILED(hr)) return false;

    hr = m_pTaskbarList->HrInit();
    if (FAILED(hr)) {
        m_pTaskbarList->Release();
        m_pTaskbarList = nullptr;
        return false;
    }

    m_initialized = true;

    if (!m_buttons.empty()) {
        AddButtons();
    } else {
        SetDefaultButtons();
    }

    return true;
}

void TaskbarIntegration::Shutdown() {
    if (m_imageList) {
        ImageList_Destroy(m_imageList);
        m_imageList = nullptr;
    }
    if (m_pTaskbarList) {
        m_pTaskbarList->Release();
        m_pTaskbarList = nullptr;
    }
    m_initialized = false;
    m_buttonsAdded = false;
}

bool TaskbarIntegration::RefreshImageList() {
    if (!m_initialized) return false;
    HIMAGELIST hList = BuildImageList();
    if (!hList) return false;
    HRESULT hr = m_pTaskbarList->ThumbBarSetImageList(m_hwnd, hList);
    if (m_imageList) ImageList_Destroy(m_imageList);
    m_imageList = hList;
    return SUCCEEDED(hr);
}

HICON TaskbarIntegration::ResolveIcon(const std::string& base64, int size) {
    if (!base64.empty()) {
        HICON h = IconLoader::FromBase64(base64);
        if (h) return h;
    }
    // Fallback: foobar2000 main icon.
    try {
        static_api_ptr_t<ui_control> fb_ui;
        return fb_ui->load_main_icon(size, size);
    } catch (...) {
        return nullptr;
    }
}

namespace {
constexpr const char* kPrevIconBase64 = "AAABAAEAEBAAAAEAIABoBAAAFgAAACgAAAAQAAAAIAAAAAEAIAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAAAD+/v5I/v7+cAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA////YP7+/of+/v4T/v7+nv////AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAD+/v4d/v7+rP///////////f39hf7+/pP+/v7fAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAD+/v5c/v7+5/////////////////7+/pT+/v6T/v7+3wAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAP7+/hb+/v6j///////////////////////////+/v6Q/v7+k/7+/t8AAAAAAAAAAAAAAAAAAAAAAAAAAP7+/lL+/v7g/////////////////////////////////v7+kP7+/pP+/v7fAAAAAAAAAAAAAAAA////D/39/Zr///////////////////////////////////////////7+/pD+/v6T/v7+3wAAAAAAAAAA////RP7+/tn////////////////////////////////////////////////+/v6Q/v7+k/7+/t8AAAAA/v7+Lf///////////////////////////////////////////////////////////v7+kP7+/pP+/v7fAAAAAPn5+S3///////////////////////////////////////////////////////////7+/pD+/v6T/v7+3wAAAAAAAAAA/v7+Rf7+/tj////////////////////////////////////////////////+/v6Q/v7+k/7+/t8AAAAAAAAAAAAAAAD+/v4O/v7+l////////////////////////////////////////////v7+kP7+/pP+/v7fAAAAAAAAAAAAAAAAAAAAAAAAAAD+/v5N/v7+2/////////////////////////////////7+/pD+/v6T/v7+3wAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAP///xH9/f2Z///////////////////////////+/v6Q/v7+k/7+/t8AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAP///1D+/v7f/////////////////v7+lf7+/p3+/v7vAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA////FP7+/p3///////////7+/nz+/v49////YAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA////UP7+/nP+/v4LAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA==";
constexpr const char* kPlayIconBase64 = "AAABAAEAEBAAAAEAIABoBAAAFgAAACgAAAAQAAAAIAAAAAEAIAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAD+/v5d/v7+i/7+/i4AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAD///9A///////////+/v75/v7+kv7+/hwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA/v7+ZP/////////////////////+/v7s/v7+fP///w8AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAP///2D////////////////////////////////+/v7d/v7+ZP///wIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAD///9g///////////////////////////////////////////+/v7K/v7+TQAAAAAAAAAAAAAAAAAAAAAAAAAA////YP/////////////////////////////////////////////////////9/f23/v7+OgAAAAAAAAAAAAAAAP///2D////////////////////////////////////////////////////////////////+/v6aAAAAAAAAAAD///9g/////////////////////////////////////////////////////////////////v7+/QAAAAAAAAAA////YP////////////////////////////////////////////////////////////////7+/v4AAAAAAAAAAP///2D///////////////////////////////////////////////////////////7+/vX+/v6HAAAAAAAAAAD///9g//////////////////////////////////////////////////////7+/qP+/v4pAAAAAAAAAAAAAAAA////YP///////////////////////////////////////////v7+uf///zwAAAAAAAAAAAAAAAAAAAAAAAAAAP///2D////////////////////////////////+/v7N////UAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAD+/v5k//////////////////////7+/t/+/v5o////BAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA/v7+Ov///////////v7+7vz8/H////8QAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAD+/v5M/v7+df///x4AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA==";
constexpr const char* kPauseIconBase64 = "AAABAAEAEBAAAAEAIABoBAAAFgAAACgAAAAQAAAAIAAAAAEAIAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAC/v7/A7+/v8L+/v8AAAAAAAAAAAL+/v8Dv7+/wv7+/wAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA/v7+//7+/v/+/v7/AAAAAAAAAAD+/v7//v7+//7+/v8AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAP7+/v/+/v7//v7+/wAAAAAAAAAA/v7+//7+/v/+/v7/AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAD+/v7//v7+//7+/v8AAAAAAAAAAP7+/v/+/v7//v7+/wAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA/v7+//7+/v/+/v7/AAAAAAAAAAD+/v7//v7+//7+/v8AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAP7+/v/+/v7//v7+/wAAAAAAAAAA/v7+//7+/v/+/v7/AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAD+/v7//v7+//7+/v8AAAAAAAAAAP7+/v/+/v7//v7+/wAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA/v7+//7+/v/+/v7/AAAAAAAAAAD+/v7//v7+//7+/v8AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAP7+/v/+/v7//v7+/wAAAAAAAAAA/v7+//7+/v/+/v7/AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAD+/v7//v7+//7+/v8AAAAAAAAAAP7+/v/+/v7//v7+/wAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA/v7+//7+/v/+/v7/AAAAAAAAAAD+/v7//v7+//7+/v8AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAL+/v8Dv7+/wv7+/wAAAAAAAAAAAv7+/wO/v7/C/v7/AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA//8AAP//AADxjwAA8Y8AAPGPAADxjwAA8Y8AAPGPAADxjwAA8Y8AAPGPAADxjwAA8Y8AAPGPAAD//wAA//8AAA==";
constexpr const char* kNextIconBase64 = "AAABAAEAEBAAAAEAIABoBAAAFgAAACgAAAAQAAAAIAAAAAEAIAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAAAD+/v4O/v7+g/7+/mUAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAP7+/mv///9Q/v7+d////////////v7+tP7+/iQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAD9/f3m/v7+q/7+/or////////////////+/v7u/Pz8YwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA/v7+1v7+/p/+/v6G///////////////////////////9/f2r/v7+HAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAP7+/tb+/v6f/v7+hv////////////////////////////////7+/uf+/v5ZAAAAAAAAAAAAAAAAAAAAAAAAAAD+/v7W/v7+n/7+/ob//////////////////////////////////////////////6D+/v4VAAAAAAAAAAAAAAAA/v7+1v7+/p/+/v6G/////////////////////////////////////////////////v7+3/7+/k0AAAAAAAAAAP7+/tb+/v6f/v7+hv///////////////////////////////////////////////////////////v7+OQAAAAD+/v7W/v7+n/7+/ob///////////////////////////////////////////////////////////7+/jgAAAAA/v7+1v7+/p/+/v6G/////////////////////////////////////////////////v7+4P7+/k8AAAAAAAAAAP7+/tb+/v6f/v7+hv///////////////////////////////////////////v7+nv7+/hUAAAAAAAAAAAAAAAD+/v7W/v7+n/7+/ob////////////////////////////////+/v7h/v7+UwAAAAAAAAAAAAAAAAAAAAAAAAAA/v7+1v7+/p/+/v6G///////////////////////////+/v6i////GAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAP7+/tb+/v6f/v7+i/////////////////7+/uP8/PxYAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAD+/v7W/v7+n/7+/nD///////////7+/qX///8YAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA/v7+5f///6r///8I/v7+cfz8/FYAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAP7+/lv///9EAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA==";
constexpr const char* kStopIconBase64 = "AAABAAEAEBAAAAEAIABoBAAAFgAAACgAAAAQAAAAIAAAAAEAIAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAAAD///8F/Pz8b/7+/pH+/v6Q/v7+kP7+/pD+/v6Q/v7+kP7+/pD+/v6Q/v7+kP7+/pD+/v6Q/v7+kf7+/nL///8I/Pz8Zf///////////////////////////////////////////////////////////////////////////v7+cP7+/ov///////////////////////////////////////////////////////////////////////////7+/pX+/v6G///////////////////////////////////////////////////////////////////////////+/v6Q/v7+hv///////////////////////////////////////////////////////////////////////////v7+kP7+/ob///////////////////////////////////////////////////////////////////////////7+/pD+/v6G///////////////////////////////////////////////////////////////////////////+/v6Q/v7+hv///////////////////////////////////////////////////////////////////////////v7+kP7+/ob///////////////////////////////////////////////////////////////////////////7+/pD+/v6G///////////////////////////////////////////////////////////////////////////+/v6Q/v7+hv///////////////////////////////////////////////////////////////////////////v7+kP7+/ob///////////////////////////////////////////////////////////////////////////7+/pD+/v6G///////////////////////////////////////////////////////////////////////////+/v6Q/v7+i////////////////////////////////////////////////////////////////////////////v7+lv7+/lv///////////////////////////////////////////////////////////////////////////7+/mf///8B/v7+Wv7+/n/+/v5//v7+f/7+/n/+/v5//v7+f/7+/n/+/v5//v7+f/7+/n/+/v5//v7+f/7+/l3///8DAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA==";

const char* SelectDefaultIconBase64(const std::string& id, bool usePauseIcon) {
    if (id == "_prev") return kPrevIconBase64;
    if (id == "_play") return usePauseIcon ? kPauseIconBase64 : kPlayIconBase64;
    if (id == "_next") return kNextIconBase64;
    return kStopIconBase64;
}

// Built-in default thumbnail icons are generated from Microsoft Fluent UI System Icons (MIT).
HICON CreateDefaultButtonIcon(const std::string& id, bool usePauseIcon) {
    return IconLoader::FromBase64(SelectDefaultIconBase64(id, usePauseIcon));
}
}

HIMAGELIST TaskbarIntegration::BuildImageList() {
    constexpr int SIZE = 16;
    HIMAGELIST hList = ImageList_Create(SIZE, SIZE, ILC_COLOR32,
        static_cast<int>(m_buttons.size()), 0);
    if (!hList) return nullptr;

    // HICON ownership matters: IconLoader-cached handles must NOT be destroyed
    // here, otherwise explorer.exe will marshal a dangling handle from the
    // image list and crash when the user hovers the taskbar thumbnail.
    for (const auto& btn : m_buttons) {
        HICON hIcon = nullptr;
        bool ownsIcon = false;
        if (!btn.icon.empty()) {
            hIcon = IconLoader::FromBase64(btn.icon);
        } else if (m_usingDefaultButtons) {
            hIcon = CreateDefaultButtonIcon(btn.id,
                btn.id == "_play" && m_defaultPlayIconPaused);
        } else {
            try {
                static_api_ptr_t<ui_control> fb_ui;
                hIcon = fb_ui->load_main_icon(SIZE, SIZE);
                ownsIcon = (hIcon != nullptr);
            } catch (...) {}
        }

        if (hIcon) {
            ImageList_AddIcon(hList, hIcon);
            if (ownsIcon) DestroyIcon(hIcon);
        } else {
            HICON blank = static_cast<HICON>(
                LoadImage(nullptr, IDI_APPLICATION, IMAGE_ICON, SIZE, SIZE, LR_SHARED));
            ImageList_AddIcon(hList, blank);
        }
    }
    return hList;
}

bool TaskbarIntegration::AddButtons() {
    if (!m_initialized || m_buttons.empty() || m_buttons.size() > 7) return false;

    if (!RefreshImageList()) return false;

    std::vector<THUMBBUTTON> tb(m_buttons.size());
    for (size_t i = 0; i < m_buttons.size(); ++i) {
        tb[i] = {};
        tb[i].dwMask = THB_BITMAP | THB_TOOLTIP | THB_FLAGS;
        tb[i].iId = static_cast<UINT>(i);
        tb[i].iBitmap = static_cast<UINT>(i);
        tb[i].dwFlags = THBF_ENABLED;
        wcsncpy_s(tb[i].szTip, m_buttons[i].tooltip.c_str(), _TRUNCATE);
        if (!m_buttons[i].enabled) tb[i].dwFlags |= THBF_DISABLED;
        if (!m_buttons[i].visible) tb[i].dwFlags |= THBF_HIDDEN;
        if (m_buttons[i].dismissOnClick) tb[i].dwFlags |= THBF_DISMISSONCLICK;
    }

    HRESULT hr = m_pTaskbarList->ThumbBarAddButtons(
        m_hwnd, static_cast<UINT>(m_buttons.size()), tb.data());
    m_buttonsAdded = SUCCEEDED(hr);
    return m_buttonsAdded;
}

bool TaskbarIntegration::SetThumbnailButtons(const std::vector<ThumbnailButton>& buttons) {
    if (!m_initialized) return false;
    if (!taskbar_tray_contracts::CanInstallUserThumbnailButtons(
        m_usingDefaultButtons, m_buttonsAdded, buttons.size())) return false;

    m_usingDefaultButtons = false;
    m_buttons = buttons;
    const size_t slotCount = taskbar_tray_contracts::ThumbnailToolbarSlotCount();
    while (m_buttons.size() < slotCount) {
        ThumbnailButton placeholder;
        placeholder.id = "_hidden" + std::to_string(m_buttons.size());
        placeholder.visible = false;
        m_buttons.push_back(std::move(placeholder));
    }

    if (m_buttonsAdded) {
        if (!RefreshImageList()) return false;

        std::vector<THUMBBUTTON> tb(m_buttons.size());
        for (size_t i = 0; i < m_buttons.size(); ++i) {
            tb[i] = {};
            tb[i].dwMask = THB_BITMAP | THB_TOOLTIP | THB_FLAGS;
            tb[i].iId = static_cast<UINT>(i);
            tb[i].iBitmap = static_cast<UINT>(i);
            tb[i].dwFlags = THBF_ENABLED;
            wcsncpy_s(tb[i].szTip, m_buttons[i].tooltip.c_str(), _TRUNCATE);
            if (!m_buttons[i].enabled) tb[i].dwFlags |= THBF_DISABLED;
            if (!m_buttons[i].visible) tb[i].dwFlags |= THBF_HIDDEN;
            if (m_buttons[i].dismissOnClick) tb[i].dwFlags |= THBF_DISMISSONCLICK;
        }
        return SUCCEEDED(m_pTaskbarList->ThumbBarUpdateButtons(
            m_hwnd, static_cast<UINT>(m_buttons.size()), tb.data()));
    }

    if (!m_buttonsAdded) return AddButtons();

    if (!RefreshImageList()) return false;

    std::vector<THUMBBUTTON> tb(m_buttons.size());
    for (size_t i = 0; i < m_buttons.size(); ++i) {
        tb[i] = {};
        tb[i].dwMask = THB_BITMAP | THB_TOOLTIP | THB_FLAGS;
        tb[i].iId = static_cast<UINT>(i);
        tb[i].iBitmap = static_cast<UINT>(i);
        tb[i].dwFlags = THBF_ENABLED;
        wcsncpy_s(tb[i].szTip, m_buttons[i].tooltip.c_str(), _TRUNCATE);
        if (!m_buttons[i].enabled) tb[i].dwFlags |= THBF_DISABLED;
        if (!m_buttons[i].visible) tb[i].dwFlags |= THBF_HIDDEN;
        if (m_buttons[i].dismissOnClick) tb[i].dwFlags |= THBF_DISMISSONCLICK;
    }
    return SUCCEEDED(m_pTaskbarList->ThumbBarUpdateButtons(
        m_hwnd, static_cast<UINT>(m_buttons.size()), tb.data()));
}

bool TaskbarIntegration::UpdateButton(const std::string& id,
                                      std::optional<bool> enabled,
                                      std::optional<bool> visible,
                                      const std::string& icon, const std::wstring& tooltip) {
    if (!m_initialized || !m_buttonsAdded) return false;

    for (size_t i = 0; i < m_buttons.size(); ++i) {
        if (m_buttons[i].id != id) continue;
        if (enabled.has_value()) m_buttons[i].enabled = *enabled;
        if (visible.has_value()) m_buttons[i].visible = *visible;
        if (!icon.empty()) m_buttons[i].icon = icon;
        if (!tooltip.empty()) m_buttons[i].tooltip = tooltip;

        if (!icon.empty() && !RefreshImageList()) return false;

        THUMBBUTTON tb = {};
        tb.dwMask = THB_BITMAP | THB_TOOLTIP | THB_FLAGS;
        tb.iId = static_cast<UINT>(i);
        tb.iBitmap = static_cast<UINT>(i);
        tb.dwFlags = THBF_ENABLED;
        wcsncpy_s(tb.szTip, m_buttons[i].tooltip.c_str(), _TRUNCATE);
        if (!m_buttons[i].enabled) tb.dwFlags |= THBF_DISABLED;
        if (!m_buttons[i].visible) tb.dwFlags |= THBF_HIDDEN;

        return SUCCEEDED(m_pTaskbarList->ThumbBarUpdateButtons(m_hwnd, 1, &tb));
    }
    return false;
}

bool TaskbarIntegration::SetProgressState(TBPFLAG state) {
    if (!m_initialized) return false;
    return SUCCEEDED(m_pTaskbarList->SetProgressState(m_hwnd, state));
}

bool TaskbarIntegration::SetProgressValue(ULONGLONG completed, ULONGLONG total) {
    if (!m_initialized) return false;
    return SUCCEEDED(m_pTaskbarList->SetProgressValue(m_hwnd, completed, total));
}

bool TaskbarIntegration::SetOverlayIcon(HICON hIcon, const wchar_t* description) {
    if (!m_initialized) return false;
    return SUCCEEDED(m_pTaskbarList->SetOverlayIcon(m_hwnd, hIcon, description));
}

bool TaskbarIntegration::Flash(UINT count, DWORD interval) {
    if (!m_hwnd) return false;
    FLASHWINFO fi = {};
    fi.cbSize = sizeof(FLASHWINFO);
    fi.hwnd = m_hwnd;
    fi.dwFlags = FLASHW_ALL;
    fi.uCount = count;
    fi.dwTimeout = interval;
    return FlashWindowEx(&fi) != FALSE;
}

void TaskbarIntegration::HandleButtonClicked(int index) {
    if (index < 0 || index >= static_cast<int>(m_buttons.size())) return;
    if (m_usingDefaultButtons) {
        // Route default buttons directly to playback_control.
        const std::string& id = m_buttons[index].id;
        try {
            auto pc = playback_control::get();
            if      (id == "_prev")  pc->previous();
            else if (id == "_play")  pc->play_or_pause();
            else if (id == "_next")  pc->next();
            else if (id == "_stop")  pc->stop();
        } catch (...) {}
        return;
    }
    if (m_callback) m_callback(m_buttons[index].id);
}

void TaskbarIntegration::SetDefaultButtons() {
    m_usingDefaultButtons = true;
    m_defaultPlayIconPaused = false;
    m_buttons = {
        { "_prev", "",  L"\u4e0a\u4e00\u9996", true, true, false },
        { "_play", "",  L"\u64ad\u653e",       true, true, false },
        { "_next", "",  L"\u4e0b\u4e00\u9996", true, true, false },
        { "_stop", "",  L"\u505c\u6b62",       true, true, false },
    };
    const size_t slotCount = taskbar_tray_contracts::ThumbnailToolbarSlotCount();
    while (m_buttons.size() < slotCount) {
        ThumbnailButton placeholder;
        placeholder.id = "_hidden" + std::to_string(m_buttons.size());
        placeholder.visible = false;
        m_buttons.push_back(std::move(placeholder));
    }
    AddButtons();
}

void TaskbarIntegration::OnPlaybackStateChanged(const char* state) {
    if (!m_usingDefaultButtons || !m_buttonsAdded) return;
    bool isPlaying = (strcmp(state, "playing") == 0);
    m_defaultPlayIconPaused = isPlaying;
    RefreshImageList();
    UpdateButton("_play",
        std::nullopt, std::nullopt, "",
        isPlaying ? L"\u6682\u505c" : L"\u64ad\u653e");
}

// ============================================================
// initquit: explicit cleanup of tray icon + ITaskbarList3 so foobar2000
// component reload (without process restart) does not leak Shell_NotifyIcon
// or COM references.
// ============================================================
namespace {
class TaskbarTrayInitQuit : public initquit {
public:
    void on_quit() override {
        try {
            TrayIcon::GetInstance().Destroy();
        } catch (...) {}
        try {
            TaskbarIntegration::GetInstance().Shutdown();
        } catch (...) {}
        try {
            IconLoader::Clear();
        } catch (...) {}
    }
};
static initquit_factory_t<TaskbarTrayInitQuit> g_taskbar_tray_initquit;
}

#pragma once
#include "pch.h"
#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <ShObjIdl.h>

struct ThumbnailButton {
    std::string id;
    std::string icon;   // base64 or empty (default buttons use built-in icons)
    std::wstring tooltip;
    bool enabled = true;
    bool visible = true;
    bool dismissOnClick = false;
};

/**
 * TaskbarIntegration owns the singleton ITaskbarList3 integration.
 */
class TaskbarIntegration {
public:
    static TaskbarIntegration& GetInstance();

    // Called by MainWindow after TaskbarButtonCreated.
    bool Initialize(HWND hwnd);
    void Shutdown();
    bool IsInitialized() const { return m_initialized; }

    // Thumbnail toolbar buttons. ThumbBarAddButtons may only be called once.
    bool SetThumbnailButtons(const std::vector<ThumbnailButton>& buttons);
    bool UpdateButton(const std::string& id,
                      std::optional<bool> enabled,
                      std::optional<bool> visible,
                      const std::string& icon, const std::wstring& tooltip);

    // Progress indicator
    bool SetProgressState(TBPFLAG state);
    bool SetProgressValue(ULONGLONG completed, ULONGLONG total);

    // Overlay icon
    bool SetOverlayIcon(HICON hIcon, const wchar_t* description);

    // Taskbar flash
    bool Flash(UINT count, DWORD interval);

    // Button click handler called from MainWindow WM_COMMAND/THBN_CLICKED.
    void HandleButtonClicked(int index);

    // Callback
    using ButtonClickCallback = std::function<void(const std::string& id)>;
    void SetButtonClickCallback(ButtonClickCallback cb) { m_callback = std::move(cb); }

    // TaskbarButtonCreated message id — registered at DLL load time.
    static UINT GetTaskbarCreatedMsg();

    // Called by PlaybackCallback to update default button tooltips.
    void OnPlaybackStateChanged(const char* state);

private:
    void SetDefaultButtons();
    TaskbarIntegration() = default;
    ~TaskbarIntegration() { Shutdown(); }
    TaskbarIntegration(const TaskbarIntegration&) = delete;
    TaskbarIntegration& operator=(const TaskbarIntegration&) = delete;

    bool AddButtons();
    bool RefreshImageList();
    HIMAGELIST BuildImageList();
    HICON ResolveIcon(const std::string& base64, int size = 16);

    HWND m_hwnd = nullptr;
    ITaskbarList3* m_pTaskbarList = nullptr;
    HIMAGELIST m_imageList = nullptr;
    std::vector<ThumbnailButton> m_buttons;
    bool m_initialized = false;
    bool m_buttonsAdded = false;
    bool m_usingDefaultButtons = false;
    bool m_defaultPlayIconPaused = false;
    UINT m_taskbarCreatedMsg = 0;
    ButtonClickCallback m_callback;
};

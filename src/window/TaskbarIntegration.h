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

    // 播放进度映射（Window 子页「任务栏播放进度」开关）。
    // OnPlaybackProgress：PlaybackCallback 在状态变化与每秒时间回调时调用；开关关着或主题接管期间不动进度条。
    // NoteThemeProgressOverride：taskbar.setProgress 被调用时记一笔，主题接管到下一次播放状态变化。
    // RefreshProgressFromPlayback：开关刚被 Apply 时按当前播放状态立即刷新一次（关掉则清空进度条）。
    void OnPlaybackProgress(const char* state, double positionSec, double lengthSec);
    void NoteThemeProgressOverride();
    void RefreshProgressFromPlayback();

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
    bool m_themeOwnsProgress = false;   // 主题调过 taskbar.setProgress，进度条归主题直到下次状态变化
    UINT m_taskbarCreatedMsg = 0;
    ButtonClickCallback m_callback;
};

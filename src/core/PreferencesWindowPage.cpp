// ============================================
// PreferencesWindowPage.cpp - WebView2 UI › Window 子页
// ============================================

#include "pch.h"
#include "core/PreferencesWindowPage.h"
#include "core/PreferencesFields.h"
#include "core/PreferencesPage.h"
#include "core/SecurityConfig.h"
#include "window/TaskbarIntegration.h"
#include "window/TrayIcon.h"
#include "utils/I18n.h"
#include <algorithm>
#include <string>
#include <vector>

namespace webview_prefs {

namespace window = prefs_fields::window;

namespace {

// {6B183CC5-7F46-49AD-A382-9D2087C5C64E}
constexpr GUID guid_prefs_window_page =
    { 0x6b183cc5, 0x7f46, 0x49ad, { 0xa3, 0x82, 0x9d, 0x20, 0x87, 0xc5, 0xc6, 0x4e } };

enum ControlIds {
    IDC_GROUP_WINDOW = 1200,
    IDC_CHK_REMEMBER_POSITION = 1201,
    IDC_CHK_BACKGROUND_MODE = 1202,
    IDC_STATIC_BACKGROUND_NOTE = 1203,
    IDC_CHK_RESTORE_VISIBILITY = 1204,
    IDC_STATIC_RESTORE_NOTE = 1205,

    IDC_GROUP_TRAY = 1210,
    IDC_CHK_MINIMIZE_TO_TRAY = 1211,
    IDC_CHK_CLOSE_TO_TRAY = 1212,
    IDC_STATIC_TRAY_NOTE = 1213,

    IDC_GROUP_TASKBAR = 1220,
    IDC_CHK_TASKBAR_BUTTONS = 1221,
    IDC_CHK_TASKBAR_PROGRESS = 1222,
    IDC_STATIC_TASKBAR_NOTE = 1223,
};

// 复选框控件 ID 与字段下标的对应；OnMessage 与 SyncControlsFromDraft 都按它走。
struct CheckBinding {
    int id;
    size_t field;
};
constexpr CheckBinding kChecks[] = {
    {IDC_CHK_REMEMBER_POSITION, window::RememberPosition},
    {IDC_CHK_BACKGROUND_MODE, window::BackgroundMode},
    {IDC_CHK_RESTORE_VISIBILITY, window::RestoreVisibility},
    {IDC_CHK_MINIMIZE_TO_TRAY, window::MinimizeToTray},
    {IDC_CHK_CLOSE_TO_TRAY, window::CloseToTray},
    {IDC_CHK_TASKBAR_BUTTONS, window::TaskbarButtons},
    {IDC_CHK_TASKBAR_PROGRESS, window::TaskbarProgress},
};

}  // namespace

// ============================================
// WebViewPrefsWindowPage
// ============================================

const char* WebViewPrefsWindowPage::get_name() {
    // 宿主缓存树节点名，切换组件语言后在重启后更新。
    return TRU("Window", "窗口");
}

GUID WebViewPrefsWindowPage::get_guid() {
    return guid_prefs_window_page;
}

GUID WebViewPrefsWindowPage::get_parent_guid() {
    return GetPreferencesPageGuid();
}

double WebViewPrefsWindowPage::get_sort_priority() {
    // Window 1.0 / Performance 2.0 / Developer 3.0
    return 1.0;
}

bool WebViewPrefsWindowPage::get_help_url(pfc::string_base& p_out) {
    p_out = "https://github.com/user/foo_ui_webview2";
    return true;
}

preferences_page_instance::ptr WebViewPrefsWindowPage::instantiate(
    fb2k::hwnd_t parent, preferences_page_callback::ptr callback) {
    return fb2k::service_new<WebViewPrefsWindowInstance>(parent, callback);
}

// ============================================
// WebViewPrefsWindowInstance
// ============================================

WebViewPrefsWindowInstance::WebViewPrefsWindowInstance(HWND parent, preferences_page_callback::ptr callback)
    : PreferencesPageBase(std::move(callback), window::Table()) {
    static const bool s_startupBackgroundMode = security_config::IsBackgroundModeEnabled();
    static const bool s_startupTaskbarButtons = GetTaskbarButtonsEnabled();
    startupBackgroundMode_ = s_startupBackgroundMode;
    startupTaskbarButtons_ = s_startupTaskbarButtons;
    CreatePageWindow(parent);
}

prefs_draft::Snapshot WebViewPrefsWindowInstance::ReadSnapshotFromConfig() const {
    prefs_draft::Snapshot s = prefs_draft::Defaults(fields());
    s.SetBool(window::RememberPosition, GetRememberWindowPosition());
    s.SetBool(window::BackgroundMode, security_config::IsBackgroundModeEnabled());
    s.SetBool(window::RestoreVisibility, GetStartWithFoobar());
    s.SetBool(window::MinimizeToTray, GetMinimizeToTrayPreference());
    s.SetBool(window::CloseToTray, GetCloseToTrayPreference());
    s.SetBool(window::TaskbarButtons, GetTaskbarButtonsEnabled());
    s.SetBool(window::TaskbarProgress, GetTaskbarProgressEnabled());
    return s;
}

prefs_draft::Snapshot WebViewPrefsWindowInstance::StartupSnapshot() const {
    prefs_draft::Snapshot s = prefs_draft::Defaults(fields());
    s.SetBool(window::BackgroundMode, startupBackgroundMode_);
    s.SetBool(window::TaskbarButtons, startupTaskbarButtons_);
    return s;
}

bool WebViewPrefsWindowInstance::WriteField(size_t field, const prefs_draft::Snapshot& value) {
    try {
        const bool v = value.GetBool(field);
        switch (field) {
        case window::RememberPosition:
            SetRememberWindowPosition(v);
            return GetRememberWindowPosition() == v;
        case window::BackgroundMode:
            security_config::SetBackgroundModeEnabled(v);
            return security_config::IsBackgroundModeEnabled() == v;
        case window::RestoreVisibility:
            SetStartWithFoobar(v);
            return GetStartWithFoobar() == v;
        case window::MinimizeToTray:
            // 写偏好并同步到已存在的托盘图标；图标还没创建时由 TrayIcon::Create 读偏好作初值。
            SetMinimizeToTrayPreference(v);
            TrayIcon::GetInstance().SetMinimizeToTray(v);
            return GetMinimizeToTrayPreference() == v;
        case window::CloseToTray:
            SetCloseToTrayPreference(v);
            TrayIcon::GetInstance().SetCloseToTray(v);
            return GetCloseToTrayPreference() == v;
        case window::TaskbarButtons:
            SetTaskbarButtonsEnabled(v);
            return GetTaskbarButtonsEnabled() == v;
        case window::TaskbarProgress:
            SetTaskbarProgressEnabled(v);
            return GetTaskbarProgressEnabled() == v;
        default:
            break;
        }
    } catch (...) {
    }
    return false;
}

std::wstring WebViewPrefsWindowInstance::FieldDisplayName(size_t field) const {
    switch (field) {
    case window::RememberPosition: return TR("Remember window position and size", "记住窗口位置和大小");
    case window::BackgroundMode: return TR("Background mode", "后台模式");
    case window::RestoreVisibility:
        return TR("Restore background window visibility on startup", "启动时恢复后台窗口可见状态");
    case window::MinimizeToTray: return TR("Minimize to tray", "最小化到托盘");
    case window::CloseToTray: return TR("Close to tray", "关闭到托盘");
    case window::TaskbarButtons: return TR("Thumbnail toolbar playback buttons", "任务栏缩略图播放按钮");
    case window::TaskbarProgress: return TR("Playback progress on the taskbar button", "任务栏播放进度");
    default: break;
    }
    return {};
}

void WebViewPrefsWindowInstance::AfterApply(const std::vector<size_t>& changed) {
    // 进度开关刚写入就按当前播放状态刷一次（关掉则清空），不等下一次状态变化。
    if (std::find(changed.begin(), changed.end(), static_cast<size_t>(window::TaskbarProgress)) != changed.end()) {
        TaskbarIntegration::GetInstance().RefreshProgressFromPlayback();
    }
}

// ============================================
// 控件
// ============================================

void WebViewPrefsWindowInstance::CreateControls(HWND hwnd) {
    HINSTANCE hInst = core_api::get_my_instance();
    const HFONT pageFont = font();

    auto create = [&](const wchar_t* cls, DWORD exStyle, const wchar_t* text, DWORD style, int id) {
        HWND h = CreateWindowExW(exStyle, cls, text, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | style,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), hInst, nullptr);
        SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(pageFont), FALSE);
        return h;
    };
    // BS_NOTIFY 让复选框在获得焦点时发 BN_SETFOCUS，基类据此把它滚入可见范围。
    auto createCheck = [&](int id, const wchar_t* text) {
        return create(L"BUTTON", 0, text, WS_TABSTOP | BS_NOTIFY | BS_AUTOCHECKBOX | BS_MULTILINE, id);
    };
    auto createNote = [&](int id, const wchar_t* text) {
        return create(L"STATIC", 0, text, SS_LEFT | SS_NOPREFIX, id);
    };

    // 分组框先创建，排在 Z 序底部，避免盖住组内控件。
    create(L"BUTTON", WS_EX_TRANSPARENT, TR("Window", "窗口"), BS_GROUPBOX, IDC_GROUP_WINDOW);
    create(L"BUTTON", WS_EX_TRANSPARENT, TR("Tray", "托盘"), BS_GROUPBOX, IDC_GROUP_TRAY);
    create(L"BUTTON", WS_EX_TRANSPARENT, TR("Taskbar", "任务栏"), BS_GROUPBOX, IDC_GROUP_TASKBAR);

    // ---- Window ----
    createCheck(IDC_CHK_REMEMBER_POSITION, TR("Remember window position and size", "记住窗口位置和大小"));
    createCheck(IDC_CHK_BACKGROUND_MODE,
        TR("Background mode: keep the WebView running while another user interface is active",
           "后台模式：使用其他界面时仍运行 WebView"));
    createNote(IDC_STATIC_BACKGROUND_NOTE,
        TR("The background service starts with foobar2000; changing this takes effect after a restart.",
           "后台服务随 foobar2000 启动；改动后重启生效。"));
    createCheck(IDC_CHK_RESTORE_VISIBILITY,
        TR("Restore background window visibility on startup", "启动时恢复后台窗口可见状态"));
    createNote(IDC_STATIC_RESTORE_NOTE, L"");

    // ---- Tray ----
    createCheck(IDC_CHK_MINIMIZE_TO_TRAY, TR("Minimize to tray", "最小化到托盘"));
    createCheck(IDC_CHK_CLOSE_TO_TRAY, TR("Close to tray", "关闭到托盘"));
    createNote(IDC_STATIC_TRAY_NOTE, L"");

    // ---- Taskbar ----
    createCheck(IDC_CHK_TASKBAR_BUTTONS,
        TR("Show playback buttons on the taskbar thumbnail (Previous / Play / Next / Stop)",
           "任务栏缩略图显示播放按钮（上一首 / 播放 / 下一首 / 停止）"));
    createCheck(IDC_CHK_TASKBAR_PROGRESS,
        TR("Show playback progress on the taskbar button", "在任务栏按钮上显示播放进度"));
    createNote(IDC_STATIC_TASKBAR_NOTE,
        TR("Buttons: takes effect after a restart; buttons set by the theme with taskbar.setButtons are not "
           "affected. Progress: playing shows the position, paused shows it in the paused colour, stopped or "
           "unknown length hides it; after the theme calls taskbar.setProgress the theme owns the bar until the "
           "next playback state change.",
           "按钮：重启后生效；主题用 taskbar.setButtons 设置的按钮不受影响。进度：播放中显示进度，暂停显示为暂停色，"
           "停止或时长未知时隐藏；主题调用 taskbar.setProgress 后由主题接管，直到下一次播放状态变化。"));

    SyncControlsFromDraft();
}

void WebViewPrefsWindowInstance::LayoutContent(prefs_layout::PageLayoutBuilder& builder) {
    builder.BeginGroup(IDC_GROUP_WINDOW, {});
    builder.CheckBox(IDC_CHK_REMEMBER_POSITION);
    builder.CheckBox(IDC_CHK_BACKGROUND_MODE);
    builder.Note(IDC_STATIC_BACKGROUND_NOTE);
    builder.CheckBox(IDC_CHK_RESTORE_VISIBILITY);
    builder.Note(IDC_STATIC_RESTORE_NOTE);
    builder.EndGroup();

    builder.BeginGroup(IDC_GROUP_TRAY, {});
    builder.CheckBox(IDC_CHK_MINIMIZE_TO_TRAY);
    builder.CheckBox(IDC_CHK_CLOSE_TO_TRAY);
    builder.Note(IDC_STATIC_TRAY_NOTE);
    builder.EndGroup();

    builder.BeginGroup(IDC_GROUP_TASKBAR, {});
    builder.CheckBox(IDC_CHK_TASKBAR_BUTTONS);
    builder.CheckBox(IDC_CHK_TASKBAR_PROGRESS);
    builder.Note(IDC_STATIC_TASKBAR_NOTE);
    builder.EndGroup();
}

void WebViewPrefsWindowInstance::SyncControlsFromDraft() {
    if (!hwnd()) return;
    for (const CheckBinding& b : kChecks) {
        CheckDlgButton(hwnd(), b.id, draft().GetBool(b.field) ? BST_CHECKED : BST_UNCHECKED);
    }
    RefreshRestoreVisibilityState();
    RefreshTrayState();
}

void WebViewPrefsWindowInstance::RefreshRestoreVisibilityState() {
    if (!hwnd()) return;
    // 跟草稿里的后台模式走：勾上就能编辑，不必先重启；关着时禁用但保留已存的值。
    const bool backgroundMode = draft().GetBool(window::BackgroundMode);
    if (HWND check = GetDlgItem(hwnd(), IDC_CHK_RESTORE_VISIBILITY)) EnableWindow(check, backgroundMode ? TRUE : FALSE);
    if (HWND note = GetDlgItem(hwnd(), IDC_STATIC_RESTORE_NOTE)) {
        SetWindowTextW(note, backgroundMode
            ? TR("Restores the last Show / Hide state of the background window; it does not force the window to show.",
                 "恢复后台窗口上次的显示 / 隐藏状态，不会强制显示窗口。")
            : TR("Available once background mode above is checked. The saved value is kept.",
                 "勾选上方的后台模式后可用；已存配置保留。"));
    }
}

void WebViewPrefsWindowInstance::RefreshTrayState() {
    if (!hwnd()) return;
    const bool hasIcon = TrayIcon::GetInstance().IsCreated();
    for (int id : {IDC_CHK_MINIMIZE_TO_TRAY, IDC_CHK_CLOSE_TO_TRAY}) {
        if (HWND check = GetDlgItem(hwnd(), id)) EnableWindow(check, hasIcon ? TRUE : FALSE);
    }
    if (HWND note = GetDlgItem(hwnd(), IDC_STATIC_TRAY_NOTE)) {
        SetWindowTextW(note, hasIcon
            ? TR("The tray icon is created by the current theme through the tray API; these switches take effect "
                 "immediately on Apply. Values set by the theme at run time are not written back here.",
                 "托盘图标由当前主题通过 tray API 创建；这两项 Apply 后立即生效。主题在运行期改的值不会写回这里。")
            : TR("The current theme has not created a tray icon, so these switches have nothing to act on. "
                 "The values are saved and take effect once an icon exists.",
                 "当前主题未创建托盘图标，这两项暂无作用；设置会保存，图标创建后生效。"));
    }
}

INT_PTR WebViewPrefsWindowInstance::OnMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    if (msg != WM_COMMAND || HIWORD(wParam) != BN_CLICKED) return FALSE;
    const int id = LOWORD(wParam);
    for (const CheckBinding& b : kChecks) {
        if (b.id != id) continue;
        draft().SetBool(b.field, IsDlgButtonChecked(hwnd, id) == BST_CHECKED);
        if (b.field == window::BackgroundMode) RefreshRestoreVisibilityState();
        UpdateState();
        return TRUE;
    }
    return FALSE;
}

// ============================================
// 注册
// ============================================

static preferences_page_factory_t<WebViewPrefsWindowPage> g_prefs_window_page_factory;

} // namespace webview_prefs

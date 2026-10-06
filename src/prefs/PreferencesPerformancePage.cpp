// ============================================
// PreferencesPerformancePage.cpp - WebView2 UI › Performance 子页
// ============================================

#include "pch.h"
#include "prefs/PreferencesPerformancePage.h"
#include "prefs/PreferencesFields.h"
#include "prefs/PreferencesPage.h"
#include "prefs/PreferencesZoom.h"
#include "settings/SecurityConfig.h"
#include "utils/I18n.h"
#include <algorithm>
#include <string>
#include <vector>

namespace webview_prefs {

namespace performance = prefs_fields::performance;

namespace {

// {660F36C1-92C8-4792-9140-2B8D46E38DEF}
constexpr GUID guid_prefs_performance_page =
    { 0x660f36c1, 0x92c8, 0x4792, { 0x91, 0x40, 0x2b, 0x8d, 0x46, 0xe3, 0x8d, 0xef } };

enum ControlIds {
    IDC_GROUP_STARTUP = 1400,
    IDC_CHK_PREHEAT = 1401,
    IDC_STATIC_PREHEAT_NOTE = 1402,

    IDC_GROUP_HIDDEN = 1410,
    IDC_CHK_DEEP_SUSPEND = 1411,
    IDC_STATIC_DEEP_SUSPEND_NOTE = 1412,

    IDC_GROUP_ZOOM = 1420,
    IDC_STATIC_ZOOM = 1421,
    IDC_COMBO_ZOOM = 1422,
    IDC_STATIC_ZOOM_NOTE = 1423,
};

// 复选框控件 ID 与字段下标的对应；OnMessage 与 SyncControlsFromDraft 都按它走。
struct CheckBinding {
    int id;
    size_t field;
};
constexpr CheckBinding kChecks[] = {
    {IDC_CHK_PREHEAT, performance::Preheat},
    {IDC_CHK_DEEP_SUSPEND, performance::DeepSuspend},
};

}  // namespace

// ============================================
// WebViewPrefsPerformancePage
// ============================================

const char* WebViewPrefsPerformancePage::get_name() {
    // 宿主缓存树节点名，切换组件语言后在重启后更新。
    return TRU("Performance", "性能");
}

GUID WebViewPrefsPerformancePage::get_guid() {
    return guid_prefs_performance_page;
}

GUID WebViewPrefsPerformancePage::get_parent_guid() {
    return GetPreferencesPageGuid();
}

double WebViewPrefsPerformancePage::get_sort_priority() {
    // Window 1.0 / Performance 2.0 / Developer 3.0
    return 2.0;
}

bool WebViewPrefsPerformancePage::get_help_url(pfc::string_base& p_out) {
    p_out = "https://github.com/user/foo_ui_webview2";
    return true;
}

preferences_page_instance::ptr WebViewPrefsPerformancePage::instantiate(
    fb2k::hwnd_t parent, preferences_page_callback::ptr callback) {
    return fb2k::service_new<WebViewPrefsPerformanceInstance>(parent, callback);
}

// ============================================
// WebViewPrefsPerformanceInstance
// ============================================

WebViewPrefsPerformanceInstance::WebViewPrefsPerformanceInstance(HWND parent, preferences_page_callback::ptr callback)
    : PreferencesPageBase(std::move(callback), performance::Table()) {
    static const bool s_startupPreheat = GetPreheatEnabled();
    startupPreheat_ = s_startupPreheat;
    CreatePageWindow(parent);
}

prefs_draft::Snapshot WebViewPrefsPerformanceInstance::ReadSnapshotFromConfig() const {
    prefs_draft::Snapshot s = prefs_draft::Defaults(fields());
    s.SetBool(performance::Preheat, GetPreheatEnabled());
    s.SetBool(performance::DeepSuspend, security_config::IsDeepSuspendEnabled());
    s.SetInt(performance::ZoomPercent, GetDefaultZoomPercent());
    return s;
}

prefs_draft::Snapshot WebViewPrefsPerformanceInstance::StartupSnapshot() const {
    prefs_draft::Snapshot s = prefs_draft::Defaults(fields());
    s.SetBool(performance::Preheat, startupPreheat_);
    return s;
}

bool WebViewPrefsPerformanceInstance::WriteField(size_t field, const prefs_draft::Snapshot& value) {
    try {
        switch (field) {
        case performance::Preheat: {
            const bool v = value.GetBool(field);
            SetPreheatEnabled(v);
            return GetPreheatEnabled() == v;
        }
        case performance::DeepSuspend: {
            const bool v = value.GetBool(field);
            security_config::SetDeepSuspendEnabled(v);
            return security_config::IsDeepSuspendEnabled() == v;
        }
        case performance::ZoomPercent: {
            const int percent = value.GetInt(field);
            SetDefaultZoomPercent(percent);
            return GetDefaultZoomPercent() == percent;
        }
        default:
            break;
        }
    } catch (...) {
    }
    return false;
}

std::wstring WebViewPrefsPerformanceInstance::FieldDisplayName(size_t field) const {
    switch (field) {
    case performance::Preheat: return TR("Preheat WebView2 on startup", "启动时预热 WebView2");
    case performance::DeepSuspend: return TR("Deep-suspend hidden WebView", "隐藏时深度挂起 WebView");
    case performance::ZoomPercent: return TR("Default content zoom", "默认内容缩放");
    default: break;
    }
    return {};
}

void WebViewPrefsPerformanceInstance::AfterApply(const std::vector<size_t>& changed) {
    // 已存在的 WebView 不会自己重读偏好：缩放刚写入就推给它们（主题覆盖过的除外）。
    if (std::find(changed.begin(), changed.end(), static_cast<size_t>(performance::ZoomPercent)) != changed.end()) {
        ApplyDefaultZoomToFollowingHosts();
    }
}

// ============================================
// 控件
// ============================================

void WebViewPrefsPerformanceInstance::CreateControls(HWND hwnd) {
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
    create(L"BUTTON", WS_EX_TRANSPARENT, TR("Startup", "启动"), BS_GROUPBOX, IDC_GROUP_STARTUP);
    create(L"BUTTON", WS_EX_TRANSPARENT, TR("Hidden windows", "窗口隐藏时"), BS_GROUPBOX, IDC_GROUP_HIDDEN);
    create(L"BUTTON", WS_EX_TRANSPARENT, TR("Content zoom", "内容缩放"), BS_GROUPBOX, IDC_GROUP_ZOOM);

    // ---- Startup ----
    createCheck(IDC_CHK_PREHEAT,
        TR("Create the WebView2 environment when foobar2000 starts", "foobar2000 启动时预先创建 WebView2 环境"));
    createNote(IDC_STATIC_PREHEAT_NOTE,
        TR("Starts creating the WebView2 environment at startup. When off, creation starts when a window first "
           "needs it. Takes effect after a restart.",
           "启动时开始创建 WebView2 环境。关闭后，首次打开需要它的窗口时才开始创建。重启后生效。"));

    // ---- Hidden windows ----
    createCheck(IDC_CHK_DEEP_SUSPEND,
        TR("Deep-suspend the WebView while the main window is hidden",
           "主窗口隐藏时深度挂起 WebView"));
    createNote(IDC_STATIC_DEEP_SUSPEND_NOTE,
        TR("Applies to the main window only, when it is minimized, in the tray or behind the lock screen. "
           "Off: the renderer is only asked to lower its memory use. Not applied while CDP keep-alive is active. "
           "Takes effect on the next hide.",
           "只对主窗口生效：最小化、进入托盘或锁屏时。关闭：只要求渲染进程降低内存用量。CDP 保活生效期间不挂起。"
           "下一次隐藏时生效。"));

    // ---- Content zoom ----
    createNote(IDC_STATIC_ZOOM, TR("Default zoom", "默认缩放"));
    HWND combo = create(L"COMBOBOX", 0, L"", WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, IDC_COMBO_ZOOM);
    // 项序与 prefs_zoom::PercentAt 一致：第 index 项即 kMinPercent + index * kStepPercent。
    for (int index = 0; index < prefs_zoom::kOptionCount; ++index) {
        const std::wstring text = std::to_wstring(prefs_zoom::PercentAt(index)) + L"%";
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
    }
    createNote(IDC_STATIC_ZOOM_NOTE,
        TR("Applied on top of the system display scaling; 100% adds no extra zoom. Apply updates open windows, "
           "except those with zoom set separately by the theme. New windows use this default. Theme zoom "
           "changes do not change the saved default.",
           "叠加在系统显示缩放之上；100% 表示不额外缩放。应用后会更新已打开的窗口，主题单独设置了缩放的窗口除外。"
           "新窗口使用此默认值。主题调整缩放不会改变保存的默认值。"));

    SyncControlsFromDraft();
}

void WebViewPrefsPerformanceInstance::LayoutContent(prefs_layout::PageLayoutBuilder& builder) {
    builder.BeginGroup(IDC_GROUP_STARTUP, {});
    builder.CheckBox(IDC_CHK_PREHEAT);
    builder.Note(IDC_STATIC_PREHEAT_NOTE);
    builder.EndGroup();

    builder.BeginGroup(IDC_GROUP_HIDDEN, {});
    builder.CheckBox(IDC_CHK_DEEP_SUSPEND);
    builder.Note(IDC_STATIC_DEEP_SUSPEND_NOTE);
    builder.EndGroup();

    builder.BeginGroup(IDC_GROUP_ZOOM, {IDC_STATIC_ZOOM});
    builder.LabelControl(IDC_STATIC_ZOOM, IDC_COMBO_ZOOM, builder.comboWindowHeight());
    builder.Note(IDC_STATIC_ZOOM_NOTE);
    builder.EndGroup();
}

void WebViewPrefsPerformanceInstance::SyncControlsFromDraft() {
    if (!hwnd()) return;
    for (const CheckBinding& b : kChecks) {
        CheckDlgButton(hwnd(), b.id, draft().GetBool(b.field) ? BST_CHECKED : BST_UNCHECKED);
    }
    if (HWND combo = GetDlgItem(hwnd(), IDC_COMBO_ZOOM)) {
        SendMessageW(combo, CB_SETCURSEL,
            static_cast<WPARAM>(prefs_zoom::IndexOf(draft().GetInt(performance::ZoomPercent))), 0);
    }
}

INT_PTR WebViewPrefsPerformanceInstance::OnMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    if (msg != WM_COMMAND) return FALSE;
    const WORD code = HIWORD(wParam);
    const int id = LOWORD(wParam);

    if (id == IDC_COMBO_ZOOM) {
        if (code != CBN_SELCHANGE) return TRUE;
        const int sel = static_cast<int>(SendMessageW(GetDlgItem(hwnd, IDC_COMBO_ZOOM), CB_GETCURSEL, 0, 0));
        draft().SetInt(performance::ZoomPercent, prefs_zoom::PercentAt(sel));
        UpdateState();
        return TRUE;
    }

    if (code != BN_CLICKED) return FALSE;
    for (const CheckBinding& b : kChecks) {
        if (b.id != id) continue;
        draft().SetBool(b.field, IsDlgButtonChecked(hwnd, id) == BST_CHECKED);
        UpdateState();
        return TRUE;
    }
    return FALSE;
}

// ============================================
// 注册
// ============================================

static preferences_page_factory_t<WebViewPrefsPerformancePage> g_prefs_performance_page_factory;

} // namespace webview_prefs

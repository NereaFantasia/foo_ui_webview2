// ============================================
// PreferencesDeveloperPage.cpp - WebView2 UI › Developer 子页
// ============================================

#include "pch.h"
#include "core/PreferencesDeveloperPage.h"
#include "core/PreferencesCdpPort.h"
#include "core/PreferencesDevServerUrl.h"
#include "core/PreferencesFields.h"
#include "core/PreferencesPage.h"
#include "core/SecurityConfig.h"
#include "utils/I18n.h"
#include <algorithm>
#include <string>
#include <vector>

namespace webview_prefs {

namespace developer = prefs_fields::developer;

namespace {

// {4D357687-5AB8-4CEE-8942-6DCFA6669ABA}
constexpr GUID guid_prefs_developer_page =
    { 0x4d357687, 0x5ab8, 0x4cee, { 0x89, 0x42, 0x6d, 0xcf, 0xa6, 0x66, 0x9a, 0xba } };

enum ControlIds {
    IDC_GROUP_DEV_SERVER = 1300,
    IDC_CHK_USE_DEV_SERVER = 1301,
    IDC_STATIC_DEV_SERVER_URL = 1302,
    IDC_EDIT_DEV_SERVER_URL = 1303,
    IDC_STATIC_DEV_SERVER_NOTE = 1304,

    IDC_GROUP_ADVANCED = 1320,
    IDC_STATIC_ADVANCED_NOTE = 1321,

    IDC_GROUP_DEVTOOLS = 1330,
    IDC_CHK_DEVTOOLS = 1331,
    IDC_STATIC_DEVTOOLS_NOTE = 1332,

    IDC_GROUP_CDP = 1340,
    IDC_CHK_CDP_REMOTE = 1341,
    IDC_STATIC_CDP_PORT = 1342,
    IDC_EDIT_CDP_PORT = 1343,
    IDC_STATIC_CDP_NOTE = 1344,
};

std::wstring ControlText(HWND control) {
    if (!control) return {};
    const int length = GetWindowTextLengthW(control);
    if (length <= 0) return {};
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(control, text.data(), length + 1);
    text.resize(static_cast<size_t>(length));
    return text;
}

std::string Utf8(const std::wstring& text) {
    return pfc::stringcvt::string_utf8_from_wide(text.c_str()).get_ptr();
}

std::wstring Wide(const std::string& text) {
    return pfc::stringcvt::string_wide_from_utf8(text.c_str()).get_ptr();
}

}  // namespace

// ============================================
// WebViewPrefsDeveloperPage
// ============================================

const char* WebViewPrefsDeveloperPage::get_name() {
    // 宿主缓存树节点名，切换组件语言后在重启后更新。
    return TRU("Developer", "开发者");
}

GUID WebViewPrefsDeveloperPage::get_guid() {
    return guid_prefs_developer_page;
}

GUID WebViewPrefsDeveloperPage::get_parent_guid() {
    return GetPreferencesPageGuid();
}

double WebViewPrefsDeveloperPage::get_sort_priority() {
    // Window 1.0 / Performance 2.0 / Developer 3.0
    return 3.0;
}

bool WebViewPrefsDeveloperPage::get_help_url(pfc::string_base& p_out) {
    p_out = "https://github.com/user/foo_ui_webview2";
    return true;
}

preferences_page_instance::ptr WebViewPrefsDeveloperPage::instantiate(
    fb2k::hwnd_t parent, preferences_page_callback::ptr callback) {
    return fb2k::service_new<WebViewPrefsDeveloperInstance>(parent, callback);
}

// ============================================
// WebViewPrefsDeveloperInstance
// ============================================

WebViewPrefsDeveloperInstance::WebViewPrefsDeveloperInstance(HWND parent, preferences_page_callback::ptr callback)
    : PreferencesPageBase(std::move(callback), developer::Table()) {
    static const bool s_startupDevTools = GetDevToolsEnabled();
    static const bool s_startupCdpRemote = security_config::IsCdpRemoteEnabled();
    static const int s_startupCdpPort = GetCdpPort();
    startupDevTools_ = s_startupDevTools;
    startupCdpRemote_ = s_startupCdpRemote;
    startupCdpPort_ = s_startupCdpPort;
    CreatePageWindow(parent);
}

prefs_draft::Snapshot WebViewPrefsDeveloperInstance::ReadSnapshotFromConfig() const {
    prefs_draft::Snapshot s = prefs_draft::Defaults(fields());
    // 开发者工具读存储值而不是 security_config::IsDevToolsEnabled()：后者在 Debug 构建恒为真。
    s.SetBool(developer::DevTools, GetDevToolsEnabled());
    s.SetBool(developer::UseDevServer, security_config::UseDevServer());
    const char* url = security_config::GetDevServerUrl();
    s.SetString(developer::DevServerUrl, url ? url : "");
    s.SetBool(developer::CdpRemote, security_config::IsCdpRemoteEnabled());
    s.SetInt(developer::CdpPort, GetCdpPort());
    return s;
}

prefs_draft::Snapshot WebViewPrefsDeveloperInstance::StartupSnapshot() const {
    prefs_draft::Snapshot s = prefs_draft::Defaults(fields());
    s.SetBool(developer::DevTools, startupDevTools_);
    s.SetBool(developer::CdpRemote, startupCdpRemote_);
    s.SetInt(developer::CdpPort, startupCdpPort_);
    return s;
}

bool WebViewPrefsDeveloperInstance::WriteField(size_t field, const prefs_draft::Snapshot& value) {
    try {
        switch (field) {
        case developer::DevTools: {
            const bool enabled = value.GetBool(developer::DevTools);
            security_config::SetDevToolsEnabled(enabled);
            return GetDevToolsEnabled() == enabled;
        }
        case developer::CdpRemote: {
            const bool enabled = value.GetBool(developer::CdpRemote);
            security_config::SetCdpRemoteEnabled(enabled);
            return security_config::IsCdpRemoteEnabled() == enabled;
        }
        case developer::CdpPort: {
            const int port = value.GetInt(developer::CdpPort);
            SetCdpPort(port);
            return GetCdpPort() == port;
        }
        case developer::UseDevServer: {
            const bool use = value.GetBool(developer::UseDevServer);
            security_config::SetUseDevServer(use);
            return security_config::UseDevServer() == use;
        }
        case developer::DevServerUrl: {
            const std::string& url = value.GetString(developer::DevServerUrl);
            security_config::SetDevServerUrl(url.c_str());
            const char* stored = security_config::GetDevServerUrl();
            return stored && url == stored;
        }
        default:
            break;
        }
    } catch (...) {
    }
    return false;
}

std::wstring WebViewPrefsDeveloperInstance::FieldDisplayName(size_t field) const {
    switch (field) {
    case developer::DevTools: return TR("Developer Tools (F12)", "开发者工具 (F12)");
    case developer::UseDevServer: return TR("Use development server", "使用开发服务器");
    case developer::DevServerUrl: return TR("Development server URL", "开发服务器 URL");
    case developer::CdpRemote: return TR("CDP remote debugging", "CDP 远程调试");
    case developer::CdpPort: return TR("CDP port", "CDP 端口");
    default: break;
    }
    return {};
}

std::wstring WebViewPrefsDeveloperInstance::ValidationMessage(const prefs_draft::ValidationResult& result) const {
    if (result.field == developer::DevServerUrl && result.error == prefs_draft::ValidationError::Invalid) {
        return TR("The development server URL must start with http:// or https:// and name a host, "
                  "for example http://localhost:5173. The host and port may only use ASCII letters, digits, "
                  "dots, hyphens and a half-width colon. Leave it empty to load the local template instead.",
                  "开发服务器 URL 须以 http:// 或 https:// 开头并带主机名，例如 http://localhost:5173；"
                  "主机名和端口只能用半角字母、数字、点、连字符和半角冒号。留空则加载本地模板。");
    }
    if (result.field == developer::CdpPort) {
        return TR("The CDP port must be a whole number from 1024 to 65535, digits only (default 9222). "
                  "Nothing was saved.",
                  "CDP 端口须是 1024 到 65535 之间的整数，只能用半角数字（默认 9222），本次没有保存。");
    }
    return PreferencesPageBase::ValidationMessage(result);
}

void WebViewPrefsDeveloperInstance::AfterApply(const std::vector<size_t>& changed) {
    // 开发服务器开关与 URL 都在加载前端时读取：重载一次跟随全局模板的前端即生效，不需要重启。
    // 开发者工具与 CDP 两组在启动期 / controller 创建期读取，这里不做任何事，由 needs_restart 提示。
    const bool devServerChanged =
        std::find(changed.begin(), changed.end(), static_cast<size_t>(developer::UseDevServer)) != changed.end() ||
        std::find(changed.begin(), changed.end(), static_cast<size_t>(developer::DevServerUrl)) != changed.end();
    if (devServerChanged) ReloadFrontendsForDevServerChange();
}

// ============================================
// 控件
// ============================================

void WebViewPrefsDeveloperInstance::CreateControls(HWND hwnd) {
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
    create(L"BUTTON", WS_EX_TRANSPARENT, TR("Developer Tools", "开发者工具"), BS_GROUPBOX, IDC_GROUP_DEVTOOLS);
    create(L"BUTTON", WS_EX_TRANSPARENT, TR("Development server", "开发服务器"), BS_GROUPBOX, IDC_GROUP_DEV_SERVER);
    create(L"BUTTON", WS_EX_TRANSPARENT, TR("CDP remote debugging", "CDP 远程调试"), BS_GROUPBOX, IDC_GROUP_CDP);
    create(L"BUTTON", WS_EX_TRANSPARENT, TR("Advanced", "高级"), BS_GROUPBOX, IDC_GROUP_ADVANCED);

    // ---- Developer Tools ----
    createCheck(IDC_CHK_DEVTOOLS, TR("Enable Developer Tools (F12)", "启用开发者工具 (F12)"));
    createNote(IDC_STATIC_DEVTOOLS_NOTE,
        TR("Read when a WebView is created; windows that are already open pick it up after a restart.",
           "在创建 WebView 时读取；已打开的窗口重启后生效。"));

    // ---- Development server ----
    createCheck(IDC_CHK_USE_DEV_SERVER, TR("Use development server (HMR hot reload)", "使用开发服务器（HMR 热重载）"));
    createNote(IDC_STATIC_DEV_SERVER_URL, TR("URL", "URL"));
    create(L"EDIT", WS_EX_CLIENTEDGE, L"", WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL, IDC_EDIT_DEV_SERVER_URL);
    createNote(IDC_STATIC_DEV_SERVER_NOTE,
        TR("Applies on Apply: the main window and panels that follow the global template reload from the "
           "development server (or from the local template when the URL is empty or the switch is off). "
           "Panels with their own template or URL and popup windows are not affected.",
           "按下应用即生效：主窗口与跟随全局模板的面板会从开发服务器重载（URL 留空或关闭开关时回到本地模板）。"
           "显式指定模板或 URL 的面板与弹出窗口不受影响。"));

    // ---- CDP remote debugging ----
    createCheck(IDC_CHK_CDP_REMOTE,
        TR("Enable CDP remote debugging (for MCP / AI agents)", "启用 CDP 远程调试（供 MCP / AI 代理使用）"));
    createNote(IDC_STATIC_CDP_PORT, TR("Port", "端口"));
    // 不用 ES_NUMBER：它会在输入时静默丢弃非数字键与整段含非数字的粘贴，用户得不到任何提示。
    // 这里让文字照常进框，Apply 时由 ParsePort 判失败并给范围提示，与 URL 框同一套反馈。
    create(L"EDIT", WS_EX_CLIENTEDGE, L"", WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL, IDC_EDIT_CDP_PORT);
    createNote(IDC_STATIC_CDP_NOTE,
        TR("Port 1024 to 65535, default 9222. The port is opened when the WebView2 environment is created, so both "
           "settings take effect after a restart. If you change the port, point the MCP server at it too "
           "(FB2K_CDP_PORT).",
           "端口 1024 到 65535，默认 9222。端口在创建 WebView2 环境时打开，两项都在重启后生效。"
           "改了端口，MCP 服务器也要指向新端口（FB2K_CDP_PORT）。"));

    // ---- Advanced ----
    createNote(IDC_STATIC_ADVANCED_NOTE,
        TR("Security exceptions (local network access, insecure TLS, CDP keep-alive) stay in Advanced Preferences "
           "under Tools > WebView2 UI.",
           "安全例外（本地网络访问、不安全 TLS、CDP 保活）仍在 高级首选项 > Tools > WebView2 UI 中配置。"));

    SyncControlsFromDraft();
}

void WebViewPrefsDeveloperInstance::LayoutContent(prefs_layout::PageLayoutBuilder& builder) {
    builder.BeginGroup(IDC_GROUP_DEVTOOLS, {});
    builder.CheckBox(IDC_CHK_DEVTOOLS);
    builder.Note(IDC_STATIC_DEVTOOLS_NOTE);
    builder.EndGroup();

    builder.BeginGroup(IDC_GROUP_DEV_SERVER, {IDC_STATIC_DEV_SERVER_URL});
    builder.CheckBox(IDC_CHK_USE_DEV_SERVER);
    builder.LabelControl(IDC_STATIC_DEV_SERVER_URL, IDC_EDIT_DEV_SERVER_URL);
    builder.Note(IDC_STATIC_DEV_SERVER_NOTE);
    builder.EndGroup();

    builder.BeginGroup(IDC_GROUP_CDP, {IDC_STATIC_CDP_PORT});
    builder.CheckBox(IDC_CHK_CDP_REMOTE);
    builder.LabelControl(IDC_STATIC_CDP_PORT, IDC_EDIT_CDP_PORT);
    builder.Note(IDC_STATIC_CDP_NOTE);
    builder.EndGroup();

    builder.BeginGroup(IDC_GROUP_ADVANCED, {});
    builder.Note(IDC_STATIC_ADVANCED_NOTE);
    builder.EndGroup();
}

void WebViewPrefsDeveloperInstance::SyncControlsFromDraft() {
    if (!hwnd()) return;
    syncingControls_ = true;
    CheckDlgButton(hwnd(), IDC_CHK_DEVTOOLS, draft().GetBool(developer::DevTools) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwnd(), IDC_CHK_USE_DEV_SERVER,
        draft().GetBool(developer::UseDevServer) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwnd(), IDC_CHK_CDP_REMOTE, draft().GetBool(developer::CdpRemote) ? BST_CHECKED : BST_UNCHECKED);
    // 文字相同就不重设，免得把光标顶回行首。
    if (HWND edit = GetDlgItem(hwnd(), IDC_EDIT_DEV_SERVER_URL)) {
        const std::wstring wanted = Wide(draft().GetString(developer::DevServerUrl));
        if (ControlText(edit) != wanted) SetWindowTextW(edit, wanted.c_str());
    }
    if (HWND edit = GetDlgItem(hwnd(), IDC_EDIT_CDP_PORT)) {
        // 解析失败的草稿值（kInvalidPort）不回写，保留用户正在输入的文字。
        const int port = draft().GetInt(developer::CdpPort);
        if (prefs_cdp::IsValidPort(port)) {
            const std::wstring wanted = std::to_wstring(port);
            if (ControlText(edit) != wanted) SetWindowTextW(edit, wanted.c_str());
        }
    }
    syncingControls_ = false;
}

INT_PTR WebViewPrefsDeveloperInstance::OnMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    if (msg != WM_COMMAND) return FALSE;
    const WORD code = HIWORD(wParam);

    switch (LOWORD(wParam)) {
    case IDC_CHK_DEVTOOLS:
        if (code != BN_CLICKED) return TRUE;
        draft().SetBool(developer::DevTools, IsDlgButtonChecked(hwnd, IDC_CHK_DEVTOOLS) == BST_CHECKED);
        UpdateState();
        return TRUE;

    case IDC_CHK_USE_DEV_SERVER:
        if (code != BN_CLICKED) return TRUE;
        draft().SetBool(developer::UseDevServer, IsDlgButtonChecked(hwnd, IDC_CHK_USE_DEV_SERVER) == BST_CHECKED);
        UpdateState();
        return TRUE;

    case IDC_EDIT_DEV_SERVER_URL:
        if (code != EN_CHANGE || syncingControls_) return TRUE;
        // 只进草稿，Apply 时校验；首尾空白在这里就去掉，免得只差空白也算改动。
        draft().SetString(developer::DevServerUrl,
            prefs_devserver::Trim(Utf8(ControlText(GetDlgItem(hwnd, IDC_EDIT_DEV_SERVER_URL)))));
        UpdateState();
        return TRUE;

    case IDC_CHK_CDP_REMOTE:
        if (code != BN_CLICKED) return TRUE;
        draft().SetBool(developer::CdpRemote, IsDlgButtonChecked(hwnd, IDC_CHK_CDP_REMOTE) == BST_CHECKED);
        UpdateState();
        return TRUE;

    case IDC_EDIT_CDP_PORT:
        if (code != EN_CHANGE || syncingControls_) return TRUE;
        // 解析失败存 kInvalidPort：页面变脏、Apply 被校验拒绝并给出范围提示，配置不会被写。
        draft().SetInt(developer::CdpPort,
            prefs_cdp::ParsePort(Utf8(ControlText(GetDlgItem(hwnd, IDC_EDIT_CDP_PORT)))));
        UpdateState();
        return TRUE;

    default:
        break;
    }
    return FALSE;
}

// ============================================
// 注册
// ============================================

static preferences_page_factory_t<WebViewPrefsDeveloperPage> g_prefs_developer_page_factory;

} // namespace webview_prefs

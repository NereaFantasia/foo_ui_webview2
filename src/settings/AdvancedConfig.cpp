#include "pch.h"
#include "settings/AdvancedConfig.h"
#include "settings/SecurityConfig.h"

// ============================================
// 安全: 高级设置 (Advanced Preferences)
// ============================================

// 高级设置分支 GUID
// {A1B2C3D4-E5F6-7890-1234-56789ABCDEF0}
static constexpr GUID guid_adv_branch_webview = 
    { 0xa1b2c3d4, 0xe5f6, 0x7890, { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0 } };

// DevTools 开关的旧 Advanced 键 GUID：叶子已移除，只用于首次迁移时读 configStore 旧值。
// {A1B2C3D5-E5F6-7890-1234-56789ABCDEF1}
static constexpr GUID guid_adv_devtools = 
    { 0xa1b2c3d5, 0xe5f6, 0x7890, { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf1 } };

// 本地网络访问开关 GUID
// {A1B2C3D6-E5F6-7890-1234-56789ABCDEF2}
static constexpr GUID guid_adv_local_network = 
    { 0xa1b2c3d6, 0xe5f6, 0x7890, { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf2 } };

// 允许 HTTP 明文连接 GUID (禁用 HSTS)
// {A1B2C3D7-E5F6-7890-1234-56789ABCDEF3}
static constexpr GUID guid_adv_allow_insecure = 
    { 0xa1b2c3d7, 0xe5f6, 0x7890, { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf3 } };

// 允许自签 / 无效 TLS 证书 GUID (绕过证书校验,fb.http.* 用)
// {A1B2C3DC-E5F6-7890-1234-56789ABCDEF8}
static constexpr GUID guid_adv_allow_insecure_tls =
    { 0xa1b2c3dc, 0xe5f6, 0x7890, { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf8 } };

// CDP 远程调试开关的旧 Advanced 键 GUID：叶子已移除，只用于首次迁移时读 configStore 旧值。
// {A1B2C3DB-E5F6-7890-1234-56789ABCDEF7}
static constexpr GUID guid_adv_cdp_remote =
    { 0xa1b2c3db, 0xe5f6, 0x7890, { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf7 } };

// CDP 自动化 keep-alive 开关 GUID
// {A1B2C3DE-E5F6-7890-1234-56789ABCDEF9}
static constexpr GUID guid_adv_cdp_keepalive =
    { 0xa1b2c3de, 0xe5f6, 0x7890, { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf9 } };

// 深度挂起（TrySuspend）开关的旧 Advanced 键 GUID：叶子已移除，只用于首次迁移时读 configStore 旧值。
// {A1B2C3DF-E5F6-7890-1234-56789ABCDEFA}
static constexpr GUID guid_adv_deep_suspend =
    { 0xa1b2c3df, 0xe5f6, 0x7890, { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xfa } };

// 高级设置分支: Preferences → Advanced → Tools → WebView2 UI
// 分支名保持英文：宿主核心分支均为原文，组件分支与之并列。
advconfig_branch_factory g_adv_branch(
    "WebView2 UI",
    guid_adv_branch_webview,
    advconfig_branch::guid_branch_tools,
    0.0
);

// 剩余叶子条目用 advconfig_i18n 工厂注册：显示名双语；GUID、默认值、排序优先级与原注册一致。
// 四条都不带 preferences_state::needs_restart：三条在使用处实时读取，HSTS 一条目前没有运行时读取点。

// 从 Advanced 叶子搬进 Preferences 子页的开关（开发者工具、CDP 远程调试、深度挂起；后台模式与开发服务器
// 两项见下文各自的段落）改存 cfg_int：-1 表示还没从旧的 Advanced 键迁移过，首次读取时用旧 GUID 的
// configStore 键取旧值写回 0 / 1；旧键保留，降级到旧版本仍能读到。Advanced 里不再出现这些条目，避免同一设置两处编辑。

// 开发者工具 (F12)，默认关闭。新建 controller 时读取，改动后需重启；编辑入口在 Developer 子页。
// {8F272169-43BD-4DC4-89E4-CE800D9EF74E}
static constexpr GUID guid_cfg_devtools_v2 =
    { 0x8f272169, 0x43bd, 0x4dc4, { 0x89, 0xe4, 0xce, 0x80, 0x0d, 0x9e, 0xf7, 0x4e } };
static cfg_var_modern::cfg_int g_cfg_devtools_v2(guid_cfg_devtools_v2, -1);

// CDP 远程调试，默认关闭 - MCP/AI Agent 用。端口（Developer 子页另存）在环境创建时注入，改动后需重启。
// {870AAC8F-17C9-4E22-9103-79757F16DB44}
static constexpr GUID guid_cfg_cdp_remote_v2 =
    { 0x870aac8f, 0x17c9, 0x4e22, { 0x91, 0x03, 0x79, 0x75, 0x7f, 0x16, 0xdb, 0x44 } };
static cfg_var_modern::cfg_int g_cfg_cdp_remote_v2(guid_cfg_cdp_remote_v2, -1);

// 深度挂起，默认开启。最小化/托盘/锁屏隐藏页面后用 TrySuspend 冻结 renderer（等效 Edge sleeping tab，
// OS 可回收其内存）；关闭时回退为仅 MemoryUsageTargetLevel=Low 的路径。keep-alive 生效时本开关不参与
//（页面根本不隐藏）。下一次隐藏/恢复策略计算即消费，不需要重启；编辑入口在 Performance 子页。
// {81774757-BD73-481C-BFE0-C785331D7D68}
static constexpr GUID guid_cfg_deep_suspend_v2 =
    { 0x81774757, 0xbd73, 0x481c, { 0xbf, 0xe0, 0xc7, 0x85, 0x33, 0x1d, 0x7d, 0x68 } };
static cfg_var_modern::cfg_int g_cfg_deep_suspend_v2(guid_cfg_deep_suspend_v2, -1);

// 迁移开关共用的读取：-1 时从旧键取值并写回。
static bool ReadMigratedSwitch(cfg_var_modern::cfg_int& var, const GUID& legacyGuid, bool legacyDefault) {
    int64_t stored = var.get();
    if (stored < 0) {
        const bool legacy = fb2k::configStore::get()->getConfigBool(
            fb2k::advconfig_autoName(legacyGuid), legacyDefault);
        stored = legacy ? 1 : 0;
        var.set(stored);
    }
    return stored != 0;
}

// CDP 自动化 keep-alive 子开关 (默认开启)。仅在本进程实际开启 CDP 端口时
// 参与判定 (见 security_config::IsAutomationKeepAliveActive)；生效时托盘/
// 最小化/锁屏不再挂起页面，以保证 CDP 截图与时序类工具稳定。
// 下一次可见性策略计算即消费，不需要重启。
advconfig_i18n::CheckboxFactory g_cfg_cdp_keepalive(
    {"Keep WebView active in background while CDP remote debugging is on (tray/minimize/lock)",
     "CDP 远程调试开启期间在后台保持 WebView 活动（托盘 / 最小化 / 锁屏）"},
    guid_adv_cdp_keepalive,
    guid_adv_branch_webview,
    0.6,
    true  // 默认开启 (开 CDP 即默认要自动化稳定；牺牲后台省电为显式权衡)
);

// 本地网络访问开关 (默认关闭) - SSRF 防护用。fb.http.* 每次请求实时读取，不需要重启。
advconfig_i18n::CheckboxFactory g_cfg_local_network(
    {"Allow HTTP access to local network (127.0.0.1, 192.168.x.x)",
     "允许 HTTP 访问本地网络 (127.0.0.1、192.168.x.x)"},
    guid_adv_local_network,
    guid_adv_branch_webview,
    1.0,
    false  // 默认关闭 (安全)
);

// 允许 HTTP 明文连接 (默认关闭)。目前没有运行时读取点，只保留存储与 GUID；
// 显示名如实标为保留项，不再宣称重启后生效。
advconfig_i18n::CheckboxFactory g_cfg_allow_insecure(
    {"Allow insecure HTTP connections (disable HSTS) - reserved, currently has no effect",
     "允许不安全的 HTTP 连接（禁用 HSTS）- 保留项，当前无运行效果"},
    guid_adv_allow_insecure,
    guid_adv_branch_webview,
    2.0,
    false  // 默认关闭 (安全)
);

// 允许自签 / 无效 TLS 证书 (默认关闭)
// 仅作用于 fb.http.* 请求,且每个请求还需显式 opt-in (HttpRequestOptions.insecureTls = true)。
// WebView2 自身的 fetch / 资源加载始终走严格证书校验,不受此开关影响。实时读取，不需要重启。
advconfig_i18n::CheckboxFactory g_cfg_allow_insecure_tls(
    {"Allow self-signed / invalid TLS certificates for fb.http.* (per-request opt-in still required)",
     "允许 fb.http.* 使用自签名 / 无效 TLS 证书（每个请求仍需显式选择加入）"},
    guid_adv_allow_insecure_tls,
    guid_adv_branch_webview,
    2.5,
    false  // 默认关闭 (安全)
);

// ============================================
// 后台模式配置
// ============================================

// 后台模式开关的旧 Advanced 键 GUID：叶子已移除，只用于首次迁移时读 configStore 旧值。
// {A1B2C3D8-E5F6-7890-1234-56789ABCDEF4}
static constexpr GUID guid_adv_background_mode =
    { 0xa1b2c3d8, 0xe5f6, 0x7890, { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf4 } };

// 后台模式开关 (默认关闭)。编辑入口在 Preferences → Display → WebView2 UI → Window 子页，
// 不再作为 Advanced 叶子出现。存储改为 cfg_int：-1 表示还没从旧的 Advanced 键迁移过，
// 首次读取时用 guid_adv_background_mode 的 configStore 键取旧值写回 0 / 1；旧键保留，
// 降级到旧版本仍能读到。后台服务在启动时初始化，改动后需重启。
// {F8A9E9A3-AC67-4D64-95D2-C93C3D43AA21}
static constexpr GUID guid_cfg_background_mode_v2 =
    { 0xf8a9e9a3, 0xac67, 0x4d64, { 0x95, 0xd2, 0xc9, 0x3c, 0x3d, 0x43, 0xaa, 0x21 } };
static cfg_var_modern::cfg_int g_cfg_background_mode_v2(guid_cfg_background_mode_v2, -1);

// ============================================
// 开发服务器配置
// ============================================

// 开发服务器 URL 与开关的旧 Advanced 键 GUID：叶子已移除，只用于首次迁移时读 configStore 旧值。
// {A1B2C3D9-E5F6-7890-1234-56789ABCDEF5}
static constexpr GUID guid_adv_dev_server_url =
    { 0xa1b2c3d9, 0xe5f6, 0x7890, { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf5 } };
// {A1B2C3DA-E5F6-7890-1234-56789ABCDEF6}
static constexpr GUID guid_adv_use_dev_server =
    { 0xa1b2c3da, 0xe5f6, 0x7890, { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf6 } };

// 开发服务器 URL (默认空) 与开关 (默认关闭)。编辑入口在 Developer 子页，不再作为 Advanced 叶子出现。
// 每次加载前端时读取（WebViewPanel / MainWindow / PopupWindow 的加载路径与 WebViewHost 的来源校验
// 都是逐次读），改动在下一次加载页面时生效，不需要重启。
// 两项一起迁移：开关存 cfg_int，-1 表示还没从旧的 Advanced 键迁移过；URL 没有可用的哨兵值（空串是合法取值），
// 所以由开关的 -1 代表两项的迁移状态，首次读写任一项时把 URL 与开关的旧值一起搬过来，旧键保留。
// {34D8DFE3-2D0B-4A7D-B35E-1CB8DA5B310F}
static constexpr GUID guid_cfg_dev_server_url_v2 =
    { 0x34d8dfe3, 0x2d0b, 0x4a7d, { 0xb3, 0x5e, 0x1c, 0xb8, 0xda, 0x5b, 0x31, 0x0f } };
static cfg_var_modern::cfg_string g_cfg_dev_server_url_v2(guid_cfg_dev_server_url_v2, "");
// {B84E823C-B5F3-4671-BF74-8CC883E202EC}
static constexpr GUID guid_cfg_use_dev_server_v2 =
    { 0xb84e823c, 0xb5f3, 0x4671, { 0xbf, 0x74, 0x8c, 0xc8, 0x83, 0xe2, 0x02, 0xec } };
static cfg_var_modern::cfg_int g_cfg_use_dev_server_v2(guid_cfg_use_dev_server_v2, -1);

// 读写开发服务器两项前调用。写入也要先迁移：否则新写的值会被之后的首次迁移覆盖。
static void EnsureDevServerConfigMigrated() {
    if (g_cfg_use_dev_server_v2.get() >= 0) return;
    const pfc::string8 legacyUrl = fb2k::configStore::get()->getConfigString(
        fb2k::advconfig_autoName(guid_adv_dev_server_url), "")->c_str();
    g_cfg_dev_server_url_v2.set(legacyUrl);
    ReadMigratedSwitch(g_cfg_use_dev_server_v2, guid_adv_use_dev_server, false);
}

// 全局访问函数
namespace security_config {
    bool GetDevToolsSetting() {
        return ReadMigratedSwitch(g_cfg_devtools_v2, guid_adv_devtools, false);
    }

    bool IsDevToolsEnabled() {
        #ifndef NDEBUG
            return true;  // Debug: DevTools always available for development
        #else
            return GetDevToolsSetting();
        #endif
    }
    
    void SetDevToolsEnabled(bool enabled) {
        g_cfg_devtools_v2.set(enabled ? 1 : 0);
    }
    
    bool IsCdpRemoteEnabled() {
        // CDP remote port must always respect user config
        // even in Debug builds, to avoid unintended network exposure
        return ReadMigratedSwitch(g_cfg_cdp_remote_v2, guid_adv_cdp_remote, false);
    }

    void SetCdpRemoteEnabled(bool enabled) {
        g_cfg_cdp_remote_v2.set(enabled ? 1 : 0);
    }

    // CDP 端口进程级快照：配置的实时值与"端口需重启"语义不一致,
    // 运行中勾/取消 CDP 会产生 keep-alive 失配窗口, 故绑定真实端口状态。
    // 环境创建先于任何后台挂起事件, 单线程置位无竞争顾虑。
    static bool g_cdp_port_opened_this_process = false;

    void NoteCdpPortOpenedThisProcess() {
        g_cdp_port_opened_this_process = true;
    }

    bool IsAutomationKeepAliveActive() {
        return g_cdp_port_opened_this_process && g_cfg_cdp_keepalive.get();
    }

    bool IsDeepSuspendEnabled() {
        return ReadMigratedSwitch(g_cfg_deep_suspend_v2, guid_adv_deep_suspend, true);
    }

    void SetDeepSuspendEnabled(bool enabled) {
        g_cfg_deep_suspend_v2.set(enabled ? 1 : 0);
    }

    bool IsLocalNetworkAccessAllowed() {
        return g_cfg_local_network.get();
    }
    
    bool IsInsecureHttpAllowed() {
        return g_cfg_allow_insecure.get();
    }

    bool IsInsecureTlsAllowed() {
        return g_cfg_allow_insecure_tls.get();
    }
    
    bool IsBackgroundModeEnabled() {
        int64_t stored = g_cfg_background_mode_v2.get();
        if (stored < 0) {
            // 首次读取：把旧 Advanced 叶子存在 configStore 里的值搬过来（没有旧值就是默认关闭）。
            const bool legacy = fb2k::configStore::get()->getConfigBool(
                fb2k::advconfig_autoName(guid_adv_background_mode), false);
            stored = legacy ? 1 : 0;
            g_cfg_background_mode_v2.set(stored);
        }
        return stored != 0;
    }

    void SetBackgroundModeEnabled(bool enabled) {
        g_cfg_background_mode_v2.set(enabled ? 1 : 0);
    }
    
    // HMR 开发服务器配置
    bool UseDevServer() {
        EnsureDevServerConfigMigrated();
        return g_cfg_use_dev_server_v2.get() != 0;
    }
    
    const char* GetDevServerUrl() {
        // cfg_string 只按值交出内容，没有可借出的内部缓冲；这里复制进静态缓冲，指针到下一次调用前有效。
        // 调用方（前端加载、来源校验、偏好页）都在主线程。
        EnsureDevServerConfigMigrated();
        static pfc::string8 s_devServerUrl;
        g_cfg_dev_server_url_v2.get(s_devServerUrl);
        return s_devServerUrl.c_str();
    }
    
    void SetDevServerUrl(const char* url) {
        EnsureDevServerConfigMigrated();
        g_cfg_dev_server_url_v2.set(url ? url : "");
    }
    
    void SetUseDevServer(bool use) {
        EnsureDevServerConfigMigrated();
        g_cfg_use_dev_server_v2.set(use ? 1 : 0);
    }
}

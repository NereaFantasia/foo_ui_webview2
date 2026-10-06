/**
 * WebViewPanel - WebView 面板基类
 * 
 * 提供 WebView2 托管的通用功能，可被以下模式继承：
 * - MainWindow: 独立窗口模式
 * - WebViewDuiElement: DUI 面板模式
 * - WebViewCuiPanel: CUI 面板模式
 */

#pragma once
#include "core/FrontendDirectoryPolicy.h"
#include "panels/PanelConfig.h"

// 前向声明
class WebViewHost;
class BridgeCore;
class SelectionHolder;

namespace fb2k_dnd {
class DndRegistrar;
}

/**
 * 面板模式枚举
 */
enum class WebViewPanelMode {
    Standalone,     // 独立窗口 (MainWindow)
    DuiPanel,       // DUI 面板 (ui_element)
    CuiPanel        // CUI 面板 (uie::window)
};

/**
 * WebViewPanel 基类
 * 
 * 封装 WebView2 的通用初始化、API 注册、消息处理等逻辑。
 * 派生类只需实现窗口创建和特定模式的功能。
 */
class WebViewPanel {
public:
    WebViewPanel();
    virtual ~WebViewPanel();
    
    // 禁止拷贝
    WebViewPanel(const WebViewPanel&) = delete;
    WebViewPanel& operator=(const WebViewPanel&) = delete;
    
    // ========== 生命周期 ==========
    
    /**
     * 初始化 WebView（在 HWND 创建后调用）
     * @param hwnd 宿主窗口句柄
     * @param mode 面板模式
     * @return 是否成功启动初始化
     */
    bool InitializeWebView(HWND hwnd, WebViewPanelMode mode);
    
    /**
     * 销毁 WebView 和相关资源
     */
    void DestroyWebView();
    
    // ========== 访问器 ==========
    
    HWND GetHwnd() const { return hwnd_; }
    WebViewHost* GetWebView() const { return webView_.get(); }
    BridgeCore* GetBridge() const { return bridge_.get(); }
    WebViewPanelMode GetMode() const { return mode_; }
    
    bool IsWebViewReady() const { return webViewReady_ && !webViewProcessDead_; }
    bool IsPanelMode() const { return mode_ != WebViewPanelMode::Standalone; }
    bool HasFocus() const { return hasFocus_; }

    /**
     * WebView2 宿主进程是否已崩溃退出（僵尸态）。
     * 浏览器进程退出后 COM 指针依然非空，但所有调用都会返回
     * ERROR_INVALID_STATE (0x8007139F)；此标记让门卫能拦住死对象。
     */
    bool IsWebViewProcessDead() const { return webViewProcessDead_; }
    
    // 窗口 ID（多窗口系统用）
    const std::string& GetWindowId() const { return windowId_; }
    void SetWindowId(const std::string& id) { windowId_ = id; }
    
    // 面板模板名（面板级别覆盖，空字符串跟随全局设置）
    // 注意：委托到 panelConfig_.templateName，保持向后兼容
    const std::string& GetPanelTemplateName() const { return panelConfig_.templateName; }
    void SetPanelTemplateName(const std::string& name) { panelConfig_.templateName = name; }
    
    // ========== 面板配置（v2）==========
    
    // 获取面板配置（只读）
    const PanelConfig& GetPanelConfig() const { return panelConfig_; }
    
    // 设置面板配置
    void SetPanelConfig(const PanelConfig& config) { panelConfig_ = config; }
    
    // 别名（用于 JS API）
    const PanelConfig& GetConfig() const { return panelConfig_; }
    PanelConfig& GetConfigMutable() { return panelConfig_; }
    
    /**
     * 配置热重载（对比新旧配置，差异化应用）
     * 根据配置变化应用相应的更新，避免不必要的页面重载
     * @param oldCfg 旧配置
     * @param newCfg 新配置
     */
    void ApplyConfig(const PanelConfig& oldCfg, const PanelConfig& newCfg);
    
    // 获取 SelectionHolder（用于 Selection API）
    SelectionHolder* GetSelectionHolder() const { return selectionHolder_.get(); }

    // Null until the WebView is ready, and again after teardown.
    fb2k_dnd::DndRegistrar* GetDndRegistrar() { return dndRegistrar_.get(); }
    
    // ========== WebView 操作 ==========
    
    /**
     * 调整 WebView 大小以适应客户区
     */
    void ResizeWebView();
    
    /**
     * 导航到指定 URL
     */
    bool Navigate(const std::wstring& url);
    
    /**
     * 导航到 HTML 字符串
     */
    bool NavigateToString(const std::wstring& html);
    
    /**
     * 重新加载当前页面
     */
    void Reload();
    
    /**
     * 重新加载前端（模板切换后调用）
     * 重新设置虚拟主机映射并导航到新模板
     */
    void ReloadFrontend();

    /**
     * 宿主最近一次让本实例导航到的页面来源，在提交导航时记下。
     * 页面之后自己导航到别处不会更新它。
     */
    struct FrontendOrigin {
        enum class Kind { None, DevServer, Url, Directory, BuiltInPage };
        Kind kind = Kind::None;
        // kind 为 Directory 时：命中的候选级别、映射的目录与模板名（组件目录这一级没有模板名）
        frontend_directory_policy::Source directorySource = frontend_directory_policy::Source::None;
        std::wstring directory;
        std::string templateName;
        // kind 为 DevServer 或 Url 时：提交导航的地址
        std::wstring url;
    };
    const FrontendOrigin& GetFrontendOrigin() const { return frontendOrigin_; }
    
protected:
    // ========== 可重写的虚函数 ==========
    
    /**
     * WebView 准备就绪时调用（子类可重写以添加额外初始化）
     */
    virtual void OnWebViewReady();
    
    /**
     * 处理来自 JS 的消息（子类可重写以添加自定义处理）
     */
    virtual void HandleWebMessage(const std::wstring& json);
    
    /**
     * 初始导航完成回调（Standalone 窗口重写用于启动可见性收敛）
     */
    virtual void OnNavigationCompleted(bool success);

    /**
     * 页面开始顶层导航（含重载），旧文档即将离开时在 UI 线程调用。同文档导航
     * （锚点、history.pushState）不触发。子类用它丢掉只属于旧页面的登记。
     */
    virtual void OnTopLevelNavigationStarting() {}
    
    /**
     * 页面 ready handshake 信号（Standalone 窗口重写用于启动可见性收敛）
     */
    virtual void OnWindowReadySignal(const std::string& source);
    virtual void OnVisualReadySignal(const std::string& source);
    
    /**
     * 焦点获得时调用（用于 Selection API）
     */
    virtual void OnSetFocus();
    
    /**
     * 焦点丢失时调用（用于 Selection API）
     */
    virtual void OnKillFocus();
    
    /**
     * WebView2 进程崩溃时调用（Visual Hosting 模式下可能只剩空窗口）。
     * 基类空实现；Standalone 窗口可重写以记录诊断证据或在宿主进程级失效时重建。
     * @param failedKind  崩溃的进程类型 (COREWEBVIEW2_PROCESS_FAILED_KIND)
     * @param recovered   WebViewHost 是否已自行恢复（如渲染进程崩溃后 Reload 成功）
     */
    virtual void OnWebViewProcessFailed(COREWEBVIEW2_PROCESS_FAILED_KIND failedKind, bool recovered);
    
    /**
     * WebView 异步初始化失败时调用（controller / environment 创建回调失败）。
     * InitializeWebView 返回的只是"是否成功启动"，真正的失败发生在异步回调里，
     * 因此持有重建/初始化状态的子类必须在此清理，否则状态会永久悬挂。
     * 基类空实现。
     */
    virtual void OnWebViewInitFailed();
    
    /**
     * 按本实例的面板模板与全局配置解析本地前端目录；查找顺序见 core/FrontendDirectoryResolver.h。
     */
    frontend_directory_policy::Resolution ResolveFrontendDirectory() const;

    /**
     * 解析结果对应的页面来源记录（kind 为 Directory），供提交导航后写入 frontendOrigin_。
     */
    FrontendOrigin LocalFrontendOrigin(const frontend_directory_policy::Resolution& resolution) const;

    /**
     * 解析本地前端目录、建虚拟主机映射并导航到 index.html，成功提交时记下页面来源。
     * 没有可加载的目录、映射或导航提交失败时返回 false，当前页面不动。
     */
    bool NavigateToLocalFrontend();
    
    /**
     * 获取内嵌测试页面 HTML
     */
    virtual std::wstring GetTestPageHtml() const;
    
    // ========== 辅助方法 ==========
    
    /**
     * 注册所有 API 到 BridgeCore
     */
    void RegisterAllApis();
    
    /**
     * 初始化所有回调
     */
    void InitializeCallbacks();
    
    /**
     * 加载前端页面（开发服务器或本地文件）
     */
    void LoadFrontendPage();

    /**
     * 导航完成回调的实际处理体（开发服务器回退 + 拖放来源门 + 收敛信号）。
     *
     * 非虚且不放进 OnNavigationCompleted：后者被 MainWindow 与 PopupWindow
     * 覆盖，挂在虚函数上的逻辑会被静默跳过。
     */
    void HandleNavigationCompleted(bool success);

    /**
     * 加载不经开发服务器的常规前端页面：URL 覆盖 → 本地资源 → 内嵌测试页。
     *
     * 既是首次加载的常规路径，也是开发服务器导航失败后的回退路径。
     * 返回是否成功提交了某一次导航；全部失败时调用方要自行宣告导航结束，
     * 否则不会再有 NavigationCompleted 回调，窗口会卡在不可见状态。
     * PopupWindow 覆盖它：弹窗的本地地址要带 windowId 与 route。
     */
    virtual bool LoadFallbackFrontendPage();
    
    /**
     * 设置虚拟主机映射
     */
    bool SetupVirtualHostMapping(const std::wstring& resourcesDir);
    
    /**
     * WebView 是否可安全接受 COM 调用。
     * 统一门卫：宿主存在 + 宿主就绪 + 进程未崩溃。所有会下发 COM 调用的
     * 面板级操作都必须经过此判定，否则崩溃后的僵尸指针会被放行。
     */
    bool IsWebViewOperable() const;

    /**
     * WebView 创建成功后的收尾工作。
     *
     * 从 InitializeWebView 的异步回调里抽出，让该回调保持为纯分发。
     * 非虚：拖放注册必须对每种宿主都生效，而 PopupWindow 覆盖
     * OnWebViewReady() 时不调用基类实现，挂在虚函数上会被静默跳过。
     *
     * generation 为发起本次初始化时的代际值，用于向下传递给拖放注册器。
     */
    void CompleteWebViewInit(uint64_t generation);

protected:
    // 窗口句柄
    HWND hwnd_ = nullptr;
    
    // 面板模式
    WebViewPanelMode mode_ = WebViewPanelMode::Standalone;
    
    // WebView2 宿主
    std::unique_ptr<WebViewHost> webView_;
    
    // 消息桥接（per-instance）
    std::unique_ptr<BridgeCore> bridge_;
    
    // WebView 是否已准备就绪
    bool webViewReady_ = false;
    
    // WebView2 宿主进程已崩溃退出（僵尸态）。由 OnWebViewProcessFailed 在
    // 不可自愈的崩溃上置位，DestroyWebView / 重建成功后清零。
    bool webViewProcessDead_ = false;

    // 最近一次导航是提交给开发服务器的。Navigate 只负责提交，服务器没在监听
    // 要等到 NavigationCompleted 才知道，此时需要回退到本地资源——否则整个
    // 界面停在浏览器错误页，而错误页里没有 bridge，用户无法从前端改回配置。
    bool devServerNavPending_ = false;

    // Bumped on every InitializeWebView and on DestroyWebView, so a late
    // creation callback from a superseded generation can be discarded.
    uint64_t webViewGeneration_ = 0;

    // Sentinel proving the panel is still alive. Generation alone cannot
    // prevent use-after-free, because comparing it already dereferences this.
    struct PanelAlive {};
    std::shared_ptr<PanelAlive> alive_ = std::make_shared<PanelAlive>();

    // Owns the IDropTarget registered on hwnd_. Created after the WebView is
    // ready and destroyed at the start of DestroyWebView.
    std::unique_ptr<fb2k_dnd::DndRegistrar> dndRegistrar_;
    
    // 焦点状态（用于 Selection API）
    bool hasFocus_ = false;
    
    // 选择持有者（用于 Selection API）
    std::unique_ptr<SelectionHolder> selectionHolder_;
    
    // 窗口 ID（多窗口系统用）
    std::string windowId_;
    
    // 面板配置（包含模板名、边框样式、透明背景等 v2 字段）
    PanelConfig panelConfig_;

    // 宿主最近一次提交导航时的页面来源，见 GetFrontendOrigin。
    FrontendOrigin frontendOrigin_;
    
    // 虚拟主机名（编译时加密，运行时解密）
    static std::wstring GetVirtualHostName();
};


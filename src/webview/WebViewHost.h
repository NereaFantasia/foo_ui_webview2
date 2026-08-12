#pragma once
#include "pch.h"

#include "webview/ArtworkRequestInstrumentation.h"
#include "webview/ArtworkRequestLifecycle.h"
#include "webview/ArtworkWorkerQueue.h"
#include <dcomp.h>
#include <atomic>

// ============================================
// WebView2 宿主类
// 负责 WebView2 的创建和管理
// 使用 Visual Hosting 模式实现 DWM 透明效果和边缘调整
// ============================================
class WebViewHost {
public:
    using InitCallback = std::function<void(bool success)>;
    using MessageHandler = std::function<void(const std::wstring& json)>;
    using ScriptCallback = std::function<void(const std::wstring& result)>;
    using PageHealthCallback = std::function<void(bool healthy)>;

    WebViewHost() = default;
    ~WebViewHost();

    // 禁止复制
    WebViewHost(const WebViewHost&) = delete;
    WebViewHost& operator=(const WebViewHost&) = delete;

    // ============================================
    // 初始化
    // ============================================
    
    // 初始化 WebView2 (异步)
    // useVisualHosting=true: Visual Hosting 模式 (独立窗口, DWM 透明)
    // useVisualHosting=false: 标准 Controller 模式 (面板, 参与 Win32 z-order)
    HRESULT Initialize(HWND parentHwnd, InitCallback callback, bool useVisualHosting = true);
    
    // 是否初始化完成
    bool IsReady() const { return webview_ != nullptr; }

    // ============================================
    // 导航
    // ============================================
    
    // 导航到 URL
    HRESULT Navigate(const std::wstring& url);
    
    // 导航到 HTML 字符串
    HRESULT NavigateToString(const std::wstring& html);

    // ============================================
    // JavaScript 交互
    // ============================================
    
    // 执行 JavaScript
    HRESULT ExecuteScript(const std::wstring& script, ScriptCallback callback = nullptr);
    
    // 发送消息到 JavaScript (JSON 格式)
    HRESULT PostMessage(const std::wstring& json);

    // 发送事件消息。投递可靠且无条件：事件语义只有生产者知道，宿主不得按
    // 事件名字符串推断可丢弃性——事件名空间是开放的（port.emit 等允许调用方
    // 传入任意名字），字符串分类必然误判。可再生流由生产侧自行按
    // IsPageHidden() 跳过投递（见 AudioApi 频谱与 playback:timeHighRes）。
    // eventName 仅用于诊断，不参与投递决策。
    HRESULT PostEventMessage(const std::string& eventName, const std::wstring& json);

    // 页面是否处于 hidden（SetVisible(false) 生效中）。
    // put_IsVisible 是使页面 visibilityState=hidden 的执行动作，
    // 最小化/托盘/锁屏三条隐藏路径必经，故此记账即门控权威。
    bool IsPageHidden() const {
        return suspendState_->pageHidden.load(std::memory_order_acquire);
    }
    
    // 设置消息处理器
    void SetMessageHandler(MessageHandler handler);

    // Set focus change callback (for panel:focus/blur events)
    using FocusChangedCallback = std::function<void(bool gotFocus)>;
    void SetFocusChangedCallback(FocusChangedCallback callback);

    // 将键盘焦点移入 WebView 内容（Visual Hosting 模式必需）
    // Visual Hosting (CompositionController) 模式下 WebView2 没有子 HWND，
    // 宿主窗口收到 WM_SETFOCUS 时必须主动调用此方法把焦点交给 Chromium，
    // 否则 Alt+Tab 切回窗口后键盘输入不会进入页面。
    // reason 默认 PROGRAMMATIC；Tab 链遍历可传 NEXT/PREVIOUS。
    // 返回 true 表示成功调用底层 MoveFocus。
    bool MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON reason =
                       COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);

    // Navigation completed callback (启动可见性收敛)
    using NavigationCompletedCallback = std::function<void(bool success)>;
    void SetNavigationCompletedCallback(NavigationCompletedCallback callback);

    // WebView2 进程崩溃回调（Visual Hosting 模式下进程崩溃只剩空窗口的诊断/恢复入口）
    // recovered=true 表示 WebViewHost 已自行尝试恢复（如渲染进程崩溃后 Reload）；
    // recovered=false 表示宿主进程级失效，需要上层重建窗口/WebView。
    // failedKind 透传 COREWEBVIEW2_PROCESS_FAILED_KIND，供上层决策。
    using ProcessFailedCallback =
        std::function<void(COREWEBVIEW2_PROCESS_FAILED_KIND failedKind, bool recovered)>;
    void SetProcessFailedCallback(ProcessFailedCallback callback);

    // ============================================
    // 窗口控制
    // ============================================
    
    // 调整大小
    void Resize(int width, int height);
    void Resize(const RECT& bounds);
    
    // 显示/隐藏。返回底层 Controller 是否确认目标 IsVisible 状态。
    // visible=true 时按 WebView2 官方恢复顺序先 Resume() 再
    // put_IsVisible(TRUE)，避免会话解锁期间的低频恢复竞态。
    bool SetVisible(bool visible);
    
    // Bounds-based 可见性控制（DUI 模式专用）
    // 不使用 put_IsVisible()，改用 put_Bounds({0,0,0,0}) 隐藏
    void SetBoundsVisible(bool visible);

    // 内存占用目标等级（最小化时主动 trim Chromium 可回收内存）
    // low=true  -> COREWEBVIEW2_MEMORY_USAGE_TARGET_LEVEL_LOW
    // low=false -> COREWEBVIEW2_MEMORY_USAGE_TARGET_LEVEL_NORMAL
    // 不要求 IsVisible=false、不暂停脚本，与 Visual Hosting 架构兼容；
    // 运行时不支持 ICoreWebView2_19 时安全 no-op。
    void SetMemoryUsageLow(bool low);

    // 深度挂起：ICoreWebView2_3::TrySuspend，等效 Edge sleeping
    // tab——冻结渲染进程定时器/动画，允许 OS 直接复用 renderer 内存。
    // 前置条件（API 约束）：必须先 SetVisible(false)，可见页面调用会失败
    // （ERROR_INVALID_STATE）。互斥铁律（官方）：TrySuspend 内部自动置
    // MemoryUsageTargetLevel=Low、Resume/put_IsVisible(TRUE) 自动回 Normal，
    // 走深挂起的路径不得再手动 SetMemoryUsageLow。
    // 失败是降级不是错误：接口不可用、调用失败或完成回调 false（sleeping tabs
    // 排除条件，如 DevTools 打开）时回退 SetMemoryUsageLow(true) 并记日志。
    // 代数防竞态：请求携带自增代数，SetVisible(true) 亦使代数前进；
    // 完成回调发现代数过期即放弃写状态，若此刻页面应可见而运行时仍挂起则立即
    // Resume() 兜底（防"最小化→秒恢复"时回调晚于 put_IsVisible(TRUE)）。
    void TrySuspendDeep();

    // 页面当前是否处于挂起态（ICoreWebView2_3::get_IsSuspended 实时查询；
    // 接口不可用或 WebView 未就绪时返回 false）。
    bool IsSuspended() const;
    
    // 开发者工具
    void OpenDevTools();
    
    // 重新加载
    void Reload();

    // 异步确认当前 document 和脚本运行时仍可执行。
    // Controller 存在且 IsVisible=true 并不代表 renderer/page 仍然健康。
    HRESULT ProbePageHealth(const PageHealthCallback& callback);

    // 低频宿主生命周期诊断，追加写入 profile://webview_lifecycle.log。
    // 每条记录立即 flush，文件达到上限后轮转为 .log.1，适用于锁屏后
    // DevTools/page target 均不可用的现场取证。
    void LogLifecycle(const char* event, const std::string& detail = {}) const;
    static void WriteLifecycleLog(const std::string& line);

    // ============================================
    // 获取接口
    // ============================================
    
    ICoreWebView2* GetWebView() const { return webview_.get(); }
    ICoreWebView2Controller* GetController() const { return controller_.get(); }
    ICoreWebView2Environment* GetEnvironment() const { return environment_.get(); }

    // 设置虚拟主机映射（解决 file:// CORS 问题）
    HRESULT SetVirtualHostMapping(const std::wstring& hostName, const std::wstring& folderPath);
    
    // 清除虚拟主机映射（URL 覆盖时需要）
    HRESULT ClearVirtualHostMapping(const std::wstring& hostName);

    // ============================================
    // DWM 透明效果支持
    // ============================================
    
    // 设置 WebView 背景透明度（用于 DWM 效果穿透）
    HRESULT SetBackgroundTransparent(bool transparent);
    
    // 刷新 WebView 渲染（DWM 效果变更后调用）
    void RefreshForDwmEffect();

    // ============================================
    // 缩放控制 (DPI 适配)
    // ============================================
    
    // 设置缩放比例 (1.0 = 100%, 1.5 = 150%, etc.)
    HRESULT SetZoomFactor(double zoomFactor);
    
    // 获取当前缩放比例
    double GetZoomFactor() const;
    
    // 根据 DPI 自动调整缩放
    HRESULT SetZoomForDpi(int dpi);

    // ============================================
    // Visual Hosting 模式 - 鼠标输入转发
    // ============================================
    
    // 处理鼠标消息，返回 true 表示已处理，false 表示应交给 DefWindowProc
    bool HandleMouseMessage(UINT message, WPARAM wParam, LPARAM lParam);

    // ============================================
    // Cursor 控制 (CompositionController 模式)
    //
    // 用法 (在 MainWindow / PopupWindow 的 WM_SETCURSOR 处理中):
    //     case WM_SETCURSOR:
    //         if (LOWORD(lParam) == HTCLIENT && webView_ &&
    //             webView_->ApplyCurrentCursor()) {
    //             return TRUE;
    //         }
    //         break;
    //
    // ApplyCurrentCursor 内部根据 cursorHidden_ 决定 SetCursor(nullptr) 还是
    // SetCursor(currentCursor_)。返回 true 表示已经处理光标,父窗口应直接
    // return TRUE; 返回 false 表示当前没有有效光标 (例如 WebView 未就绪),
    // 父窗口应回退到 DefWindowProc。
    // ============================================
    bool ApplyCurrentCursor();

    // 由 cursor.setHidden API 调用,显式控制客户区光标隐藏/显示。
    // 设为 true 后,所有 HTCLIENT 区域的 WM_SETCURSOR 都会被强制隐藏,
    // 不依赖 Chromium 的 CursorChanged 回调或 CSS cursor:none。
    //
    // 返回 true 表示状态实际发生了变化 (hidden 与之前不同),
    // 调用方可据此决定是否发射 cursor:hiddenChanged 事件。
    bool SetCursorHidden(bool hidden);
    bool IsCursorHidden() const { return cursorHidden_; }
    
    // 获取 CompositionController（用于 Visual Hosting）
    ICoreWebView2CompositionController* GetCompositionController() const { return compositionController_.get(); }

    // CompositionController3 exposes the drag-drop forwarding entry points used
    // by Visual Hosting. QI'd once and cached; null when the runtime predates it.
    ICoreWebView2CompositionController3* GetCompositionController3();

    // Origin of the document currently loaded, as scheme://host[:port].
    //
    // Read live from the WebView rather than from the configured start URL: a
    // window may have navigated elsewhere since creation. Empty when the WebView
    // is not ready or the source cannot be parsed, which callers must treat as
    // untrusted rather than as a match.
    std::wstring GetCurrentOriginNormalized();

    // ============================================
    // CSS app-region 查询 (用于 WM_NCHITTEST)
    // ============================================
    
    // 查询指定点的 CSS app-region 类型
    // 返回值: 0 = 无/客户区, 1 = drag区域, 2 = 不在WebView内
    int GetNonClientRegionAtPoint(POINT clientPt);

private:
    HWND parentHwnd_ = nullptr;
    bool useVisualHosting_ = true;           // Visual Hosting 模式标记
    bool isBackgroundTransparent_ = false;   // 跟踪透明状态

    // WebView 输入焦点状态（用于决定是否让 fb2k 快捷键优先）
    std::atomic<bool> isWebInputFocused_{false};
    
    wil::com_ptr<ICoreWebView2Environment> environment_;
    wil::com_ptr<ICoreWebView2Controller> controller_;
    wil::com_ptr<ICoreWebView2> webview_;
    
    // ============================================
    // Visual Hosting 模式成员
    // ============================================
    wil::com_ptr<ICoreWebView2CompositionController> compositionController_;

    // Cached result of the CompositionController3 QI. The probed flag is set on
    // failure too, so an older runtime is not re-queried on every drag event.
    wil::com_ptr<ICoreWebView2CompositionController3> compositionController3_;
    bool compositionController3Probed_ = false;
    
    // DirectComposition 对象
    wil::com_ptr<IDCompositionDevice> dcompDevice_;
    wil::com_ptr<IDCompositionTarget> dcompTarget_;
    wil::com_ptr<IDCompositionVisual> dcompRootVisual_;
    wil::com_ptr<IDCompositionVisual> dcompWebViewVisual_;
    
    // 光标变化事件 token
    EventRegistrationToken cursorChangedToken_ = {};
    
    // 鼠标捕获状态
    bool isCapturingMouse_ = false;
    bool isTrackingMouse_ = false;

    // ============================================
    // Cursor 状态 (Visual Hosting 模式 - 客户区光标 source of truth)
    //
    // currentCursor_   — Chromium CursorChanged 最近一次提供的光标 handle,
    //                    供 MainWindow/PopupWindow 的 WM_SETCURSOR 拦截使用。
    // cursorHidden_    — 前端通过 cursor.setHidden(true) 显式设置的隐藏标志。
    //                    设为 true 时 WM_SETCURSOR 拦截器会 SetCursor(nullptr),
    //                    用于全屏播放器 idle 等场景 (CSS cursor:none 不可靠)。
    // ============================================
    HCURSOR currentCursor_ = nullptr;
    bool cursorHidden_ = false;
    
    struct SuspendState {
        std::atomic<bool> pageHidden{false};
        std::atomic<uint64_t> generation{0};
        std::atomic<bool> alive{true};
    };

    // pageHidden 由 SetVisible 维护，供生产侧可见性判定（IsPageHidden）使用。
    // 异步 TrySuspend 回调只捕获此共享状态和 COM 引用，不捕获 WebViewHost
    // 裸指针；析构先置 alive=false，使迟到回调安全退出。
    std::shared_ptr<SuspendState> suspendState_ = std::make_shared<SuspendState>();

    MessageHandler messageHandler_;
    FocusChangedCallback focusChangedCallback_;
    NavigationCompletedCallback navigationCompletedCallback_;
    ProcessFailedCallback processFailedCallback_;
    EventRegistrationToken gotFocusToken_ = {};
    EventRegistrationToken lostFocusToken_ = {};
    EventRegistrationToken processFailedToken_ = {};

    // Artwork request lifecycle is independent from suspendState_. The three
    // WebView2 tokens are removed before controller close; navigation starting
    // is the only document-generation source.
    EventRegistrationToken artworkResourceRequestedToken_ = {};
    EventRegistrationToken artworkNavigationStartingToken_ = {};
    EventRegistrationToken navigationCompletedToken_ = {};
    artwork_request::ArtworkRequestLifecycle artworkLifecycle_;
    artwork_instrumentation::ArtworkRequestInstrumentation artworkInstrumentation_;
    DWORD ownerThreadId_ = 0;
    void RemoveArtworkEventHandlers() noexcept;

    // Async worker, owned per WebViewHost instance.
    // Must be destroyed (DrainAll + join) before controller close.
    std::unique_ptr<artwork_worker::ArtworkWorkerQueue> artworkWorkerQueue_;
    // Shared alive-flag captured by async completion lambdas so they skip
    // WebViewHost member access after the host is destroyed.
    std::shared_ptr<std::atomic<bool>> artworkHostAlive_ =
        std::make_shared<std::atomic<bool>>(true);
    
    // 虚拟主机映射 (hostName -> folderPath) + charset 修复
    std::wstring virtualHostName_;
    std::wstring virtualHostFolderPath_;
    bool charsetFixRegistered_ = false;
    void SetupCharsetFixForVirtualHost();
    
    // 设置 WebView
    void SetupWebView();
    void SetupSettings();
    void RegisterMessageHandler();
    void InjectBridgeScript();
    void InjectSdkBridgeScript();
    
    // Visual Hosting 初始化
    HRESULT InitializeDirectComposition();
    void SetupCursorHandling();
    void CleanupDirectComposition();

    // WebView2 进程崩溃处理（注册 add_ProcessFailed + 分级恢复 + 本地文件诊断日志）
    void SetupProcessFailedHandling();
    // 将崩溃诊断写入 profile 目录下的 webview_crash.log（不污染 fb2k console）
    static void WriteCrashLog(const std::string& line);
    
    // 安全: Origin 验证
    bool IsOriginAllowed(ICoreWebView2* webview);

    // 注册一条额外的可信 origin（在静态白名单之外补充）。
    // 用途: 当 panel/window 配置了 urlOverride（指向例如 staging URL）
    //       或 popup 直接指向第三方 URL 时，调用此方法让该来源能调用 invoke。
    // 接受完整 URL 或仅 origin（scheme://host[:port]）；内部会规范化为 origin。
public:
    void AddTrustedOrigin(const std::wstring& urlOrOrigin);

private:
    static std::wstring NormalizeToOrigin(const std::wstring& urlOrOrigin);
    static bool OriginPrefixMatch(const std::wstring& origin, const std::wstring& prefix);

    std::vector<std::wstring> extraTrustedOrigins_;
    std::mutex extraTrustedOriginsMutex_;
    
    // 修复: UTF-8 编码
    void InjectCharsetFixScript();
    
    // fb2k:// 协议支持 - 高性能封面加载
    void SetupCustomProtocol();
    
    // 获取用户数据目录
    static std::wstring GetUserDataPath();
    
    // 使用预热环境创建 Controller
    void CreateControllerWithEnvironment(
        ICoreWebView2Environment* env,
        InitCallback callback,
        bool useCompositionController);
};

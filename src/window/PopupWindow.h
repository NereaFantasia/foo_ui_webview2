#pragma once
#include "pch.h"
#include "core/WebViewPanel.h"
#include "window/WindowChromeApplier.h"
#include "window/WindowChromeState.h"
#include "window/WindowShellBase.h"
#include <functional>

// ============================================
// 弹出窗口类 - 多窗口系统
// 继承 WebViewPanel 获得完整的 WebView2 + BridgeCore + 全部 API 能力
// ============================================
class PopupWindow : public WebViewPanel, public WindowShellBase {
public:
    struct DragRegion {
        int x, y, width, height;
    };

    struct PopupBehavior {
        bool showInTaskbar = false;
        bool showInAltTab = false;
        bool keepVisibleOnShowDesktop = false;
        bool allowMinimize = false;
        std::string owner = "none"; // "none" | "main"
        bool noActivate = false;
    };

    struct PopupBackdropPolicy {
        std::string activeEffect = "inherit";   // inherit | none | mica | acrylic
        std::string inactiveEffect = "inherit"; // inherit | none | mica | acrylic
        bool darkMode = true;
        bool reapplyOnActivate = false;
    };

    struct CreateParams {
        std::string url;           // 相对路由，如 "/mini-player"
        int width = 400;
        int height = 300;
        int x = CW_USEDEFAULT;     // 屏幕坐标，CW_USEDEFAULT 居中
        int y = CW_USEDEFAULT;
        std::string title = "";
        bool resizable = true;
        bool alwaysOnTop = false;
        bool showInTaskbar = false; // true = 独立任务栏按钮
        int minWidth = 200;
        int minHeight = 150;
        int maxWidth = 0;          // 0 = 无限制
        int maxHeight = 0;
        bool frame = true;         // false = 无标题栏（自定义标题栏由前端实现）
        bool transparent = false;  // true = WebView 背景透明（DWM 效果穿透）
        bool clickThrough = false; // true = 鼠标穿透窗口（桌面歌词等场景）
        bool beforeClose = false;  // true = 关闭时走异步 beforeClose 流程，false = 立即关闭

        // 扩展策略（向后兼容）
        bool hasShowInTaskbar = false;
        bool hasProfile = false;
        bool hasBehavior = false;
        bool hasBackdropPolicy = false;
        std::string profile = "";      // "standard" | "miniPlayer" | "desktopLyrics"
        json behavior = json::object();
        json backdropPolicy = json::object();
    };
    
    PopupWindow();
    ~PopupWindow();
    
    // 创建弹出窗口
    // @param params 创建参数
    // @param windowId 窗口 ID（由 WindowManager 分配）
    // @param ownerHwnd 父窗口句柄（MainWindow 的 HWND）
    HWND Create(const CreateParams& params, const std::string& windowId, HWND ownerHwnd);
    
    // 销毁窗口（跳过 beforeClose，直接销毁）
    void Destroy();
    
    // 获取创建参数
    const CreateParams& GetCreateParams() const { return createParams_; }

    // 获取策略信息
    json GetPopupBehaviorInfo() const;
    // GetBackdropPolicyInfo() — declared in WindowShellBase override section below
    json GetResolvedBehavior() const;
    json GetResolvedBackdropPolicy() const;
    const std::string& GetProfile() const { return profile_; }

    // ========== 菜单覆盖面支持（self-drawn-menu，显式接口）==========
    // 抑制 window:popupOpened / window:popupClosed 生命周期广播（菜单覆盖面专用，
    // 避免污染用户窗口事件流；普通 popup 默认 false，零影响）。
    void SetSuppressLifecycleBroadcast(bool v) { suppressLifecycleBroadcast_ = v; }
    // 设置 dismiss 回调（菜单覆盖面专用）：失焦(WM_ACTIVATEAPP)/Esc 时触发请求关闭。
    // 默认空 -> 普通 popup 行为不变（显式接口，不依赖 profile 字符串隐式分支）。
    void SetMenuDismissCallback(std::function<void(const std::string&)> cb) { menuDismissCallback_ = std::move(cb); }
    bool UpdateCompatibilityBackdropEffect(const std::optional<std::string>& effect,
                                           const std::optional<bool>& darkMode,
                                           bool clearBlur, bool forceRefresh = true);
    bool UpdateCompatibilityBlur(bool enabled, bool forceRefresh = true);
    bool UpdateCompatibilityDarkMode(bool enabled, bool forceRefresh = true);
    bool UpdateCompatibilityTransparentBackground(bool transparent, bool forceRefresh = true);

    // 运行时更新策略
    bool UpdatePopupBehavior(const std::string& profile, bool hasProfile,
                             const json& behaviorPatch, bool hasBehaviorPatch,
                             std::string& error);
    bool UpdateBackdropPolicy(const json& backdropPolicyPatch, std::string& error);
    
    // ========== beforeClose 异步关闭机制 ==========
    
    // 前端取消关闭
    void CancelClose();
    
    // 前端确认关闭
    void ConfirmClose();
    
    // 是否正在等待关闭确认
    bool IsPendingClose() const { return pendingClose_; }
    
    // ========== 运行时标题栏控制 ==========
    
    // 动态切换无标题栏模式
    // @param frameless true=移除标题栏, false=恢复标题栏
    void SetFrameless(bool frameless);
    
    // 获取当前是否无标题栏
    bool IsFrameless() const override { return !createParams_.frame; }

    // ========== 鼠标穿透控制 ==========
    
    // 设置鼠标穿透状态（运行时动态切换）
    void SetClickThrough(bool enabled);
    
    // 获取当前鼠标穿透状态
    bool IsClickThrough() const { return clickThrough_; }

    // ========== 鼠标穿透交互热区 ==========

    // 设置 click-through 交互热区（物理像素坐标，由 API 层转换）
    void SetClickThroughExcludeRegions(const std::vector<RECT>& regions);
    // 清空 click-through 交互热区
    void ClearClickThroughExcludeRegions();
    // 判断物理像素坐标是否位于任一交互热区内
    bool IsPointInExcludeRegion(POINT ptPhysical) const;
    // 获取当前交互热区数量
    size_t GetExcludeRegionCount() const { return clickThroughExcludeRegions_.size(); }

    // ========== WindowShellBase 实现 ==========
    std::string GetShellWindowId() const override;
    WindowKind GetWindowKind() const override;
    HWND GetShellHwnd() const override;
    WindowShellCapabilities GetCapabilities() const override;
    WindowShellSnapshot GetShellSnapshot() const override;
    json GetBackdropPolicyInfo() const override;
    bool PatchBackdropPolicy(const json& policyPatch, std::string& error) override;
    void PatchFrameless(bool frameless) override;
    void RefreshChrome() override;
    bool PatchCompatibilityBackdrop(const std::optional<std::string>& effect,
                                    const std::optional<bool>& darkMode,
                                    bool clearBlur, bool forceRefresh = true) override;
    bool PatchCompatibilityBlur(bool enabled, bool forceRefresh = true) override;
    bool PatchCompatibilityDarkMode(bool enabled, bool forceRefresh = true) override;
    bool PatchCompatibilityTransparency(bool transparent, bool forceRefresh = true) override;
    bool IsFullscreen() const override;
    void NotifyFullscreenChanged(bool isFullscreen) override;
    void SetFullscreenFlag(bool isFullscreen) override;
    bool IsActive() const override;

    // ========== 标题栏/拖拽区域 ==========
    static constexpr int DEFAULT_TITLEBAR_HEIGHT = 32;
    int GetTitlebarHeight() const { return titlebarHeight_; }
    void SetTitlebarHeight(int height);
    void SetDragRegions(const std::vector<DragRegion>& regions);
    void ClearDragRegions();
    void SetNoDragRegions(const std::vector<DragRegion>& regions);
    void ClearNoDragRegions();

protected:
    void OnWebViewReady() override;
    void OnNavigationCompleted(bool success) override;
    void OnWindowReadySignal(const std::string& source) override;

private:
    CreateParams createParams_;
    HWND ownerHwnd_ = nullptr;
    bool isMaximized_ = false;  // 用于 WM_NCCALCSIZE 最大化边距调整
    bool isMinimized_ = false;
    bool isActive_ = true;
    bool restoreFromMinimizePending_ = false;
    int titlebarHeight_ = DEFAULT_TITLEBAR_HEIGHT;
    std::vector<DragRegion> dragRegions_;
    std::vector<DragRegion> noDragRegions_;
    mutable std::mutex regionsMutex_;

    bool framelessDwmApplied_ = false;  // ApplyFramelessState 短路守卫（与 MainWindow 对齐）
    bool lastFrameMarginExtended_ = false; // frame:true 时是否已应用 1px DWM 帧扩展
    bool usePopupStyle_ = false;           // frame=false + transparent + no-effect → WS_POPUP 模式
    bool clickThrough_ = false;            // 鼠标穿透运行时状态（请求态）
    bool overlayIntent_ = false;           // 曾进入 clickThrough 模式（overlay 意图，永不激活）
    HWND lastForegroundBeforeClick_ = nullptr;  // WM_MOUSEACTIVATE 时保存的前台窗口（用于还原）
    // 自定义拖拽循环（仅 overlay popup 启用：noActivate / overlayIntent）
    // 普通 popup 走 SendMessage(WM_NCLBUTTONDOWN HTCAPTION) → DefWindowProc → SC_MOVE，
    // 但 SC_MOVE modal loop 内部会调 SetForegroundWindow(self) 强制激活，
    // 即使 WS_EX_NOACTIVATE 也会被程序调用绕过；popup 因 WS_EX_NOACTIVATE 又被踢出后，
    // 系统选择同进程下一个可激活窗口 = main，导致主 fb2k 窗口被前置覆盖外部应用。
    // overlay popup 改用 SetCapture + WM_MOUSEMOVE + SetWindowPos NOACTIVATE 自定义拖拽，
    // 完全绕过 DefWindowProc 的 SC_MOVE 路径，根除激活链。
    bool isOverlayDragging_ = false;
    POINT overlayDragStartCursor_ {};   // 拖拽起点光标屏幕坐标
    POINT overlayDragStartWindow_ {};   // 拖拽起点窗口左上角屏幕坐标
    bool clickThroughEffective_ = false;     // 当前原生样式是否处于穿透（生效态）
    // WebView2 子 HWND 子类化（拦截 Chromium 内部的 WM_MOUSEACTIVATE）
    // 当点击 WebView2 内可交互元素时，WM_MOUSEACTIVATE 发到的是 Chromium 子 HWND
    // （Chrome_WidgetWin_*），而非 popup 顶层窗口。popup 顶层已有的 MA_NOACTIVATE
    // 拦截器无法收到这条消息，Chromium 默认返回 MA_ACTIVATE 导致激活链绕过 NOACTIVATE。
    // 子类化子 HWND 后，在该层也拦截 WM_MOUSEACTIVATE → MA_NOACTIVATE，根除激活链。
    // 注意：Visual Hosting 模式下 WebView2 不创建子 HWND，子类化拦截不适用，
    // 改用 MoveFocusRequested 拦截。
    HWND webViewChildHwnd_ = nullptr;       // WebView2 内部最外层子 HWND
    UINT_PTR webViewSubclassId_ = 0;        // SetWindowSubclass 的 uIdSubclass
    static LRESULT CALLBACK WebViewSubclassProc(HWND hwnd, UINT msg,
        WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
    void InstallWebViewSubclass();           // 在 OnWebViewReady 中调用（仅标准 Controller 模式）
    void RemoveWebViewSubclass();            // 在 OnDestroy 中调用

    // WebView2 MoveFocusRequested 拦截（阻止 Chromium 内部焦点移交）
    // Visual Hosting 模式下，Chromium 处理 WM_LBUTTONDOWN 时会触发焦点转移请求，
    // 导致 SetForegroundWindow → 主窗口被激活。拦截 MoveFocusRequested 并标记 Handled，
    // 阻止 WebView2 的默认焦点移交行为。
    EventRegistrationToken moveFocusRequestedToken_ = {};
    void InstallMoveFocusGuard();            // 在 OnWebViewReady 中调用
    void RemoveMoveFocusGuard();             // 在 OnDestroy 中调用

    std::vector<RECT> clickThroughExcludeRegions_;  // 交互热区（物理像素，已按 DPI 缩放）
    bool wasHovering_ = false;               // hover 去重标志（仅在状态变化时发射事件）
    static constexpr UINT_PTR HOVER_TIMER_ID = 3;
    static constexpr DWORD HOVER_POLL_MS = 16;  // ~60fps（原 80ms，缩短以减少非热区可交互窗口期）

    // 策略状态
    bool hasProfile_ = false;
    std::string profile_ = "legacy";
    // 菜单覆盖面支持成员（self-drawn-menu）
    bool suppressLifecycleBroadcast_ = false;
    std::function<void(const std::string&)> menuDismissCallback_;
    json behaviorOverrides_ = json::object();
    json backdropPolicyOverrides_ = json::object();
    WindowChromeCompatibilityOverrides chromeCompatibilityOverrides_;
    ResolvedWindowChromeState resolvedChromeState_;
    PopupBehavior resolvedBehavior_;
    PopupBackdropPolicy resolvedBackdropPolicy_;
    std::string lastBackdropMode_;
    std::string lastBackdropEffect_;
    bool chromeBackdropBroadcastReady_ = false;
    
    // beforeClose 状态
    bool pendingClose_ = false;
    static constexpr UINT_PTR CLOSE_TIMER_ID = 1;
    static constexpr DWORD CLOSE_TIMEOUT_MS = 3000;
    
    // 启动 reveal 状态机
    bool startupRevealPending_ = true;
    bool startupNavigationCompleted_ = false;
    bool startupReadySignaled_ = false;
    bool startupRevealCommitted_ = false;
    bool startupRevealSettling_ = false;
    bool needsAuthoritativeChromeReapply_ = false;
    static constexpr UINT_PTR STARTUP_REVEAL_TIMER_ID = 2;
    static constexpr DWORD STARTUP_REVEAL_TIMEOUT_MS = 1500;
    static constexpr UINT WM_DEFERRED_CHROME_REAPPLY = WM_APP + 0x53;
    static constexpr UINT WM_DEFERRED_SURFACE_CONVERGE = WM_APP + 0x54;
    
    // 窗口类
    static constexpr wchar_t CLASS_NAME[] = L"foo_ui_webview2_PopupWindow";
    static bool classRegistered_;
    static bool RegisterWindowClass();
    
    // 窗口过程
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT OnNcHitTest(LPARAM lParam);
    bool HandleDragAreaMouse(UINT msg, WPARAM wParam, LPARAM lParam);
    
    // 消息处理
    void OnCreate();
    void OnClose();
    void OnDestroy();
    void OnSize(int width, int height);
    void OnActivate(WPARAM wParam);
    
    // 构建导航 URL
    std::wstring BuildNavigationUrl() const;

    // 策略处理
    void InitializePolicyState();
    WindowChromeBaseState BuildChromeBaseState() const;
    WindowChromeStandardState BuildChromeStandardState() const;
    WindowChromeDerivedState BuildChromeDerivedState() const;
    WindowChromeApplyHooks BuildChromeApplyHooks();
    bool ApplyResolvedChrome(bool forceRefresh);
    void ApplyFramelessState(bool frameless);
    void ResolvePolicies();
    void ApplyResolvedBehaviorToStyles(DWORD& style, DWORD& exStyle) const;
    void ApplyResolvedBehaviorRuntime();
    bool ApplyBackdropForActivation(bool active, bool forceRefresh);
    void ApplyBackdropEffect(const std::string& effect, bool forceRefresh);
    bool CanBroadcastBackdropStateChanged() const;
    void BroadcastBehaviorChanged() const;
    void BroadcastBackdropStateChanged(bool active, const std::string& mode,
                                       const std::string& effect) const;
    void TraceWindowPhase(const char* phase,
                          const char* active = nullptr,
                          const char* forceRefresh = nullptr,
                          const char* effect = nullptr,
                          const char* previousEffect = nullptr,
                          const char* extra = nullptr) const;
    // ActivationEvidence 诊断日志：与 MainWindow.cpp 中 [ActivationEvidence] 同口径，
    // 用于追踪 WebView2 mousedown → SetForegroundWindow → 主窗口被前置的焦点链。
    // 仅对 noActivate / overlayIntent / clickThrough 类 popup 输出，避免日志噪声。
    void EmitActivationEvidence(const char* phase, const std::string& extra = std::string()) const;
    const char* GetTraceEffect(bool active) const;
    json BuildResolvedBehaviorJson() const;
    json BuildResolvedBackdropPolicyJson() const;
    static std::string NormalizeProfile(const std::string& profile);
    static std::string NormalizeEffect(const std::string& effect, bool allowSystem, const std::string& fallback);
    bool IsPointInDragRegion(int clientX, int clientY) const;
    bool IsPointInNoDragRegion(int clientX, int clientY) const;
    void UpdateClickThroughEffective();  // 根据光标位置刷新有效穿透样式
    void TryCommitStartupReveal();
    void ArmStartupFallbackTimer();
    void RefreshStartupRevealSurface();
    bool ReapplyNativeChromeAfterStartupReveal();
    bool EnsureAuthoritativeNativeChrome(const char* reason);
    bool EnsureSurfaceConvergedAfterNativeFrame(const char* reason);
    void LogSurfaceDiagnostics(const char* phase) const;
    void MaybeCommitStartupRevealAfterChromeReady(const char* reason);
    bool IsStartupRevealChromeReady() const;
};

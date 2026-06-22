#pragma once
#include "pch.h"
#include "core/WebViewPanel.h"
#include "window/StartupPresentationCoordinator.h"
#include "window/WindowChromeApplier.h"
#include "window/WindowChromeState.h"
#include "window/WindowShellBase.h"

// ============================================
// 标题栏区域定义
// ============================================
struct TitlebarDragRegion {
    int x, y, width, height;
};

// ============================================
// 继承 WebViewPanel 获得 WebView2 通用功能
// ============================================
class MainWindow : public WebViewPanel, public WindowShellBase {
public:
    struct BackdropPolicy {
        std::string activeEffect = "inherit";   // inherit | none | mica | acrylic
        std::string inactiveEffect = "inherit";  // inherit | none | mica | acrylic | system
        bool darkMode = true;
        bool reapplyOnActivate = false;
    };

    MainWindow();
    ~MainWindow();

    // 创建窗口
    HWND Create(HWND parent);
    
    // 销毁窗口
    void Destroy();
    
    // 注: GetHwnd() 和 GetWebView() 继承自 WebViewPanel
    
    // ========== Cold-start reveal 可见性控制（背景模式协同） ==========
    // 在 Create() 之前调用，提示 cold-start reveal 流程在最终 ShowWindow
    // 阶段使用 SW_HIDE 还是恢复的 showCmd（默认 true=按 savedShowCmd_ 显示）。
    // 用途：BackgroundService 在 lastVisible=false 时让窗口从启动到关闭全程
    // 不可见，避免 SW_SHOWMINNOACTIVE/hideTimer hack 与 cold-start reveal 的
    // 冲突，保持物理 IsWindowVisible 与 cfg_background_window_visible 一致。
    void SetStartupVisibility(bool visible) { startupVisibilityHint_ = visible; }
    bool GetStartupVisibility() const { return startupVisibilityHint_; }

    // Re-present WebView/DWM surfaces after a runtime SW_HIDE -> SW_SHOW restore.
    bool RestoreSurfaceAfterHidden(const char* reason);
    
    // ========== 标题栏配置 ==========
    
    // 默认标题栏高度
    static constexpr int DEFAULT_TITLEBAR_HEIGHT = 32;
    
    // 获取/设置标题栏高度
    int GetTitlebarHeight() const { return titlebarHeight_; }
    void SetTitlebarHeight(int height);
    
    // 设置可拖拽区域（从 JavaScript 调用）
    void SetDragRegions(const std::vector<TitlebarDragRegion>& regions);
    void ClearDragRegions();
    
    // 设置非拖拽区域（按钮等交互元素）
    void SetNoDragRegions(const std::vector<TitlebarDragRegion>& regions);
    void AddNoDragRegion(const TitlebarDragRegion& region);
    void ClearNoDragRegions();
    
    // 获取系统按钮宽度（用于 JS 布局）
    int GetCaptionButtonWidth() const;
    int GetCaptionButtonsWidth() const; // 三个按钮总宽度
    
    // 刷新 DWM 背景效果（偏好设置更改后热重载）
    void RefreshBackdropEffect();
    // GetBackdropPolicyInfo() — declared in WindowShellBase override section below
    bool SetBackdropPolicy(const json& policyPatch, std::string& error);
    bool UpdateCompatibilityBackdropEffect(const std::optional<std::string>& effect,
                                           const std::optional<bool>& darkMode,
                                           bool clearBlur, bool forceRefresh = true);
    bool UpdateCompatibilityBlur(bool enabled, bool forceRefresh = true);
    bool UpdateCompatibilityDarkMode(bool enabled, bool forceRefresh = true);
    bool UpdateCompatibilityTransparentBackground(bool transparent, bool forceRefresh = true);
    
    // 显示上下文菜单（供 API 调用）
    void ShowContextMenu(int screenX, int screenY);
    
    // ========== 窗口尺寸限制 ==========
    
    // 设置/获取最小尺寸
    void SetMinSize(int width, int height);
    void GetMinSize(int& width, int& height) const { width = minWidth_; height = minHeight_; }
    
    // 设置/获取最大尺寸 (0 表示无限制)
    void SetMaxSize(int width, int height);
    void GetMaxSize(int& width, int& height) const { width = maxWidth_; height = maxHeight_; }
    
    // 设置/获取是否可调整大小
    void SetResizable(bool resizable);
    bool IsResizable() const { return resizable_; }
    
    // ========== 运行时标题栏控制 ==========
    
    // 动态切换无标题栏模式
    // @param frameless true=移除标题栏, false=恢复标题栏
    void SetFrameless(bool frameless);
    
    // 获取当前是否无标题栏
    bool IsFrameless() const override { return frameless_; }
    
    // ========== 窗口圆角 ==========
    
    // 设置窗口圆角模式
    // mode: "default" | "none" | "round" | "small"
    void SetCornerPreference(const std::string& mode);
    std::string GetCornerPreference() const { return cornerPreference_; }

    // ========== 期望置顶状态守护 ==========

    // 登记主窗口「应有」的 WS_EX_TOPMOST 状态（capability 级兜底的参照真相）。
    // 仅允许两类显式用户意图路径改写：window.setAlwaysOnTop / window.toggleAlwaysOnTop
    // （见 WindowApi.cpp）。全屏的临时 HWND_TOPMOST 不经此处——守护在全屏期间暂停。
    // WM_WINDOWPOSCHANGED 中发现实际位与期望不符即矫正（见 WndProc）。
    void SetExpectedTopmost(bool expected);

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

protected:
    // ========== 重写基类虚函数 ==========
    void OnWebViewReady() override;
    void OnNavigationCompleted(bool success) override;
    void OnWindowReadySignal(const std::string& source) override;
    void OnVisualReadySignal(const std::string& source) override;
    void OnSetFocus() override;
    void OnKillFocus() override;
    void OnWebViewProcessFailed(COREWEBVIEW2_PROCESS_FAILED_KIND failedKind, bool recovered) override;
    std::wstring GetTestPageHtml() const override;
    
private:
    HWND parentHwnd_ = nullptr;
    // 注: hwnd_, webView_, bridge_ 继承自 WebViewPanel
    
    // 窗口类名
    static constexpr wchar_t CLASS_NAME[] = L"foo_ui_webview2_MainWindow";
    static bool classRegistered_;
    
    // ========== 标题栏状态 ==========
    int titlebarHeight_ = DEFAULT_TITLEBAR_HEIGHT;
    int resizeBorderWidth_ = 8;  // 边框调整大小区域
    int captionButtonWidth_ = 46; // 单个系统按钮宽度
    
    // 可拖拽区域（由 JS 设置）
    std::vector<TitlebarDragRegion> dragRegions_;
    std::vector<TitlebarDragRegion> noDragRegions_;
    mutable std::mutex regionsMutex_;
    
    // 窗口状态
    bool isMaximized_ = false;
    bool isMinimized_ = false;
    bool isActive_ = true;
    bool isFullscreen_ = false;  // per-window fullscreen state (replaces g_isFullscreen as authority)
    bool interactiveSizingActive_ = false;  // instrumentation-only: tracks native interactive sizing loop state
    unsigned int interactiveSizingSessionId_ = 0;  // increments on each WM_ENTERSIZEMOVE for evidence correlation
    // 注: webViewInitialized_ 替换为基类的 IsWebViewReady()
    bool resizable_ = true;  // 是否允许调整大小
    bool frameless_ = true;  // 默认无标题栏（主窗口始终以 frameless 模式启动）
    bool framelessDwmApplied_ = false;  // ApplyFramelessState 短路守卫（v1.1.19 对齐）
    bool hasBroadcastWindowState_ = false;
    bool lastBroadcastIsMaximized_ = false;
    bool lastBroadcastIsMinimized_ = false;
    bool lastBroadcastIsActive_ = true;
    bool lastBroadcastIsFullscreen_ = false;

    // ===== 后台挂起（锁屏 / 被完全覆盖）→ 隐藏 WebView 省内存 =====
    // 与最小化/托盘各自的可见性管理并行；用位掩码避免多个原因互相误恢复
    // （例如同时锁屏+被覆盖，解锁后仍被覆盖则保持隐藏）。
    enum BgSuspendReason : unsigned {
        kBgSuspendLocked  = 1u << 0,   // 会话锁定（锁屏）
        kBgSuspendCovered = 1u << 1,   // 被其它窗口完全覆盖（预留，后续实现）
    };
    unsigned bgSuspendReasons_ = 0;
    // 置位首个原因 → SetVisible(false)+Low；清除最后一个原因 → 经
    // RestoreSurfaceAfterHidden 复原（SetVisible(true)+nudge+Normal）。
    // 仅在“普通可见态”（非最小化、非托盘隐藏）下改动可见性。
    void SetBgSuspend(unsigned reason, bool active, const char* why);

    // 被完全覆盖（梯度 C）：保守判定 + 轮询恢复
    bool IsMainWindowFullyCovered() const;  // 前台窗口同屏且几何完全包含本窗口（不透明/非 cloaked）
    void ScheduleCoverReevaluation();       // 防抖：激活/位置变化后稍后评估一次
    void EvaluateCoverState();              // 评估覆盖并置/清 kBgSuspendCovered + 管理轮询
    void ClearCoverSuspend();               // 清覆盖记账并停轮询（不改可见性；交回最小化/托盘）

    // overlay 误激活防御（见 OnActivate）：一次性撤销主窗口被 overlay 交互激活的状态，
    // 后续同一窗口期内的激活消息被抑制，避免 SetForegroundWindow → WM_ACTIVATEAPP 震荡。
    bool overlayRedirectInProgress_ = false;   // 正在执行 redirect，后续激活消息静默跳过
    unsigned long long lastOverlayRedirectTick_ = 0;  // 最近一次 redirect 时间（用于超时清除）

    // 期望置顶状态（见 SetExpectedTopmost / WM_WINDOWPOSCHANGED 守护）。
    // 默认 false：主窗口启动时从不置顶；之后仅随用户显式 alwaysOnTop 意图变化。
    bool expectedTopmost_ = false;

    // ========== 主窗口 chrome 相位 ==========
    enum class MainChromePhase {
        Bootstrap,    // 启动期：OnCreate DWM 直写路径
        SteadyState   // 稳态：shared chrome apply 接管
    };
    MainChromePhase chromePhase_ = MainChromePhase::Bootstrap;
    
    // ========== 启动 reveal 状态机 ==========
    bool startupRevealPending_ = true;       // 仍处于启动隐藏期
    bool startupNavigationCompleted_ = false; // 初始导航已完成
    bool startupReadySignaled_ = false;       // 已收到页面 ready handshake
    bool startupRevealCommitted_ = false;     // 已真正显示窗口
    bool startupRevealSettling_ = false;      // ShowWindow 后仍在等待 native chrome settle
    StartupPresentationCoordinator startupPresentationCoordinator_;
    bool startupSurfaceAuthorityPending_ = true; // startup reveal 期间由 TryCommitStartupReveal 持有 surface bounds 提交权
    bool startupSurfaceCommitPending_ = true;    // startup reveal 后需要一次 post-show first-present 提交
    RECT pendingStartupSurfaceBounds_ = { 0, 0, 0, 0 };
    bool hasPendingStartupSurfaceBounds_ = false;
    bool startupSurfaceFirstPresentCommitted_ = false;
    bool startupDeferredSurfaceConvergePending_ = false; // 一次性 startup-post-first-present converge 门控
    bool needsAuthoritativeChromeReapply_ = false; // 下一消息周期需要 DWM 完整重置
    bool startupImmediateShowMode_ = false;        // 默认 cold-start reveal 模式
    bool startupChromeLayerSuppressed_ = false;     // Legacy: immediate-show 旧实验门控，cold-start reveal 默认不启用
    int savedShowCmd_ = SW_SHOW;              // 延迟 reveal 的 showCmd
    bool startupVisibilityHint_ = true;       // BackgroundService 协同：false=cold-start reveal 末尾用 SW_HIDE 替代 savedShowCmd_
    static constexpr UINT_PTR STARTUP_REVEAL_TIMER_ID = 100;
    static constexpr DWORD STARTUP_REVEAL_TIMEOUT_MS = 1500;
    static constexpr UINT WM_DEFERRED_CHROME_REAPPLY = WM_APP + 0x60;
    static constexpr UINT WM_DEFERRED_SURFACE_CONVERGE = WM_APP + 0x61;
    static constexpr UINT_PTR STARTUP_SETTLEMENT_PROBE_T100_TIMER_ID = 110;
    static constexpr UINT_PTR STARTUP_SETTLEMENT_PROBE_T250_TIMER_ID = 111;
    static constexpr UINT_PTR STARTUP_SETTLEMENT_PROBE_T500_TIMER_ID = 112;
    static constexpr UINT_PTR SIZE_MAXIMIZED_PROBE_T100_TIMER_ID = 120;
    static constexpr UINT_PTR SIZE_RESTORED_PROBE_T100_TIMER_ID = 121;
    // 最小化→恢复后强制一次 DComp surface 重呈现，兜底 occlusion 节流恢复失败（空窗口）
    static constexpr UINT_PTR RESTORE_SURFACE_CONVERGE_TIMER_ID = 122;
    static constexpr DWORD RESTORE_SURFACE_CONVERGE_DELAY_MS = 120;
    // 被完全覆盖检测（梯度 C + 轮询恢复）
    static constexpr UINT_PTR COVER_REEVAL_TIMER_ID = 123;     // 激活/位置变化后的防抖评估
    static constexpr DWORD    COVER_REEVAL_DELAY_MS = 600;
    static constexpr UINT_PTR COVER_POLL_TIMER_ID = 124;       // 覆盖态周期重检（检测重新露出）
    static constexpr DWORD    COVER_POLL_INTERVAL_MS = 1000;
    static constexpr UINT_PTR DEFERRED_BACKDROP_TIMER_ID = 130;
    static constexpr DWORD DEFERRED_BACKDROP_DELAY_MS = 200;
    static constexpr UINT_PTR BACKDROP_KICK_EXIT_TIMER_ID = 131;
    static constexpr DWORD BACKDROP_KICK_EXIT_DELAY_MS = 50;
    static constexpr UINT_PTR SHORT_REVEAL_TIMER_ID = 140;
    static constexpr DWORD SHORT_REVEAL_TIMEOUT_MS = 250;
    
    // 窗口尺寸限制
    int minWidth_ = 400;
    int minHeight_ = 300;
    int maxWidth_ = 0;   // 0 表示无限制
    int maxHeight_ = 0;  // 0 表示无限制
    
    // 窗口圆角偏好
    std::string cornerPreference_ = "default";  // default, none, round, small

    // DWM 背景策略（活跃/失焦）
    json backdropPolicyOverrides_ = json::object();
    WindowChromeCompatibilityOverrides chromeCompatibilityOverrides_;
    ResolvedWindowChromeState resolvedChromeState_;
    BackdropPolicy resolvedBackdropPolicy_;
    std::string lastBackdropMode_;
    std::string lastBackdropEffect_;
    bool chromeBackdropBroadcastReady_ = false;
    bool deferredBackdropPending_ = false;  // 延迟 backdrop 写入门控
    
    // 注册窗口类
    static bool RegisterWindowClass();
    
    // 窗口过程
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    
    // 消息处理
    void OnCreate();
    void OnClose();
    void OnDestroy();
    void OnSize(int width, int height);
    void OnPaint();
    void OnContextMenu(int x, int y);
    void OnCommand(WORD cmdId);
    
    // ========== 客户区扩展到标题栏 ==========
    LRESULT OnNcCalcSize(WPARAM wParam, LPARAM lParam);
    LRESULT OnNcHitTest(int screenX, int screenY);
    void OnActivate(WPARAM wParam);
    void OnActivateApp(bool active);
    void OnDpiChanged(WPARAM wParam, LPARAM lParam);
    
    // DWM 效果
    void EnableMicaEffect();
    void ExtendFrameIntoClientArea();
    void UpdateDpiDependentSizes();
    // DwmDefWindowProc 分阶段路由
    bool ShouldRouteToDwmDefWindowProc(UINT msg) const;
    // 启动 reveal 辅助
    void TryCommitStartupReveal();
    void ArmStartupFallbackTimer();
    void RefreshStartupRevealSurface();
    bool ReapplyNativeChromeAfterStartupReveal();
    bool EnsureAuthoritativeNativeChrome(const char* reason);
    bool EnsureSurfaceConvergedAfterNativeFrame(const char* reason);
    void FinalizeStartupRevealSettlement();
    void CaptureRuntimeDomProbe(const std::string& phase, int scheduledDelayMs = -1);
    void ScheduleFinalizeStartupRuntimeProbes();
    void ScheduleManualResizeRuntimeProbes(const char* sizeType);
    void LogSurfaceDiagnostics(const char* phase) const;
    void EmitInteractiveResizeEvidence(const char* phase,
                                       const char* detail,
                                       const RECT* sizingRect,
                                       const WINDOWPOS* windowPos) const;
    void MaybeCommitStartupRevealAfterChromeReady(const char* reason);
    bool IsStartupRevealChromeReady() const;
    bool IsForegroundOwnedByMainWindow() const;
    void ApplyWindowActivationState(bool active, const char* reason);
    void SyncStartupRevealProjection();
    void BypassStartupRevealForImmediateShow();
    void ApplyStartupPresentationDecision(const StartupPresentationCoordinatorDecision& decision,
                                          const char* reason);
    WindowChromeBaseState BuildChromeBaseState() const;
    WindowChromeStandardState BuildChromeStandardState() const;
    WindowChromeDerivedState BuildChromeDerivedState() const;
    WindowChromeApplyHooks BuildChromeApplyHooks();
    bool ApplyResolvedChrome(bool forceRefresh);
    void ApplyFramelessState(bool frameless);
    void ApplyNativeFrameStrategy(WindowNativeFrameStrategy strategy);
    void ApplyCornerPreferenceState(const std::string& mode);
    void ResolveBackdropPolicy();
    bool ApplyBackdropPolicyForActivation(bool active, bool forceRefresh);
    void ApplyBackdropEffect(const std::string& effect, bool forceRefresh);
    bool CanBroadcastBackdropStateChanged() const;
    void BroadcastBackdropStateChanged(bool active, const std::string& mode, const std::string& effect) const;
    bool BroadcastWindowStateChangedIfNeeded(bool force = false);
    void TraceWindowPhase(const char* phase,
                          const char* active = nullptr,
                          const char* forceRefresh = nullptr,
                          const char* effect = nullptr,
                          const char* previousEffect = nullptr,
                          const char* extra = nullptr) const;
    const char* GetTraceEffect(bool active) const;
    static std::string NormalizeBackdropEffect(const std::string& effect, bool allowSystem, const std::string& fallback);
    
    // 注: OnWebViewReady() 和 HandleWebMessage() 已移至 protected 区域或使用基类实现
    
    // 辅助方法
    bool IsPointInDragRegion(int clientX, int clientY) const;
    bool IsPointInNoDragRegion(int clientX, int clientY) const;
    RECT GetCaptionButtonsRect() const;
    void ShowSystemMenu(int screenX, int screenY);
    bool HandleWebViewMenuCommand(WORD cmdId);
    bool HandleStandardMenuCommand(WORD cmdId);
    bool HandleMainMenuLookupCommand(WORD cmdId);
    bool HandleLibraryMenuCommand(WORD cmdId);
    bool HandleHelpMenuCommand(WORD cmdId);
    
    // 菜单命令辅助 - 使用 SDK 的 g_find_by_name 快速查找
    bool ExecuteMainMenuCommand(const char* commandPath);
    
    // 窗口位置记忆
    void SaveWindowPosition();
    void RestoreWindowPosition(int& x, int& y, int& width, int& height, int& showCmd);
    
    // 菜单命令ID
    enum MenuCommands {
        CMD_OPEN_FILE = 1001,
        CMD_OPEN_FOLDER = 1002,
        CMD_PREFERENCES = 1003,
        CMD_DEVTOOLS = 1004,
        CMD_RELOAD = 1005,
        CMD_ABOUT = 1006,
        CMD_CONSOLE = 1007,
        CMD_TEST_PAGE = 1008,
        CMD_MAIN_PAGE = 1009,
    };
};

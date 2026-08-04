#pragma once
#include "pch.h"
#include "core/WebViewPanel.h"
#include "window/BackgroundSuspendPolicy.h"
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
    // 不可见，避免外部 SW_SHOWMINNOACTIVE + 延迟隐藏序列与 cold-start reveal
    // 争抢，保持物理 IsWindowVisible 与 cfg_background_window_visible 一致。
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
    
    // ========== 窗口尺寸限制（存储单位：DIP） ==========
    //
    // 权威单位是 DIP；物理像素换算只发生在 WM_GETMINMAXINFO 内
    // （那里才知道窗口当前 DPI）。详见 WindowShellBase 的接口说明。
    
    void SetMinSizeDip(int widthDip, int heightDip) override;
    void GetMinSizeDip(int& widthDip, int& heightDip) const override {
        widthDip = minWidthDip_;
        heightDip = minHeightDip_;
    }
    
    // 0 表示无限制
    void SetMaxSizeDip(int widthDip, int heightDip) override;
    void GetMaxSizeDip(int& widthDip, int& heightDip) const override {
        widthDip = maxWidthDip_;
        heightDip = maxHeightDip_;
    }
    
    // 设置/获取是否可调整大小
    void SetResizable(bool resizable);
    bool IsResizable() const { return resizable_; }
    
    // 依当前 min/max 约束校正窗口尺寸。
    // 供 SetMinSizeDip/SetMaxSizeDip 与 WM_DPICHANGED 复用：DPI 变化或约束
    // 变化后，已存在的窗口尺寸不会被系统自动纠正，需主动 revalidate。
    void RevalidateSizeAgainstConstraints();
    
    bool SetResizableShell(bool resizable) override {
        SetResizable(resizable);
        return true;
    }
    bool IsResizableShell() const override { return resizable_; }
    
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

    // 登记主窗口「应有」的 WS_EX_TOPMOST 状态，作为守护逻辑的参照真相。
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
    void OnWebViewInitFailed() override;
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

    // ===== 后台降级（最小化 / 托盘隐藏 / 锁屏 / 被完全覆盖） =====
    // 最小化/托盘/锁屏三条隐藏路径统一由 SetBgSuspend 投影驱动。完全覆盖仍只
    // 降低内存目标，必须保留供 DWM 任务栏预览消费的 DirectComposition surface。
    // 位掩码用于正确投影组合状态。
    enum BgSuspendReason : unsigned {
        kBgSuspendLocked     = background_suspend_policy::kLocked,
        kBgSuspendCovered    = background_suspend_policy::kCovered,
        kBgSuspendMinimized  = background_suspend_policy::kMinimized,
        kBgSuspendTrayHidden = background_suspend_policy::kTrayHidden,
    };
    unsigned bgSuspendReasons_ = 0;
    // 上次经 SetBgSuspend 实际应用的页面隐藏状态（全路径唯一 hidden 记账，
    // 事件门控的权威输入经 WebViewHost::SetVisible 执行端同步）。迁移检测必须
    // 比较"实际应用"而非用当前 keep-alive 值重算 prev 投影：挂起期间子开关
    // 变化会漏检 hidden→visible 迁移。
    bool bgSuspendPageHidden_ = false;
    // 深度挂起在途/生效记账：TrySuspendDeep 已下发且尚未恢复可见。
    // 互斥铁律——该状态期间不得手动 SetMemoryUsageLow（TrySuspend 自动置 Low、
    // Resume 自动回 Normal，混用会被运行时忽略并破坏状态推理）。
    bool bgSuspendDeepRequested_ = false;
    unsigned bgSuspendResumeRetryCount_ = 0;
    unsigned bgSuspendHealthRecoveryAttempts_ = 0;
    bool webViewRebuildInProgress_ = false;
    bool pageHealthProbePending_ = false;
    uint64_t pageHealthProbeGeneration_ = 0;
    // 僵尸态重建的限流状态（webview_crash_policy::DecideBrowserExitRebuild 持有
    // 判定，此处只存"窗口内累计次数"与"窗口起点 tick"）。崩溃源若是每次创建
    // 后必然复现的外部因素（实测：第三方注入钩子打崩浏览器进程），无界重试会
    // 退化成重建死循环，故必须限流。
    unsigned browserExitRebuildAttempts_ = 0;
    unsigned long long browserExitRebuildWindowStartMs_ = 0;
    // 每次原因变化都重新投影：hidden 迁移经 SetVisible(false)+TrySuspend（深挂起
    // 开）或 SetVisible(false)+Low（回退）；hidden→visible 迁移经
    // RestoreSurfaceAfterHidden 执行 SetVisible(true)+nudge，再按当前 reasons
    // 重投影内存等级（统一恢复序列）。
    void SetBgSuspend(unsigned reason, bool active, const char* why);

    // 隐藏窗口到托盘：SC_MINIMIZE(minimizeToTray) / SC_CLOSE(closeToTray) /
    // WM_CLOSE(closeToTray) 三条路径共用的"隐藏到托盘"动作
    //（暂停 WebView 省内存 + 清覆盖记账 + SW_HIDE）。
    void HideWindowToTray();

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
    static constexpr UINT_PTR BG_RESUME_RETRY_TIMER_ID = 125;  // hidden→visible 失败后的有限重试
    static constexpr DWORD BG_RESUME_RETRY_DELAY_MS = 100;
    static constexpr unsigned BG_RESUME_RETRY_LIMIT = 50;
    static constexpr UINT_PTR PAGE_HEALTH_RETRY_TIMER_ID = 126;
    static constexpr DWORD PAGE_HEALTH_RETRY_DELAY_MS = 1000;
    static constexpr DWORD PAGE_HEALTH_PROBE_TIMEOUT_MS = 3000;
    static constexpr unsigned PAGE_HEALTH_RECOVERY_LIMIT = 4;
    // 僵尸态（浏览器进程退出 / Reload 失败）整窗重建的延迟调度。必须延迟：
    // ProcessFailed 是 COM 回调，不能在其栈上销毁 WebViewHost 本体。
    static constexpr UINT_PTR PROCESS_DEAD_REBUILD_TIMER_ID = 127;
    static constexpr DWORD PROCESS_DEAD_REBUILD_DELAY_MS = 500;
    static constexpr UINT_PTR DEFERRED_BACKDROP_TIMER_ID = 130;
    static constexpr DWORD DEFERRED_BACKDROP_DELAY_MS = 200;
    static constexpr UINT_PTR BACKDROP_KICK_EXIT_TIMER_ID = 131;
    static constexpr DWORD BACKDROP_KICK_EXIT_DELAY_MS = 50;
    static constexpr UINT_PTR SHORT_REVEAL_TIMER_ID = 140;
    static constexpr DWORD SHORT_REVEAL_TIMEOUT_MS = 250;
    
    // 主窗口默认尺寸下限的**历史口径是物理像素**。
    //
    // P1' 把约束存储改为 DIP 时，若直接把这两个字面量当作 DIP，在 125%
    // 缩放下实际下限会从 400 物理变成 500 物理——即默认值被静默放大。
    // 故保留「物理」语义，由 SeedDefaultConstraintsFromDpi() 在窗口创建前
    // 按初始显示器 DPI 换算成 DIP，使任一缩放下的**物理**下限与改动前一致。
    static constexpr int kDefaultMinWidthPhysical = 400;
    static constexpr int kDefaultMinHeightPhysical = 300;
    
    // 窗口尺寸限制（单位：DIP）
    // 字段名带 Dip 后缀是有意为之：这些值不可直接写入 MINMAXINFO，
    // 必须先经 window_geometry::DipToPhysical 换算。
    //
    // 初值见 kDefaultMinWidthPhysical 等常量的说明：历史默认值是**物理像素**，
    // 故 Create() 会按初始显示器 DPI 换算后覆盖这里的初值。此处的字面量只是
    // 「尚未换算」时的保底，等价于 96 DPI 下的历史行为。
    int minWidthDip_ = kDefaultMinWidthPhysical;
    int minHeightDip_ = kDefaultMinHeightPhysical;
    int maxWidthDip_ = 0;   // 0 表示无限制
    int maxHeightDip_ = 0;  // 0 表示无限制

    // 最小化期间收到的约束变更被推迟到恢复后执行。
    // 对 iconic 窗口调 SetWindowPos 会改写还原态几何，故不能就地校正。
    bool pendingConstraintRevalidate_ = false;
    
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
    bool RefreshStartupRevealSurface();
    bool ReapplyNativeChromeAfterStartupReveal();
    bool EnsureAuthoritativeNativeChrome(const char* reason);
    bool EnsureSurfaceConvergedAfterNativeFrame(const char* reason);
    void ProbeRestoredPageHealth(const char* reason);
    void HandleRestoredPageHealthResult(bool healthy, const char* reason);
    bool RebuildWebViewAfterHealthFailure(const char* reason);
    // 僵尸态整窗重建的唯一入口：限流后经 PROCESS_DEAD_REBUILD_TIMER_ID 延迟到
    // 下一消息周期执行，避免在 ProcessFailed 的 COM 回调栈上销毁宿主。
    void ScheduleProcessDeadRebuild(const char* reason);
    void FinalizeStartupRevealSettlement();
    void CaptureRuntimeDomProbe(const std::string& phase, int scheduledDelayMs = -1);
    void ScheduleFinalizeStartupRuntimeProbes();
    void ScheduleManualResizeRuntimeProbes(const char* sizeType);
    void LogSurfaceDiagnostics(const char* phase) const;
    void LogWebViewLifecycle(const char* event, const std::string& detail = {}) const;
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
    
    // 推导「保存的窗口矩形」所在显示器的 DPI。
    // RestoreWindowPosition 在 CreateWindowExW 之前运行，无 hwnd_ 可用，
    // 故不能用 GetDpiForWindow。取不到时返回基准 DPI（等价于不缩放）。
    static int GetDpiForSavedRect(int x, int y, int width, int height);
    
    // 把物理口径的默认尺寸下限迁移为 DIP 存储值。
    // 必须在 RestoreWindowPosition 之前调用——后者会用这些约束钳制保存的尺寸。
    void SeedDefaultConstraintsFromInitialDpi();
    
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

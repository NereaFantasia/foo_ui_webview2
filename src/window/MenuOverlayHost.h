#pragma once
#include "pch.h"
#include "window/PopupWindow.h"
#include "window/MenuActionContract.h"
#include "window/MenuOverlayGeometry.h"
#include "window/MenuTokenTable.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// ============================================
// 自绘菜单覆盖面宿主（Self-Drawn Menu：自包含渲染器，不依赖 sdk/tsup/floating-ui）//
// 设计要点：
//   1) 独立宿主：内持 PopupWindow，不进 WindowManager::popups_、不计 MAX_POPUPS。
//   2) 池化：首次创建走完整 startup，后续 show/hide 复用同一 WebView（已修合成刷新）。
//   3) 获焦策略：noActivate=false + show 时 SetForegroundWindow，Esc/失焦/键盘可用。
//   4) 渲染：overlay 内联一张自包含菜单页（原生 JS，pull 模式从 C++ 拉 items 渲染）。
// ============================================

// 菜单覆盖面专用窗口：复用 PopupWindow 能力，WebView ready 后注入自包含菜单渲染页。
class MenuOverlayWindow : public PopupWindow {
public:
    void SetReadyHtml(std::wstring html) { readyHtml_ = std::move(html); }
    void SetWebViewReadyCallback(std::function<void()> callback) { webViewReadyCallback_ = std::move(callback); }

protected:
    void OnWebViewReady() override;

private:
    std::wstring readyHtml_;
    std::function<void()> webViewReadyCallback_;
};

// 窗口几何模型：与 owner-mode 事件路由【正交】，禁止由 ownerMode_ 隐式推导。
//   FullscreenOverlay = 现状，铺满工作区（menu.show 默认）。
//   ContentSized      = 内容尺寸窗，按光标定位、rcMonitor 边界、可浮于任务栏之上（tray 用）。
enum class MenuWindowModel { FullscreenOverlay, ContentSized };

// Overlay DOM input model：与 windowModel / ownerMode 正交。
//   LegacyItems = menu.show 与 tray layoutMode:'flat' — 扁平 items，根直接子 DOM。
//   TrayZones   = tray layoutMode:'zones' — 结构化 zones 数组。
enum class MenuOverlayModel { LegacyItems, TrayZones };

// Show 选项（显式参数）。与 sink（事件路由）正交。
// Prefer designated initializers at call sites when adding fields (C++20).
struct MenuShowOptions {
    MenuWindowModel windowModel = MenuWindowModel::FullscreenOverlay;  // 默认=现状，零回归
    MenuOverlayModel overlayModel = MenuOverlayModel::LegacyItems;     // 默认=legacy flat items
    // 前端样式接管：css = 前端注入样式字符串；
    // cssReplace = true 时 replace 模式（禁用默认样式，仅留受保护结构层），false（默认）= override 叠加。
    std::string css;
    bool cssReplace = false;
    // DWM 系统背景（webview tray 菜单）：backdrop 词表与主窗口一致（none/mica/mica-alt/acrylic
    // → DWMSBT 1/2/4/3），默认 acrylic（瞬态面，菜单语义正确）；backdropDarkMode = 背景暗色调。
    // 每次 show 经 PopupWindow::UpdateBackdropPolicy 运行时应用。
    std::string backdrop = "acrylic";
    bool backdropDarkMode = true;
    // 退场动画（opt-in，仅 webview 自绘菜单）：0（默认）= 关闭即立即隐藏（零回归）；
    // >0 = 关闭前延迟该毫秒数播退场动画再隐藏（C++ 端已 clamp 到 0..1000）。前端应让该值
    // ≈ 自身 #menu.out 的 transition 时长。
    int closeAnimationMs = 0;
    // ContentSized 根窗锚定策略（内部字段，不作为公共 API 参数暴露）：
    //   "bottomUp"（默认）= 托盘现状，根底贴光标向上展开；
    //   "cursor"          = 标准右键菜单，根左上贴光标向下展开，越界才翻转。
    // 托盘调用点不传 = 维持 bottomUp = 零回归。取值经
    // menu_overlay_geometry::ParseAnchorPolicy 归一，未知串回落 bottomUp。
    std::string anchorPolicy = "bottomUp";
};

// 菜单覆盖面宿主（单例）。
class MenuOverlayHost {
public:
    static MenuOverlayHost& GetInstance();

    // 确保 overlay 已创建（池化）。仅 UI 线程调用。
    bool EnsureCreated();

    // owner-mode sink：tray 适配层发起 show 时提供。
    // 提供后 select/dismiss 改走 sink（→ tray:menuItemClicked），不向公共
    // menu:select / menu:dismiss 泄漏；不提供 = menu.* 普通模式（维持现状）。
    // select sink 收到 token 解析出的 ResolvedAction（public id + 可信 origin/builtin），
    // 不再是裸 public id 字符串：内置动作按 origin 路由，杜绝 public id 前缀伪造（DESIGN 8.3）。
    using SelectSink  = std::function<void(const menu_action::ResolvedAction& action)>;
    using DismissSink = std::function<void(const std::string& reason)>;
    // 富控件（rating/slider/segmented）值变更 sink：值改变但【不关闭菜单】。仅 owner-mode 用。
    // token 解析出的 public id + 已按控件类型/范围校验过的 value。
    using ValueSink   = std::function<void(const std::string& itemId, int value)>;

    // 在屏幕物理像素 (screenX, screenY) 所在显示器显示菜单，渲染 items。
    // 返回 menuId（空串=失败）。仅 UI 线程调用。
    // onSelect/onDismiss 非空 = owner-mode（事件路由，见上）。
    // onValue 非空 = 富控件值变更路由（与 onSelect 正交；值变更不关闭菜单）。
    // opts.windowModel = 窗口几何模型（与 owner-mode 正交）：
    //   FullscreenOverlay（默认）= 铺工作区现状；ContentSized = 内容尺寸窗（测量回报后定位）。
    std::string Show(const json& items, int screenX, int screenY,
                     SelectSink onSelect = nullptr, DismissSink onDismiss = nullptr,
                     const MenuShowOptions& opts = {}, ValueSink onValue = nullptr);

    // 隐藏（池化保留 WebView）。reason 进入 menu:dismiss。
    void Hide(const std::string& reason);

    // Internal renderer dismiss. Escape in the independent submenu returns to
    // the root panel; other reasons preserve the existing whole-menu behavior.
    void OnDismissRequested(HWND caller, const std::string& reason);

    // 菜单项被选中（menu.__select 内部 IPC → 此方法）：入参是渲染器回传的 opaque token。
    // 解析 token → 校验存在 + enabled + canSelect → owner-mode 走 selectSink_(ResolvedAction)，
    // 否则发公共 menu:select（仅 token 索引里的 publicId）+ 关闭。未知/过期 token 或非法项静默拒绝。
    void OnSelect(const std::string& token);

    // 富控件值变更（menu.__valueChanged 内部 IPC → 此方法）：入参是 opaque token + 值。
    // 解析 token → 仅接受 rating/slider/segmented 且值在合法范围（rating 0..5 / slider [min,max]
    // / segmented 存在且 enabled 的段索引）→ 经 valueSink_ 回报（→ tray:menuItemClicked {id,value}），
    // 【不关闭菜单】。无 sink / 非法 token / 越界值静默忽略。
    void OnValueChanged(const std::string& token, int value);

    // ContentSized 测量回报（menu.__ready 内部 IPC → 此方法）：
    // renderer 回报根菜单与所有一级子菜单的真实最大物理像素尺寸。host 从当前
    // normalized model 判断是否存在一级 submenu，不信任 renderer boolean；经 64 位
    // 槽位计算后只执行一次可见 SetWindowPos。根菜单保持紧凑 HWND；展开时一级
    // submenu 使用独立的紧凑 HWND，因此 DWM backdrop 不会合成到预留 slot。
    bool OnContentMeasured(const menu_overlay_geometry::MeasureReport& report);

    // renderer 在一级 submenu 展开/收起时上报当前可见 panel 的物理像素矩形。
    // 可见 submenu 由独立 PopupWindow 承载；收起时仅隐藏该窗口。参数 parentToken
    // 必须定位当前根层真实 submenu，不能让 renderer 自行选择子菜单内容。
    bool OnSubmenuPanelChanged(const menu_overlay_geometry::SubmenuPanelRequest& request,
                               const std::string& parentToken = {});

    // The independent submenu renderer has pulled and rendered its current
    // token-scoped state while still hidden. Only then may its tight HWND be
    // shown with a DWM backdrop.
    bool OnSubmenuSurfaceReady(HWND caller, const std::string& parentToken);

    // 当前菜单状态（供前端 pull 渲染）：{visible, menuId, items, anchorX, anchorY}。
    json GetMenuStateJson(HWND caller = nullptr) const;

    // fb2k on_quit / WindowManager::Shutdown 钩子。
    void Shutdown();

    bool IsVisible() const { return visible_; }
    const std::string& CurrentMenuId() const { return currentMenuId_; }

    // 内部 IPC 边界校验（menu.__* handler 用；校验逻辑集中于此窄接口，不在各 handler
    // 复制散落的 HWND / menuId 判断）：
    //   IsOverlayCaller — caller 必须是当前 overlay 窗口（_callerHwnd == overlayHwnd_）。
    //   ValidateMenuId  — menuId 必须等于当前可见菜单，挡掉旧页面回报 / 其它窗口伪造。
    bool IsOverlayCaller(HWND caller) const;
    bool IsRootOverlayCaller(HWND caller) const;
    bool ValidateMenuId(const std::string& menuId) const;
    bool HasFirstLevelSubmenu() const;

    // 宿主窗口激活观感保持（原生菜单语义：菜单弹出不使宿主变灰）。
    // 菜单以 SetForegroundWindow 夺走前台期间，打开菜单的宿主窗口在
    // WM_NCACTIVATE / WM_ACTIVATE 里据此判断"本次失活是否让位给菜单面"，
    // 是则维持激活外观（DWM 材质调暗跟随 NCACTIVATE 状态）。
    // other = 消息报告的对端窗口；WM_NCACTIVATE 的 lParam 不保证携带对端，
    // 交接瞬间由 activationHandoff_ 兜底。判定刻意保守：other 非菜单面且
    // 不在交接瞬间 → 一律不保持，保证外点击/切他窗的失活语义不被劫持。
    bool ShouldHoldOwnerActivation(HWND other) const;

    // 窗口是否属于菜单面（根/子菜单），按 GA_ROOT 顶层归一后比较。
    // WebView2/Chromium 会把 Win32 焦点落到其内部子 HWND（Chrome_WidgetWin_*）：
    // WM_ACTIVATE 的对端与 GetForegroundWindow() 都可能是该子窗而非顶层窗，
    // 裸指针相等会把菜单内部的焦点腾挪误判成"焦点离开菜单"（blur 误关整菜单）。
    bool IsMenuSurfaceWindow(HWND hwnd) const;

    static constexpr const char* kOverlayWindowId = "__menu_overlay__";

private:
    MenuOverlayHost() = default;
    ~MenuOverlayHost() = default;
    MenuOverlayHost(const MenuOverlayHost&) = delete;
    MenuOverlayHost& operator=(const MenuOverlayHost&) = delete;

    void ArmWatchdog();
    void KillWatchdog();
    static void CALLBACK WatchdogTimerProc(HWND hwnd, UINT msg, UINT_PTR idEvent, DWORD elapsed);

    // ContentSized 测量回报兜底 timer：临时尺寸显示后启，超时未收 menu.__ready 即 Hide。
    void ArmMeasureTimer();
    void KillMeasureTimer();
    static void CALLBACK MeasureTimerProc(HWND hwnd, UINT msg, UINT_PTR idEvent, DWORD elapsed);

    // 退场动画 close-timer：Hide 进入 Closing 后启，超时（≈closeAnimationMs）由 CloseTimerProc 收尾。
    void ArmCloseTimer(int ms);
    void KillCloseTimer();
    static void CALLBACK CloseTimerProc(HWND hwnd, UINT msg, UINT_PTR idEvent, DWORD elapsed);

    // 两段式 Hide 的同步收尾（立即路径 / close-timer 收尾共用）：置 visible_=false、closing_=false、
    // 杀 timer、捕获并复位 sink、SW_HIDE、分发 dismissSink_/menu:dismiss。务必在 SW_HIDE 前置
    // visible_=false 且 closing_=false（SW_HIDE 同步触发 WM_KILLFOCUS→dismiss→重入 Hide，靠 !visible_ 早退）。
    void FinalizeHide(const std::string& reason);
    // Show 取消挂起关闭复用：绕过 animate 判定直接强制立即收尾（淡出期 visible_&&closing_，
    // 新 Hide("replaced") 会被 if(closing_) return 挡掉，故 Show 不能依赖 Hide("replaced")）。
    void ForceHideImmediate();
    bool ApplyRootContentWindow(const menu_overlay_geometry::Placement& placement);
    bool EnsureSubmenuCreated();
    bool ShowSubmenuWindow(const menu_overlay_geometry::Rect& virtualRect,
                           const std::string& parentToken);
    void HideSubmenuWindow(bool restoreRootFocus, bool notifyRoot = false,
                           bool clearState = true);
    void OnSubmenuWebViewReady();
    // other = 失活消息的对端窗口（WM_ACTIVATE lParam；WM_ACTIVATEAPP/escape 为
    // nullptr）。内部交接判定只信 other（GA_ROOT 归一）：失活消息同步处理期间
    // GetForegroundWindow() 仍返回失活窗口自己（同进程转移未完成），不可作判据。
    void OnRootDismissRequested(const std::string& reason, HWND other);
    void OnSubmenuDismissRequested(const std::string& reason, HWND other);
    void SyncWebViewToClient(MenuOverlayWindow* window, HWND hwnd) const;
    json BuildRootGeometryJson(const menu_overlay_geometry::Placement& placement) const;
    std::optional<json> FindSubmenuItems(const std::string& parentToken) const;

    std::unique_ptr<MenuOverlayWindow> overlay_;
    HWND overlayHwnd_ = nullptr;
    std::unique_ptr<MenuOverlayWindow> submenuOverlay_;
    HWND submenuHwnd_ = nullptr;
    std::string currentMenuId_;
    json currentItems_ = json::array();   // LegacyItems: 扁平根 items（已注入 _token）
    json currentZones_ = json::array();   // TrayZones: [{id, items:[...]}]（items 已注入 _token）
    MenuOverlayModel overlayModel_ = MenuOverlayModel::LegacyItems;

    // Per-show opaque token state machine (DESIGN 8.3): assigns an unpredictable
    // single-use token (CoCreateGuid) per actionable node, resolves select/value
    // through the index, and invalidates the whole set on the first accepted
    // select. Rebuilt every Show, cleared on Hide/ForceHide/Shutdown, so a token
    // is valid only within the current menu. A user public id (even "_pb_next")
    // can never drive a built-in; unknown/expired token, disabled item and
    // out-of-range value are rejected. Extracted (MenuTokenTable.h) for tests.
    MenuTokenTable tokenTable_;
    std::string currentCss_;              // 前端样式接管（S-CSS）：本次 show 的前端 CSS（下发渲染器 fb-user）
    bool currentCssReplace_ = false;      // replace 模式开关（true=禁用默认样式，仅留 fb-user+fb-protected）
    int anchorClientX_ = 0;               // 锚点相对覆盖面客户区坐标
    int anchorClientY_ = 0;
    bool visible_ = false;
    bool closing_ = false;                // 退场动画期：visible_ 仍 true，已发 menu:__hide，等 close-timer 收尾
    int currentCloseAnimationMs_ = 0;     // 本次 show 的退场动画时长（来自 opts.closeAnimationMs，已 clamp）
    std::string pendingCloseReason_;      // 进入 Closing 时记录的原始 reason，供 close-timer 收尾分发
    int showSeq_ = 0;
    unsigned long long showTick_ = 0;     // show 时间戳（失焦防抖用）

    SelectSink  selectSink_;              // owner-mode：select 路由（空=menu.* 普通模式）
    DismissSink dismissSink_;             // owner-mode：dismiss 路由（可空）
    ValueSink   valueSink_;               // 富控件值变更路由（空=不回报，不关闭菜单）
    bool        ownerMode_ = false;       // 本次 show 是否 owner-mode

    // 窗口几何模型（与 ownerMode_ 正交）。
    MenuWindowModel windowModel_ = MenuWindowModel::FullscreenOverlay;
    // Overlay DOM 输入模型（与 windowModel_ / ownerMode_ 正交）。
    // 注意：成员 overlayModel_ 与 opts.overlayModel 在 Show 时同步。
    menu_overlay_geometry::MeasureGate measureGate_;  // invalid report does not consume state
    POINT pendingAnchor_{};               // ContentSized：待定位的光标屏幕物理坐标
    // 本次 show 的锚定策略与左手模式快照：measure 回报是异步的，若届时再读系统设置，
    // 用户在菜单弹出后改设置会让同一个菜单前后两次定位口径不一致。
    menu_overlay_geometry::AnchorPolicy currentAnchorPolicy_ =
        menu_overlay_geometry::AnchorPolicy::BottomUp;
    bool currentDropAlignLeft_ = false;   // SPI_GETMENUDROPALIGNMENT（show 时读取）
    std::optional<menu_overlay_geometry::Placement> placedGeometry_;
    POINT contentVirtualOrigin_{};        // measured virtual root/submenu canvas in screen pixels
    std::optional<menu_overlay_geometry::Rect> openSubmenuVirtualRect_;
    std::string openSubmenuParentToken_;
    std::string currentBackdrop_ = "acrylic";
    bool currentBackdropDarkMode_ = true;
    // 激活交接状态（宿主激活观感保持）：handoff 仅在菜单面 SetForegroundWindow
    // 的同步窗口期内为 true（宿主此刻收到的 NCACTIVATE/ACTIVATE 即交接产物）；
    // previousForeground_ 记录 Show 前的前台窗口，FinalizeHide 时归还。
    bool activationHandoff_ = false;
    HWND previousForeground_ = nullptr;
    bool suppressSubmenuDismiss_ = false;
    bool openingSubmenu_ = false;
    std::uint64_t lastSubmenuPanelSequence_ = 0;
    std::optional<std::uint64_t> pendingSubmenuPanelSequence_;

    static constexpr UINT_PTR kWatchdogTimerId = 0xFACE01;
    static constexpr UINT kWatchdogTimeoutMs = 30000;  // 默认 30s

    static constexpr UINT_PTR kMeasureTimerId = 0xFACE02;
    static constexpr UINT kMeasureTimeoutMs = 1500;    // ContentSized 测量回报兜底；30s 看门狗仍是最终兜底

    static constexpr UINT_PTR kCloseTimerId = 0xFACE03;  // 退场动画收尾 timer
    static constexpr int kCloseAnimationMaxMs = 1000;    // closeAnimationMs clamp 上限
};
#pragma once
#include "pch.h"
#include "window/PopupWindow.h"
#include <functional>
#include <memory>
#include <string>

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

protected:
    void OnWebViewReady() override;

private:
    std::wstring readyHtml_;
};

// 窗口几何模型：与 owner-mode 事件路由【正交】，禁止由 ownerMode_ 隐式推导。
//   FullscreenOverlay = 现状，铺满工作区（menu.show 默认）。
//   ContentSized      = 内容尺寸窗，按光标定位、rcMonitor 边界、可浮于任务栏之上（tray 用）。
enum class MenuWindowModel { FullscreenOverlay, ContentSized };

// Show 选项（显式参数）。与 sink（事件路由）正交。
struct MenuShowOptions {
    MenuWindowModel windowModel = MenuWindowModel::FullscreenOverlay;  // 默认=现状，零回归
    // 前端样式接管（S-CSS，STYLING_TAKEOVER_DESIGN §12）：css = 前端注入样式字符串；
    // cssReplace = true 时 replace 模式（禁用默认样式，仅留受保护结构层），false（默认）= override 叠加。
    // 注意：ShowContextMenu 用聚合初始化 MenuShowOptions{...}，新增字段顺序须与此处一致。
    std::string css;
    bool cssReplace = false;
    // DWM 系统背景（webview tray 菜单）：backdrop 词表与主窗口一致（none/mica/mica-alt/acrylic
    // → DWMSBT 1/2/4/3），默认 acrylic（瞬态面，菜单语义正确）；backdropDarkMode = 背景暗色调。
    // 每次 show 经 PopupWindow::UpdateBackdropPolicy 运行时应用。聚合初始化字段顺序须与此一致。
    std::string backdrop = "acrylic";
    bool backdropDarkMode = true;
    // 退场动画（opt-in，仅 webview 自绘菜单）：0（默认）= 关闭即立即隐藏（零回归）；
    // >0 = 关闭前延迟该毫秒数播退场动画再隐藏（C++ 端已 clamp 到 0..1000）。前端应让该值
    // ≈ 自身 #menu.out 的 transition 时长。注意：ShowContextMenu 用聚合初始化 MenuShowOptions{...}，
    // 新增字段顺序须与此处一致。
    int closeAnimationMs = 0;
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
    using SelectSink  = std::function<void(const std::string& itemId)>;
    using DismissSink = std::function<void(const std::string& reason)>;
    // 富控件（rating/slider）值变更 sink：值改变但【不关闭菜单】。仅 owner-mode 用。
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

    // 菜单项被选中：发射 menu:select + 关闭。
    void OnSelect(const std::string& itemId);

    // 富控件值变更（menu.__valueChanged 内部 IPC → 此方法）：经 valueSink_ 回报
    // （→ tray:menuItemClicked {id,value}），【不关闭菜单】。无 sink 时静默忽略。
    void OnValueChanged(const std::string& itemId, int value);

    // ContentSized 测量回报（menu.__ready 内部 IPC → 此方法）：
    // 渲染器测得【根菜单】物理像素后，按 rcMonitor 定位并把窗口一次定为「根 + 子菜单预留」固定
    // 尺寸（全程不再 resize；子菜单纯 CSS 在窗内右展），再发内部事件 menu:__placed 触发入场动画。
    // 仅 ContentSized 且测量期（awaitingMeasure_）生效；物理像素入参；originXPhysical 在固定窗设计下未用；仅 UI 线程。
    // hasSubmenu = 根菜单是否含子菜单：true 才把窗口宽 ×2 预留右侧展开区；false 则窗口=精确根宽
    // （避免空预留区在亚克力/云母背景下露出空白半透明面板）。
    void OnContentMeasured(int wPhysical, int hPhysical, int originXPhysical, bool hasSubmenu);

    // 当前菜单状态（供前端 pull 渲染）：{visible, menuId, items, anchorX, anchorY}。
    json GetMenuStateJson() const;

    // fb2k on_quit / WindowManager::Shutdown 钩子。
    void Shutdown();

    bool IsVisible() const { return visible_; }
    const std::string& CurrentMenuId() const { return currentMenuId_; }

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

    std::unique_ptr<MenuOverlayWindow> overlay_;
    HWND overlayHwnd_ = nullptr;
    std::string currentMenuId_;
    json currentItems_ = json::array();   // 当前根菜单 items
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
    bool  awaitingMeasure_ = false;       // ContentSized：是否等待 menu.__ready 测量回报
    POINT pendingAnchor_{};               // ContentSized：待定位的光标屏幕物理坐标

    static constexpr UINT_PTR kWatchdogTimerId = 0xFACE01;
    static constexpr UINT kWatchdogTimeoutMs = 30000;  // 默认 30s

    static constexpr UINT_PTR kMeasureTimerId = 0xFACE02;
    static constexpr UINT kMeasureTimeoutMs = 1500;    // ContentSized 测量回报兜底；30s 看门狗仍是最终兜底

    static constexpr UINT_PTR kCloseTimerId = 0xFACE03;  // 退场动画收尾 timer
    static constexpr int kCloseAnimationMaxMs = 1000;    // closeAnimationMs clamp 上限
};
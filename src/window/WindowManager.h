#pragma once
#include "pch.h"
#include "window/PopupWindow.h"

class MainWindow;

// ============================================
// 窗口管理器 - 单例
// 管理所有弹出窗口的创建、销毁和查询
// ============================================
class WindowManager {
public:
    static WindowManager& GetInstance();
    
    // 初始化（设置主窗口引用）
    void Initialize(MainWindow* mainWindow);  // 独立窗口模式（保持向后兼容）
    void InitializeForPanel();                // 面板模式（无 MainWindow）
    
    // 关闭所有弹出窗口（跳过 beforeClose，直接销毁）
    void Shutdown();
    
    // ========== 窗口操作 ==========
    
    // 创建弹出窗口，返回窗口 ID（空字符串表示失败）
    std::string CreatePopup(const PopupWindow::CreateParams& params);
    
    // 关闭弹出窗口（触发 beforeClose 机制）
    bool ClosePopup(const std::string& windowId);
    
    // 关闭所有弹出窗口（触发 beforeClose 机制）
    bool CloseAllPopups();
    
    // 从管理器中移除弹出窗口（由 PopupWindow::OnDestroy 调用）
    void RemovePopup(const std::string& windowId);
    
    // ========== 自定义跨窗口消息 ==========
    
    // 定向发送消息到指定窗口
    // @return false 如果目标窗口不存在
    bool SendWindowMessage(const std::string& sourceId, const std::string& targetId, const json& message);
    
    // 广播消息到除发送者外的所有窗口
    void BroadcastMessage(const std::string& sourceId, const json& message);
    
    // ========== 查询 ==========
    
    PopupWindow* GetPopup(const std::string& windowId);
    std::vector<std::string> GetAllWindowIds() const;
    json GetWindowInfo(const std::string& windowId) const;
    json GetAllWindowsInfo() const;
    size_t GetPopupCount() const;
    
    // 主窗口访问
    MainWindow* GetMainWindow() const { return mainWindow_; }
    std::string GetMainWindowId() const { return "main"; }

    // ========== 激活承接窗口 (activation sink) ==========
    // overlay / noActivate popup（如 desktopLyrics）被 WebView2 内部 SetFocus 激活后，
    // 因 WS_EX_NOACTIVATE 立即失活，系统会回退激活同线程下一个可激活顶层窗口。
    // 实测：回退目标取决于「最近活动 / z-order」而非 owner，会落到 fb2k 主窗口并覆盖外部应用。
    // 本 sink 是一个「可激活但不可感知」的 topmost 隐藏窗口，作为这些 popup 的承接目标：
    // 既作 owner，也作反应式 SetForegroundWindow(sink) 的目标，把激活引到 sink 而非主窗口。
    // 惰性创建，仅应在 UI 线程调用；返回 nullptr 表示创建失败（调用方退回无 owner）。
    HWND GetActivationSinkHwnd();

    // ========== overlay 交互信号 ==========
    // overlay popup（desktopLyrics 等）被点击时由 PopupWindow 调用，记录点击瞬间的前台
    // 窗口（外部应用）与时间戳。WebView2 会在 popup 与主窗口两个 controller 间回授焦点，
    // 使主窗口在 overlay 点击后短时内被无意激活。MainWindow 用这些信号判断该激活是否
    // 由 overlay 交互引发，并据此把激活引回外部应用 / sink。仅 UI 线程读写。
    void NotifyOverlayInteraction(HWND foregroundBeforeClick);
    bool IsOverlayInteractionRecent(unsigned int windowMs) const;
    HWND GetOverlayInteractionForeground() const;

    // 面板模式：引用计数管理
    void RegisterPanel();    // 面板注册（引用计数 +1）
    void UnregisterPanel();  // 面板注销（引用计数 -1，归零时清理 popup）
    bool IsInitialized() const { return initialized_; }
    
    // 统一分配面板 ID（DUI/CUI 共用计数器）
    std::string GeneratePanelId();
    
    // 弹出窗口数量限制
    static constexpr int MAX_POPUPS = 8;

private:
    WindowManager() = default;
    ~WindowManager() = default;
    WindowManager(const WindowManager&) = delete;
    WindowManager& operator=(const WindowManager&) = delete;
    
    MainWindow* mainWindow_ = nullptr;
    std::map<std::string, std::unique_ptr<PopupWindow>> popups_;
    mutable std::mutex mutex_;
    int nextId_ = 0;
    
    bool initialized_ = false;     // 是否已初始化
    int activePanelCount_ = 0;     // 活跃面板引用计数
    int nextPanelId_ = 0;          // 面板 ID 计数器

    unsigned long long overlayInteractionTick_ = 0;  // GetTickCount64() at last overlay click
    HWND overlayInteractionForeground_ = nullptr;    // foreground window captured before that click
    
    std::string GenerateId();
};

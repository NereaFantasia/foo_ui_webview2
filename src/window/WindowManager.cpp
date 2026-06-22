#include "pch.h"
#include "window/WindowManager.h"
#include "window/MainWindow.h"
#include "window/WindowShellBase.h"
#include "core/WebViewContext.h"

namespace {

json BuildWindowBoundsInfo(HWND hwnd) {
    RECT rect = {};
    if (hwnd) {
        GetWindowRect(hwnd, &rect);
    }

    return {
        {"x", rect.left},
        {"y", rect.top},
        {"width", rect.right - rect.left},
        {"height", rect.bottom - rect.top}
    };
}

std::string GetWindowTitleUtf8(HWND hwnd, const std::string& fallback) {
    if (!hwnd) {
        return fallback;
    }

    int titleLength = GetWindowTextLengthW(hwnd);
    std::wstring titleBuffer(static_cast<size_t>(titleLength) + 1, L'\0');

    ::SetLastError(0);
    int copied = GetWindowTextW(hwnd, titleBuffer.data(), static_cast<int>(titleBuffer.size()));
    if (copied == 0 && GetLastError() != 0) {
        return fallback;
    }

    return pfc::stringcvt::string_utf8_from_wide(std::wstring(titleBuffer.data(), copied).c_str()).get_ptr();
}

json ShellSnapshotToJson(const WindowShellSnapshot& snap) {
    return {
        {"lifecycle", {
            {"created", snap.lifecycle.created},
            {"visible", snap.lifecycle.visible},
            {"active", snap.lifecycle.active},
            {"minimized", snap.lifecycle.minimized},
            {"maximized", snap.lifecycle.maximized},
            {"fullscreen", snap.lifecycle.fullscreen},
            {"pendingDestroy", snap.lifecycle.pendingDestroy}
        }},
        {"startupPresentation", {
            {"phase", snap.startupPresentation.phase},
            {"navigationCompleted", snap.startupPresentation.navigationCompleted},
            {"windowReadySignaled", snap.startupPresentation.windowReadySignaled},
            {"visualReadySignaled", snap.startupPresentation.visualReadySignaled},
            {"revealPending", snap.startupPresentation.revealPending},
            {"revealCommitted", snap.startupPresentation.revealCommitted},
            {"revealSettling", snap.startupPresentation.revealSettling},
            {"fallbackArmed", snap.startupPresentation.fallbackArmed},
            {"fallbackUsed", snap.startupPresentation.fallbackUsed}
        }}
    };
}

json BuildMainWindowCapabilities() {
    return {
        {"supportsBackdropPolicy", true},
        {"supportsFrameless", true},
        {"supportsCornerPreference", true},
        {"supportsPopupBehavior", false},
        {"supportsMicaAlt", true},
        {"supportsFullscreen", true}
    };
}

json BuildPopupWindowCapabilities(const WindowShellSnapshot& snap) {
    return {
        {"supportsBackdropPolicy", snap.capabilities.supportsBackdropPolicy},
        {"supportsFrameless", true},
        {"supportsCornerPreference", snap.capabilities.supportsCornerPreference},
        {"supportsPopupBehavior", true},
        {"supportsMicaAlt", snap.capabilities.supportsMicaAlt},
        {"supportsFullscreen", snap.capabilities.supportsFullscreen},
        {"supportsOwnerPolicy", snap.capabilities.supportsOwnerPolicy},
        {"supportsNoActivate", snap.capabilities.supportsNoActivate},
        {"supportsBeforeClose", snap.capabilities.supportsBeforeClose}
    };
}

json BuildMainWindowInfo(const MainWindow& mainWindow) {
    json backdropInfo = mainWindow.GetBackdropPolicyInfo();
    WindowShellSnapshot snap = mainWindow.GetShellSnapshot();

    return {
        {"windowId", "main"},
        {"isMain", true},
        {"title", GetWindowTitleUtf8(mainWindow.GetHwnd(), "foobar2000")},
        {"backdropPolicy", backdropInfo.value("backdropPolicy", json::object())},
        {"resolvedBackdropPolicy", backdropInfo.value("resolvedBackdropPolicy", json::object())},
        {"capabilities", BuildMainWindowCapabilities()},
        {"bounds", BuildWindowBoundsInfo(mainWindow.GetHwnd())},
        {"shell", ShellSnapshotToJson(snap)}
    };
}

json BuildPopupWindowInfo(const std::string& windowId, const PopupWindow& popup) {
    const auto& createParams = popup.GetCreateParams();
    json behaviorInfo = popup.GetPopupBehaviorInfo();
    json backdropInfo = popup.GetBackdropPolicyInfo();
    WindowShellSnapshot snap = popup.GetShellSnapshot();

    return {
        {"windowId", windowId},
        {"isMain", false},
        {"title", GetWindowTitleUtf8(popup.GetHwnd(), createParams.title)},
        {"url", createParams.url},
        {"profile", popup.GetProfile()},
        {"behavior", behaviorInfo.value("behavior", json::object())},
        {"resolvedBehavior", behaviorInfo.value("resolvedBehavior", json::object())},
        {"backdropPolicy", backdropInfo.value("backdropPolicy", json::object())},
        {"resolvedBackdropPolicy", backdropInfo.value("resolvedBackdropPolicy", json::object())},
        {"capabilities", BuildPopupWindowCapabilities(snap)},
        {"bounds", BuildWindowBoundsInfo(popup.GetHwnd())},
        {"shell", ShellSnapshotToJson(snap)}
    };
}

// ============================================
// Activation sink window（见 WindowManager::GetActivationSinkHwnd 注释）
// 「可激活但不可感知」的顶层窗口：屏幕外 1x1、全透明 (alpha=0)、工具窗口
// （不进任务栏 / Alt-Tab）。无 WS_EX_NOACTIVATE，故系统可把它选为回退激活目标；
// 必须为「可见态」(SW_SHOWNOACTIVATE)——系统不会回退激活隐藏窗口。
// 设 WS_EX_TOPMOST：使其在 z-order 回退搜索中排在非 topmost 的主窗口之前，
// 让被拒绝激活的 topmost overlay popup 优先回退到本 sink 而非主窗口。
// ============================================
constexpr wchar_t kActivationSinkClassName[] = L"foo_ui_webview2_ActivationSink";
HWND g_activationSinkHwnd = nullptr;
bool g_activationSinkClassRegistered = false;

LRESULT CALLBACK ActivationSinkWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

HWND EnsureActivationSink() {
    if (g_activationSinkHwnd && IsWindow(g_activationSinkHwnd)) {
        return g_activationSinkHwnd;
    }
    HINSTANCE hInst = core_api::get_my_instance();
    if (!g_activationSinkClassRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = ActivationSinkWndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = kActivationSinkClassName;
        if (!RegisterClassExW(&wc)) {
            DWORD err = GetLastError();
            if (err != ERROR_CLASS_ALREADY_EXISTS) {
                console::printf("[WindowManager] ActivationSink class register failed: %u", err);
                return nullptr;
            }
        }
        g_activationSinkClassRegistered = true;
    }
    g_activationSinkHwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        kActivationSinkClassName,
        L"",
        WS_POPUP,
        -32000, -32000, 1, 1,
        nullptr, nullptr, hInst, nullptr);
    if (!g_activationSinkHwnd) {
        console::printf("[WindowManager] ActivationSink create failed: %u", GetLastError());
        return nullptr;
    }
    // 全透明：用户不可感知，但仍是「可见态」窗口。
    SetLayeredWindowAttributes(g_activationSinkHwnd, 0, 0, LWA_ALPHA);
    ShowWindow(g_activationSinkHwnd, SW_SHOWNOACTIVATE);
    FB2K_console_formatter() << "[WindowManager] ActivationSink created, hwnd=0x"
        << pfc::format_hex((size_t)g_activationSinkHwnd);
    return g_activationSinkHwnd;
}

void DestroyActivationSink() {
    if (g_activationSinkHwnd && IsWindow(g_activationSinkHwnd)) {
        DestroyWindow(g_activationSinkHwnd);
    }
    g_activationSinkHwnd = nullptr;
}

} // namespace

WindowManager& WindowManager::GetInstance() {
    static WindowManager instance;
    return instance;
}

HWND WindowManager::GetActivationSinkHwnd() {
    // 不加 mutex_：sink 与 popups_ 无关，且仅在 UI 线程调用；
    // 而 CreatePopup 已持 mutex_ 调用本函数（PopupWindow::Create -> 此处），
    // 再次加锁会自死锁（std::mutex 非递归）。
    return EnsureActivationSink();
}

void WindowManager::NotifyOverlayInteraction(HWND foregroundBeforeClick) {
    // 仅 UI 线程调用；不加 mutex_（与 popups_ 无关，且可能在 popup 消息处理中调用）。
    overlayInteractionTick_ = GetTickCount64();
    overlayInteractionForeground_ = foregroundBeforeClick;
}

bool WindowManager::IsOverlayInteractionRecent(unsigned int windowMs) const {
    if (overlayInteractionTick_ == 0) return false;
    const unsigned long long now = GetTickCount64();
    return now >= overlayInteractionTick_ &&
           (now - overlayInteractionTick_) <= windowMs;
}

HWND WindowManager::GetOverlayInteractionForeground() const {
    return overlayInteractionForeground_;
}

void WindowManager::Initialize(MainWindow* mainWindow) {
    std::lock_guard<std::mutex> lock(mutex_);
    mainWindow_ = mainWindow;
    initialized_ = true;
    FB2K_console_formatter() << "[WindowManager] Initialized with MainWindow=0x" 
        << pfc::format_hex((size_t)mainWindow);
}

void WindowManager::InitializeForPanel() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return;  // 已初始化则跳过
    initialized_ = true;
    mainWindow_ = nullptr;  // 面板模式无主窗口
    FB2K_console_formatter() << "[WindowManager] Initialized for panel mode";
}

void WindowManager::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    FB2K_console_formatter() << "[WindowManager] Shutdown: closing " 
        << (unsigned)popups_.size() << " popups";
    
    // 直接销毁所有弹出窗口，跳过 beforeClose
    for (auto& [id, popup] : popups_) {
        if (popup) {
            popup->Destroy();
        }
    }
    popups_.clear();
    // popup 已全部销毁，再销毁其 owner（activation sink），避免悬空 owner。
    DestroyActivationSink();
    mainWindow_ = nullptr;
    initialized_ = false;
    
    console::printf("[WindowManager] Shutdown complete");
}

std::string WindowManager::GenerateId() {
    // 调用方已持有锁
    ++nextId_;
    return "popup_" + std::to_string(nextId_);
}

std::string WindowManager::CreatePopup(const PopupWindow::CreateParams& params) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 数量限制检查
    if (static_cast<int>(popups_.size()) >= MAX_POPUPS) {
        console::printf("[WindowManager] CreatePopup failed: max popups reached (%d)", MAX_POPUPS);
        return "";
    }
    
    if (!initialized_) {
        console::printf("[WindowManager] CreatePopup failed: not initialized");
        return "";
    }
    
    std::string windowId = GenerateId();
    auto popup = std::make_unique<PopupWindow>();
    
    HWND ownerHwnd = mainWindow_ ? mainWindow_->GetHwnd() : nullptr;
    HWND hwnd = popup->Create(params, windowId, ownerHwnd);
    if (!hwnd) {
        console::printf("[WindowManager] CreatePopup failed: window creation failed");
        return "";
    }
    
    popups_[windowId] = std::move(popup);
    
    FB2K_console_formatter() << "[WindowManager] Created popup: " 
        << windowId.c_str() << ", total=" << (unsigned)popups_.size();
    
    return windowId;
}

bool WindowManager::ClosePopup(const std::string& windowId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = popups_.find(windowId);
    if (it == popups_.end()) {
        return false;
    }
    
    // 触发 WM_CLOSE → beforeClose 机制
    HWND hwnd = it->second->GetHwnd();
    if (hwnd && IsWindow(hwnd)) {
        // 使用 PostMessage 避免在持有锁时调用 WndProc
        ::PostMessageW(hwnd, WM_CLOSE, 0, 0);
        return true;
    }
    
    return false;
}

bool WindowManager::CloseAllPopups() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    bool anyClosing = false;
    for (auto& [id, popup] : popups_) {
        if (popup) {
            HWND hwnd = popup->GetHwnd();
            if (hwnd && IsWindow(hwnd)) {
                ::PostMessageW(hwnd, WM_CLOSE, 0, 0);
                anyClosing = true;
            }
        }
    }
    
    return anyClosing;
}

void WindowManager::RemovePopup(const std::string& windowId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = popups_.find(windowId);
    if (it != popups_.end()) {
        popups_.erase(it);
        FB2K_console_formatter() << "[WindowManager] Removed popup: " 
            << windowId.c_str() << ", remaining=" << (unsigned)popups_.size();
    }
}

PopupWindow* WindowManager::GetPopup(const std::string& windowId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = popups_.find(windowId);
    if (it != popups_.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<std::string> WindowManager::GetAllWindowIds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> ids;
    ids.reserve(popups_.size() + 1);
    if (mainWindow_) {
        ids.emplace_back("main");
    }
    for (const auto& [id, _] : popups_) {
        ids.push_back(id);
    }
    return ids;
}

json WindowManager::GetWindowInfo(const std::string& windowId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (windowId == "main") {
        if (!mainWindow_ || !mainWindow_->GetHwnd()) {
            return json::object();
        }

        return BuildMainWindowInfo(*mainWindow_);
    }
    
    auto it = popups_.find(windowId);
    if (it == popups_.end() || !it->second) {
        return json::object();
    }
    
    return BuildPopupWindowInfo(windowId, *it->second);
}

json WindowManager::GetAllWindowsInfo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json windows = json::array();
    
    // 主窗口
    if (mainWindow_ && mainWindow_->GetHwnd()) {
        windows.push_back(BuildMainWindowInfo(*mainWindow_));
    }
    
    // 弹出窗口
    for (const auto& [id, popup] : popups_) {
        if (!popup) continue;
        
        windows.push_back(BuildPopupWindowInfo(id, *popup));
    }
    
    return windows;
}

size_t WindowManager::GetPopupCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return popups_.size();
}

// ============================================
// 自定义跨窗口消息
// ============================================

bool WindowManager::SendWindowMessage(const std::string& sourceId, const std::string& targetId, const json& message) {
    auto& ctx = WebViewContext::GetInstance();
    
    json payload = {
        {"sourceWindowId", sourceId},
        {"message", message}
    };
    
    return ctx.SendEventTo(targetId, "window:message", payload);
}

void WindowManager::BroadcastMessage(const std::string& sourceId, const json& message) {
    auto& ctx = WebViewContext::GetInstance();
    
    json payload = {
        {"sourceWindowId", sourceId},
        {"message", message}
    };
    
    HWND sourceHwnd = ctx.GetHwndByWindowId(sourceId);
    if (sourceHwnd) {
        ctx.BroadcastEventExcept("window:message", payload, sourceHwnd);
    } else {
        // 找不到发送者 HWND，广播到所有窗口
        ctx.BroadcastEvent("window:message", payload);
    }
}

// ============================================
// 面板模式：引用计数管理
// ============================================

void WindowManager::RegisterPanel() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++activePanelCount_;
    FB2K_console_formatter() << "[WindowManager] Panel registered, count=" << activePanelCount_;
}

void WindowManager::UnregisterPanel() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (activePanelCount_ <= 0) {
        FB2K_console_formatter() << "[WindowManager] UnregisterPanel called but count already 0, ignoring";
        return;
    }
    --activePanelCount_;
    FB2K_console_formatter() << "[WindowManager] Panel unregistered, count=" << activePanelCount_;
    
    if (activePanelCount_ == 0 && !mainWindow_) {
        // 面板模式下最后一个面板销毁 → 清理所有 popup
        // 注意: 不要在这里调用 Shutdown()（会死锁，已持有 mutex_）
        for (auto& [id, popup] : popups_) {
            if (popup) popup->Destroy();
        }
        popups_.clear();
        initialized_ = false;
        activePanelCount_ = 0;
        FB2K_console_formatter() << "[WindowManager] All panels gone, cleaned up";
    }
}

std::string WindowManager::GeneratePanelId() {
    std::lock_guard<std::mutex> lock(mutex_);
    return "panel_" + std::to_string(++nextPanelId_);
}

#include "pch.h"
#include "window/WindowManager.h"
#include "window/MainWindow.h"
#include "window/WindowShellBase.h"
#include "core/WebViewContext.h"
#include "api/EventEmit.h"
#include "api/generated/WindowSchema.h"
#include "window/WindowChromeTrace.h"
#include <commctrl.h>
#include <sstream>

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

json BuildMainWindowCapabilities(const WindowShellCapabilities& capabilities) {
    return {
        {"supportsBackdropPolicy", true},
        {"supportsFrameless", true},
        {"supportsCornerPreference", capabilities.supportsCornerPreference},
        {"supportsPopupBehavior", false},
        {"supportsMicaAlt", capabilities.supportsMicaAlt},
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
        {"capabilities", BuildMainWindowCapabilities(snap.capabilities)},
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

// 与 MenuOverlayHost::~MenuOverlayHost 同一条规则：静态析构期不销毁窗口。
// 正常关停时 Shutdown() 已把 popups_ 清空，这里无事可做；仍有残留说明是
// 没接关停点的宿主模式，此时走 OnDestroy 会碰到已析构的注册表。
WindowManager::~WindowManager() noexcept {
    ClearActivationTargets();
    for (auto& entry : popups_) {
        (void)entry.second.release();
    }
}

HWND WindowManager::GetActivationSinkHwnd() {
    // 不加 mutex_：sink 与 popups_ 无关，且仅在 UI 线程调用；
    // 而 CreatePopup 已持 mutex_ 调用本函数（PopupWindow::Create -> 此处），
    // 再次加锁会自死锁（std::mutex 非递归）。
    const HWND sink = EnsureActivationSink();
    RegisterActivationTarget(sink);
    return sink;
}

window_activation::Interaction WindowManager::NotifyOverlayInteraction(HWND source, HWND foregroundBeforeClick) {
    // 仅 UI 线程调用；不加 mutex_（与 popups_ 无关，且可能在 popup 消息处理中调用）。
    overlayInteractionTick_ = GetTickCount64();
    overlayInteractionForeground_ = foregroundBeforeClick;
    overlayForegroundReference_ = CaptureWindowReference(foregroundBeforeClick);
    overlayInteraction_ = activationPolicy_.Interact(RegisterActivationTarget(source), overlayInteractionTick_);
    return overlayInteraction_;
}

bool WindowManager::IsOverlayInteractionRecent(HWND target, unsigned int windowMs) const {
    return activationPolicy_.IsRecent(overlayInteraction_,
        activationPolicy_.Find(reinterpret_cast<std::uintptr_t>(target)), GetTickCount64(), windowMs);
}

HWND WindowManager::GetOverlayInteractionForeground() const {
    return IsWindowReferenceLive(overlayForegroundReference_) ? overlayInteractionForeground_ : nullptr;
}

namespace {
constexpr UINT_PTR kActivationSubclassId = 1;
UINT ExplicitSystemCommandMessage() {
    static const UINT message = RegisterWindowMessageW(L"FooUiWebView2.ExplicitSystemCommand");
    return message;
}
}

window_activation::Target WindowManager::RegisterActivationTarget(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd) || GetWindowThreadProcessId(hwnd, nullptr) != GetCurrentThreadId()) return {};
    const auto key = reinterpret_cast<std::uintptr_t>(hwnd);
    const auto known = activationPolicy_.Find(key);
    if (known.instance) return known;
    if (!SetWindowSubclass(hwnd, ActivationSubclassProc, kActivationSubclassId, 0)) return {};
    return activationPolicy_.Register(key);
}

void WindowManager::UnregisterActivationTarget(HWND hwnd) {
    const auto target = activationPolicy_.Find(reinterpret_cast<std::uintptr_t>(hwnd));
    activationPolicy_.Forget(target);
    std::erase_if(pendingSystemCommands_, [&](const auto& entry) { return entry.second.target == target; });
    RemoveWindowSubclass(hwnd, ActivationSubclassProc, kActivationSubclassId);
}

window_activation::Scope WindowManager::BeginExplicitActivation(HWND hwnd, const char* source) {
    const auto request = activationPolicy_.BeginExplicit(RegisterActivationTarget(hwnd));
    std::ostringstream line;
    line << "[ActivationIntent] source=" << source << " target=" << hwnd << " instance="
         << request.target.instance << " epoch=" << request.epoch;
    WindowChromeTrace::EmitAuxiliaryLine(line.str());
    return window_activation::Scope(activationPolicy_, request);
}

window_activation::Scope WindowManager::ProtectActivation(HWND hwnd) {
    return window_activation::Scope(activationPolicy_, {RegisterActivationTarget(hwnd), activationPolicy_.Epoch()});
}

bool WindowManager::IsExplicitActivation(HWND hwnd) const {
    return activationPolicy_.IsExplicit(activationPolicy_.Find(reinterpret_cast<std::uintptr_t>(hwnd)));
}

bool WindowManager::IsActivationTarget(HWND hwnd) const {
    return activationPolicy_.Find(reinterpret_cast<std::uintptr_t>(hwnd)).instance != 0;
}

bool WindowManager::RequestForeground(HWND hwnd, const char* source) {
    const BOOL result = SetForegroundWindow(hwnd);
    std::ostringstream line;
    line << "[ActivationIntent] source=" << source << " target=" << hwnd
         << " setForegroundOk=" << (result != FALSE) << " foreground=" << GetForegroundWindow();
    WindowChromeTrace::EmitAuxiliaryLine(line.str());
    return result != FALSE;
}

bool WindowManager::PostExplicitSystemCommand(HWND hwnd, UINT command) {
    const auto target = RegisterActivationTarget(hwnd);
    if (!target.instance) return false;
    // Lifetime guards queued commands; a later focus must not silently cancel a restore.
    auto intent = BeginExplicitActivation(hwnd, "window.systemCommand.queued");
    if (++nextSystemCommand_ == 0) ++nextSystemCommand_;
    pendingSystemCommands_.emplace(nextSystemCommand_, PendingSystemCommand{target, command});
    if (PostMessageW(hwnd, ExplicitSystemCommandMessage(), nextSystemCommand_, 0)) return true;
    pendingSystemCommands_.erase(nextSystemCommand_);
    return false;
}

LRESULT CALLBACK WindowManager::ActivationSubclassProc(HWND hwnd, UINT message, WPARAM wParam,
    LPARAM lParam, UINT_PTR, DWORD_PTR) {
    auto& manager = GetInstance();
    if (message == ExplicitSystemCommandMessage()) {
        const auto found = manager.pendingSystemCommands_.find(wParam);
        if (found == manager.pendingSystemCommands_.end()) return 0;
        const auto command = found->second;
        manager.pendingSystemCommands_.erase(found);
        if (command.target.handle != reinterpret_cast<std::uintptr_t>(hwnd) ||
            !manager.activationPolicy_.IsLive(command.target)) return 0;
        auto intent = manager.BeginExplicitActivation(hwnd, "window.systemCommand.execute");
        return SendMessageW(hwnd, WM_SYSCOMMAND, command.command, 0);
    }
    if (message == WM_NCDESTROY) manager.UnregisterActivationTarget(hwnd);
    if (message == WM_SYSCOMMAND && ((wParam & 0xFFF0) == SC_RESTORE || (wParam & 0xFFF0) == SC_MAXIMIZE) &&
        !manager.IsExplicitActivation(hwnd)) {
        auto intent = manager.BeginExplicitActivation(hwnd, "native.systemCommand");
        return DefSubclassProc(hwnd, message, wParam, lParam);
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

window_activation::Undo WindowManager::CaptureActivationUndo(window_activation::Interaction interaction, HWND target) const {
    return activationPolicy_.CaptureUndo(interaction, activationPolicy_.Find(reinterpret_cast<std::uintptr_t>(target)));
}

window_activation::Undo WindowManager::CaptureOverlayUndo(HWND target) const {
    return CaptureActivationUndo(overlayInteraction_, target);
}

bool WindowManager::CanUndoActivation(const window_activation::Undo& undo) const {
    const HWND foreground = GetAncestor(GetForegroundWindow(), GA_ROOT);
    return activationPolicy_.CanUndo(undo, activationPolicy_.Find(reinterpret_cast<std::uintptr_t>(foreground)));
}

bool WindowManager::CanContinueActivationUndo(const window_activation::Undo& undo) const {
    return activationPolicy_.CanContinue(undo);
}

bool WindowManager::CanContinueActivationUndo(const window_activation::Undo& undo, HWND intermediary) const {
    const auto foreground = reinterpret_cast<std::uintptr_t>(GetAncestor(GetForegroundWindow(), GA_ROOT));
    return activationPolicy_.CanContinue(undo, activationPolicy_.Find(foreground),
        activationPolicy_.Find(reinterpret_cast<std::uintptr_t>(intermediary)));
}

window_activation::Reference WindowManager::CaptureWindowReference(HWND hwnd) {
    DWORD process = 0;
    const DWORD thread = hwnd ? GetWindowThreadProcessId(hwnd, &process) : 0;
    auto target = RegisterActivationTarget(hwnd);
    if (!target.instance) target.handle = reinterpret_cast<std::uintptr_t>(hwnd);
    return {target, process, thread};
}

bool WindowManager::IsWindowReferenceLive(window_activation::Reference reference) const {
    if (reference.target.instance) return activationPolicy_.IsLive(reference.target);
    if (!reference.process || reference.process == GetCurrentProcessId()) return false;
    DWORD process = 0;
    const DWORD thread = GetWindowThreadProcessId(reinterpret_cast<HWND>(reference.target.handle), &process);
    return thread && thread == reference.thread && process == reference.process;
}

void WindowManager::ClearActivationTargets() {
    while (!activationPolicy_.Targets().empty()) {
        const auto [handle, instance] = *activationPolicy_.Targets().begin();
        activationPolicy_.Forget({handle, instance});
        RemoveWindowSubclass(reinterpret_cast<HWND>(handle), ActivationSubclassProc, kActivationSubclassId);
    }
    pendingSystemCommands_.clear();
    activationPolicy_.Clear();
    overlayInteraction_ = {};
    overlayForegroundReference_ = {};
    overlayInteractionTick_ = 0;
    overlayInteractionForeground_ = nullptr;
}

window_activation::Return WindowManager::CaptureFocusReturn(HWND surface) {
    return activationPolicy_.CaptureReturn(RegisterActivationTarget(surface));
}

bool WindowManager::CanReturnFocus(window_activation::Return ticket) const {
    return activationPolicy_.CanReturn(ticket);
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
    ClearActivationTargets();
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
    api::window::MessagePayload payload;
    payload.sourceWindowId = sourceId;
    payload.message = message;
    return api::emit::SendTo<api::window::events::Message>(targetId, payload);
}

void WindowManager::BroadcastMessage(const std::string& sourceId, const json& message) {
    auto& ctx = WebViewContext::GetInstance();
    
    api::window::MessagePayload payload;
    payload.sourceWindowId = sourceId;
    payload.message = message;

    HWND sourceHwnd = ctx.GetHwndByWindowId(sourceId);
    if (sourceHwnd) {
        api::emit::BroadcastExcept<api::window::events::Message>(payload, sourceHwnd);
    } else {
        // 找不到发送者 HWND，广播到所有窗口
        api::emit::Broadcast<api::window::events::Message>(payload);
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
        ClearActivationTargets();
        activePanelCount_ = 0;
        FB2K_console_formatter() << "[WindowManager] All panels gone, cleaned up";
    }
}

std::string WindowManager::GeneratePanelId() {
    std::lock_guard<std::mutex> lock(mutex_);
    return "panel_" + std::to_string(++nextPanelId_);
}

// KeyboardApi.cpp - Keyboard Hotkey API
// Provides global hotkey registration and keyboard shortcuts

#include "pch.h"
#include "api/KeyboardApi.h"
#include "api/BridgeCore.h"
#include "api/CallerContext.h"
#include "api/EventEmit.h"
#include "api/KeyboardHotkeyRules.h"
#include "api/TypedApi.h"
#include "api/generated/KeyboardSchema.h"
#include "core/WebViewContext.h"
#include <map>
#include <mutex>
#include <set>
#include <utility>
#include <vector>

namespace {
    using json = nlohmann::json;
    namespace keyboard = api::keyboard;
    using keyboard_hotkey_rules::ParseKeyString;

    // 已废弃，不要再调用：热键改注册到组件自己的消息窗口（见下文 EnsureHotkeyWindow）后没有调用方。
    // 取 foobar2000 主窗口句柄，取不到时退回当前活动窗口。
    HWND GetMainWindowHandle() {
        HWND hwnd = core_api::get_main_window();
        if (hwnd) return hwnd;
        return GetActiveWindow();
    }

    //==========================================================================
    // Hotkey management
    //==========================================================================

    struct HotkeyInfo {
        int id;
        std::string key;
        std::string action;
        bool isGlobal;
        UINT modifiers;
        UINT vk;
        HWND callerHwnd = nullptr;     // 注册者的 panel hwnd
        std::string callerWindowId;    // 注册者的 windowId
    };

    std::map<int, HotkeyInfo> g_hotkeys;
    std::map<std::string, HotkeyInfo> g_shortcuts;  // WebView shortcuts (non-global)
    std::mutex g_hotkeyMutex;
    int g_nextHotkeyId = 0x0001;  // Start from 1

    // RegisterHotKey 给应用程序留的 id 范围是 0x0000–0xBFFF。
    constexpr int kMaxHotkeyId = 0xBFFF;

    // 页面的保留键（keyboard.setReservedKeys）：按页面所在的面板窗口归属，元素为 (MOD_* 修饰键, 虚拟键)。
    // 写在 API handler，读在按键转发；两边都在主线程，锁只为与热键表保持同一套约定。
    std::map<HWND, std::set<std::pair<UINT, UINT>>> g_reservedKeys;
    // setReservedKeys 带 all 的页面：全部按键都留给页面，例如录制快捷键期间（规格 §13.1）
    std::set<HWND> g_reservedAllPages;

    //==========================================================================
    // Hotkey message window
    //
    // 热键注册在本组件自己的 message-only 窗口上，WM_HOTKEY 在它的窗口过程里处理。
    // foobar2000 主窗口的窗口过程不归本组件，注册在它上面的热键收不到消息；message_filter
    // 在模态循环里又不执行。窗口与注册都在主线程（handler 经 inMainThread 执行），
    // 满足 RegisterHotKey「窗口属于调用线程」的要求。
    //==========================================================================

    constexpr wchar_t kHotkeyWindowClass[] = L"foo_ui_webview2_HotkeyWindow";
    HWND g_hotkeyWindow = nullptr;
    bool g_hotkeyClassRegistered = false;

    LRESULT CALLBACK HotkeyWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (msg == WM_HOTKEY) {
            ProcessHotkeyMessage(static_cast<int>(wParam));
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    HWND EnsureHotkeyWindow() {
        if (g_hotkeyWindow && IsWindow(g_hotkeyWindow)) return g_hotkeyWindow;
        HINSTANCE instance = core_api::get_my_instance();
        if (!g_hotkeyClassRegistered) {
            WNDCLASSEXW wc{};
            wc.cbSize = sizeof(wc);
            wc.lpfnWndProc = HotkeyWindowProc;
            wc.hInstance = instance;
            wc.lpszClassName = kHotkeyWindowClass;
            if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
                return nullptr;
            }
            g_hotkeyClassRegistered = true;
        }
        g_hotkeyWindow = CreateWindowExW(0, kHotkeyWindowClass, L"", 0, 0, 0, 0, 0,
                                         HWND_MESSAGE, nullptr, instance, nullptr);
        return g_hotkeyWindow;
    }

    // 取一个未被占用的 id；表满时返回 0。调用方持有 g_hotkeyMutex。
    int AllocateHotkeyIdLocked() {
        for (int tries = 0; tries < kMaxHotkeyId; ++tries) {
            if (g_nextHotkeyId < 1 || g_nextHotkeyId > kMaxHotkeyId) g_nextHotkeyId = 1;
            const int id = g_nextHotkeyId++;
            if (!g_hotkeys.contains(id)) return id;
        }
        return 0;
    }

    // 注销并移除满足条件的热键。调用方持有 g_hotkeyMutex。
    template <typename Pred>
    size_t RemoveHotkeysLocked(Pred pred) {
        size_t removed = 0;
        for (auto it = g_hotkeys.begin(); it != g_hotkeys.end();) {
            if (pred(it->second)) {
                if (g_hotkeyWindow) UnregisterHotKey(g_hotkeyWindow, it->first);
                it = g_hotkeys.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }
        return removed;
    }

    //==========================================================================
    // keyboard.registerHotkey - Register global hotkey
    //==========================================================================
    api::Result<keyboard::RegisterHotkeyResult> KeyboardRegisterHotkey(const keyboard::RegisterHotkeyParams& p, const CallerContext& caller) {
        UINT modifiers, vk;
        if (!ParseKeyString(p.key, modifiers, vk) || vk == 0) {
            return api::Fail("Invalid key string: " + p.key, ApiErrorCode::INVALID_PARAMS);
        }
        if (const char* reason = keyboard_hotkey_rules::RejectReason(modifiers, vk)) {
            return api::Fail("Hotkey " + p.key + " " + reason, ApiErrorCode::INVALID_PARAMS);
        }

        HWND hwnd = EnsureHotkeyWindow();
        if (!hwnd) {
            const DWORD error = GetLastError();
            return api::Fail("Hotkey window not available (error " + std::to_string(error) + ")",
                             ApiErrorCode::OPERATION_FAILED, {{"details", json{{"win32Error", error}}}});
        }

        std::lock_guard<std::mutex> lock(g_hotkeyMutex);

        // 系统对同一线程上重复的组合同样报 1409，先查自己的表，才能说清是谁持有。
        for (const auto& [heldId, held] : g_hotkeys) {
            if (held.modifiers == modifiers && held.vk == vk) {
                const std::string holder = held.callerWindowId.empty() ? "an unknown window" : "window " + held.callerWindowId;
                return api::Fail("Hotkey " + p.key + " is already registered by " + holder + " of this component (id " +
                                     std::to_string(heldId) + ")",
                                 ApiErrorCode::OPERATION_FAILED, {{"details", json{{"id", heldId}, {"windowId", held.callerWindowId}}}});
            }
        }

        const int id = AllocateHotkeyIdLocked();
        if (id == 0) {
            return api::Fail("No hotkey id left", ApiErrorCode::OPERATION_FAILED);
        }

        // MOD_NOREPEAT: holding the keys down does not repeat WM_HOTKEY.
        if (!RegisterHotKey(hwnd, id, modifiers | MOD_NOREPEAT, vk)) {
            const DWORD error = GetLastError();
            if (error == ERROR_HOTKEY_ALREADY_REGISTERED) {
                return api::Fail("Hotkey " + p.key + " is held by another program or by a global shortcut of foobar2000",
                                 ApiErrorCode::OPERATION_FAILED, {{"details", json{{"win32Error", error}}}});
            }
            return api::Fail("Failed to register hotkey " + p.key + " (error " + std::to_string(error) + ")",
                             ApiErrorCode::OPERATION_FAILED, {{"details", json{{"win32Error", error}}}});
        }

        // Store hotkey info (capture caller identity for panel-mode routing)
        HotkeyInfo info;
        info.id = id;
        info.key = p.key;
        info.action = p.action;
        info.isGlobal = p.global;
        info.modifiers = modifiers;
        info.vk = vk;
        info.callerHwnd = caller.callerHwnd;
        info.callerWindowId = caller.windowId;

        g_hotkeys[id] = info;

        keyboard::RegisterHotkeyResult result;
        result.id = id;
        return result;
    }

    //==========================================================================
    // keyboard.registerShortcut - Register WebView shortcut (non-global)
    // 已废弃：只登记，没有任何代码按它匹配按键；3.0 移除。
    //==========================================================================
    api::Result<void> KeyboardRegisterShortcut(const keyboard::RegisterShortcutParams& p) {
        UINT modifiers, vk;
        if (!ParseKeyString(p.key, modifiers, vk)) {
            return api::Fail("Invalid key string: " + p.key, ApiErrorCode::INVALID_PARAMS);
        }

        std::lock_guard<std::mutex> lock(g_hotkeyMutex);

        HotkeyInfo info;
        info.id = 0;  // Not a Windows hotkey
        info.key = p.key;
        info.action = p.action;
        info.isGlobal = false;
        info.modifiers = modifiers;
        info.vk = vk;

        g_shortcuts[p.key] = info;

        return api::Ok();
    }

    //==========================================================================
    // keyboard.unregisterHotkey - Unregister a hotkey
    //==========================================================================
    api::Result<void> KeyboardUnregisterHotkey(const keyboard::UnregisterHotkeyParams& p) {
        if (!p.id && !p.key) {
            return api::Fail("id or key is required", ApiErrorCode::INVALID_PARAMS);
        }
        if (p.id && p.key) {
            return api::Fail("id and key cannot both be given", ApiErrorCode::INVALID_PARAMS);
        }

        std::lock_guard<std::mutex> lock(g_hotkeyMutex);

        if (p.id) {
            // Unregister by ID
            auto it = g_hotkeys.find(static_cast<int>(*p.id));
            if (it != g_hotkeys.end()) {
                if (g_hotkeyWindow) {
                    UnregisterHotKey(g_hotkeyWindow, it->first);
                }
                g_hotkeys.erase(it);
                return api::Ok();
            }
        } else {
            // Unregister by key string
            for (auto it = g_hotkeys.begin(); it != g_hotkeys.end(); ++it) {
                if (it->second.key == *p.key) {
                    if (g_hotkeyWindow) {
                        UnregisterHotKey(g_hotkeyWindow, it->first);
                    }
                    g_hotkeys.erase(it);
                    return api::Ok();
                }
            }

            // Also check shortcuts
            auto sit = g_shortcuts.find(*p.key);
            if (sit != g_shortcuts.end()) {
                g_shortcuts.erase(sit);
                return api::Ok();
            }
        }

        return api::Fail("Hotkey not found", ApiErrorCode::NOT_FOUND);
    }

    //==========================================================================
    // keyboard.getRegisteredHotkeys - List all registered hotkeys
    //==========================================================================
    api::Result<keyboard::GetRegisteredHotkeysResult> KeyboardGetRegisteredHotkeys(const keyboard::GetRegisteredHotkeysParams&) {
        std::lock_guard<std::mutex> lock(g_hotkeyMutex);

        keyboard::GetRegisteredHotkeysResult result;
        for (const auto& [id, info] : g_hotkeys) {
            keyboard::KeyboardHotkey row;
            row.id = id;
            row.key = info.key;
            row.action = info.action;
            row.global = info.isGlobal;
            if (!info.callerWindowId.empty()) row.windowId = info.callerWindowId;
            result.hotkeys.push_back(std::move(row));
        }
        for (const auto& [key, info] : g_shortcuts) {
            keyboard::KeyboardHotkey row;
            row.id = 0;
            row.key = info.key;
            row.action = info.action;
            row.global = false;
            result.hotkeys.push_back(std::move(row));
        }
        return result;
    }

    //==========================================================================
    // keyboard.setReservedKeys - Keys that stay with the page
    //==========================================================================
    api::Result<void> KeyboardSetReservedKeys(const keyboard::SetReservedKeysParams& p, const CallerContext& caller) {
        std::set<std::pair<UINT, UINT>> keys;
        for (const std::string& key : p.keys) {
            UINT modifiers, vk;
            if (!ParseKeyString(key, modifiers, vk) || vk == 0) {
                return api::Fail("Invalid key string: " + key, ApiErrorCode::INVALID_PARAMS);
            }
            keys.emplace(modifiers, vk);
        }

        std::lock_guard<std::mutex> lock(g_hotkeyMutex);
        if (keys.empty()) {
            g_reservedKeys.erase(caller.callerHwnd);
        } else {
            g_reservedKeys[caller.callerHwnd] = std::move(keys);
        }
        if (p.all) {
            g_reservedAllPages.insert(caller.callerHwnd);
        } else {
            g_reservedAllPages.erase(caller.callerHwnd);
        }
        return api::Ok();
    }

    //==========================================================================
    // 退出时注销全部热键并销毁消息窗口
    //==========================================================================
    class HotkeyShutdownInitQuit : public initquit {
    public:
        void on_quit() override {
            // 从 on_quit 抛出会打断宿主的关停序列，自己兜住
            try {
                std::lock_guard<std::mutex> lock(g_hotkeyMutex);
                RemoveHotkeysLocked([](const HotkeyInfo&) { return true; });
                g_shortcuts.clear();
                g_reservedKeys.clear();
                g_reservedAllPages.clear();
                if (g_hotkeyWindow) {
                    DestroyWindow(g_hotkeyWindow);
                    g_hotkeyWindow = nullptr;
                }
                if (g_hotkeyClassRegistered) {
                    UnregisterClassW(kHotkeyWindowClass, core_api::get_my_instance());
                    g_hotkeyClassRegistered = false;
                }
            } catch (...) {
            }
        }
    };

    static initquit_factory_t<HotkeyShutdownInitQuit> g_hotkey_shutdown_initquit;

} // anonymous namespace

//==========================================================================
// Process hotkey message from WM_HOTKEY
//==========================================================================
void ProcessHotkeyMessage(int id) {
    keyboard::HotkeyPayload payload;
    std::string callerWindowId;
    HWND callerHwnd = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_hotkeyMutex);
        auto it = g_hotkeys.find(id);
        if (it == g_hotkeys.end()) return;
        payload.id = id;
        payload.key = it->second.key;
        payload.action = it->second.action;
        callerWindowId = it->second.callerWindowId;
        callerHwnd = it->second.callerHwnd;
    }

    // 投递时不持锁：页面收到事件后可能立即调用本命名空间的方法。
    // 路由到注册此 hotkey 的调用者实例；两级都找不到时落到单例 bridge（主窗口的页面）。
    // 窗口关闭与页面导航会注销它名下的热键（CleanupPageKeyboard），正常情况下前两级总能命中。
    using Hotkey = keyboard::events::Hotkey;
    auto& wvc = WebViewContext::GetInstance();
    bool sent = false;
    if (!callerWindowId.empty()) {
        sent = api::emit::SendTo<Hotkey>(callerWindowId, payload);
    }
    if (!sent && callerHwnd) {
        if (auto* bridge = wvc.GetBridge(callerHwnd)) {
            api::emit::Emit<Hotkey>(*bridge, payload);
            sent = true;
        }
    }
    if (!sent) {
        api::emit::Emit<Hotkey>(BridgeCore::GetInstance(), payload);
    }
}

//==========================================================================
// Release what a page registered: its hotkeys and its reserved keys
//==========================================================================
void CleanupPageKeyboard(HWND page) {
    if (!page) return;
    std::lock_guard<std::mutex> lock(g_hotkeyMutex);
    RemoveHotkeysLocked([page](const HotkeyInfo& info) { return info.callerHwnd == page; });
    g_reservedKeys.erase(page);
    g_reservedAllPages.erase(page);
}

bool IsReservedKey(HWND page, UINT vk, UINT modifiers) {
    std::lock_guard<std::mutex> lock(g_hotkeyMutex);
    if (g_reservedAllPages.contains(page)) return true;
    const auto it = g_reservedKeys.find(page);
    if (it == g_reservedKeys.end()) return false;
    const UINT mods = modifiers & (MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_WIN);
    return it->second.contains({mods, vk});
}

//==========================================================================
// Register Keyboard API
//==========================================================================
void RegisterKeyboardApi() {
    // keyboard.registerHotkey - Register global hotkey
    api::RegisterApi("keyboard.registerHotkey", KeyboardRegisterHotkey);

    // keyboard.registerShortcut - Register WebView shortcut
    api::RegisterApi("keyboard.registerShortcut", KeyboardRegisterShortcut);

    // keyboard.unregisterHotkey - Unregister hotkey
    api::RegisterApi("keyboard.unregisterHotkey", KeyboardUnregisterHotkey);

    // keyboard.getRegisteredHotkeys - List registered hotkeys
    api::RegisterApi("keyboard.getRegisteredHotkeys", KeyboardGetRegisteredHotkeys);

    // keyboard.setReservedKeys - Keys that stay with the page
    api::RegisterApi("keyboard.setReservedKeys", KeyboardSetReservedKeys);

    LOG("Keyboard API registered (5 APIs)");
}

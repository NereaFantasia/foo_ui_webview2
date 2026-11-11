#include "pch.h"
#include "webview/HostShortcutRelay.h"
#include "api/KeyboardApi.h"
#include "prefs/PreferencesPage.h"
#include "settings/SecurityConfig.h"
#include <imm.h>

#pragma comment(lib, "imm32.lib")

namespace host_shortcut_relay {

namespace {

bool IsDown(int vk) { return (GetKeyState(vk) & 0x8000) != 0; }

bool Execute(HWND page, UINT vk, UINT mods) {
    if (IsReservedKey(page, vk, mods)) return false;
    keyboard_shortcut_manager_v2::ptr manager;
    if (!keyboard_shortcut_manager_v2::tryGet(manager)) return false;
    // 调用方是 WebView2 的 COM 回调与窗口过程，异常不能穿出去
    try {
        return manager->process_keydown_simple(KeyCode(vk, mods));
    } catch (const std::exception& e) {
        LOG("Shortcut relay: command failed: ", e.what());
    } catch (...) {
        LOG("Shortcut relay: command failed");
    }
    return false;
}

}  // namespace

UINT CurrentModifiers() {
    UINT mods = 0;
    if (IsDown(VK_CONTROL)) mods |= MOD_CONTROL;
    if (IsDown(VK_MENU)) mods |= MOD_ALT;
    if (IsDown(VK_SHIFT)) mods |= MOD_SHIFT;
    if (IsDown(VK_LWIN) || IsDown(VK_RWIN)) mods |= MOD_WIN;
    return mods;
}

UINT HostWindowKey(HWND hwnd, WPARAM wParam) {
    const UINT vk = static_cast<UINT>(wParam);
    if (vk != VK_PROCESSKEY) return vk;
    const UINT real = ImmGetVirtualKey(hwnd);
    return real != 0 ? real : vk;
}

bool TryRelay(HWND page, UINT vk, UINT mods, bool isRepeat) {
    // 按住不放时只在第一次按下执行，否则按住 Ctrl+N 会反复新建播放列表。
    if (isRepeat) return false;
    const bool typing = keyboard_shortcut_manager::is_typing_key_combo(vk, mods);
    if (!ShouldRelay(vk, mods, webview_prefs::GetShortcutRelayCategories(), security_config::UseDevServer(), typing)) {
        return false;
    }
    return Execute(page, vk, mods);
}

bool TryRelayFromPage(HWND page, UINT vk, UINT mods) {
    const bool textKey = keyboard_shortcut_manager::is_text_key(vk);
    if (!ShouldRelayFromPage(vk, mods, webview_prefs::GetShortcutRelayCategories(), textKey)) return false;
    return Execute(page, vk, mods);
}

bool TryRelayFromHostWindow(HWND page, WPARAM rawKey, UINT vk, UINT mods, bool isRepeat) {
    if (TryRelay(page, vk, mods, isRepeat)) return true;
    if (isRepeat || rawKey == VK_PROCESSKEY) return false;
    return TryRelayFromPage(page, vk, mods);
}

}  // namespace host_shortcut_relay

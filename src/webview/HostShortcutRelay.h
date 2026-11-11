#pragma once

// WebView 内按键转交 foobar2000 快捷键（热键转发规格 §5）。
//
// 页面或宿主窗口有焦点时，按键经两个入口之一到达：WebView 的 AcceleratorKeyPressed，或宿主窗口的
// WM_KEYDOWN / WM_SYSKEYDOWN。实测两者互斥（规格 §10.1），都调 TryRelay。范围内、又不属于文本编辑
// 与组件自带按键的组合，交给 keyboard_shortcut_manager_v2::process_keydown_simple；它只在该组合
// 确有绑定时执行并返回 true，没有绑定的组合仍交给页面。
//
// 上半部分是不碰系统与 SDK 的纯规则，供 gtest 直接测；下半部分是运行时入口，只在主线程调用。

#include <windows.h>
#include <commctrl.h>

namespace host_shortcut_relay {

// 转发范围的类别，存为位掩码，由首选项 Window 子页的开关定义。
enum Category : unsigned {
    kCtrl = 1u << 0,   // 带 Ctrl 的组合
    kWin = 1u << 1,    // 带 Win 的组合
    kAlt = 1u << 2,    // 带 Alt 的组合
    kFKeys = 1u << 3,  // F1–F24，含它们与任意修饰键的组合
    kMedia = 1u << 4,  // 媒体键；Windows 另以 WM_APPCOMMAND 送达，默认关
    kBare = 1u << 5,   // 不带 Ctrl、Alt、Win 的其余非字符键
};
inline constexpr unsigned kDefaultCategories = kCtrl | kWin | kAlt | kFKeys;
inline constexpr unsigned kAllCategories = kCtrl | kWin | kAlt | kFKeys | kMedia | kBare;

inline bool IsModifierKey(UINT vk) {
    switch (vk) {
    case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
    case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
    case VK_MENU: case VK_LMENU: case VK_RMENU:
    case VK_LWIN: case VK_RWIN:
        return true;
    default:
        return false;
    }
}

inline bool IsFunctionKey(UINT vk) { return vk >= VK_F1 && vk <= VK_F24; }

inline bool IsMediaKey(UINT vk) {
    return (vk >= VK_VOLUME_MUTE && vk <= VK_MEDIA_PLAY_PAUSE);
}

// 组合是否落在用户选定的范围里。修饰键 mods 取 MOD_* 标志。类别取并集：Ctrl+F5 在 ctrl 或 fKeys
// 任一打开时都算在内。
inline bool InScope(UINT vk, UINT mods, unsigned categories) {
    if ((mods & MOD_CONTROL) && (categories & kCtrl)) return true;
    if ((mods & MOD_ALT) && (categories & kAlt)) return true;
    if ((mods & MOD_WIN) && (categories & kWin)) return true;
    if (IsFunctionKey(vk)) return (categories & kFKeys) != 0;
    if (IsMediaKey(vk)) return (categories & kMedia) != 0;
    if ((mods & (MOD_CONTROL | MOD_ALT | MOD_WIN)) == 0) return (categories & kBare) != 0;
    return false;
}

// 范围之内仍留给页面的组合（规格 §5.1 的排除清单）。typingCombo 由调用方用
// keyboard_shortcut_manager::is_typing_key_combo 算好传入，纯规则这里不依赖 SDK。
inline bool IsExcluded(UINT vk, UINT mods, bool devServer, bool typingCombo) {
    const UINT m = mods & (MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_WIN);
    if (vk == VK_PROCESSKEY || IsModifierKey(vk)) return true;
    // 文本编辑：删除与取消一律留给页面。
    if (vk == VK_BACK || vk == VK_DELETE || vk == VK_ESCAPE) return true;
    // 光标移动与回车：带 Alt 或 Win 且不带 Ctrl 时才转（Alt+Enter、Alt+方向键）。
    switch (vk) {
    case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN:
    case VK_HOME: case VK_END: case VK_RETURN:
        if ((m & MOD_CONTROL) || (m & (MOD_ALT | MOD_WIN)) == 0) return true;
        break;
    default:
        break;
    }
    if (m == MOD_CONTROL) {
        switch (vk) {
        case 'A': case 'C': case 'V': case 'X': case 'Y': case 'Z': case VK_INSERT:
            return true;
        default:
            break;
        }
    }
    if (m == (MOD_CONTROL | MOD_SHIFT) && vk == 'Z') return true;
    if (typingCombo) return true;
    // 组件自带的按键优先于宿主绑定（规格 §5.6）。
    if (vk == VK_F5) return true;
    if (vk == VK_F12 && devServer) return true;
    return false;
}

inline bool ShouldRelay(UINT vk, UINT mods, unsigned categories, bool devServer, bool typingCombo) {
    return InScope(vk, mods, categories) && !IsExcluded(vk, mods, devServer, typingCombo);
}

// 页面入口键（规格 §12.2）：ShouldRelay 只因文本编辑而留给页面的那部分。页面判断得出焦点是否在可编辑
// 元素里，不在时把它们交回来。textKey 由调用方用 keyboard_shortcut_manager::is_text_key 算好传入
inline bool IsPageEntryKey(UINT vk, UINT mods, bool textKey) {
    const UINT m = mods & (MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_WIN);
    if ((m & (MOD_ALT | MOD_WIN)) != 0) return false;
    const bool ctrl = (m & MOD_CONTROL) != 0;
    switch (vk) {
    case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN:
    case VK_HOME: case VK_END:
        return true;
    case VK_PRIOR: case VK_NEXT:
        return !ctrl;  // Ctrl+PageUp、Ctrl+PageDown 已由 ShouldRelay 转发
    default:
        return textKey && !ctrl;
    }
}

// 类别与 InScope 相同：Ctrl+导航键归 kCtrl，其余页面入口键归 kBare
inline bool ShouldRelayFromPage(UINT vk, UINT mods, unsigned categories, bool textKey) {
    return IsPageEntryKey(vk, mods, textKey) && InScope(vk, mods, categories);
}

// foobar2000 快捷键的按键码：低 8 位为虚拟键，修饰键为 HOTKEYF_* 左移 8 位，Win 记作 HOTKEYF_EXT。
// 与 SDK menu_manager.cpp 的 get_key_code 一致。
inline UINT KeyCode(UINT vk, UINT mods) {
    UINT code = vk & 0xFF;
    if (mods & MOD_CONTROL) code |= HOTKEYF_CONTROL << 8;
    if (mods & MOD_SHIFT) code |= HOTKEYF_SHIFT << 8;
    if (mods & MOD_ALT) code |= HOTKEYF_ALT << 8;
    if (mods & MOD_WIN) code |= HOTKEYF_EXT << 8;
    return code;
}

// ---- 运行时入口（主线程） ----

// 当前按住的修饰键，MOD_* 标志。
UINT CurrentModifiers();

// 宿主窗口收到的键码。输入法开着时 WM_KEYDOWN 多为 VK_PROCESSKEY，这里还原成真实键码；还原不出时
// 原样返回 VK_PROCESSKEY，交给排除规则。
UINT HostWindowKey(HWND hwnd, WPARAM wParam);

// 按规则判断并交给 foobar2000 快捷键。返回 true 表示确有绑定并已执行，调用方应吞掉这次按键。
// page 是页面所在的面板窗口，用来查页面的保留键（keyboard.setReservedKeys）。重复按键不转。
// 执行的命令可能销毁当前窗口，返回后调用方不得再访问面板对象。
bool TryRelay(HWND page, UINT vk, UINT mods, bool isRepeat);

// 页面入口：页面没处理按键、焦点又不在可编辑元素时同步调来，只接 ShouldRelayFromPage 的键。返回值同
// TryRelay，页面据此取消默认动作。重复按键与输入法组字由页面过滤
bool TryRelayFromPage(HWND page, UINT vk, UINT mods);

// 宿主窗口的 WM_KEYDOWN：焦点在宿主窗口时没有可编辑元素，TryRelay 之外再按页面入口键转发。
// rawKey 是消息原始的 wParam，为 VK_PROCESSKEY 时输入法正在用这个键，不按页面入口键转
bool TryRelayFromHostWindow(HWND page, WPARAM rawKey, UINT vk, UINT mods, bool isRepeat);

}  // namespace host_shortcut_relay

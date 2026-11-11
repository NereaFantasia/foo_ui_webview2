#pragma once

#include "BridgeCore.h"

// Register all keyboard related APIs
// keyboard.registerHotkey, keyboard.registerShortcut, keyboard.unregisterHotkey, keyboard.getRegisteredHotkeys,
// keyboard.setReservedKeys
/** @brief Register the keyboard.* API handlers. */
void RegisterKeyboardApi();

// WM_HOTKEY of the component's hotkey window: emits keyboard:hotkey to the registering window.
void ProcessHotkeyMessage(int id);

// Releases every hotkey and reserved key the page in this panel window registered. Called when the
// page starts a top-level navigation, when the window is destroyed and when its WebView process dies.
// Main thread.
void CleanupPageKeyboard(HWND page);

// Whether the page in this panel window reserved the combination, or every key, with
// keyboard.setReservedKeys, so the key stays with the page instead of going to a foobar2000 shortcut.
// modifiers are MOD_* flags.
bool IsReservedKey(HWND page, UINT vk, UINT modifiers);

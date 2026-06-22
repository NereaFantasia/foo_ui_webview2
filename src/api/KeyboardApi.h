#pragma once

#include "BridgeCore.h"

// Register all keyboard related APIs
// keyboard.registerHotkey, keyboard.registerShortcut, keyboard.unregisterHotkey, keyboard.getRegisteredHotkeys
/** @brief Register the keyboard.* API handlers. */
void RegisterKeyboardApi();

// Process hotkey message from main window
void ProcessHotkeyMessage(int id);

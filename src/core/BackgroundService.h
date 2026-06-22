// ============================================
// BackgroundService.h - Background Mode Support
// ============================================
// Enables WebView2 UI to run in background when using other UIs (Default UI, etc.)
// This allows API access without the full WebView2 UI being the active interface.

#pragma once
#include "pch.h"
#include <memory>

// Forward declarations
class MainWindow;

namespace background_service {

// Check if background mode is enabled in settings
bool IsBackgroundModeEnabled();

// Check if WebView2 UI is the active user interface
bool IsWebViewUIActive();

// Initialize background service (called on foobar2000 startup)
void Initialize();

// Shutdown background service (called on foobar2000 exit)
void Shutdown();

// Get the background window instance (may be hidden)
MainWindow* GetBackgroundWindow();

// Show the background window
void ShowWindow();

// Hide the background window
void HideWindow();

// Toggle window visibility
void ToggleWindow();

// Check if background window exists and is valid
bool HasBackgroundWindow();

} // namespace background_service

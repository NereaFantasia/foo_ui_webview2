#pragma once
// MainWindowInternal.h — 拆分 TU 共享的非成员辅助函数声明
// 仅供 MainWindow*.cpp 内部使用，请勿从 src/window/ 外部包含

#include <string>
#include <windows.h>
#include <dwmapi.h>

namespace mainwindow_detail {

// -- DWM stealth wrappers --
HRESULT S_DwmSetWindowAttribute(HWND h, DWORD a, LPCVOID d, DWORD s);
HRESULT S_DwmGetWindowAttribute(HWND h, DWORD a, LPVOID d, DWORD s);
HRESULT S_DwmExtendFrameIntoClientArea(HWND h, const MARGINS* m);
HRESULT S_DwmFlush();

// -- DWMWA / DWMSBT constants --
constexpr DWORD DWMWA_SYSTEMBACKDROP_TYPE_V     = 38;
constexpr DWORD DWMWA_USE_IMMERSIVE_DARK_MODE_V = 20;
constexpr int   DWMSBT_NONE_V            = 1;
constexpr int   DWMSBT_MAINWINDOW_V      = 2;
constexpr int   DWMSBT_TRANSIENTWINDOW_V = 3;
constexpr int   DWMSBT_TABBEDWINDOW_V    = 4;

// -- Dark mode helpers --
void InitDarkModeImports();
void SetAppDarkMode(bool bDark);
void AllowDarkModeForWindow(HWND hwnd, bool bDark);
bool IsFoobar2000DarkMode();

// -- Evidence logging --
void S_EmitEvidenceLine(const std::string& line);

// -- Backdrop helpers --
bool        S_IsPluginManagedBackdropEffect(const std::string& effect);
std::string S_GetUserBackdropEffectString();

// -- Resource paths --
std::wstring GetComponentDirectory();
std::wstring GetFrontendResourcesDir();

} // namespace mainwindow_detail

/**
 * PanelDpi - 面板配置对话框与编辑模式占位符共用的 DPI 辅助
 */

#pragma once

namespace panel_dpi {

// 窗口所在的 DPI；取不到时退回设备 DPI，再取不到按 96。
inline int GetDpi(HWND hwnd) {
    typedef UINT (WINAPI *GetDpiForWindowFunc)(HWND);
    static GetDpiForWindowFunc pGetDpiForWindow = nullptr;
    static bool tried = false;

    if (!tried) {
        tried = true;
        HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
        if (hUser32) {
            pGetDpiForWindow = (GetDpiForWindowFunc)GetProcAddress(hUser32, "GetDpiForWindow");
        }
    }

    if (pGetDpiForWindow && hwnd) {
        UINT dpi = pGetDpiForWindow(hwnd);
        if (dpi > 0) return (int)dpi;
    }

    HDC hdc = GetDC(hwnd);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(hwnd, hdc);
    return dpi > 0 ? dpi : 96;
}

// 把按 96 DPI 给出的像素值换算到 dpi。
inline int Scale(int value, int dpi) {
    return MulDiv(value, dpi, 96);
}

} // namespace panel_dpi

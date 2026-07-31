#include "pch.h"
#include "window/WindowDpiProbe.h"
#include "window/WindowGeometryMath.h"

namespace window_dpi_probe {

int GetDpiForScreenRect(int x, int y, int width, int height) {
    using GetDpiForMonitorFunc = HRESULT (WINAPI*)(HMONITOR, int, UINT*, UINT*);

    // 一次性解析，失败后不再重试（缺失即系统不支持，重试无意义）。
    static GetDpiForMonitorFunc pGetDpiForMonitor = []() -> GetDpiForMonitorFunc {
        HMODULE shcore = LoadLibraryW(L"shcore.dll");
        if (!shcore) return nullptr;
        return reinterpret_cast<GetDpiForMonitorFunc>(
            GetProcAddress(shcore, "GetDpiForMonitor"));
    }();

    if (!pGetDpiForMonitor) return window_geometry::kBaselineDpi;

    // 宽高非正时退化为「以左上角定位显示器」，避免构造空/反向矩形。
    RECT rc{
        x,
        y,
        x + (width > 0 ? width : 1),
        y + (height > 0 ? height : 1)
    };

    HMONITOR mon = MonitorFromRect(&rc, MONITOR_DEFAULTTONEAREST);
    if (!mon) return window_geometry::kBaselineDpi;

    UINT dpiX = 0;
    UINT dpiY = 0;
    // 第二参数为 MONITOR_DPI_TYPE::MDT_EFFECTIVE_DPI == 0。
    // 用字面量而非枚举，以免为此引入 shellscalingapi.h 依赖。
    if (FAILED(pGetDpiForMonitor(mon, 0, &dpiX, &dpiY)) || dpiX == 0) {
        return window_geometry::kBaselineDpi;
    }

    return static_cast<int>(dpiX);
}

}  // namespace window_dpi_probe

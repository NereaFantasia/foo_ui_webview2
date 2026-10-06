// MaximizeButtonRegion.cpp
#include "pch.h"
#include "window/MaximizeButtonRegion.h"

namespace maximize_button_region {

std::optional<Rect> ToPhysical(int x, int y, int width, int height, double scale) {
    Rect rect;
    rect.x = static_cast<int>(x * scale);
    rect.y = static_cast<int>(y * scale);
    rect.width = static_cast<int>(width * scale);
    rect.height = static_cast<int>(height * scale);
    if (rect.width <= 0 || rect.height <= 0) {
        return std::nullopt;
    }
    return rect;
}

bool Contains(const Rect& rect, int clientX, int clientY) {
    return clientX >= rect.x && clientX < rect.x + rect.width &&
           clientY >= rect.y && clientY < rect.y + rect.height;
}

bool ShouldAnswerMaximizeButton(const Conditions& conditions) {
    return conditions.snapLayoutsSupported && conditions.frameless && conditions.resizable &&
           !conditions.fullscreen && conditions.webViewForwarding;
}

bool SnapLayoutsSupported() noexcept {
    static const bool supported = [] {
        // RtlGetVersion reports the real build; GetVersionEx answers whatever the
        // host executable's manifest declares.
        using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
        const HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
        const auto getVersion = ntdll ? reinterpret_cast<RtlGetVersionFn>(
                                            ::GetProcAddress(ntdll, "RtlGetVersion"))
                                      : nullptr;
        if (!getVersion) {
            return false;
        }
        RTL_OSVERSIONINFOW info = {};
        info.dwOSVersionInfoSize = sizeof(info);
        return getVersion(&info) == 0 && info.dwMajorVersion >= 10 &&
               info.dwBuildNumber >= 22000;
    }();
    return supported;
}

}  // namespace maximize_button_region

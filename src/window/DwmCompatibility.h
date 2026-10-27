#pragma once

#include <Windows.h>
#include <winternl.h>
#include <string_view>

namespace dwm_compatibility {

struct Capabilities {
    DWORD build = 0;

    constexpr bool Mica() const noexcept { return build >= 22000; }
    constexpr bool SystemBackdrop() const noexcept { return build >= 22621; }
    constexpr bool RoundedCorners() const noexcept { return build >= 22000; }

    constexpr bool SupportsAttribute(DWORD attribute) const noexcept {
        if (attribute >= 33 && attribute <= 37) return RoundedCorners();
        if (attribute == 38) return SystemBackdrop();
        if (attribute == 1029) return Mica();
        return true;
    }

    constexpr std::string_view Backdrop(std::string_view effect) const noexcept {
        if (effect == "mica") return Mica() ? effect : "none";
        if (effect == "mica-alt") return SystemBackdrop() ? effect : Mica() ? "mica" : "none";
        if (effect == "acrylic") return SystemBackdrop() ? effect : "none";
        return effect;
    }
};

inline const Capabilities& Current() noexcept {
    static const Capabilities capabilities = [] {
        // RtlGetVersion 不受宿主可执行文件兼容性清单影响；读取失败时不尝试材质。
        using GetVersion = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
        const HMODULE module = ::GetModuleHandleW(L"ntdll.dll");
        const auto getVersion = module
            ? reinterpret_cast<GetVersion>(::GetProcAddress(module, "RtlGetVersion"))
            : nullptr;
        RTL_OSVERSIONINFOW info = {};
        info.dwOSVersionInfoSize = sizeof(info);
        return getVersion && getVersion(&info) == 0 && info.dwMajorVersion >= 10
            ? Capabilities{info.dwBuildNumber}
            : Capabilities{};
    }();
    return capabilities;
}

}  // namespace dwm_compatibility

// MainWindowInternal.cpp — MainWindowInternal.h 所声明辅助函数的实现
// 仅供 MainWindow*.cpp 使用
#include "pch.h"
#include "window/MainWindowInternal.h"
#include "window/WindowChromeTrace.h"
#include "prefs/PreferencesPage.h"
#include <iomanip>
#include <sstream>

using json = nlohmann::json;

// ============================================
// 深色模式辅助函数 (使用 Windows 未公开 API)
// ============================================
namespace mainwindow_detail {
    // DWM wrappers
    HRESULT S_DwmSetWindowAttribute(HWND h, DWORD a, LPCVOID d, DWORD s) {
        return ::DwmSetWindowAttribute(h, a, d, s);
    }
    HRESULT S_DwmGetWindowAttribute(HWND h, DWORD a, LPVOID d, DWORD s) {
        return ::DwmGetWindowAttribute(h, a, d, s);
    }
    HRESULT S_DwmExtendFrameIntoClientArea(HWND h, const MARGINS* m) {
        return ::DwmExtendFrameIntoClientArea(h, m);
    }
    HRESULT S_DwmFlush() {
        return ::DwmFlush();
    }
    
    enum class PreferredAppMode {
        Default,
        AllowDark,
        ForceDark,
        ForceLight,
        Max
    };
    
    using fnSetPreferredAppMode = PreferredAppMode(WINAPI*)(PreferredAppMode appMode);
    using fnAllowDarkModeForWindow = bool(WINAPI*)(HWND hWnd, bool allow);
    using fnFlushMenuThemes = void(WINAPI*)();
    
    fnSetPreferredAppMode _SetPreferredAppMode = nullptr;
    fnAllowDarkModeForWindow _AllowDarkModeForWindow = nullptr;
    fnFlushMenuThemes _FlushMenuThemes = nullptr;
    bool g_darkModeImportsInitialized = false;
    
    void InitDarkModeImports() {
        if (g_darkModeImportsInitialized) return;
        g_darkModeImportsInitialized = true;
        
        HMODULE hUxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (hUxtheme) {
            _SetPreferredAppMode = reinterpret_cast<fnSetPreferredAppMode>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135)));
            _AllowDarkModeForWindow = reinterpret_cast<fnAllowDarkModeForWindow>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(133)));
            _FlushMenuThemes = reinterpret_cast<fnFlushMenuThemes>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(136)));
        }
    }
    
    void SetAppDarkMode(bool bDark) {
        InitDarkModeImports();
        if (_SetPreferredAppMode) {
            static PreferredAppMode lastMode = PreferredAppMode::Default;
            PreferredAppMode wantMode = bDark ? PreferredAppMode::ForceDark : PreferredAppMode::ForceLight;
            if (lastMode != wantMode) {
                _SetPreferredAppMode(wantMode);
                lastMode = wantMode;
                if (_FlushMenuThemes) _FlushMenuThemes();
            }
        }
    }
    
    void AllowDarkModeForWindow(HWND hwnd, bool bDark) {
        InitDarkModeImports();
        if (_AllowDarkModeForWindow) {
            _AllowDarkModeForWindow(hwnd, bDark);
        }
    }

    bool S_IsPluginManagedBackdropEffect(const std::string& effect) {
        return effect == "mica" || effect == "mica-alt" || effect == "acrylic";
    }
    
    // 检测 foobar2000 是否处于深色模式
    bool IsFoobar2000DarkMode() {
        auto api = ui_config_manager::tryGet();
        if (api.is_valid()) {
            return api->is_dark_mode();
        }
        return false;  // 默认浅色
    }
    // DWMWA / DWMSBT constants -> MainWindowInternal.h
    constexpr const char* S_STARTUP_PROBE_VERSION = "startup-probe-20260320b";

    static std::string S_FormatFileTimeUtc(const FILETIME& fileTime) {
        SYSTEMTIME systemTime{};
        if (!FileTimeToSystemTime(&fileTime, &systemTime)) {
            return "-";
        }

        std::ostringstream stream;
        stream << std::setfill('0')
               << std::setw(4) << systemTime.wYear << '-'
               << std::setw(2) << systemTime.wMonth << '-'
               << std::setw(2) << systemTime.wDay << 'T'
               << std::setw(2) << systemTime.wHour << ':'
               << std::setw(2) << systemTime.wMinute << ':'
               << std::setw(2) << systemTime.wSecond << '.'
               << std::setw(3) << systemTime.wMilliseconds << 'Z';
        return stream.str();
    }

    void S_LogStartupProbeIdentity(const char* phase, HWND hwnd) {
        wchar_t modulePath[MAX_PATH] = {};
        std::string modulePathUtf8 = "-";
        std::string moduleWriteTimeUtc = "-";
        unsigned long long moduleSize = 0;

        HMODULE hModule = core_api::get_my_instance();
        if (GetModuleFileNameW(hModule, modulePath, MAX_PATH) > 0) {
            modulePathUtf8 = pfc::stringcvt::string_utf8_from_wide(modulePath).get_ptr();

            WIN32_FILE_ATTRIBUTE_DATA attributes{};
            if (GetFileAttributesExW(modulePath, GetFileExInfoStandard, &attributes)) {
                moduleWriteTimeUtc = S_FormatFileTimeUtc(attributes.ftLastWriteTime);
                moduleSize = (static_cast<unsigned long long>(attributes.nFileSizeHigh) << 32) |
                    attributes.nFileSizeLow;
            }
        }

        std::ostringstream stream;
        stream << "[StartupProbe]"
               << " t=" << WindowChromeTrace::RelativeMs() << "ms"
               << " phase=" << (phase && phase[0] ? phase : "-")
               << " probeVersion=" << S_STARTUP_PROBE_VERSION
               << " pid=" << GetCurrentProcessId()
               << " hwnd=0x" << pfc::format_hex((size_t)hwnd)
               << " modulePath=" << modulePathUtf8.c_str()
               << " moduleSize=" << moduleSize
               << " moduleWriteTimeUtc=" << moduleWriteTimeUtc.c_str();
        S_EmitEvidenceLine(stream.str());
    }

    std::string S_GetUserBackdropEffectString() {
        switch (webview_prefs::GetBackdropEffect()) {
            case webview_prefs::BackdropEffect::None:
                return "none";
            case webview_prefs::BackdropEffect::Acrylic:
                return "acrylic";
            case webview_prefs::BackdropEffect::MicaAlt:
            case webview_prefs::BackdropEffect::Tabbed:
                return "mica-alt";
            case webview_prefs::BackdropEffect::Mica:
            default:
                return "mica";
        }
    }

    std::string S_FormatNativeRect(const RECT& rect) {
        return std::string("(") + std::to_string(rect.left) + "," +
            std::to_string(rect.top) + "," +
            std::to_string(rect.right) + "," +
            std::to_string(rect.bottom) + ")";
    }

    const char* S_GetSizingEdgeName(WPARAM edge) {
        switch (edge) {
            case WMSZ_LEFT: return "left";
            case WMSZ_RIGHT: return "right";
            case WMSZ_TOP: return "top";
            case WMSZ_TOPLEFT: return "top-left";
            case WMSZ_TOPRIGHT: return "top-right";
            case WMSZ_BOTTOM: return "bottom";
            case WMSZ_BOTTOMLEFT: return "bottom-left";
            case WMSZ_BOTTOMRIGHT: return "bottom-right";
            default: return "unknown";
        }
    }

    std::string S_FormatWindowPosFlags(UINT flags) {
        struct FlagName {
            UINT flag;
            const char* name;
        };

        static const std::array<FlagName, 13> kFlagNames = {{
            { SWP_NOSIZE, "NOSIZE" },
            { SWP_NOMOVE, "NOMOVE" },
            { SWP_NOZORDER, "NOZORDER" },
            { SWP_NOREDRAW, "NOREDRAW" },
            { SWP_NOACTIVATE, "NOACTIVATE" },
            { SWP_FRAMECHANGED, "FRAMECHANGED" },
            { SWP_SHOWWINDOW, "SHOWWINDOW" },
            { SWP_HIDEWINDOW, "HIDEWINDOW" },
            { SWP_NOCOPYBITS, "NOCOPYBITS" },
            { SWP_NOOWNERZORDER, "NOOWNERZORDER" },
            { SWP_NOSENDCHANGING, "NOSENDCHANGING" },
            { SWP_DEFERERASE, "DEFERERASE" },
            { SWP_ASYNCWINDOWPOS, "ASYNCWINDOWPOS" },
        }};

        std::string result;
        for (const auto& flagName : kFlagNames) {
            if ((flags & flagName.flag) == 0) {
                continue;
            }
            if (!result.empty()) {
                result += '|';
            }
            result += flagName.name;
        }

        if (result.empty()) {
            result = "0";
        }
        return result;
    }

    std::string S_FormatHex32(uint32_t value) {
        std::ostringstream stream;
        stream << pfc::format_hex(value);
        return stream.str();
    }

    std::string S_FormatDomRect(const json& rect) {
        if (!rect.is_object()) {
            return "N/A";
        }

        std::ostringstream stream;
        stream << "(" << rect.value("left", 0.0)
               << "," << rect.value("top", 0.0)
               << "," << rect.value("width", 0.0)
               << "," << rect.value("height", 0.0)
               << ")";
        return stream.str();
    }

    std::string S_TruncateUtf8(const std::string& value, size_t maxLength) {
        if (value.size() <= maxLength) {
            return value;
        }
        return value.substr(0, maxLength) + "...";
    }

    bool S_ParseExecuteScriptPayload(const std::wstring& resultJson, json& out) {
        try {
            std::string utf8(pfc::stringcvt::string_utf8_from_wide(resultJson.c_str()).get_ptr());
            auto parsed = json::parse(utf8);
            if (parsed.is_string()) {
                out = json::parse(parsed.get<std::string>());
            } else {
                out = std::move(parsed);
            }
            return true;
        } catch (...) {
            return false;
        }
    }

    void S_EmitEvidenceLine(const std::string& line) {
        WindowChromeTrace::EmitAuxiliaryLine(line);
    }

} // namespace mainwindow_detail

/**
 * WebViewHostDiagnostics.cpp - WebViewHost 的诊断日志与页面健康探测
 *
 * profile 目录下的生命周期日志（达到上限轮转）与进程崩溃日志，以及 ProbePageHealth。
 */
#include "pch.h"
#include "webview/WebViewHost.h"
#include "webview/WebViewEnvironment.h"
#include "window/DwmCompatibility.h"
#include "version.h"
#include <wrl/event.h>
#include <algorithm>
#include <fstream>     // 崩溃诊断日志落盘 (webview_crash.log)
#include <ctime>       // 崩溃日志时间戳
#include <iomanip>     // 生命周期日志 UTC 时间戳
#include <mutex>       // 生命周期日志跨回调串行写盘

using namespace Microsoft::WRL;

namespace {
    constexpr unsigned long long kLifecycleLogMaxBytes = 4ULL * 1024ULL * 1024ULL;

    std::string SanitizeLifecycleLogLine(const std::string& line) {
        std::string sanitized = line;
        std::ranges::replace(sanitized, '\r', ' ');
        std::ranges::replace(sanitized, '\n', ' ');
        return sanitized;
    }

    std::wstring GetProfileLogPath(const wchar_t* fileName) {
        pfc::string8 profilePath;
        filesystem::g_get_display_path(core_api::get_profile_path(), profilePath);
        std::wstring path =
            pfc::stringcvt::string_wide_from_utf8(profilePath.get_ptr()).get_ptr();
        path += L"\\";
        path += fileName;
        return path;
    }

    std::string EnvironmentVariable(const wchar_t* name) {
        wchar_t value[1024] = {};
        const DWORD length = GetEnvironmentVariableW(name, value, ARRAYSIZE(value));
        if (length == 0 || length >= ARRAYSIZE(value)) return {};
        return pfc::stringcvt::string_utf8_from_wide(value).get_ptr();
    }

    std::string Quoted(const std::string& value) {
        return value.empty() ? std::string() : "\"" + value + "\"";
    }

    const char* DpiAwarenessName(DPI_AWARENESS_CONTEXT context) {
        if (!context) return "-";
        if (AreDpiAwarenessContextsEqual(context, DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
            return "pmv2";
        }
        switch (GetAwarenessFromDpiAwarenessContext(context)) {
            case DPI_AWARENESS_UNAWARE: return "unaware";
            case DPI_AWARENESS_SYSTEM_AWARE: return "system";
            case DPI_AWARENESS_PER_MONITOR_AWARE: return "pm";
            default: return "invalid";
        }
    }

    const char* ProcessElevation() {
        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return "?";
        TOKEN_ELEVATION elevation = {};
        DWORD size = 0;
        const BOOL ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);
        CloseHandle(token);
        if (!ok) return "?";
        return elevation.TokenIsElevated ? "1" : "0";
    }
}

void WebViewHost::WriteLifecycleLog(const std::string& line) {
    WriteProfileLog(L"webview_lifecycle.log", line);
}

void WebViewHost::WriteProfileLog(const wchar_t* fileName, const std::string& line) {
    static std::mutex logMutex;
    const std::scoped_lock lock(logMutex);

    try {
        const std::wstring path = GetProfileLogPath(fileName);
        const std::wstring rotatedPath = path + L".1";

        WIN32_FILE_ATTRIBUTE_DATA attributes{};
        if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes)) {
            const unsigned long long size =
                (static_cast<unsigned long long>(attributes.nFileSizeHigh) << 32) |
                attributes.nFileSizeLow;
            if (size >= kLifecycleLogMaxBytes) {
                DeleteFileW(rotatedPath.c_str());
                MoveFileExW(path.c_str(), rotatedPath.c_str(),
                            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
            }
        }

        SYSTEMTIME utc{};
        GetSystemTime(&utc);
        std::ofstream file(path, std::ios::app);
        if (!file.is_open()) {
            return;
        }
        file << '[' << std::setfill('0')
             << std::setw(4) << utc.wYear << '-'
             << std::setw(2) << utc.wMonth << '-'
             << std::setw(2) << utc.wDay << 'T'
             << std::setw(2) << utc.wHour << ':'
             << std::setw(2) << utc.wMinute << ':'
             << std::setw(2) << utc.wSecond << '.'
             << std::setw(3) << utc.wMilliseconds << "Z] "
             << SanitizeLifecycleLogLine(line) << '\n';
        file.flush();
    } catch (...) {
        // Diagnostics must never interfere with the recovery path.
    }
}

void WebViewHost::LogLifecycle(const char* event, const std::string& detail) const {
    try {
        BOOL visible = FALSE;
        RECT bounds{};
        const HRESULT visibleHr = controller_
            ? controller_->get_IsVisible(&visible) : E_POINTER;
        const HRESULT boundsHr = controller_
            ? controller_->get_Bounds(&bounds) : E_POINTER;

        BOOL suspended = FALSE;
        HRESULT suspendedHr = E_NOINTERFACE;
        if (webview_) {
            wil::com_ptr<ICoreWebView2_3> webview3;
            if (SUCCEEDED(webview_.try_query_to(&webview3)) && webview3) {
                suspendedHr = webview3->get_IsSuspended(&suspended);
            }
        }

        std::ostringstream stream;
        stream << "event=" << (event && event[0] ? event : "-")
               << " uptimeMs=" << GetTickCount64()
               << " pid=" << GetCurrentProcessId()
               << " tid=" << GetCurrentThreadId()
               << " hwnd=0x" << pfc::format_hex((size_t)parentHwnd_)
               << " hwndValid=" << (parentHwnd_ && IsWindow(parentHwnd_) ? 1 : 0)
               << " host=0x" << pfc::format_hex((size_t)this)
               << " controller=0x" << pfc::format_hex((size_t)controller_.get())
               << " webview=0x" << pfc::format_hex((size_t)webview_.get())
               << " composition=0x" << pfc::format_hex((size_t)compositionController_.get())
               << " dcompTarget=0x" << pfc::format_hex((size_t)dcompTarget_.get())
               << " hidden=" << (suspendState_->pageHidden.load(std::memory_order_acquire) ? 1 : 0)
               << " alive=" << (suspendState_->alive.load(std::memory_order_acquire) ? 1 : 0)
               << " generation=" << suspendState_->generation.load(std::memory_order_acquire)
               << " visibleHr=0x" << pfc::format_hex((uint32_t)visibleHr)
               << " visible=" << (visible != FALSE ? 1 : 0)
               << " boundsHr=0x" << pfc::format_hex((uint32_t)boundsHr)
               << " bounds=(" << bounds.left << ',' << bounds.top << ','
               << bounds.right << ',' << bounds.bottom << ')'
               << " suspendedHr=0x" << pfc::format_hex((uint32_t)suspendedHr)
               << " suspended=" << (suspended != FALSE ? 1 : 0)
               << " detail=" << (detail.empty() ? "-" : detail);
        WriteLifecycleLog(stream.str());
    } catch (...) {
        // COM state inspection is best-effort and must remain non-throwing.
    }
}

void WebViewHost::RecordInitFailure(const char* event, const char* stage, HRESULT hr) {
    lastInitFailure_ = {stage, hr};
    LogLifecycle(event, std::string("stage=") + stage + " hr=0x" +
        pfc::format_hex((uint32_t)hr, 8).get_ptr() + " " + DescribeInitContext(parentHwnd_, ' '));
}

std::string WebViewHost::DescribeInitContext(HWND hwnd, char separator) {
    // 路径、兼容层和环境变量的值可能含空格，加引号，免得和日志里的分隔符混在一起。
    // __COMPAT_LAYER 是 exe「属性 → 兼容性」里勾的兼容模式和高 DPI 替代。
    // WEBVIEW2_* 环境变量会覆盖组件传给 WebView2 的参数，从组件自己的设置里看不出来。
    std::string webView2Overrides;
    for (const wchar_t* name : {L"WEBVIEW2_USER_DATA_FOLDER", L"WEBVIEW2_BROWSER_EXECUTABLE_FOLDER",
                                L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS", L"WEBVIEW2_RELEASE_CHANNEL_PREFERENCE"}) {
        const std::string value = EnvironmentVariable(name);
        if (value.empty()) continue;
        if (!webView2Overrides.empty()) webView2Overrides += "; ";
        webView2Overrides += pfc::stringcvt::string_utf8_from_wide(name).get_ptr();
        webView2Overrides += '=';
        webView2Overrides += value;
    }
    const auto creation = WebViewEnvironment::GetInstance().LastCreation();
    const DPI_AWARENESS_CONTEXT windowDpi =
        hwnd && IsWindow(hwnd) ? GetWindowDpiAwarenessContext(hwnd) : nullptr;

    std::string out;
    const auto add = [&out, separator](const char* key, const std::string& value) {
        if (!out.empty()) out += separator;
        out += key;
        out += '=';
        out += value.empty() ? "-" : value;
    };
    add("component", PLUGIN_VERSION_STR);
    add("fb2k", core_version_info::g_get_version_string());
    add("osBuild", std::to_string(dwm_compatibility::Current().build));
    add("portable", core_api::is_portable_mode_enabled() ? "1" : "0");
    add("elevated", ProcessElevation());
    add("dpiThread", DpiAwarenessName(GetThreadDpiAwarenessContext()));
    add("dpiWindow", DpiAwarenessName(windowDpi));
    add("compatLayer", Quoted(EnvironmentVariable(L"__COMPAT_LAYER")));
    add("webview2Env", Quoted(webView2Overrides));
    add("runtime", creation.runtimeVersion);
    add("userData", Quoted(creation.userDataPath));
    if (!creation.effectiveUserDataPath.empty() && creation.effectiveUserDataPath != creation.userDataPath) {
        add("userDataEffective", Quoted(creation.effectiveUserDataPath));
    }
    return out;
}

// ==========================================================================
// WebView2 进程崩溃诊断日志
// 写入 foobar2000 profile 目录下的 webview_crash.log，不污染 fb2k console。
// 崩溃时浏览器/渲染进程已死、DevTools 不可用，故必须用宿主进程的原生 IO 落盘。
// ==========================================================================
void WebViewHost::WriteCrashLog(const std::string& line) {
    try {
        // profile 目录（与 ConsoleApi 的 webview_ui.log 同目录，但独立文件）。
        // 必须走 core_api::get_profile_path()：字面量 "profile://" 不是可解析的
        // 路径，g_get_display_path 会抛异常并被下方 catch 静默吞掉，导致崩溃日志
        // 全部丢失。
        std::wstring widePath = GetProfileLogPath(L"webview_crash.log");

        // 时间戳前缀
        std::time_t now = std::time(nullptr);
        std::tm tmv{};
        localtime_s(&tmv, &now);
        char ts[32] = {0};
        std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);

        std::ofstream file(widePath, std::ios::app);
        if (file.is_open()) {
            file << "[" << ts << "] " << line << "\n";
        }
    } catch (...) {
        // 崩溃日志写入失败不得影响恢复流程
    }
}

HRESULT WebViewHost::ProbePageHealth(const PageHealthCallback& callback) {
    if (!webview_) {
        LogLifecycle("health.submit-failed", "reason=no-webview");
        if (callback) callback(false);
        return E_FAIL;
    }

    LogLifecycle("health.submit");

    // Keep the probe independent from the public bridge and CDP. It verifies
    // the document execution context that owns the DirectComposition surface.
    static constexpr wchar_t kHealthProbe[] = LR"(
        (() => {
            try {
                return {
                    readyState: document.readyState,
                    href: location.href,
                    hasDocumentElement: !!document.documentElement,
                    hasBridge: !!window.fb2k
                };
            } catch (_) {
                return { readyState: "error" };
            }
        })()
    )";

    const HRESULT hr = webview_->ExecuteScript(
        kHealthProbe,
        Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
            [callback](HRESULT result, LPCWSTR resultJson) {
                try {
                    bool healthy = false;
                    bool hasReadyState = false;
                    bool hasRealLocation = false;
                    bool hasDocumentElement = false;
                    bool hasBridge = false;
                    if (SUCCEEDED(result) && resultJson) {
                        const std::wstring value(resultJson);
                        hasReadyState =
                            value.find(L"\"readyState\":\"complete\"") != std::wstring::npos ||
                            value.find(L"\"readyState\":\"interactive\"") != std::wstring::npos ||
                            value.find(L"\"readyState\":\"loading\"") != std::wstring::npos;
                        hasRealLocation =
                            value.find(L"\"href\":\"about:blank\"") == std::wstring::npos &&
                            value.find(L"\"href\":\"\"") == std::wstring::npos;
                        hasDocumentElement =
                            value.find(L"\"hasDocumentElement\":true") != std::wstring::npos;
                        hasBridge = value.find(L"\"hasBridge\":true") != std::wstring::npos;
                        healthy = hasReadyState && hasRealLocation &&
                            hasDocumentElement && hasBridge;
                    }
                    std::ostringstream completion;
                    completion << "event=health.callback"
                               << " uptimeMs=" << GetTickCount64()
                               << " pid=" << GetCurrentProcessId()
                               << " tid=" << GetCurrentThreadId()
                               << " hr=0x" << pfc::format_hex((uint32_t)result)
                               << " healthy=" << (healthy ? 1 : 0)
                               << " readyState=" << (hasReadyState ? 1 : 0)
                               << " realLocation=" << (hasRealLocation ? 1 : 0)
                               << " documentElement=" << (hasDocumentElement ? 1 : 0)
                               << " bridge=" << (hasBridge ? 1 : 0)
                               << " resultChars=" << (resultJson ? wcslen(resultJson) : 0);
                    WebViewHost::WriteLifecycleLog(completion.str());
                    if (callback) callback(healthy);
                } catch (...) {
                    // Never unwind through the WebView2 COM callback boundary.
                }
                return S_OK;
            }
        ).Get()
    );
    if (FAILED(hr) && callback) {
        LogLifecycle("health.submit-failed",
            "hr=" + std::to_string((long long)hr));
        callback(false);
    }
    return hr;
}

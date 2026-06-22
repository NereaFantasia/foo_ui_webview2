#pragma once

// ============================================
// Precompiled header
// ============================================

// Windows headers
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <comdef.h>
#include <mmsystem.h>
#include <wrl/client.h>

// WebView2
#include <WebView2.h>
#include <wil/com.h>
#include <wil/result.h>

// C++ standard library
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <algorithm>
#include <optional>
#include <variant>
#include <mutex>
#include <thread>
#include <chrono>
#include <cmath>

// JSON library
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// foobar2000 SDK
#include <foobar2000/SDK/foobar2000.h>

// Link libraries
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "winmm.lib")

// Enable std::string in pfc formatting (SDK only supports const char*)
// Must be in pfc namespace for ADL to find it during template instantiation
namespace pfc {
    inline string_base& operator<<(string_base& p_fmt, const std::string& p_source) {
        p_fmt.add_string_(p_source.c_str());
        return p_fmt;
    }
}

// Debug logging macro - uses SDK variadic console::print for proper arg concatenation
#define LOG(...) ::console::print("[WebView2 UI] ", __VA_ARGS__)


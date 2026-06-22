#pragma once

#include "pch.h"
#include <atomic>
#include <cstdlib>
#include <sstream>

struct WindowChromeTraceState {
    const char* phase = nullptr;
    const char* windowKind = nullptr;
    const char* windowId = nullptr;
    HWND hwnd = nullptr;
    bool startupRevealPending = false;
    bool startupRevealCommitted = false;
    bool startupRevealSettling = false;
    bool isActive = false;
    const char* active = nullptr;
    const char* forceRefresh = nullptr;
    const char* effect = nullptr;
    const char* previousEffect = nullptr;
    const char* extra = nullptr;
};

struct EmitWindowPhaseInput {
    const char* phase = nullptr;
    const char* windowKind = nullptr;
    const char* defaultWindowId = nullptr;
    std::string windowId;
    HWND hwnd = nullptr;
    bool startupRevealPending = false;
    bool startupRevealCommitted = false;
    bool startupRevealSettling = false;
    bool isActive = false;
    const char* active = nullptr;
    const char* forceRefresh = nullptr;
    const char* effect = nullptr;
    std::string lastBackdropEffect;
    const char* previousEffect = nullptr;
    const char* extra = nullptr;
};

class WindowChromeTrace {
public:
    static const char* BoolText(bool value) {
        return value ? "true" : "false";
    }

    // Window-chrome diagnostics (WindowChromeTrace / ActivationEvidence /
    // InteractiveResize / WindowChromeProbe / surface + DOM probes) are extremely
    // verbose and only useful when actively debugging window backdrop / activation
    // / resize behavior. They are gated OFF by default so they don't pollute the
    // console. To re-enable for a debugging session, set the environment variable
    // FOO_UI_WEBVIEW2_WINDOW_TRACE=1 before launching foobar2000.
    static bool AuxiliaryTraceEnabled() {
        static const bool enabled = ResolveAuxiliaryTraceEnabled();
        return enabled;
    }

    static void EmitAuxiliaryLine(const std::string& line) {
        if (!AuxiliaryTraceEnabled()) {
            return;
        }
        FB2K_console_formatter() << line.c_str();
    }

    static unsigned long long RelativeMs() {
        const unsigned long long now = GetTickCount64();
        unsigned long long origin = originTickMs_.load();
        if (origin == 0) {
            unsigned long long expected = 0;
            if (originTickMs_.compare_exchange_strong(expected, now)) {
                origin = now;
            } else {
                origin = originTickMs_.load();
            }
        }
        return now >= origin ? (now - origin) : 0;
    }

    static void Emit(const WindowChromeTraceState& state) {
        const unsigned long long sequence = ++sequence_;
        const bool hwndValid = state.hwnd && IsWindow(state.hwnd);
        const unsigned long long elapsedMs = RelativeMs();

        std::ostringstream stream;
        stream << "[WindowChromeTrace]"
               << " seq=" << sequence
               << " t=" << elapsedMs << "ms"
               << " phase=" << Text(state.phase)
               << " kind=" << Text(state.windowKind)
               << " windowId=" << Text(state.windowId)
               << " hwnd=0x" << pfc::format_hex((size_t)state.hwnd)
               << " startupRevealPending=" << BoolText(state.startupRevealPending)
               << " startupRevealCommitted=" << BoolText(state.startupRevealCommitted)
               << " startupRevealSettling=" << BoolText(state.startupRevealSettling)
               << " isActive=" << BoolText(state.isActive)
               << " visible=" << BoolText(hwndValid && IsWindowVisible(state.hwnd))
               << " iconic=" << BoolText(hwndValid && IsIconic(state.hwnd))
               << " zoomed=" << BoolText(hwndValid && IsZoomed(state.hwnd))
               << " active=" << Text(state.active)
               << " forceRefresh=" << Text(state.forceRefresh)
               << " effect=" << Text(state.effect)
               << " previousEffect=" << Text(state.previousEffect)
               << " extra=" << Text(state.extra);
        EmitAuxiliaryLine(stream.str());
    }

    static void EmitWindowPhase(const EmitWindowPhaseInput& p) {
        WindowChromeTraceState s;
        s.phase = p.phase;
        s.windowKind = p.windowKind;
        s.windowId = p.windowId.empty() ? p.defaultWindowId : p.windowId.c_str();
        s.hwnd = p.hwnd;
        s.startupRevealPending = p.startupRevealPending;
        s.startupRevealCommitted = p.startupRevealCommitted;
        s.startupRevealSettling = p.startupRevealSettling;
        s.isActive = p.isActive;
        s.active = (p.active && p.active[0]) ? p.active : BoolText(p.isActive);
        s.forceRefresh = (p.forceRefresh && p.forceRefresh[0]) ? p.forceRefresh : "-";
        s.effect = p.effect;
        s.previousEffect = (p.previousEffect && p.previousEffect[0]) ? p.previousEffect
            : (!p.lastBackdropEffect.empty() ? p.lastBackdropEffect.c_str() : "-");
        s.extra = p.extra;
        Emit(s);
    }

private:
    static const char* Text(const char* value) {
        return (value && value[0]) ? value : "-";
    }

    static bool ResolveAuxiliaryTraceEnabled() {
        size_t len = 0;
        char buf[8] = {};
        if (getenv_s(&len, buf, sizeof(buf), "FOO_UI_WEBVIEW2_WINDOW_TRACE") == 0 && len > 0) {
            const char c = buf[0];
            return c == '1' || c == 'y' || c == 'Y' || c == 't' || c == 'T';
        }
        return false;
    }

    inline static std::atomic<unsigned long long> sequence_ { 0 };
    inline static std::atomic<unsigned long long> originTickMs_ { 0 };
};

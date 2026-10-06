// DndTrace.cpp
#include "pch.h"
#include "webview/dnd/DndTrace.h"

#include "webview/WebViewHost.h"

#include <chrono>

namespace fb2k_dnd {

bool DndTraceEnabled() noexcept {
    static const bool enabled = [] {
        size_t len = 0;
        char buf[8] = {};
        if (getenv_s(&len, buf, sizeof(buf), "FOO_UI_WEBVIEW2_DND_TRACE") == 0 && len > 0) {
            const char c = buf[0];
            return c == '1' || c == 'y' || c == 'Y' || c == 't' || c == 'T';
        }
        return false;
    }();
    return enabled;
}

void DndTrace(const std::string& line) noexcept {
    if (!DndTraceEnabled()) {
        return;
    }
    try {
        // The file, not the foobar2000 console: a console line per DragOver
        // slows the very callback being measured, and the console is not kept.
        WebViewHost::WriteProfileLog(L"webview_dnd_trace.log", line);
    } catch (...) {
        // Diagnostics must never disturb the drag.
    }
}

int64_t DndTraceNowMs() noexcept {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

}  // namespace fb2k_dnd

// DndTrace.h - drag-and-drop diagnostics written to a file, off by default.
#pragma once

#include <cstdint>
#include <string>

namespace fb2k_dnd {

// Whether FOO_UI_WEBVIEW2_DND_TRACE was set to 1 (or y / t) when foobar2000
// started. Read once per process.
bool DndTraceEnabled() noexcept;

// Appends one line to webview_dnd_trace.log in the foobar2000 profile folder,
// with a UTC timestamp. Does nothing when tracing is off. Never throws: the
// callers run inside OLE and WebView2 callbacks.
void DndTrace(const std::string& line) noexcept;

// Milliseconds on a monotonic clock, for measuring how long a call took.
int64_t DndTraceNowMs() noexcept;

}  // namespace fb2k_dnd

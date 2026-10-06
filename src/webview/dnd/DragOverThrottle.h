// DragOverThrottle.h - how often a DragOver is passed on to the WebView.
#pragma once

#include <windows.h>

#include <cstdint>

namespace fb2k_dnd {

// OLE calls IDropTarget::DragOver for every mouse message the drag source
// sees, which during a drag from Explorer was measured at up to about 1200
// calls a second. Each forwarded call becomes a message to the WebView2
// browser and renderer processes, and a backlog there holds up everything
// behind it, including the end of the drag. Calls in between are answered with
// the answer the WebView last gave.

// Minimum gap between two forwarded calls while only the cursor moves.
constexpr int64_t kDragOverMoveIntervalMs = 16;

// While the cursor rests, a call is still forwarded this often, so a page whose
// answer changes without the cursor moving is heard.
constexpr int64_t kDragOverIdleIntervalMs = 100;

struct DragOverForwardState {
    bool hasForwarded = false;  // false until the first Enter or Over reached the WebView
    int64_t lastForwardMs = 0;
    POINTL lastPoint{};
    DWORD lastKeyState = 0;
    DWORD lastAnswer = DROPEFFECT_NONE;  // what the WebView answered last time
};

// Whether this DragOver goes to the WebView. A change of modifier keys or mouse
// buttons always does, since it can change the page's answer at once.
bool ShouldForwardDragOver(const DragOverForwardState& state, POINTL point, DWORD keyState,
                           int64_t nowMs);

// Records a call that went to the WebView and what it answered.
void RecordForwardedDragOver(DragOverForwardState& state, POINTL point, DWORD keyState,
                             int64_t nowMs, DWORD answer);

}  // namespace fb2k_dnd

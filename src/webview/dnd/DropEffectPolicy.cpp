// DropEffectPolicy.cpp
#include "pch.h"
#include "webview/dnd/DropEffectPolicy.h"

namespace fb2k_dnd {

bool RendererHasAnswered(bool sawEffect, int64_t sinceEnterMs) {
    return sawEffect || sinceEnterMs >= kRendererAnswerGraceMs;
}

DWORD ChooseDropEffect(DWORD downstream, DWORD allowedMask, bool hasFiles,
                       bool rendererAnswered, DragSource source) {
    // Clamp to COPY unless a hosted page started the drag: see the header for
    // why MOVE/LINK are unsafe from anywhere else.
    const DWORD copyIfPermitted = allowedMask & DROPEFFECT_COPY;
    if (downstream != DROPEFFECT_NONE) {
        return downstream & (source == DragSource::External ? copyIfPermitted : allowedMask);
    }
    if (hasFiles && !rendererAnswered) {
        return copyIfPermitted;
    }
    return DROPEFFECT_NONE;
}

bool DragOutMaskIsCopyOnly(DWORD allowedMask) {
    return allowedMask == DROPEFFECT_COPY;
}

}  // namespace fb2k_dnd

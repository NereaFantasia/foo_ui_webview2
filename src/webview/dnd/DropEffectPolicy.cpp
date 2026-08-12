// DropEffectPolicy.cpp
#include "pch.h"
#include "webview/dnd/DropEffectPolicy.h"

namespace fb2k_dnd {

DWORD ChooseDropEffect(DWORD downstream, DWORD allowedMask, bool hasFiles) {
    // Clamp to COPY unconditionally: see the header for why MOVE/LINK are unsafe.
    const DWORD copyIfPermitted = allowedMask & DROPEFFECT_COPY;

    if (downstream != DROPEFFECT_NONE) {
        return downstream & copyIfPermitted;
    }
    if (hasFiles) {
        return copyIfPermitted;  // optimistic: renderer has not answered yet
    }
    return DROPEFFECT_NONE;
}

}  // namespace fb2k_dnd

// DropEffectPolicy.h - decides the DROPEFFECT reported back to the drag source.
#pragma once

#include <windows.h>

namespace fb2k_dnd {

// Chooses the effect to report for one drag callback.
//
// Only DROPEFFECT_COPY is ever returned for a file drop. DROPEFFECT is an
// operation contract, not a cursor hint: reporting MOVE tells the source it may
// delete the originals, and this component never moves files - it only hands
// paths to the page. Modifier keys therefore do not influence the result.
//
// downstream    effect reported by the delegate; unreliable early in a drag
//               because the renderer answers asynchronously
// allowedMask   the in/out value the drag source passed in, i.e. permitted effects
// hasFiles      whether the session carries a CF_HDROP list
DWORD ChooseDropEffect(DWORD downstream, DWORD allowedMask, bool hasFiles);

// Whether a drag out of the window may go ahead with the effects the page
// offered through dataTransfer.effectAllowed.
//
// Only an exact DROPEFFECT_COPY passes. With CF_HDROP on board it is the drop
// target that carries out a MOVE, by relocating the user's files itself, so the
// source's one and only defence is never to offer that effect. A mask that is
// wider than COPY, or empty, is refused before any file list is exposed.
bool DragOutMaskIsCopyOnly(DWORD allowedMask);

}  // namespace fb2k_dnd

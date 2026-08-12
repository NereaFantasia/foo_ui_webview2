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

}  // namespace fb2k_dnd

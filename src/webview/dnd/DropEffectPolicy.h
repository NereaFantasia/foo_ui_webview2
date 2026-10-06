// DropEffectPolicy.h - decides the DROPEFFECT reported back to the drag source.
#pragma once

#include <windows.h>

#include <cstdint>

namespace fb2k_dnd {

// How long after a drag enters the window a NONE from the WebView is taken as
// the page refusing the drop rather than as "has not answered yet". The
// renderer answers asynchronously: first answers were measured 140-330 ms after
// the drag entered, and every call before that reported NONE.
constexpr int64_t kRendererAnswerGraceMs = 500;

// Whether the WebView's answer stands for the page's decision yet: it has
// answered with an effect at least once during this drag, or the grace period
// is over. A NONE it gives after that is a refusal.
bool RendererHasAnswered(bool sawEffect, int64_t sinceEnterMs);

// Chooses the effect to report for one drag callback.
//
// Only DROPEFFECT_COPY is ever returned for a file drop. DROPEFFECT is an
// operation contract, not a cursor hint: reporting MOVE tells the source it may
// delete the originals, and this component never moves files - it only hands
// paths to the page. Modifier keys therefore do not influence the result.
//
// A file drag the WebView has not answered yet is reported as COPY, so the
// cursor does not flash "forbidden" on the way in; once it has answered, its
// answer is reported, including NONE.
//
// downstream        effect reported by the delegate
// allowedMask       the in/out value the drag source passed in, i.e. permitted effects
// hasFiles          whether the session carries a CF_HDROP list
// rendererAnswered  RendererHasAnswered for this drag
DWORD ChooseDropEffect(DWORD downstream, DWORD allowedMask, bool hasFiles,
                       bool rendererAnswered);

// Whether a drag out of the window may go ahead with the effects the page
// offered through dataTransfer.effectAllowed.
//
// Only an exact DROPEFFECT_COPY passes. With CF_HDROP on board it is the drop
// target that carries out a MOVE, by relocating the user's files itself, so the
// source's one and only defence is never to offer that effect. A mask that is
// wider than COPY, or empty, is refused before any file list is exposed.
bool DragOutMaskIsCopyOnly(DWORD allowedMask);

}  // namespace fb2k_dnd

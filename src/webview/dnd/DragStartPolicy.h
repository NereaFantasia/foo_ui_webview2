// DragStartPolicy.h - what becomes of a drag that carries a drag token.
#pragma once

namespace fb2k_dnd {

// How far the host got in attaching files to a drag that carries the token
// carrier. Drags without the carrier never reach this policy: they go back to
// WebView2 untouched.
enum class DragOutStage {
    TokenRejected,     // unknown, expired, spent, superseded or minted by another window
    EffectsNotCopy,    // dataTransfer.effectAllowed was anything but exactly copy
    FilesNotAttached,  // the CF_HDROP block could not be built or stored
    FilesAttached,
};

struct DragStartVerdict {
    // true hands the drag back to WebView2's default handling, which runs it as
    // an ordinary page drag; false keeps it handled, which cancels it.
    bool handBack;
    // ApiErrorCode reported on dnd:dragEnded, or nullptr when nothing is reported.
    const char* failureCode;
    // Reason reported with failureCode; never contains a path.
    const char* error;
};

// Decides the outcome once the host has tried to attach the files and to blank
// the token text.
//
// A refusal still hands the drag back, without files, so the page's own drag
// keeps working: dragging an album onto a playlist in the page needs no files.
// Without CF_HDROP a drop target has nothing of the user's to move, so a refused
// effectAllowed is safe to hand back as well. The one exception is a carrier
// that could not be blanked: the drag is then cancelled rather than let the
// token text reach a text target.
//
// carrierBlanked matters only for refusals. With the files attached the token
// is already spent, so text left behind is inert and the drag goes out.
DragStartVerdict DecideDragStart(DragOutStage stage, bool carrierBlanked);

}  // namespace fb2k_dnd

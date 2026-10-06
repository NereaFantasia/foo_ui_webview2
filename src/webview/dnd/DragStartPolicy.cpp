// DragStartPolicy.cpp
#include "pch.h"
#include "webview/dnd/DragStartPolicy.h"

namespace fb2k_dnd {

// The codes are the ApiErrorCode values the dnd:dragEnded declaration allows.
// Spelled out rather than taken from ErrorEnvelope.h, which drags the fb2k
// console into this otherwise dependency-free policy; the unit test checks
// them against ApiErrorCode.
DragStartVerdict DecideDragStart(DragOutStage stage, bool carrierBlanked) {
    switch (stage) {
    case DragOutStage::FilesAttached:
        return {true, nullptr, nullptr};
    case DragOutStage::TokenRejected:
        return {carrierBlanked, "PERMISSION_DENIED", "Drag token was rejected."};
    case DragOutStage::EffectsNotCopy:
        return {carrierBlanked, "INVALID_PARAMS",
                "dataTransfer.effectAllowed must be 'copy' for a drag-out."};
    case DragOutStage::FilesNotAttached:
        return {carrierBlanked, "OPERATION_FAILED",
                "The drag's file list could not be prepared."};
    }
    // Unreachable for a valid stage; an out-of-range value gets the most
    // conservative refusal.
    return {false, "OPERATION_FAILED", "The drag's file list could not be prepared."};
}

}  // namespace fb2k_dnd

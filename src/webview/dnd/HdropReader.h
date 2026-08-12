// HdropReader.h - CF_HDROP / DROPFILES parsing, isolated from COM so it can be unit-tested.
#pragma once

#include <string>
#include <vector>

struct IDataObject;

namespace fb2k_dnd {

// Parses a raw DROPFILES blob into absolute paths.
//
// Not on the drag-and-drop path: a real CF_HDROP medium is read by
// ReadHdropPaths below, which uses DragQueryFileW. This entry point takes an
// already-addressable blob so the bounds checks can be exercised by unit tests
// against hand-built input, and it serves callers that hold a DROPFILES buffer
// from somewhere other than a drop. Its malformed-input coverage therefore says
// nothing about how ReadHdropPaths behaves on a hostile HDROP.
//
// Handles both the Unicode (fWide == TRUE) and ANSI variants, and tolerates
// malformed input: a truncated blob, a pFiles offset outside the blob, or a
// missing double-NUL terminator all yield the paths parsed so far rather than
// reading out of bounds.
//
// blob may be null when size is 0. Never throws.
std::vector<std::wstring> ParseDropFilesBlob(const void* blob, size_t size);

// Reads CF_HDROP from a data object. This is the production path for a real
// drop: the data object comes from an arbitrary source process, and the medium
// is handed to DragQueryFileW, which does its own locking and bounds handling.
//
// Returns an empty vector when the drag carries no usable file list. hadHdrop,
// when non-null, distinguishes "no CF_HDROP file list to read" from "CF_HDROP
// present but empty"; it stays false when the source answers with a medium of
// some other tymed, since such a medium cannot be read as an HDROP.
//
// Called from IDropTarget::DragEnter and Drop, which report through an HRESULT
// and cannot absorb an exception, so allocation failure yields an empty vector
// instead. noexcept is enforced, not merely intended.
std::vector<std::wstring> ReadHdropPaths(IDataObject* dataObject, bool* hadHdrop) noexcept;

}  // namespace fb2k_dnd

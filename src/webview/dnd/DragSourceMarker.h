// DragSourceMarker.h - tells a drag that started in a hosted window apart from
// one that came from elsewhere.
#pragma once

#include <windows.h>
#include <objidl.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace fb2k_dnd {

// What the page is told about a drag's origin (the dnd:* payload field `source`).
enum class DragSource {
    Self,         // started in the page of the window the drag is over
    OtherWindow,  // started in another window of this process
    External,     // no marker, a malformed one, or one from another process
};

// Stamped into the data object of every drag that starts in a WebView this
// component hosts, and read back when a drag enters a hosted window.
//
// It is a hint, not a credential: any process can put the same bytes in its
// own drag. Nothing that grants access may depend on it.
struct DragSourceMarker {
    uint32_t processId = 0;
    // The HWND of the window whose drop target the drag would reach, widened to
    // a fixed size so the layout does not depend on the build's pointer width.
    uint64_t window = 0;
};

// Fixed-size little-endian layout: 4-byte tag, 4-byte version, process id,
// 4 reserved bytes, window. Versioned so a future layout can coexist with
// builds that only understand this one.
std::vector<unsigned char> EncodeDragSourceMarker(const DragSourceMarker& marker);

// nullopt for anything that is not exactly a version-1 marker.
std::optional<DragSourceMarker> DecodeDragSourceMarker(const unsigned char* bytes,
                                                       size_t size);

DragSource ClassifyDragSource(const std::optional<DragSourceMarker>& marker,
                              uint32_t ownProcessId, uint64_t ownWindow);

// "self", "other-window" or "external".
const char* DragSourceToWire(DragSource source);

// The registered clipboard format that carries the marker. Registered on first
// use; 0 when registration failed, which callers treat as "no marker".
CLIPFORMAT DragSourceClipboardFormat() noexcept;

// Reads the marker from a drag's data object; nullopt when it carries none.
std::optional<DragSourceMarker> ReadDragSourceMarker(IDataObject* data) noexcept;

}  // namespace fb2k_dnd

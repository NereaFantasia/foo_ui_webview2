// DragSourceMarker.cpp
#include "pch.h"
#include "webview/dnd/DragSourceMarker.h"

#include <array>
#include <cstring>

namespace fb2k_dnd {
namespace {

constexpr std::array<unsigned char, 4> kTag{{'f', 'b', 'd', 's'}};
constexpr uint32_t kVersion = 1;
// tag + version + processId + reserved + window
constexpr size_t kMarkerSize = 4 + 4 + 4 + 4 + 8;

void PutU32(unsigned char* out, uint32_t value) {
    for (size_t i = 0; i < 4; ++i) {
        out[i] = static_cast<unsigned char>(value >> (8 * i));
    }
}

void PutU64(unsigned char* out, uint64_t value) {
    for (size_t i = 0; i < 8; ++i) {
        out[i] = static_cast<unsigned char>(value >> (8 * i));
    }
}

uint32_t GetU32(const unsigned char* in) {
    uint32_t value = 0;
    for (size_t i = 0; i < 4; ++i) {
        value |= static_cast<uint32_t>(in[i]) << (8 * i);
    }
    return value;
}

uint64_t GetU64(const unsigned char* in) {
    uint64_t value = 0;
    for (size_t i = 0; i < 8; ++i) {
        value |= static_cast<uint64_t>(in[i]) << (8 * i);
    }
    return value;
}

struct StgMediumGuard {
    STGMEDIUM* medium;
    ~StgMediumGuard() { ::ReleaseStgMedium(medium); }
};

struct GlobalUnlockGuard {
    HGLOBAL handle;
    ~GlobalUnlockGuard() { ::GlobalUnlock(handle); }
};

}  // namespace

std::vector<unsigned char> EncodeDragSourceMarker(const DragSourceMarker& marker) {
    std::vector<unsigned char> bytes(kMarkerSize, 0);
    std::memcpy(bytes.data(), kTag.data(), kTag.size());
    PutU32(bytes.data() + 4, kVersion);
    PutU32(bytes.data() + 8, marker.processId);
    PutU64(bytes.data() + 16, marker.window);
    return bytes;
}

std::optional<DragSourceMarker> DecodeDragSourceMarker(const unsigned char* bytes,
                                                       size_t size) {
    // An HGLOBAL may be rounded up past the size it was allocated with, so a
    // longer block is accepted and only the leading marker is read.
    if (!bytes || size < kMarkerSize) {
        return std::nullopt;
    }
    if (std::memcmp(bytes, kTag.data(), kTag.size()) != 0 || GetU32(bytes + 4) != kVersion) {
        return std::nullopt;
    }
    DragSourceMarker marker;
    marker.processId = GetU32(bytes + 8);
    marker.window = GetU64(bytes + 16);
    return marker;
}

DragSource ClassifyDragSource(const std::optional<DragSourceMarker>& marker,
                              uint32_t ownProcessId, uint64_t ownWindow) {
    if (!marker || marker->processId != ownProcessId) {
        return DragSource::External;
    }
    return marker->window == ownWindow ? DragSource::Self : DragSource::OtherWindow;
}

const char* DragSourceToWire(DragSource source) {
    switch (source) {
    case DragSource::Self:        return "self";
    case DragSource::OtherWindow: return "other-window";
    case DragSource::External:
    default:
        return "external";
    }
}

CLIPFORMAT DragSourceClipboardFormat() noexcept {
    static const CLIPFORMAT format =
        static_cast<CLIPFORMAT>(::RegisterClipboardFormatW(L"foo_ui_webview2.DragSource"));
    return format;
}

std::optional<DragSourceMarker> ReadDragSourceMarker(IDataObject* data) noexcept {
    const CLIPFORMAT cf = DragSourceClipboardFormat();
    if (!data || cf == 0) {
        return std::nullopt;
    }

    FORMATETC format = {};
    format.cfFormat = cf;
    format.ptd = nullptr;
    format.dwAspect = DVASPECT_CONTENT;
    format.lindex = -1;
    format.tymed = TYMED_HGLOBAL;

    STGMEDIUM medium = {};
    if (FAILED(data->GetData(&format, &medium))) {
        return std::nullopt;
    }
    StgMediumGuard release{&medium};
    // hGlobal shares a union with the other storage kinds, so a source that
    // answered with another tymed must not be read as memory.
    if (medium.tymed != TYMED_HGLOBAL || !medium.hGlobal) {
        return std::nullopt;
    }
    const auto* bytes = static_cast<const unsigned char*>(::GlobalLock(medium.hGlobal));
    if (!bytes) {
        return std::nullopt;
    }
    GlobalUnlockGuard unlock{medium.hGlobal};
    return DecodeDragSourceMarker(bytes, ::GlobalSize(medium.hGlobal));
}

}  // namespace fb2k_dnd

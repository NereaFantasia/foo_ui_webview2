// HdropBuilder.cpp
#include "pch.h"
#include "webview/dnd/HdropBuilder.h"

// DROPFILES lives here. Included explicitly rather than relied on through the
// precompiled header, which does not carry it for this project.
#include <ShlObj_core.h>

#include <algorithm>

namespace fb2k_dnd {

std::vector<unsigned char> BuildDropFilesBlock(const std::vector<std::wstring>& paths) {
    if (paths.empty()) {
        return {};
    }

    // Each path contributes its characters plus its own NUL; one further NUL
    // closes the list.
    size_t chars = 1;
    for (const auto& path : paths) {
        chars += path.size() + 1;
    }

    // Zero-filled, which is what supplies every NUL below: the per-entry
    // terminators and the one closing the list are never written explicitly.
    std::vector<unsigned char> blob(sizeof(DROPFILES) + chars * sizeof(wchar_t), 0);

    auto* header = reinterpret_cast<DROPFILES*>(blob.data());
    // Offset of the character region from the start of the struct, not an
    // absolute address, so the block stays relocatable once it is in an HGLOBAL.
    header->pFiles = sizeof(DROPFILES);
    header->pt = POINT{0, 0};
    header->fNC = FALSE;
    // The paths below are wchar_t. A reader that trusts this flag and finds
    // char would read every second byte as a character.
    header->fWide = TRUE;

    auto* out = reinterpret_cast<wchar_t*>(blob.data() + sizeof(DROPFILES));
    for (const auto& path : paths) {
        out = std::copy(path.begin(), path.end(), out);
        ++out;  // steps over the zero-filled terminator
    }

    return blob;
}

}  // namespace fb2k_dnd

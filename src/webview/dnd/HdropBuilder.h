// HdropBuilder.h - CF_HDROP / DROPFILES construction, isolated from COM so it can be unit-tested.
#pragma once

#include <string>
#include <vector>

namespace fb2k_dnd {

// Serialises paths into the DROPFILES layout CF_HDROP expects: the struct,
// then the wide paths back to back, each NUL-terminated, then one extra NUL to
// close the list. Returns bytes, not an HGLOBAL, so it can be unit-tested and
// so the caller decides how the memory is allocated.
//
// Returns an empty buffer for an empty list: an HDROP with no files is not a
// meaningful drag payload and callers must not build one.
std::vector<unsigned char> BuildDropFilesBlock(const std::vector<std::wstring>& paths);

}  // namespace fb2k_dnd

// DragOutPathPolicy.h - plans page-supplied paths into a CF_HDROP file list.
#pragma once

#include <functional>
#include <string>
#include <vector>

namespace fb2k_dnd {

// Why a page-supplied path cannot be dragged out.
enum class DragOutReject {
    None,
    EmptyList,     // caller passed no paths at all
    NoNativePath,  // stream, cdda://, or any location without a local file
};

struct DragOutPlan {
    bool ok = false;
    // Index into the caller's array, meaningful only when ok is false. Reported
    // back as paths[i] so the caller can locate the offending entry without the
    // path itself appearing in the error, which is a red line.
    size_t rejectedIndex = 0;
    DragOutReject reason = DragOutReject::None;
    // Deduplicated, first-occurrence order. This list, not the caller's input,
    // is what gets validated and what ends up in CF_HDROP; letting the two
    // diverge is what made protocol paths and archive entries slip through.
    std::vector<std::wstring> nativePaths;
};

// Maps one page-supplied location to a native filesystem path.
//
// Production passes an adapter over foobar2000_io::extract_native_path_archive_aware,
// which resolves file:// and rewrites archive:// / unpack:// entries to their
// container file, and rejects everything else. Injected rather than called
// directly so this file stays free of fb2k SDK dependencies and can be unit-tested.
using NativePathResolver =
    std::function<bool(const std::wstring& input, std::wstring& native)>;

// A trailing "|subsong:N" is removed before an entry reaches the resolver: it
// selects a track inside a container, and only the container is a file.
DragOutPlan BuildDragOutPlan(const std::vector<std::wstring>& input,
                             const NativePathResolver& resolve);

}  // namespace fb2k_dnd

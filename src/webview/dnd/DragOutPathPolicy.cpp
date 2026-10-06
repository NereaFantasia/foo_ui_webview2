// DragOutPathPolicy.cpp
#include "pch.h"
#include "webview/dnd/DragOutPathPolicy.h"

#include <algorithm>
#include <cwctype>
#include <unordered_set>

namespace fb2k_dnd {
namespace {

// Windows filesystem paths compare case-insensitively, so two spellings of one
// file must not produce two HDROP entries. Folded only for the dedupe key; the
// entry itself keeps the caller's spelling.
std::wstring FoldForCompare(const std::wstring& s) {
    std::wstring out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });
    return out;
}

// "path|subsong:N" names one track inside a cue sheet or a multi-track file;
// the rest of the API accepts that form, so pages hand it in here too. Only the
// container is a file, and a '|' in an HDROP entry is an invalid filename that
// a drop target silently ignores. Cut at the first marker, as QueueApi and
// MetadataApi do.
std::wstring StripSubsongSuffix(const std::wstring& s) {
    const size_t pos = s.find(L"|subsong:");
    return pos == std::wstring::npos ? s : s.substr(0, pos);
}

}  // namespace

DragOutPlan BuildDragOutPlan(const std::vector<std::wstring>& input,
                             const NativePathResolver& resolve) {
    DragOutPlan plan;
    if (input.empty() || !resolve) {
        plan.reason = DragOutReject::EmptyList;
        return plan;
    }

    std::unordered_set<std::wstring> seen;
    std::vector<std::wstring> out;
    out.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i) {
        std::wstring native;
        if (!resolve(StripSubsongSuffix(input[i]), native) || native.empty()) {
            // Fail fast rather than skip: a partial drag would silently deliver
            // fewer files than the page asked for, with no way to tell.
            plan.rejectedIndex = i;
            plan.reason = DragOutReject::NoNativePath;
            return plan;
        }
        if (seen.insert(FoldForCompare(native)).second) {
            out.push_back(std::move(native));
        }
    }

    plan.ok = true;
    plan.nativePaths = std::move(out);
    return plan;
}

}  // namespace fb2k_dnd

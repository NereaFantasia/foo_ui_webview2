// DndOriginPolicy.cpp
#include "pch.h"
#include "webview/dnd/DndOriginPolicy.h"

#include "webview/OriginPolicy.h"

namespace fb2k_dnd {

// Exact equality on normalised origins: a prefix test would also accept hosts
// such as "https://foo-ui-webview2.local.attacker.test".
bool AllowsPaths(std::wstring_view origin, std::wstring_view devServerOrigin) {
    if (origin.empty()) return false;
    if (origin == origin_policy::kBundledOrigin) return true;
    return !devServerOrigin.empty() && origin == devServerOrigin;
}

}  // namespace fb2k_dnd

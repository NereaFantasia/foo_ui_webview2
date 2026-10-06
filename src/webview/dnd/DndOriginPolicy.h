// DndOriginPolicy.h - gates the real-path side channel by page origin.
#pragma once

#include <string>
#include <string_view>

namespace fb2k_dnd {

// Whether a page at this origin may receive real filesystem paths.
//
// Closed by default. Real paths leak the user name, directory layout and
// project names, so only the component's own bundled frontend (and, in dev
// mode, the developer's own dev server) is trusted.
//
// Deliberately narrower than the invoke gate (WebViewHost::IsTrustedOrigin):
// that also trusts a panel's configured URL and the http(s) URLs popups were
// opened at, so reusing it would open paths to those pages too.
//
// origin           normalised origin (origin_policy::OriginOf of the page URL)
// devServerOrigin  origin of the configured dev server, empty when it is off
//                  (origin_policy::DevServerOrigin); injected rather than read
//                  here so this file stays free of fb2k SDK dependencies
bool AllowsPaths(std::wstring_view origin, std::wstring_view devServerOrigin);

}  // namespace fb2k_dnd

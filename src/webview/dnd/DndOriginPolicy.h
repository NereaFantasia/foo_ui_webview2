// DndOriginPolicy.h - gates the real-path side channel by page origin.
#pragma once

#include <string>

namespace fb2k_dnd {

// Whether a page at this origin may receive real filesystem paths.
//
// Closed by default. Real paths leak the user name, directory layout and
// project names, so only the component's own bundled frontend (and, in dev
// mode, the developer's own dev server) is trusted.
//
// Deliberately independent of WebViewHost::IsOriginAllowed: that list is an
// invoke transport allow-list which popups add arbitrary third-party URLs to,
// so reusing it would open paths to every popup.
//
// origin            normalised origin (scheme://host[:port], no path)
// devServerEnabled  caller passes security_config::UseDevServer(); injected
//                   rather than read here so this file stays free of fb2k SDK
//                   dependencies and can be unit-tested
bool AllowsPaths(const std::wstring& origin, bool devServerEnabled);

}  // namespace fb2k_dnd

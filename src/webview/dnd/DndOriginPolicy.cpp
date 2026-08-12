// DndOriginPolicy.cpp
#include "pch.h"
#include "webview/dnd/DndOriginPolicy.h"

#include <string_view>

namespace fb2k_dnd {
namespace {

// Origins served by the component's own bundled frontend. Matched by exact
// equality: a prefix test would also accept hosts such as
// "https://app.local.attacker.test".
constexpr std::wstring_view kTrustedOrigins[] = {
    L"https://foo-ui-webview2.local",
    L"https://app.local",
    L"https://fb2k.local",
};

// Schemes a local dev server may be reached over.
constexpr std::wstring_view kDevSchemes[] = {
    L"http://",
    L"https://",
};

// Loopback hosts only. A dev server reachable from the network is not trusted,
// because enabling dev mode must not widen the gate to arbitrary pages.
constexpr std::wstring_view kDevHosts[] = {
    L"localhost",
    L"127.0.0.1",
    L"[::1]",
};

// Whether the text is a decimal TCP port in 1..65535.
bool IsPortNumber(std::wstring_view text) {
    if (text.empty() || text.size() > 5) return false;
    unsigned value = 0;
    for (wchar_t c : text) {
        if (c < L'0' || c > L'9') return false;
        value = value * 10 + static_cast<unsigned>(c - L'0');
    }
    return value >= 1 && value <= 65535;
}

// Requires all three components to match: an allowed scheme, a loopback host by
// exact equality, and an explicit numeric port. Anything left over (a path, a
// query, extra labels on the host) fails one of the three and is rejected.
bool IsLoopbackDevOrigin(std::wstring_view origin) {
    for (std::wstring_view scheme : kDevSchemes) {
        // The size check also rejects a bare scheme with an empty authority.
        if (origin.size() <= scheme.size()) continue;
        if (!origin.starts_with(scheme)) continue;

        const std::wstring_view authority = origin.substr(scheme.size());
        const size_t colon = authority.rfind(L':');
        if (colon == std::wstring_view::npos) return false;
        if (!IsPortNumber(authority.substr(colon + 1))) return false;

        const std::wstring_view host = authority.substr(0, colon);
        for (std::wstring_view devHost : kDevHosts) {
            if (host == devHost) return true;
        }
        return false;
    }
    return false;
}

}  // namespace

bool AllowsPaths(const std::wstring& origin, bool devServerEnabled) {
    if (origin.empty()) return false;

    for (std::wstring_view trusted : kTrustedOrigins) {
        if (origin == trusted) return true;
    }

    return devServerEnabled && IsLoopbackDevOrigin(origin);
}

}  // namespace fb2k_dnd

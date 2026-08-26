// DragSession.h - tracks one drag-and-drop session and its two expiry windows.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "webview/dnd/ShortcutResolver.h"

namespace fb2k_dnd {

// Host-side session retention. Serves getPathsAsync, which is the only reliable
// way for a page to obtain paths, so it must outlive long async handlers.
constexpr int64_t kHostSessionTtlMs = 60000;

// Page-side snapshot retention. Deliberately short: it only backs the optimistic
// synchronous getPaths, and a longer window would leave stale paths readable by
// any script after the drag ended.
constexpr int64_t kPageSnapshotTtlMs = 1500;

// Upper bound for a session that never received its leave or drop. Reached when
// the source process dies mid-hover, which leaves the session Active forever and
// would otherwise keep the paths readable for the lifetime of the window.
constexpr int64_t kActiveSessionMaxAgeMs = 300000;

enum class SessionPhase { Active, Ended };

struct SessionData {
    std::string sessionId;
    std::vector<std::wstring> paths;
    // Shortcut targets, one entry per path, in the same order. Empty entries
    // for paths that are not shortcuts or whose target could not be read.
    //
    // The store keeps this the same length as paths, so a reader may index it
    // with a paths index without a bounds check of its own.
    std::vector<ResolvedTarget> resolvedPaths;
    bool hasFiles = false;
    SessionPhase phase = SessionPhase::Active;
    int64_t startedAtMs = 0;
    int64_t endedAtMs = 0;
};

// Not thread-safe by itself; the drop target only touches it on the UI thread.
class DragSessionStore {
public:
    // Starts a new session, discarding any previous one (re-entrancy guard).
    // Returns the new sessionId.
    //
    // resolvedPaths is padded or truncated to the length of paths, so the two
    // arrays cannot get out of step even if a caller supplies a mismatched one
    // or omits it. Omitting it means "no shortcut targets known".
    std::string BeginSession(std::vector<std::wstring> paths, bool hasFiles,
                             int64_t nowMs,
                             std::vector<ResolvedTarget> resolvedPaths = {});

    // Marks the session ended. Ignored when sessionId does not match.
    void EndSession(const std::string& sessionId, int64_t nowMs);

    // Replaces the path list, used because Drop carries the final list. Both
    // arrays are replaced together, under the same length rule as BeginSession.
    void UpdatePaths(const std::string& sessionId,
                     std::vector<std::wstring> paths, bool hasFiles,
                     std::vector<ResolvedTarget> resolvedPaths = {});

    // Lookup for dnd.getPathsAsync. Empty sessionId means "current or most
    // recently ended". Returns nullptr when absent, past kHostSessionTtlMs after
    // ending, or past kActiveSessionMaxAgeMs without ever ending.
    const SessionData* Query(const std::string& sessionId, int64_t nowMs) const;

    bool HasActiveSession() const;
    void Clear();

private:
    SessionData current_{};
    bool valid_ = false;
};

// True while a page-side snapshot should still be considered readable.
bool IsPageSnapshotFresh(const SessionData& s, int64_t nowMs);

}  // namespace fb2k_dnd

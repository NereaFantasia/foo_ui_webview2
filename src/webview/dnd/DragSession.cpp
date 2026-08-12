// DragSession.cpp
#include "pch.h"
#include "webview/dnd/DragSession.h"

#include <atomic>

namespace fb2k_dnd {
namespace {

// Process-wide, so ids stay unique across every window that owns a store.
// getPathsAsync uses the id to find which window holds the session, and
// per-store counters would hand out the same id in two windows at once.
std::atomic<uint64_t> g_sessionCounter{0};

std::string MakeSessionId(int64_t nowMs) {
    const uint64_t ordinal = g_sessionCounter.fetch_add(1) + 1;
    return "dnd-" + std::to_string(ordinal) + "-" + std::to_string(nowMs);
}

}  // namespace

std::string DragSessionStore::BeginSession(std::vector<std::wstring> paths,
                                          bool hasFiles, int64_t nowMs) {
    // A new drag always supersedes the previous one: a source can re-enter
    // without a matching leave, and keeping both would make Query ambiguous.
    current_ = SessionData{};
    current_.sessionId = MakeSessionId(nowMs);
    current_.paths = std::move(paths);
    current_.hasFiles = hasFiles;
    current_.startedAtMs = nowMs;
    valid_ = true;
    return current_.sessionId;
}

void DragSessionStore::EndSession(const std::string& sessionId, int64_t nowMs) {
    if (!valid_ || current_.sessionId != sessionId) {
        return;  // a stale leave/drop belonging to a superseded session
    }
    current_.phase = SessionPhase::Ended;
    current_.endedAtMs = nowMs;
}

void DragSessionStore::UpdatePaths(const std::string& sessionId,
                                   std::vector<std::wstring> paths,
                                   bool hasFiles) {
    if (!valid_ || current_.sessionId != sessionId) {
        return;
    }
    current_.paths = std::move(paths);
    current_.hasFiles = hasFiles;
}

const SessionData* DragSessionStore::Query(const std::string& sessionId,
                                           int64_t nowMs) const {
    if (!valid_) {
        return nullptr;
    }
    if (!sessionId.empty() && sessionId != current_.sessionId) {
        return nullptr;
    }
    // An active drag stays readable while the user keeps hovering, but not
    // indefinitely: a session whose leave or drop never arrived would otherwise
    // never expire, since only BeginSession or Clear can replace it.
    const int64_t age = current_.phase == SessionPhase::Ended
                            ? nowMs - current_.endedAtMs
                            : nowMs - current_.startedAtMs;
    const int64_t limit = current_.phase == SessionPhase::Ended ? kHostSessionTtlMs
                                                               : kActiveSessionMaxAgeMs;
    if (age > limit) {
        return nullptr;
    }
    return &current_;
}

bool DragSessionStore::HasActiveSession() const {
    return valid_ && current_.phase == SessionPhase::Active;
}

void DragSessionStore::Clear() {
    current_ = SessionData{};
    valid_ = false;
}

bool IsPageSnapshotFresh(const SessionData& s, int64_t nowMs) {
    if (s.phase == SessionPhase::Active) {
        return true;
    }
    return nowMs - s.endedAtMs <= kPageSnapshotTtlMs;
}

}  // namespace fb2k_dnd

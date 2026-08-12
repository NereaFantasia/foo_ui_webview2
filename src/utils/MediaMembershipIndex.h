// MediaMembershipIndex.h - membership set over already-canonical media paths.
// No entry cap by design: an unbounded set is a correctness requirement, do not add a limit back.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace fb2k_utils {

// Stores the set of media paths a caller considers "known" and answers exact
// membership queries against it.
//
// Standard library only: no foobar2000 SDK, Win32 or core_api dependency, so
// the type is constructible in unit tests and in worker threads.
//
// Input contract: every path handed to Rebuild must already be canonical, in a
// single consistent form chosen by the caller (comparison is byte-exact, with
// no case folding, separator fixing or prefix rewriting). Canonicalisation
// belongs to the caller; this class never transforms a path.
//
// Contains never returns an approximate answer. There is no size threshold past
// which lookups degrade or bail out, so a false result always means "absent"
// and never "gave up counting".
//
// Locking uses a shared_mutex for a read-heavy pattern. It is precautionary:
// every current caller runs on the foobar2000 main thread. Sharding is
// deliberately absent because no contention has been measured.
class MediaMembershipIndex {
public:
    MediaMembershipIndex() = default;

    MediaMembershipIndex(const MediaMembershipIndex&) = delete;
    MediaMembershipIndex& operator=(const MediaMembershipIndex&) = delete;

    // Replaces the whole set and marks the index valid, even for an empty
    // input. Bumps the generation counter once per call.
    void Rebuild(std::vector<std::string> canonicalPaths);

    // Exact-match lookup. Always false while the index is invalid.
    bool Contains(const std::string& canonicalPath) const;

    // Validity and membership read under one lock acquisition. Returns nullopt
    // when the index is invalid, so a caller can tell "no answer available"
    // apart from a definite "absent" without a check-then-act window.
    std::optional<bool> Query(const std::string& canonicalPath) const;

    // Drops all entries, marks the index invalid and bumps the generation.
    void Invalidate();

    // True once Rebuild has run and no Invalidate has followed. Distinct from
    // an empty set: "built but empty" still answers queries authoritatively.
    bool IsValid() const;

    // Monotonic counter, incremented by Rebuild and Invalidate. Callers cache
    // it to detect that a snapshot they read has since been replaced.
    uint64_t Generation() const;

    size_t Size() const;

private:
    mutable std::shared_mutex mutex_;
    std::unordered_set<std::string> paths_;
    bool valid_ = false;
    uint64_t generation_ = 0;
};

}  // namespace fb2k_utils

// PathCanonicalCache.h - memoises resolved paths against a parent-directory stamp.
#pragma once

#include <cstddef>
#include <cstdint>
#include <list>
#include <optional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace fb2k_utils {

// Caches the expensive "raw path -> fully resolved path" step of path
// validation, keyed by the exact input string and guarded by a caller-supplied
// stamp describing the state of the path's parent directory.
//
// Standard library only: no foobar2000 SDK, Win32 or core_api dependency. The
// parent stamp is a parameter rather than something this class reads, which
// keeps the type constructible in unit tests. Callers on Windows build the
// stamp from GetFileAttributesExW; the class never interprets its value and
// only compares it for equality.
//
// Correctness model: a stored entry is reused only when the stamp handed to
// Lookup equals the stamp recorded by Store. There is no time-based expiry.
// Any residency limit here is memory hygiene, never a correctness window: an
// entry that survives forever is still only returned while its stamp matches.
//
// Known coverage boundary, accepted deliberately: a directory stamp reflects
// the final path component's own directory entry, so re-pointing an ancestor
// directory link is not observed. Verifying every ancestor costs about as much
// as re-resolving the path outright, which would remove the reason for caching.
// This boundary is structural and enumerable rather than a vague time window,
// so it can be described and tested precisely.
//
// Locking uses a plain mutex, not a shared_mutex. Recency tracking makes a
// successful lookup a mutation of the eviction order, so there is no read-only
// path for a shared lock to accelerate. Sharding is deliberately absent because
// no contention has been measured; every current caller runs on the
// foobar2000 main thread.
class PathCanonicalCache {
public:
    // Entry ceiling chosen to comfortably span a batch over one album or artist
    // directory while keeping worst-case residency in the low megabytes.
    static constexpr size_t kDefaultCapacity = 4096;

    explicit PathCanonicalCache(size_t capacity = kDefaultCapacity);

    PathCanonicalCache(const PathCanonicalCache&) = delete;
    PathCanonicalCache& operator=(const PathCanonicalCache&) = delete;

    // Returns the stored resolved path when an entry exists and its recorded
    // stamp equals parentStamp. A stamp mismatch drops the stale entry and
    // reports a miss, so the caller re-resolves and re-stores. A hit marks the
    // entry most recently used.
    std::optional<std::wstring> Lookup(const std::wstring& key, uint64_t parentStamp);

    // Inserts or replaces the entry for key and marks it most recently used,
    // evicting the least recently used entry when the capacity is exceeded.
    // Storing into a zero-capacity cache is a no-op.
    void Store(const std::wstring& key, std::wstring resolvedPath, uint64_t parentStamp);

    // Drops every entry. Provided for callers that learn the filesystem changed
    // in a way no per-entry stamp would catch; not required for correctness.
    void Clear();

    size_t Size() const;
    size_t Capacity() const;

    // Cumulative counters, for tests and diagnostics. A stamp mismatch counts
    // as a miss, not as a separate category.
    uint64_t HitCount() const;
    uint64_t MissCount() const;
    uint64_t EvictionCount() const;

private:
    struct Entry {
        std::wstring resolvedPath;
        uint64_t parentStamp = 0;
        // Position of this key in recency_, so a hit can splice it to the front
        // without searching the list.
        std::list<std::wstring>::iterator recencyPos;
    };

    mutable std::mutex mutex_;
    size_t capacity_;
    std::unordered_map<std::wstring, Entry> entries_;
    // Most recently used at the front, least recently used at the back.
    std::list<std::wstring> recency_;
    uint64_t hits_ = 0;
    uint64_t misses_ = 0;
    uint64_t evictions_ = 0;
};

}  // namespace fb2k_utils

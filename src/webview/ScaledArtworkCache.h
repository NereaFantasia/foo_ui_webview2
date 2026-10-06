#pragma once
// ScaledArtworkCache - bounded in-memory cache for resized artwork bytes.
//
// Pure STL, no SDK/COM dependency, so the same type is exercised by unit tests
// and by the WebView resource handler. The cache owns its lock: Get/Put may be
// called from the owner thread and from worker completion callbacks alike.
//
// Eviction runs inside every Put, so the async completion path and the
// synchronous fallback share one policy instead of each carrying its own:
//   1. drop unverified entries (empty fingerprint) older than unverifiedTtl;
//   2. when the entry count or the byte total would exceed the limits, evict a
//      20% batch of the least-recently-accessed entries and keep evicting until
//      the new entry fits.
//
// Freshness: an entry that carries a source fingerprint is compared against the
// fingerprint of the current request on Get and dropped on mismatch, so it does
// not need to age out by TTL. An entry without a fingerprint could not be tied
// to a source file and is only kept for a short session window.

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace artwork_cache {

struct ScaledArtworkLimits {
    size_t maxEntries = 500;
    size_t maxBytes = 64ull * 1024 * 1024;
    std::chrono::seconds unverifiedTtl{300};
};

struct ScaledArtworkHit {
    std::shared_ptr<const std::vector<uint8_t>> bytes;
    std::string mimeType;
};

class ScaledArtworkCache {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using SharedBytes = std::shared_ptr<const std::vector<uint8_t>>;

    explicit ScaledArtworkCache(ScaledArtworkLimits limits) : limits_(limits) {}

    ScaledArtworkCache(const ScaledArtworkCache&) = delete;
    ScaledArtworkCache& operator=(const ScaledArtworkCache&) = delete;

    // Returns the cached bytes and refreshes lastAccess on a hit.
    // Returns nullopt (and removes the entry) when the entry is stale:
    // fingerprint mismatch for verified entries, TTL expiry for unverified ones.
    std::optional<ScaledArtworkHit> Get(const std::string& key,
                                        const std::string& currentFingerprint,
                                        TimePoint now) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entries_.find(key);
        if (it == entries_.end()) {
            return std::nullopt;
        }
        Entry& entry = it->second;
        if (entry.fingerprint.empty()) {
            if (now - entry.created >= limits_.unverifiedTtl) {
                EraseLocked(it);
                return std::nullopt;
            }
        } else if (entry.fingerprint != currentFingerprint) {
            EraseLocked(it);
            return std::nullopt;
        }
        entry.lastAccess = now;
        return ScaledArtworkHit{entry.bytes, entry.mimeType};
    }

    // Inserts or replaces an entry; runs the eviction policy first so the
    // limits hold after the insert. Null bytes and payloads that exceed the
    // whole byte budget on their own are not cached.
    void Put(const std::string& key,
             SharedBytes bytes,
             std::string mimeType,
             std::string fingerprint,
             TimePoint now) {
        if (!bytes || bytes->size() > limits_.maxBytes) {
            return;
        }
        const size_t incoming = bytes->size();
        std::lock_guard<std::mutex> lock(mutex_);
        auto existing = entries_.find(key);
        if (existing != entries_.end()) {
            EraseLocked(existing);
        }
        EvictLocked(now, incoming);
        totalBytes_ += incoming;
        entries_.emplace(key, Entry{std::move(bytes), std::move(mimeType),
                                    std::move(fingerprint), now, now});
    }

    size_t EntryCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_.size();
    }

    size_t TotalBytes() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return totalBytes_;
    }

private:
    struct Entry {
        SharedBytes bytes;
        std::string mimeType;
        std::string fingerprint;
        TimePoint created;
        TimePoint lastAccess;
    };
    using Map = std::unordered_map<std::string, Entry>;

    // Must hold mutex_. Returns the iterator following the erased entry.
    Map::iterator EraseLocked(Map::iterator it) {
        totalBytes_ -= it->second.bytes->size();
        return entries_.erase(it);
    }

    // Must hold mutex_.
    void EvictLocked(TimePoint now, size_t incomingBytes) {
        for (auto it = entries_.begin(); it != entries_.end();) {
            const Entry& entry = it->second;
            if (entry.fingerprint.empty() && now - entry.created >= limits_.unverifiedTtl) {
                it = EraseLocked(it);
            } else {
                ++it;
            }
        }

        const auto fits = [&]() {
            return entries_.size() < limits_.maxEntries &&
                   totalBytes_ + incomingBytes <= limits_.maxBytes;
        };
        if (fits()) {
            return;
        }

        std::vector<Map::iterator> byAccess;
        byAccess.reserve(entries_.size());
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            byAccess.push_back(it);
        }
        std::sort(byAccess.begin(), byAccess.end(), [](Map::iterator a, Map::iterator b) {
            return a->second.lastAccess < b->second.lastAccess;
        });

        // Evict a 20% batch so a full cache does not thrash on every insert,
        // then keep going only if the incoming bytes still do not fit. The batch
        // is sized from the current population: at the entry-count trigger that
        // equals maxEntries / 5, while a byte-budget trigger on a sparsely filled
        // cache does not wipe most of it.
        const size_t batch = std::max<size_t>(1, entries_.size() / 5);
        size_t evicted = 0;
        for (Map::iterator it : byAccess) {
            if (evicted >= batch && fits()) {
                break;
            }
            EraseLocked(it);
            ++evicted;
        }
    }

    ScaledArtworkLimits limits_;
    mutable std::mutex mutex_;
    Map entries_;
    size_t totalBytes_ = 0;
};

}  // namespace artwork_cache

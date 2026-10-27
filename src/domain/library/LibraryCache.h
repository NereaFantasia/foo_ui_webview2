/**
 * LibraryCache - In-memory cache for media library data
 * 
 * Tells when the library changed. The API layer keeps its own declared
 * results (albums, artists, status, the full track list) and drops them when
 * the generations below move, so this header does not depend on the generated
 * API types. Invalidate() runs whenever tracks are added, removed or retagged;
 * InvalidateHookFields() when only fields supplied by a component changed.
 */

#pragma once

#include "pch.h"
#include <shared_mutex>
#include <chrono>
#include <cstdint>

using json = nlohmann::json;

/**
 * Singleton class managing media library cache
 */
class LibraryCache {
public:
    static LibraryCache& GetInstance();
    
    // Prevent copying
    LibraryCache(const LibraryCache&) = delete;
    LibraryCache& operator=(const LibraryCache&) = delete;
    
    //==========================================================================
    // Cache Control
    //==========================================================================
    
    /**
     * Invalidate all caches. Called when library changes are detected.
     */
    void Invalidate();

    /**
     * For a change from a display hook: a component such as foo_playcount
     * asked for its fields (%rating%, %play_count%) to be shown anew, and paths
     * and tags are as before. Moves GetGeneration() only; IsValid() and
     * GetLastModified() keep describing the last Invalidate().
     */
    void InvalidateHookFields();

    /**
     * Check if cache is valid
     */
    bool IsValid() const;
    
    /**
     * Get cache statistics
     */
    json GetStats() const;
    
    /**
     * Get last modification timestamp (milliseconds since epoch)
     */
    int64_t GetLastModified() const;

    /**
     * Counts Invalidate() and InvalidateHookFields() calls. A result kept
     * elsewhere that carries a hook field is stale once the generation it was
     * stored under differs from this.
     */
    uint64_t GetGeneration() const;

    /**
     * Counts Invalidate() calls only. A result built from paths and tags alone
     * is stale once the generation it was stored under differs from this.
     */
    uint64_t GetFileGeneration() const;

    /**
     * Marks the cache valid on behalf of a result kept elsewhere, so that
     * IsValid() keeps meaning "something was kept since the last Invalidate()".
     */
    void NoteKept();

    /**
     * Counts one lookup of a result kept elsewhere in the hit/miss statistics.
     */
    void RecordLookup(bool hit) const;

private:
    LibraryCache();
    
    // Thread safety
    mutable std::shared_mutex m_mutex;
    
    // Cache validity
    std::atomic<bool> m_valid{false};
    std::atomic<int64_t> m_lastModified{0};
    std::atomic<uint64_t> m_generation{0};
    std::atomic<uint64_t> m_fileGeneration{0};

    // Statistics
    mutable std::atomic<size_t> m_cacheHits{0};
    mutable std::atomic<size_t> m_cacheMisses{0};
};

// Convenience macro
#define g_LibraryCache LibraryCache::GetInstance()

/**
 * LibraryCache - In-memory cache for media library data
 * 
 * Tells when the library changed. The API layer keeps its own declared
 * results (albums, artists, status, the full track list) and drops them when
 * GetGeneration() moves, so this header does not depend on the generated API
 * types. Invalidate() runs whenever a library change is detected.
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
     * Counts Invalidate() calls. A result kept elsewhere is stale once the
     * generation it was stored under differs from this.
     */
    uint64_t GetGeneration() const;

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
    
    // Statistics
    mutable std::atomic<size_t> m_cacheHits{0};
    mutable std::atomic<size_t> m_cacheMisses{0};
};

// Convenience macro
#define g_LibraryCache LibraryCache::GetInstance()

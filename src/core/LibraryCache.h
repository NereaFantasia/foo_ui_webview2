/**
 * LibraryCache - In-memory cache for media library data
 * 
 * Provides fast access to albums and tracks without re-scanning the library.
 * Cache is automatically invalidated when library changes are detected.
 */

#pragma once

#include "pch.h"
#include <shared_mutex>
#include <chrono>
#include <optional>
#include <memory>

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
    
    //==========================================================================
    // Albums Cache
    //==========================================================================
    
    /**
     * Get cached albums data. Returns nullopt if cache is invalid.
     * @param query Filter query (empty = all)
     * @param sortBy Sort field
     * @param includeCover Whether covers were included
     */
    std::optional<json> GetCachedAlbums(
        const std::string& query,
        const std::string& sortBy,
        bool includeCover
    ) const;
    
    /**
     * Store albums data in cache
     */
    void SetCachedAlbums(
        const std::string& query,
        const std::string& sortBy,
        bool includeCover,
        const json& data
    );
    
    //==========================================================================
    // Tracks Cache
    //==========================================================================
    
    /**
     * Get cached tracks data. Returns nullopt if cache is invalid.
     */
    std::optional<json> GetCachedTracks() const;

    /**
     * Get cached tracks data without deep-copying the payload.
     *
     * Returns a shared, immutable handle to the cached tracks json so a
     * cache hit costs only an atomic refcount bump instead of cloning the
     * entire library (which can be multiple MB). Returns nullptr when the
     * cache is invalid or empty. Callers must not mutate the pointee; copy
     * out only the slice they need (e.g. one page of tracks).
     */
    std::shared_ptr<const json> GetCachedTracksShared() const;

    /**
     * Store tracks data in cache
     */
    void SetCachedTracks(const json& data);
    
    //==========================================================================
    // Artists / Genres / Stats Cache
    //==========================================================================
    
    std::optional<json> GetCachedArtists() const;
    void SetCachedArtists(const json& data);
    
    std::optional<json> GetCachedGenres() const;
    void SetCachedGenres(const json& data);
    
    std::optional<json> GetCachedStats() const;
    void SetCachedStats(const json& data);
    
    //==========================================================================
    // Cover Art Cache
    //==========================================================================
    
    /**
     * Get cached cover art dataUrl for a path
     */
    std::optional<std::string> GetCachedCover(const std::string& path) const;
    
    /**
     * Store cover art in cache
     * @param path Track path
     * @param dataUrl Base64 data URL
     * @param sizeBytes Size in bytes (for memory management)
     */
    void SetCachedCover(const std::string& path, const std::string& dataUrl, size_t sizeBytes);
    
    /**
     * Get cover cache statistics
     */
    json GetCoverCacheStats() const;

private:
    LibraryCache();
    
    // Thread safety
    mutable std::shared_mutex m_mutex;
    
    // Cache validity
    std::atomic<bool> m_valid{false};
    std::atomic<int64_t> m_lastModified{0};
    
    // Albums cache key: query + sortBy + includeCover
    struct AlbumsCacheKey {
        std::string query;
        std::string sortBy;
        bool includeCover;
        
        bool operator==(const AlbumsCacheKey& other) const {
            return query == other.query && sortBy == other.sortBy && includeCover == other.includeCover;
        }
    };
    
    struct AlbumsCacheKeyHash {
        size_t operator()(const AlbumsCacheKey& k) const {
            return std::hash<std::string>()(k.query) ^ 
                   (std::hash<std::string>()(k.sortBy) << 1) ^
                   (std::hash<bool>()(k.includeCover) << 2);
        }
    };
    
    std::unordered_map<AlbumsCacheKey, json, AlbumsCacheKeyHash> m_albumsCache;
    
    // Tracks cache (all tracks, no filtering).
    // Stored as a shared_ptr<const json> so cache hits hand out a refcounted
    // immutable handle instead of deep-copying the whole library payload.
    std::shared_ptr<const json> m_tracksCache;
    
    // Aggregate query caches
    std::optional<json> m_artistsCache;
    std::optional<json> m_genresCache;
    std::optional<json> m_statsCache;
    
    // Cover art cache: path -> dataUrl
    std::unordered_map<std::string, std::string> m_coverCache;
    size_t m_coverCacheBytes{0};
    static constexpr size_t MAX_COVER_CACHE_BYTES = 100 * 1024 * 1024;  // 100MB limit
    
    // Statistics
    mutable std::atomic<size_t> m_cacheHits{0};
    mutable std::atomic<size_t> m_cacheMisses{0};
};

// Convenience macro
#define g_LibraryCache LibraryCache::GetInstance()

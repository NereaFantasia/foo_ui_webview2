/**
 * LibraryCache Implementation
 */

#include "pch.h"
#include "core/LibraryCache.h"

//==============================================================================
// Singleton Instance
//==============================================================================

LibraryCache& LibraryCache::GetInstance() {
    static LibraryCache instance;
    return instance;
}

LibraryCache::LibraryCache() {
    m_lastModified = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

//==============================================================================
// Cache Control
//==============================================================================

void LibraryCache::Invalidate() {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    m_valid = false;
    m_albumsCache.clear();
    m_tracksCache.reset();
    m_artistsCache.reset();
    m_genresCache.reset();
    m_statsCache.reset();
    // Note: We keep cover cache as covers don't change with library updates
    
    m_lastModified = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    FB2K_console_print("[LibraryCache] Cache invalidated");
}

bool LibraryCache::IsValid() const {
    return m_valid.load();
}

json LibraryCache::GetStats() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    
    return {
        {"valid", m_valid.load()},
        {"lastModified", m_lastModified.load()},
        {"albumsCacheEntries", m_albumsCache.size()},
        {"tracksCached", static_cast<bool>(m_tracksCache)},
        {"artistsCached", m_artistsCache.has_value()},
        {"genresCached", m_genresCache.has_value()},
        {"statsCached", m_statsCache.has_value()},
        {"coversCached", m_coverCache.size()},
        {"coverCacheBytes", m_coverCacheBytes},
        {"coverCacheMB", static_cast<double>(m_coverCacheBytes) / (1024.0 * 1024.0)},
        {"cacheHits", m_cacheHits.load()},
        {"cacheMisses", m_cacheMisses.load()},
    };
}

int64_t LibraryCache::GetLastModified() const {
    return m_lastModified.load();
}

//==============================================================================
// Albums Cache
//==============================================================================

std::optional<json> LibraryCache::GetCachedAlbums(
    const std::string& query,
    const std::string& sortBy,
    bool includeCover
) const {
    if (!m_valid.load()) {
        return std::nullopt;
    }
    
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    
    AlbumsCacheKey key{query, sortBy, includeCover};
    auto it = m_albumsCache.find(key);
    
    if (it != m_albumsCache.end()) {
        m_cacheHits++;
        return it->second;
    }
    
    m_cacheMisses++;
    return std::nullopt;
}

void LibraryCache::SetCachedAlbums(
    const std::string& query,
    const std::string& sortBy,
    bool includeCover,
    const json& data
) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    AlbumsCacheKey key{query, sortBy, includeCover};
    m_albumsCache[key] = data;
    m_valid = true;
    
    FB2K_console_print("[LibraryCache] Albums cached (query='", query.c_str(), 
                       "', sort='", sortBy.c_str(), "', cover=", includeCover ? "yes" : "no", ")");
}

//==============================================================================
// Tracks Cache
//==============================================================================

std::optional<json> LibraryCache::GetCachedTracks() const {
    // Backward-compatible accessor. Prefer GetCachedTracksShared() to avoid
    // deep-copying the full payload; this overload still clones for callers
    // that need an owned json.
    auto shared = GetCachedTracksShared();
    if (!shared) {
        return std::nullopt;
    }
    return *shared;
}

std::shared_ptr<const json> LibraryCache::GetCachedTracksShared() const {
    if (!m_valid.load()) {
        return nullptr;
    }

    std::shared_lock<std::shared_mutex> lock(m_mutex);

    if (m_tracksCache) {
        m_cacheHits++;
        return m_tracksCache;
    }

    m_cacheMisses++;
    return nullptr;
}

void LibraryCache::SetCachedTracks(const json& data) {
    auto stored = std::make_shared<const json>(data);

    std::unique_lock<std::shared_mutex> lock(m_mutex);

    m_tracksCache = std::move(stored);
    m_valid = true;

    FB2K_console_print("[LibraryCache] Tracks cached (", data.value("total", 0), " tracks)");
}

//==============================================================================
// Artists / Genres / Stats Cache
//==============================================================================

std::optional<json> LibraryCache::GetCachedArtists() const {
    if (!m_valid.load()) return std::nullopt;
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    if (m_artistsCache.has_value()) { m_cacheHits++; return m_artistsCache; }
    m_cacheMisses++;
    return std::nullopt;
}

void LibraryCache::SetCachedArtists(const json& data) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_artistsCache = data;
    m_valid = true;
}

std::optional<json> LibraryCache::GetCachedGenres() const {
    if (!m_valid.load()) return std::nullopt;
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    if (m_genresCache.has_value()) { m_cacheHits++; return m_genresCache; }
    m_cacheMisses++;
    return std::nullopt;
}

void LibraryCache::SetCachedGenres(const json& data) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_genresCache = data;
    m_valid = true;
}

std::optional<json> LibraryCache::GetCachedStats() const {
    if (!m_valid.load()) return std::nullopt;
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    if (m_statsCache.has_value()) { m_cacheHits++; return m_statsCache; }
    m_cacheMisses++;
    return std::nullopt;
}

void LibraryCache::SetCachedStats(const json& data) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_statsCache = data;
    m_valid = true;
}

//==============================================================================
// Cover Art Cache
//==============================================================================

std::optional<std::string> LibraryCache::GetCachedCover(const std::string& path) const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    
    auto it = m_coverCache.find(path);
    if (it != m_coverCache.end()) {
        m_cacheHits++;
        return it->second;
    }
    
    return std::nullopt;
}

void LibraryCache::SetCachedCover(const std::string& path, const std::string& dataUrl, size_t sizeBytes) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    // Check if we'd exceed the limit
    if (m_coverCacheBytes + sizeBytes > MAX_COVER_CACHE_BYTES) {
        // Don't cache this cover if it would exceed limit
        // A more sophisticated implementation could evict old entries
        return;
    }
    
    // Don't cache if already exists
    if (m_coverCache.contains(path)) {
        return;
    }
    
    m_coverCache[path] = dataUrl;
    m_coverCacheBytes += sizeBytes;
}

json LibraryCache::GetCoverCacheStats() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    
    return {
        {"count", m_coverCache.size()},
        {"bytes", m_coverCacheBytes},
        {"megabytes", static_cast<double>(m_coverCacheBytes) / (1024.0 * 1024.0)},
        {"maxMegabytes", static_cast<double>(MAX_COVER_CACHE_BYTES) / (1024.0 * 1024.0)},
    };
}

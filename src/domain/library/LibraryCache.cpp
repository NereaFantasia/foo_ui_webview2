/**
 * LibraryCache Implementation
 */

#include "pch.h"
#include "domain/library/LibraryCache.h"

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
    ++m_generation;
    ++m_fileGeneration;

    m_lastModified = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    FB2K_console_print("[LibraryCache] Cache invalidated");
}

void LibraryCache::InvalidateHookFields() {
    ++m_generation;
}

bool LibraryCache::IsValid() const {
    return m_valid.load();
}

json LibraryCache::GetStats() const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    
    return {
        {"valid", m_valid.load()},
        {"lastModified", m_lastModified.load()},
        {"cacheHits", m_cacheHits.load()},
        {"cacheMisses", m_cacheMisses.load()},
    };
}

int64_t LibraryCache::GetLastModified() const {
    return m_lastModified.load();
}

uint64_t LibraryCache::GetGeneration() const {
    return m_generation.load();
}

uint64_t LibraryCache::GetFileGeneration() const {
    return m_fileGeneration.load();
}

void LibraryCache::NoteKept() {
    m_valid = true;
}

void LibraryCache::RecordLookup(bool hit) const {
    if (hit) {
        m_cacheHits++;
    } else {
        m_cacheMisses++;
    }
}

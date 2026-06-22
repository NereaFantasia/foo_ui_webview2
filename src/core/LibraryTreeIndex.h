/**
 * LibraryTreeIndex - Media root and directory tree indexer
 *
 * Uses library_manager::get_relative_path() + filesystem::g_get_native_path()
 * path tail-comparison algorithm to deduce actual media root, avoiding raw path prefix guessing.
 *
 * Scope: roots indexing (library.getRoots) and the typed directory tree (library.browseTree).
 *
 * Built lazily on first API access, thread-safe with sync build + result caching.
 */

#pragma once

#include "pch.h"
#include <chrono>
#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <string>

using json = nlohmann::json;

// ============================================
// Data Structures
// ============================================

/**
 * Library root record
 */
struct LibraryRootRecord {
    std::string id;             // Stable root identifier (normalized absolutePath)
    std::string displayName;    // Default: last directory name; falls back to full path on conflict
    std::string rawPath;        // Same as absolutePath
    std::string absolutePath;   // Normalized local absolute path
    size_t trackCount = 0;      // Track count under this root
};

/**
 * Directory node record
 */
struct DirectoryNodeRecord {
    std::string id;                 // "rootId::pathId"
    std::string rootId;             // Root ID
    std::string pathId;             // Relative path ("/" delimited, root = "")
    std::string parentPathId;       // Parent directory pathId (root = "")
    std::string name;               // Current directory name
    std::string displayName;        // Display name (currently equals name)
    std::string rawPath;            // Same as absolutePath (no transform applied)
    std::string absolutePath;       // Normalized absolute path
    std::string relativePath;       // Same as pathId
    int depth = 0;                  // Number of "/" delimiters in pathId
    size_t trackCount = 0;          // Cumulative track count (including subdirectories)
    size_t directTrackCount = 0;    // Direct file count
    size_t childDirectoryCount = 0; // Direct subdirectory count
    std::vector<std::string> childPathIds;  // Direct subdirectory pathId list
};

/**
 * Index statistics
 */
struct LibraryTreeIndexStats {
    size_t indexedTracks = 0;   // Successfully indexed tracks
    size_t skippedTracks = 0;   // Skipped tracks (protocol URIs, non-local, etc.)
    int64_t lastBuilt = 0;      // Last build timestamp (milliseconds)
};

// ============================================
// Main Class
// ============================================

/**
 * Media root and directory tree indexer
 *
 * Root index responsibilities:
 * 1. Build roots index lazily
 * 2. Provide GetRootsJson() for library.getRoots API
 * 3. Provide Invalidate() for cache invalidation
 * 4. Provide GetStats() for library.getCacheStats extension
 *
 * Directory tree responsibilities:
 * 5. Build typed directory tree data
 * 6. Provide GetBrowseTreeJson() for library.browseTree API
 */
class LibraryTreeIndex {
public:
    static LibraryTreeIndex& GetInstance();

    // Non-copyable
    LibraryTreeIndex(const LibraryTreeIndex&) = delete;
    LibraryTreeIndex& operator=(const LibraryTreeIndex&) = delete;

    /**
     * Invalidate index cache; next access triggers rebuild
     */
    void Invalidate();

    /**
     * Check whether index is valid
     */
    bool IsValid() const;

    /**
     * Get root list JSON (return value of library.getRoots).
     * First call triggers synchronous index build.
     */
    json GetRootsJson();

    /**
     * Get directory tree JSON (return value of library.browseTree).
     * The files field is an empty array; callers should use GetDirectoryFileHandles instead.
     * First call triggers synchronous index build.
     */
    json GetBrowseTreeJson(const std::string& rootId, const std::string& pathId,
                           bool includeFiles, bool recursiveFiles);

    /**
     * Get file handle list under a specified directory
     * @param rootId   Root ID
     * @param pathId   Directory pathId ("" for root)
     * @param recursive true to include all descendant files
     * @param outHandles [out] File handle list
     */
    void GetDirectoryFileHandles(const std::string& rootId, const std::string& pathId,
                                 bool recursive, metadb_handle_list& outHandles,
                                 std::vector<size_t>& outGlobalIndices);

    /**
     * Get index statistics (used by library.getCacheStats)
     */
    json GetStats() const;

private:
    LibraryTreeIndex();

    /**
     * Ensure index is built (build on demand, thread-safe with sync)
     */
    void EnsureBuilt();

    /**
     * Execute full scan to build root index and directory tree
     */
    void BuildIndex();

    /**
     * Convert directory node to JSON
     */
    json DirectoryNodeToJson(const DirectoryNodeRecord& node) const;

    /**
     * Collect files under a directory (recursive or direct files only)
     */
    json CollectFiles(const std::string& rootId, const std::string& pathId,
                      bool recursive) const;

    // Synchronization
    mutable std::mutex m_mutex;

    // Core data
    std::vector<LibraryRootRecord> m_roots;
    LibraryTreeIndexStats m_stats;
    bool m_valid = false;

    // Cached JSON result
    json m_cachedRootsJson;

    // Directory data
    // key: "rootId::pathId" => directory node
    std::unordered_map<std::string, DirectoryNodeRecord> m_directoryNodes;
    // key: "rootId::pathId" => direct file metadb handles (stores index, not handle)
    struct FileEntry {
        size_t globalIndex;     // Index in m_allItems
        std::string path;       // File path
    };
    std::unordered_map<std::string, std::vector<FileEntry>> m_directFiles;

    // Global item reference (saved during BuildIndex for file lookups)
    metadb_handle_list m_allItems;
};

// Convenience macro
#define g_LibraryTreeIndex LibraryTreeIndex::GetInstance()

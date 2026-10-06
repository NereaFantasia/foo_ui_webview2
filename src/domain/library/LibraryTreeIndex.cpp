/**
 * LibraryTreeIndex Implementation
 *
 * Algorithm: path tail-comparison to deduce media root
 * 1. filesystem::g_get_native_path() to get native absolute path
 * 2. library_manager::get_relative_path() to get relative path
 * 3. Split both into segments, verify tail match
 * 4. Remaining prefix forms the root directory absolute path
 *
 * Also builds typed directory tree data
 */

#include "pch.h"
#include "domain/library/LibraryTreeIndex.h"
#include "domain/library/LibraryPathAlgebra.h"
#include <algorithm>
#include <unordered_map>

using namespace library_path;

// ============================================
// LibraryTreeIndex Implementation
// ============================================

LibraryTreeIndex::LibraryTreeIndex() = default;

LibraryTreeIndex& LibraryTreeIndex::GetInstance() {
    static LibraryTreeIndex instance;
    return instance;
}

void LibraryTreeIndex::Invalidate() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_valid = false;
    m_roots.clear();
    m_cachedRootsJson = json();
    // Clear directory data
    m_directoryNodes.clear();
    m_directFiles.clear();
    m_allItems.remove_all();
    // Also reset stats so they return zero when treeIndexValid=false
    m_stats = LibraryTreeIndexStats{};
    FB2K_console_print("[LibraryTreeIndex] Index cache invalidated");
}

bool LibraryTreeIndex::IsValid() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_valid;
}

json LibraryTreeIndex::GetRootsJson() {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto lib = library_manager::get();

    // Library not enabled
    if (!lib->is_library_enabled()) {
        return {
            {"success", true},
            {"enabled", false},
            {"roots", json::array()},
            {"total", 0},
            {"indexedTracks", 0},
            {"skippedTracks", 0},
            {"fromCache", false}
        };
    }

    // Ensure index is built
    bool wasCached = m_valid;
    EnsureBuilt();

    // Return cached result (set fromCache to reflect actual cache hit)
    if (wasCached && m_cachedRootsJson.contains("fromCache")) {
        m_cachedRootsJson["fromCache"] = true;
    }
    return m_cachedRootsJson;
}

json LibraryTreeIndex::GetStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return {
        {"treeIndexValid", m_valid},
        {"rootsCached", m_valid ? m_roots.size() : 0},
        {"treeIndexedTracks", m_stats.indexedTracks},
        {"treeSkippedTracks", m_stats.skippedTracks},
        {"treeLastBuilt", m_stats.lastBuilt}
    };
}

void LibraryTreeIndex::EnsureBuilt() {
    if (m_valid) return;

    try {
        BuildIndex();
    }
    catch (const std::exception& e) {
        FB2K_console_print("[LibraryTreeIndex] Build index exception: ", e.what());
        m_valid = false;
        m_cachedRootsJson = {
            {"success", false},
            {"enabled", true},
            {"roots", json::array()},
            {"total", 0},
            {"indexedTracks", 0},
            {"skippedTracks", 0},
            {"fromCache", false},
            {"error", "Failed to build library root index"}
        };
    }
    catch (...) {
        FB2K_console_print("[LibraryTreeIndex] Build index unknown exception");
        m_valid = false;
        m_cachedRootsJson = {
            {"success", false},
            {"enabled", true},
            {"roots", json::array()},
            {"total", 0},
            {"indexedTracks", 0},
            {"skippedTracks", 0},
            {"fromCache", false},
            {"error", "Failed to build library root index"}
        };
    }
}

// ============================================
// GetBrowseTreeJson
// ============================================

json LibraryTreeIndex::DirectoryNodeToJson(const DirectoryNodeRecord& node) const {
    return {
        {"id", node.id},
        {"rootId", node.rootId},
        {"pathId", node.pathId},
        {"parentPathId", node.parentPathId},
        {"name", node.name},
        {"displayName", node.displayName},
        {"rawPath", node.rawPath},
        {"absolutePath", node.absolutePath},
        {"relativePath", node.relativePath},
        {"depth", node.depth},
        {"trackCount", node.trackCount},
        {"childDirectoryCount", node.childDirectoryCount},
        {"hasChildren", node.childDirectoryCount > 0}
    };
}

json LibraryTreeIndex::GetBrowseTreeJson(const std::string& rootId, const std::string& pathId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Ensure index is built
    bool wasValid = m_valid;
    EnsureBuilt();

    if (!m_valid) {
        return {{"success", false}, {"error", "Failed to build library tree index"}};
    }

    // Find root
    const LibraryRootRecord* foundRoot = nullptr;
    for (const auto& root : m_roots) {
        if (CaseInsensitiveEqual(root.id, rootId)) {
            foundRoot = &root;
            break;
        }
    }

    if (!foundRoot) {
        return {{"success", false}, {"error", "Unknown rootId"}};
    }

    // Find target directory node
    std::string nodeKey = foundRoot->id + "::" + pathId;
    auto nodeIt = m_directoryNodes.find(nodeKey);
    if (nodeIt == m_directoryNodes.end()) {
        return {{"success", false}, {"error", "Path not found"}};
    }

    const auto& targetNode = nodeIt->second;

    // Build child directory list
    json dirsArray = json::array();
    for (const auto& childPathId : targetNode.childPathIds) {
        std::string childKey = foundRoot->id + "::" + childPathId;
        auto childIt = m_directoryNodes.find(childKey);
        if (childIt != m_directoryNodes.end()) {
            dirsArray.push_back(DirectoryNodeToJson(childIt->second));
        }
    }

    // Sort by displayName (case-insensitive), then by absolutePath (case-insensitive)
    std::sort(dirsArray.begin(), dirsArray.end(),
        [](const json& a, const json& b) {
            std::string aName = a.value("displayName", "");
            std::string bName = b.value("displayName", "");
            if (!CaseInsensitiveEqual(aName, bName)) {
                return CaseInsensitiveLess(aName, bName);
            }
            std::string aPath = a.value("absolutePath", "");
            std::string bPath = b.value("absolutePath", "");
            return CaseInsensitiveLess(aPath, bPath);
        });

    // Build return structure
    json result = {
        {"success", true},
        {"root", {
            {"id", foundRoot->id},
            {"displayName", foundRoot->displayName},
            {"rawPath", foundRoot->rawPath},
            {"absolutePath", foundRoot->absolutePath},
            {"trackCount", foundRoot->trackCount}
        }},
        {"pathId", pathId},
        {"absolutePath", targetNode.absolutePath},
        {"directories", dirsArray},
        {"fromCache", wasValid}
    };

    return result;
}

// ============================================
// GetDirectoryFileHandles
// ============================================

void LibraryTreeIndex::GetDirectoryFileHandles(const std::string& rootId, const std::string& pathId,
                                                bool recursive, metadb_handle_list& outHandles,
                                                std::vector<size_t>& outGlobalIndices) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_valid) return;

    // Find root (case-insensitive)
    std::string resolvedRootId;
    for (const auto& root : m_roots) {
        if (CaseInsensitiveEqual(root.id, rootId)) {
            resolvedRootId = root.id;
            break;
        }
    }
    if (resolvedRootId.empty()) return;

    if (!recursive) {
        // Non-recursive: direct files only
        std::string dirKey = resolvedRootId + "::" + pathId;
        auto it = m_directFiles.find(dirKey);
        if (it == m_directFiles.end()) return;
        for (const auto& entry : it->second) {
            if (entry.globalIndex >= m_allItems.get_count()) continue;
            outHandles.add_item(m_allItems[entry.globalIndex]);
            outGlobalIndices.push_back(entry.globalIndex);
        }
        return;
    }

    // Recursive: collect files from current directory and all descendants
    std::vector<std::string> pending;
    pending.push_back(pathId);

    while (!pending.empty()) {
        std::string currentPathId = pending.back();
        pending.pop_back();

        std::string dirKey = resolvedRootId + "::" + currentPathId;

        // Collect direct files from current directory
        auto fileIt = m_directFiles.find(dirKey);
        if (fileIt != m_directFiles.end()) {
            for (const auto& entry : fileIt->second) {
                if (entry.globalIndex >= m_allItems.get_count()) continue;
                outHandles.add_item(m_allItems[entry.globalIndex]);
                outGlobalIndices.push_back(entry.globalIndex);
            }
        }

        // Add child directories to traversal list
        auto nodeIt = m_directoryNodes.find(dirKey);
        if (nodeIt == m_directoryNodes.end()) continue;
        for (const auto& childPathId : nodeIt->second.childPathIds) {
            pending.push_back(childPathId);
        }
    }
}

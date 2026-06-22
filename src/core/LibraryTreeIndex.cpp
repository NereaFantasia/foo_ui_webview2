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
#include "core/LibraryTreeIndex.h"
#include <algorithm>
#include <unordered_map>

// ============================================
// Internal helper functions
// ============================================

namespace {

/**
 * Split a path into prefix and segments.
 * For Windows drive paths (e.g. D:\Music\...), prefix is "D:\", rest are segments.
 * For UNC paths (e.g. \\server\share\...), prefix is "\\server\share\"
 *
 * @param path Absolute path
 * @param prefix [out] Path prefix (drive letter or UNC root)
 * @param segments [out] Path segments
 * @return Whether parsing succeeded
 */
bool SplitPathSegments(const std::string& path, std::string& prefix, std::vector<std::string>& segments) {
    if (path.empty()) return false;

    // Normalize all separators to backslash
    std::string normalized = path;
    for (auto& c : normalized) {
        if (c == '/') c = '\\';
    }

    size_t startPos = 0;

    // Handle UNC path
    if (normalized.size() >= 2 && normalized[0] == '\\' && normalized[1] == '\\') {
        // UNC: \\server\share\...
        size_t serverEnd = normalized.find('\\', 2);
        if (serverEnd == std::string::npos) return false;
        size_t shareEnd = normalized.find('\\', serverEnd + 1);
        if (shareEnd == std::string::npos) {
            // \\server\share (no trailing path)
            prefix = normalized;
            if (prefix.back() != '\\') prefix += '\\';
            return true;
        }
        prefix = normalized.substr(0, shareEnd + 1); // including trailing backslash
        startPos = shareEnd + 1;
    }
    // Handle drive letter path (e.g. C:\...)
    else if (normalized.size() >= 2 && std::isalpha(static_cast<unsigned char>(normalized[0])) && normalized[1] == ':') {
        if (normalized.size() >= 3 && normalized[2] == '\\') {
            prefix = normalized.substr(0, 3); // "C:\"
            startPos = 3;
        } else {
            // "C:" without backslash -- non-standard but handled
            prefix = normalized.substr(0, 2) + "\\";
            startPos = 2;
        }
    } else {
        // Unrecognized path format
        return false;
    }

    // Parse remaining part into segments
    segments.clear();
    std::string remaining = normalized.substr(startPos);
    size_t pos = 0;
    while (pos < remaining.size()) {
        size_t nextSep = remaining.find('\\', pos);
        if (nextSep == std::string::npos) {
            std::string seg = remaining.substr(pos);
            if (!seg.empty()) segments.push_back(seg);
            break;
        }
        std::string seg = remaining.substr(pos, nextSep - pos);
        if (!seg.empty()) segments.push_back(seg);
        pos = nextSep + 1;
    }

    return true;
}

/**
 * Split a relative path into segments (by \ or /)
 */
std::vector<std::string> SplitRelativeSegments(const std::string& relPath) {
    std::vector<std::string> segments;
    if (relPath.empty()) return segments;

    size_t pos = 0;
    while (pos < relPath.size()) {
        size_t nextSep = relPath.find_first_of("\\/", pos);
        if (nextSep == std::string::npos) {
            std::string seg = relPath.substr(pos);
            if (!seg.empty()) segments.push_back(seg);
            break;
        }
        std::string seg = relPath.substr(pos, nextSep - pos);
        if (!seg.empty()) segments.push_back(seg);
        pos = nextSep + 1;
    }

    return segments;
}

/**
 * Case-insensitive string equality comparison
 */
bool CaseInsensitiveEqual(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

/**
 * Case-insensitive string less-than (for sorting)
 */
bool CaseInsensitiveLess(const std::string& a, const std::string& b) {
    size_t minLen = std::min(a.size(), b.size());
    for (size_t i = 0; i < minLen; ++i) {
        char ca = std::tolower(static_cast<unsigned char>(a[i]));
        char cb = std::tolower(static_cast<unsigned char>(b[i]));
        if (ca < cb) return true;
        if (ca > cb) return false;
    }
    return a.size() < b.size();
}

/**
 * Normalize root path: unify backslash, keep prefix and UNC intact, no trailing backslash
 */
std::string NormalizeRootPath(const std::string& prefix, const std::vector<std::string>& rootSegments) {
    std::string result = prefix;
    for (size_t i = 0; i < rootSegments.size(); ++i) {
        result += rootSegments[i];
        if (i + 1 < rootSegments.size()) {
            result += "\\";
        }
    }
    // If rootSegments is empty, result is just prefix (e.g. "C:\")
    // Otherwise ensure proper trailing backslash: "C:\" not "C:"
    return result;
}

/**
 * Check whether a path is a local filesystem path (not a protocol URI)
 */
bool IsLocalPath(const std::string& nativePath) {
    if (nativePath.empty()) return false;

    // Check for protocol-based paths
    static const char* blockedPrefixes[] = {
        "http://", "https://", "cdda://", "file-relative://",
        "unpack://", "archive://", "file://",
    };
    for (const char* p : blockedPrefixes) {
        if (nativePath.size() >= strlen(p)) {
            std::string lower = nativePath.substr(0, strlen(p));
            std::transform(lower.begin(), lower.end(), lower.begin(),
                [](unsigned char c) { return std::tolower(c); });
            if (lower == p) return false;
        }
    }

    // Only allow drive letter paths and UNC paths
    if (nativePath.size() >= 2) {
        // Drive letter path: C:\...
        if (std::isalpha(static_cast<unsigned char>(nativePath[0])) && nativePath[1] == ':') {
            return true;
        }
        // UNC path: \\server\...
        if (nativePath[0] == '\\' && nativePath[1] == '\\') {
            return true;
        }
    }

    return false;
}

/**
 * Synthesize a native-style absolute path from an unpack:// URI.
 * Format: unpack://<handler>|<size>|<archive_path>|<entry_path>
 * Result: resolve(archive_path) + "\" + entry_path (normalized)
 * 
 * This allows archived tracks to appear in the tree under the
 * directory containing the archive file, with the archive name
 * and internal path as subdirectory segments.
 */
std::string SynthesizeUnpackAbsolutePath(const char* fbPath) {
    std::string path(fbPath);
    // Case-insensitive prefix check
    if (path.size() < 9) return "";
    std::string pfx = path.substr(0, 9);
    std::transform(pfx.begin(), pfx.end(), pfx.begin(),
        [](unsigned char c) { return std::tolower(c); });
    if (pfx != "unpack://") return "";

    // Parse: unpack://handler|size|archive_path|entry_path
    size_t pipe1 = path.find('|', 9);
    if (pipe1 == std::string::npos) return "";
    size_t pipe2 = path.find('|', pipe1 + 1);
    if (pipe2 == std::string::npos) return "";
    size_t pipe3 = path.find('|', pipe2 + 1);
    if (pipe3 == std::string::npos) return "";

    std::string archivePath = path.substr(pipe2 + 1, pipe3 - pipe2 - 1);
    std::string entryPath = path.substr(pipe3 + 1);

    // Resolve archive path to native
    pfc::string8 nativeBuf;
    filesystem::g_get_native_path(archivePath.c_str(), nativeBuf);
    std::string nativeArchive = nativeBuf.get_ptr();

    if (!IsLocalPath(nativeArchive)) return "";

    // Normalize entry path separators
    for (char& c : entryPath) {
        if (c == '/') c = '\\';
    }

    // Combine: archive_native_path\entry_path
    // e.g. E:\OST\void\album\bonus.7z\internal\file.flac
    return nativeArchive + "\\" + entryPath;
}

/**
 * Case-insensitive map comparator
 */
struct CaseInsensitiveHash {
    size_t operator()(const std::string& s) const {
        size_t h = 0;
        for (char c : s) {
            h = h * 31 + std::tolower(static_cast<unsigned char>(c));
        }
        return h;
    }
};

struct CaseInsensitiveKeyEqual {
    bool operator()(const std::string& a, const std::string& b) const {
        return CaseInsensitiveEqual(a, b);
    }
};

} // anonymous namespace

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

void LibraryTreeIndex::BuildIndex() {
    auto lib = library_manager::get();

    // Get all items from library
    m_allItems.remove_all();
    lib->get_all_items(m_allItems);

    // Use case-insensitive map to collect root directories
    std::unordered_map<std::string, LibraryRootRecord, CaseInsensitiveHash, CaseInsensitiveKeyEqual> rootMap;

    // Temporary storage for each track's rootId + relative segments (for directory building)
    struct TrackEntry {
        size_t index;                       // Global index in m_allItems
        std::string rootAbsPath;            // Root absolute path
        std::string absolutePath;           // File absolute path
        std::vector<std::string> relSegments; // Relative path segments from root (including filename)
    };
    std::vector<TrackEntry> trackEntries;
    trackEntries.reserve(m_allItems.get_count());

    // Deferred unpack:// items — processed after roots are discovered
    struct DeferredUnpackItem {
        size_t index;
        std::string absolutePath; // Synthesized native-style path
    };
    std::vector<DeferredUnpackItem> deferredUnpackItems;

    size_t indexedTracks = 0;
    size_t skippedTracks = 0;

    for (size_t i = 0; i < m_allItems.get_count(); ++i) {
        auto& item = m_allItems[i];
        if (!item.is_valid()) {
            ++skippedTracks;
            continue;
        }

        // Step 1: Get native absolute path
        pfc::string8 nativePath;
        filesystem::g_get_native_path(item->get_path(), nativePath);
        std::string absolutePath = nativePath.get_ptr();

        // Step 2: Local path eligibility check
        if (!IsLocalPath(absolutePath)) {
            // Fallback: resolve unpack:// archive host + entry path
            absolutePath = SynthesizeUnpackAbsolutePath(item->get_path());
            if (absolutePath.empty()) {
                ++skippedTracks;
                continue;
            }
            // unpack:// items: defer to second pass (root-prefix matching)
            // because get_relative_path() may not support protocol URIs
            deferredUnpackItems.push_back({ i, absolutePath });
            continue;
        }

        // Step 3: Get library-relative path
        pfc::string8 relPathBuf;
        if (!lib->get_relative_path(item, relPathBuf)) {
            ++skippedTracks;
            continue;
        }
        std::string relativePath = relPathBuf.get_ptr();

        if (relativePath.empty()) {
            ++skippedTracks;
            continue;
        }

        // Step 4: Tail-comparison to deduce root directory
        std::string absPrefix;
        std::vector<std::string> absSegments;
        if (!SplitPathSegments(absolutePath, absPrefix, absSegments)) {
            ++skippedTracks;
            continue;
        }

        std::vector<std::string> relSegments = SplitRelativeSegments(relativePath);
        if (relSegments.empty()) {
            ++skippedTracks;
            continue;
        }

        // Verify tail segment match
        if (relSegments.size() > absSegments.size()) {
            ++skippedTracks;
            continue;
        }

        bool tailMatch = true;
        size_t absOffset = absSegments.size() - relSegments.size();
        for (size_t j = 0; j < relSegments.size(); ++j) {
            if (!CaseInsensitiveEqual(absSegments[absOffset + j], relSegments[j])) {
                tailMatch = false;
                break;
            }
        }

        if (!tailMatch) {
            ++skippedTracks;
            continue;
        }

        // Step 5: Construct root directory path
        std::vector<std::string> rootSegments(absSegments.begin(), absSegments.begin() + absOffset);
        std::string rootAbsPath = NormalizeRootPath(absPrefix, rootSegments);

        // Step 6: Accumulate to rootMap
        auto& record = rootMap[rootAbsPath];
        if (record.absolutePath.empty()) {
            record.id = rootAbsPath;
            record.absolutePath = rootAbsPath;
            record.rawPath = rootAbsPath; // rawPath == absolutePath
        }
        record.trackCount++;
        indexedTracks++;

        // Record track directory traversal info
        trackEntries.push_back({ i, rootAbsPath, absolutePath, relSegments });
    }

    // Step 6.5: Process deferred unpack:// items by matching against discovered roots
    for (auto& deferred : deferredUnpackItems) {
        std::string absPrefix;
        std::vector<std::string> absSegments;
        if (!SplitPathSegments(deferred.absolutePath, absPrefix, absSegments) || absSegments.empty()) {
            ++skippedTracks;
            continue;
        }

        // Find the best matching root (longest prefix match)
        std::string bestRoot;
        size_t bestRootSegCount = 0;
        for (auto& [rootKey, rootRecord] : rootMap) {
            std::string rootPrefix;
            std::vector<std::string> rootSegs;
            if (!SplitPathSegments(rootKey, rootPrefix, rootSegs)) continue;

            // Must share the same drive/UNC prefix
            if (!CaseInsensitiveEqual(absPrefix, rootPrefix)) continue;
            // Root segments must be a strict prefix of absolute segments
            if (rootSegs.size() >= absSegments.size()) continue;

            bool prefixMatch = true;
            for (size_t j = 0; j < rootSegs.size(); ++j) {
                if (!CaseInsensitiveEqual(rootSegs[j], absSegments[j])) {
                    prefixMatch = false;
                    break;
                }
            }
            if (prefixMatch && rootSegs.size() > bestRootSegCount) {
                bestRoot = rootKey;
                bestRootSegCount = rootSegs.size();
            }
        }

        if (bestRoot.empty()) {
            ++skippedTracks;
            continue;
        }

        // Compute relative segments by stripping the root prefix segments
        std::vector<std::string> relSegments(absSegments.begin() + bestRootSegCount, absSegments.end());
        if (relSegments.empty()) {
            ++skippedTracks;
            continue;
        }

        // Register the track under the matched root
        auto& record = rootMap[bestRoot];
        record.trackCount++;
        indexedTracks++;
        trackEntries.push_back({ deferred.index, bestRoot, deferred.absolutePath, relSegments });
    }

    // Step 7: displayName deduplication and conflict resolution
    // First count how many times each last-segment name appears
    std::unordered_map<std::string, int, CaseInsensitiveHash, CaseInsensitiveKeyEqual> lastSegmentCount;
    for (auto& [key, record] : rootMap) {
        std::string prefix;
        std::vector<std::string> segs;
        if (SplitPathSegments(record.absolutePath, prefix, segs) && !segs.empty()) {
            lastSegmentCount[segs.back()]++;
        } else {
            // Cannot split or no segments -- use full path
            lastSegmentCount[record.absolutePath]++;
        }
    }

    // Assign displayName
    for (auto& [key, record] : rootMap) {
        std::string prefix;
        std::vector<std::string> segs;
        if (SplitPathSegments(record.absolutePath, prefix, segs) && !segs.empty()) {
            const std::string& lastName = segs.back();
            if (lastSegmentCount[lastName] > 1) {
                // Name conflict -- fall back to full path
                record.displayName = record.absolutePath;
            } else {
                record.displayName = lastName;
            }
        } else {
            // Default: use full path
            record.displayName = record.absolutePath;
        }
    }

    // Step 8: Convert to sorted vector
    m_roots.clear();
    m_roots.reserve(rootMap.size());
    for (auto& [key, record] : rootMap) {
        m_roots.push_back(std::move(record));
    }

    // Sort by displayName (case-insensitive), then by absolutePath (case-insensitive)
    std::sort(m_roots.begin(), m_roots.end(),
        [](const LibraryRootRecord& a, const LibraryRootRecord& b) {
            if (!CaseInsensitiveEqual(a.displayName, b.displayName)) {
                return CaseInsensitiveLess(a.displayName, b.displayName);
            }
            return CaseInsensitiveLess(a.absolutePath, b.absolutePath);
        });

    // ========== Build directory data structures ==========
    m_directoryNodes.clear();
    m_directFiles.clear();

    // Create root directory node for each root (pathId = "")
    for (const auto& root : m_roots) {
        std::string nodeKey = root.id + "::";
        auto& rootNode = m_directoryNodes[nodeKey];
        rootNode.id = root.id + "::";
        rootNode.rootId = root.id;
        rootNode.pathId = "";
        rootNode.parentPathId = "";
        rootNode.name = root.displayName;
        rootNode.displayName = root.displayName;
        rootNode.rawPath = root.absolutePath;
        rootNode.absolutePath = root.absolutePath;
        rootNode.relativePath = "";
        rootNode.depth = 0;
        rootNode.trackCount = 0; // Will be aggregated later
    }

    // Iterate all tracks to build directory nodes and record direct files
    for (const auto& entry : trackEntries) {
        const std::string& rootId = entry.rootAbsPath;

        // relSegments: last element is filename, preceding elements are directories
        if (entry.relSegments.empty()) continue;

        // dirSegments = relSegments[0..n-2], filename = relSegments[n-1]
        size_t dirSegCount = entry.relSegments.size() - 1;

        // Create/reuse directory nodes layer by layer
        std::string currentPathId;
        std::string currentAbsPath = rootId;

        for (size_t d = 0; d < dirSegCount; ++d) {
            const std::string& seg = entry.relSegments[d];
            std::string parentPathId = currentPathId;

            if (currentPathId.empty()) {
                currentPathId = seg;
            } else {
                currentPathId += "/" + seg;
            }
            currentAbsPath += "\\" + seg;

            std::string nodeKey = rootId + "::" + currentPathId;

            auto it = m_directoryNodes.find(nodeKey);
            if (it == m_directoryNodes.end()) {
                // Create new directory node
                DirectoryNodeRecord node;
                node.id = nodeKey;
                node.rootId = rootId;
                node.pathId = currentPathId;
                node.parentPathId = parentPathId;
                node.name = seg;
                node.displayName = seg;
                node.rawPath = currentAbsPath;
                node.absolutePath = currentAbsPath;
                node.relativePath = currentPathId;
                node.depth = static_cast<int>(d + 1);
                node.trackCount = 0;
                m_directoryNodes[nodeKey] = std::move(node);

                // Register as child of parent directory
                std::string parentKey = rootId + "::" + parentPathId;
                auto parentIt = m_directoryNodes.find(parentKey);
                if (parentIt != m_directoryNodes.end()) {
                    auto& children = parentIt->second.childPathIds;
                    if (std::find(children.begin(), children.end(), currentPathId) == children.end()) {
                        children.push_back(currentPathId);
                        parentIt->second.childDirectoryCount++;
                    }
                }
            }
        }

        // Increment trackCount for each ancestor
        std::string ancestorPathId;
        for (size_t d = 0; d <= dirSegCount; ++d) {
            std::string ancestorKey = rootId + "::" + ancestorPathId;
            auto it = m_directoryNodes.find(ancestorKey);
            if (it != m_directoryNodes.end()) {
                it->second.trackCount++;
            }
            if (d < dirSegCount) {
                ancestorPathId = ancestorPathId.empty()
                    ? entry.relSegments[d]
                    : (ancestorPathId + "/" + entry.relSegments[d]);
            }
        }

        // Record direct file (leaf directory = innermost directory)
        std::string leafDirPathId;
        for (size_t d = 0; d < dirSegCount; ++d) {
            if (d == 0) {
                leafDirPathId = entry.relSegments[d];
            } else {
                leafDirPathId += "/" + entry.relSegments[d];
            }
        }
        std::string leafDirKey = rootId + "::" + leafDirPathId;
        m_directFiles[leafDirKey].push_back({ entry.index, entry.absolutePath });

        // Update direct file count
        auto nodeIt = m_directoryNodes.find(leafDirKey);
        if (nodeIt != m_directoryNodes.end()) {
            nodeIt->second.directTrackCount++;
        }
    }

    // Update statistics
    m_stats.indexedTracks = indexedTracks;
    m_stats.skippedTracks = skippedTracks;
    m_stats.lastBuilt = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    // Build cached JSON
    json rootsArray = json::array();
    for (const auto& root : m_roots) {
        rootsArray.push_back({
            {"id", root.id},
            {"displayName", root.displayName},
            {"rawPath", root.rawPath},
            {"absolutePath", root.absolutePath},
            {"trackCount", root.trackCount}
        });
    }

    m_cachedRootsJson = {
        {"success", true},
        {"enabled", true},
        {"roots", rootsArray},
        {"total", m_roots.size()},
        {"indexedTracks", indexedTracks},
        {"skippedTracks", skippedTracks},
        {"fromCache", false}
    };

    m_valid = true;

    FB2K_console_print("[LibraryTreeIndex] Index built: ",
        m_roots.size(), " roots, ",
        m_directoryNodes.size(), " directory nodes, ",
        indexedTracks, " tracks indexed, ",
        skippedTracks, " tracks skipped, ",
        deferredUnpackItems.size(), " unpack items deferred");
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

json LibraryTreeIndex::GetBrowseTreeJson(const std::string& rootId, const std::string& pathId,
                                          bool includeFiles, bool /*recursiveFiles*/) {
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

    // Build file entries if requested
    json filesArray = json::array();
    if (includeFiles) {
        std::string fileKey = foundRoot->id + "::" + pathId;
        auto fileIt = m_directFiles.find(fileKey);
        if (fileIt != m_directFiles.end()) {
            for (const auto& fe : fileIt->second) {
                if (fe.globalIndex >= m_allItems.get_count()) continue;
                auto& item = m_allItems[fe.globalIndex];
                if (!item.is_valid()) continue;

                // Extract filename from path
                std::string filename;
                size_t lastSep = fe.path.find_last_of("\\/");
                filename = (lastSep != std::string::npos) ? fe.path.substr(lastSep + 1) : fe.path;

                // Get metadata
                std::string title;
                double duration = item->get_length();
                uint32_t subsong = item->get_subsong_index();
                metadb_info_container::ptr infoContainer = item->get_info_ref();
                if (infoContainer.is_valid()) {
                    const file_info& info = infoContainer->info();
                    const char* t = info.meta_get("title", 0);
                    if (t) title = t;
                }
                if (title.empty()) {
                    // Strip extension from filename as fallback title
                    size_t dot = filename.find_last_of('.');
                    title = (dot != std::string::npos) ? filename.substr(0, dot) : filename;
                }

                filesArray.push_back({
                    {"globalIndex", fe.globalIndex},
                    {"absolutePath", fe.path},
                    {"filename", filename},
                    {"title", title},
                    {"duration", duration},
                    {"subsong", subsong}
                });
            }
        }
    }

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
        {"files", filesArray},
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

json LibraryTreeIndex::CollectFiles(const std::string& /*rootId*/, const std::string& /*pathId*/,
                                     bool /*recursive*/) const {
    // This method is an internal placeholder, not used externally; file JSON is built at API layer
    return json::array();
}

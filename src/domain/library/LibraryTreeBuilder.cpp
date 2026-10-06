/**
 * LibraryTreeBuilder.cpp - 库树索引的全量构建
 *
 * LibraryTreeIndex::BuildIndex：扫描媒体库，按路径尾比对推断媒体根，
 * 第二遍把 unpack:// 条目挂到已发现的根下，再建目录树与各目录的直属文件表。
 */

#include "pch.h"
#include "domain/library/LibraryTreeIndex.h"
#include "domain/library/LibraryPathAlgebra.h"
#include <algorithm>
#include <unordered_map>

using namespace library_path;

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

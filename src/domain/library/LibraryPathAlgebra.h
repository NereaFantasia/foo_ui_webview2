#pragma once
/**
 * LibraryPathAlgebra - 库树索引的路径代数
 *
 * 路径拆段、大小写不敏感的比较与哈希、根路径拼接、本地路径判定与 unpack:// 路径合成。
 * 由 LibraryTreeIndex.cpp 与 LibraryTreeBuilder.cpp 共用。
 */

#include <cctype>
#include <cstddef>
#include <string>
#include <vector>

// ============================================
// Internal helper functions
// ============================================

namespace library_path {

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
bool SplitPathSegments(const std::string& path, std::string& prefix, std::vector<std::string>& segments);

/**
 * Split a relative path into segments (by \ or /)
 */
std::vector<std::string> SplitRelativeSegments(const std::string& relPath);

/**
 * Case-insensitive string equality comparison
 */
bool CaseInsensitiveEqual(const std::string& a, const std::string& b);

/**
 * Case-insensitive string less-than (for sorting)
 */
bool CaseInsensitiveLess(const std::string& a, const std::string& b);

/**
 * Normalize root path: unify backslash, keep prefix and UNC intact, no trailing backslash
 */
std::string NormalizeRootPath(const std::string& prefix, const std::vector<std::string>& rootSegments);

/**
 * Check whether a path is a local filesystem path (not a protocol URI)
 */
bool IsLocalPath(const std::string& nativePath);

/**
 * Synthesize a native-style absolute path from an unpack:// URI.
 * Format: unpack://<handler>|<size>|<archive_path>|<entry_path>
 * Result: resolve(archive_path) + "\" + entry_path (normalized)
 * 
 * This allows archived tracks to appear in the tree under the
 * directory containing the archive file, with the archive name
 * and internal path as subdirectory segments.
 */
std::string SynthesizeUnpackAbsolutePath(const char* fbPath);

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

}  // namespace library_path

/**
 * LibraryPathAlgebra.cpp - 库树索引路径代数的实现
 *
 * 各函数的说明在 LibraryPathAlgebra.h 的声明处。
 */

#include "pch.h"
#include "domain/library/LibraryPathAlgebra.h"
#include <algorithm>
#include <cstring>

namespace library_path {

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

}  // namespace library_path

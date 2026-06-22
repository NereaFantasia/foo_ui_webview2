#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace artwork_cache {

inline std::string NormalizePathForCache(std::string path) {
    std::replace(path.begin(), path.end(), '/', '\\');
    for (char& ch : path) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return path;
}

inline std::string NormalizeTypeForCache(std::string artworkType) {
    if (artworkType.empty()) {
        return "front";
    }
    for (char& ch : artworkType) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return artworkType;
}

inline std::string BuildScaledArtworkCacheKey(
    const std::string& requestPath,
    const std::string& artworkType,
    int maxSize,
    std::vector<std::string> sourcePaths)
{
    if (maxSize <= 0) {
        return {};
    }

    for (std::string& sourcePath : sourcePaths) {
        sourcePath = NormalizePathForCache(std::move(sourcePath));
    }
    sourcePaths.erase(
        std::remove_if(sourcePaths.begin(), sourcePaths.end(),
                       [](const std::string& path) { return path.empty(); }),
        sourcePaths.end()
    );
    std::sort(sourcePaths.begin(), sourcePaths.end());
    sourcePaths.erase(std::unique(sourcePaths.begin(), sourcePaths.end()), sourcePaths.end());

    std::string identity;
    if (!sourcePaths.empty()) {
        identity = "source:";
        for (size_t i = 0; i < sourcePaths.size(); ++i) {
            if (i != 0) {
                identity.push_back('\n');
            }
            identity += sourcePaths[i];
        }
    } else {
        identity = "track:" + NormalizePathForCache(requestPath);
    }

    identity += "|";
    identity += NormalizeTypeForCache(artworkType);
    identity += "|";
    identity += std::to_string(maxSize);
    return identity;
}

} // namespace artwork_cache

inline std::string BuildScaledArtworkCacheKey(
    const std::string& requestPath,
    const std::string& artworkType,
    int maxSize,
    std::vector<std::string> sourcePaths)
{
    return artwork_cache::BuildScaledArtworkCacheKey(
        requestPath, artworkType, maxSize, std::move(sourcePaths)
    );
}

#pragma once
#include "utils/MediaContainerPath.h"
#include "utils/PathTraversalSegments.h"
#include "utils/SubsongPath.h"
#include <string>
#include <string_view>

namespace media_read_path {

struct Result {
    std::string path;
    const char* error = nullptr;
};

inline bool HasTraversal(std::string_view path) {
    size_t start = 0;
    for (size_t end = 0; end <= path.size(); ++end) {
        if (end == path.size() || path[end] == '\\' || path[end] == '/') {
            if (path_traversal::IsTraversalSegment(path.substr(start, end - start))) return true;
            start = end + 1;
        }
    }
    return false;
}

inline bool IsNativeAbsolute(std::string_view path) {
    const bool drive = path.size() >= 3 &&
        ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':' && (path[2] == '\\' || path[2] == '/');
    return drive || path.starts_with(R"(\\)");
}

// 只准备授权用的物理位置；原始曲目身份继续交给 handler。
// 非本地协议由调用方分流。原生解析器只接收便携路径，不得自行推测其基准目录。
template <class NativeResolver>
Result Resolve(const std::string& input, NativeResolver&& resolveNative) {
    if (input.empty()) return {{}, "Empty path"};
    if (input.find('\0') != std::string::npos) return {{}, "Invalid protocol path"};

    const auto track = fb2k_paths::SplitSubsongPath(input);
    const std::string suffix = input.substr(track.path.size());
    if (HasTraversal(suffix)) return {{}, "Path traversal detected"};

    std::string physical = media_container_path::Unwrap(track.path);
    if (physical.empty()) return {{}, "Invalid protocol path"};
    if (!media_container_path::StartsWith(physical, "file-relative://")) {
        if (const bool archive = media_container_path::StartsWith(track.path, "unpack://") ||
                                 media_container_path::StartsWith(track.path, "archive://");
            archive && physical.find("://") != std::string::npos && !IsNativeAbsolute(physical)) {
            return {{}, "Unsupported archive container path"};
        }
        return {physical + suffix, nullptr};
    }

    // 协议名规范化，文件名的原始字节保持不变。
    const std::string relative = "file-relative://" + physical.substr(16);
    std::string native;
    try {
        if (!resolveNative(relative, native) || !IsNativeAbsolute(native)) {
            return {{}, "Cannot resolve portable media path"};
        }
    } catch (...) {
        // 原生解析器是外部扩展边界；任何解析异常都必须转成授权失败。
        return {{}, "Cannot resolve portable media path"};
    }
    // 保留轨号文本，让后续安全流水线仍能检查它；实际取数不会使用这个物理路径。
    return {native + suffix, nullptr};
}

} // namespace media_read_path

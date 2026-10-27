#pragma once
#include <charconv>
#include <string>
#include <string_view>

namespace media_container_path {

inline bool StartsWith(std::string_view path, std::string_view prefix) {
    if (path.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        char c = path[i];
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        if (c != prefix[i]) return false;
    }
    return true;
}

// unpack 的长度是 UTF-8 字节数，必须在转成 Windows 宽字符串之前拆容器。
// 嵌套压缩包逐层拆到外层文件；只返回物理容器，内部条目名不参与本地目录校验。
// 返回空串表示协议损坏。file 路径保留子曲目后缀，交给调用方先检查遍历再剥离。
inline std::string Unwrap(std::string_view path) {
    for (;;) {
        if (StartsWith(path, "file://")) return std::string(path.substr(7));
        if (StartsWith(path, "archive://")) {
            path.remove_prefix(10);
            path = path.substr(0, path.find('|'));
            continue;
        }
        if (!StartsWith(path, "unpack://")) return std::string(path);
        const size_t typeEnd = path.find('|', 9);
        if (typeEnd == std::string_view::npos || typeEnd == 9) return {};
        const size_t sizeEnd = path.find('|', typeEnd + 1);
        if (sizeEnd == std::string_view::npos) return {};
        size_t length = 0;
        const char* sizeBegin = path.data() + typeEnd + 1;
        const char* sizeLimit = path.data() + sizeEnd;
        const auto parsed = std::from_chars(sizeBegin, sizeLimit, length);
        const size_t start = sizeEnd + 1;
        if (parsed.ec != std::errc{} || parsed.ptr != sizeLimit || length == 0 ||
            length >= path.size() - start || path[start + length] != '|') return {};
        path = path.substr(start, length);
    }
}

} // namespace media_container_path

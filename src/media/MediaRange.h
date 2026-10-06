#pragma once

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <string_view>
#include <system_error>

namespace media {

inline constexpr std::uint64_t kMediaSegmentBytes = 2u * 1024u * 1024u;

struct ByteRange {
    int status = 416;
    std::uint64_t offset = 0;
    std::uint64_t length = 0;
};

inline bool ParseByteCount(std::string_view text, std::uint64_t& value) {
    if (text.empty()) return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

// 206 最多返回所请求范围的前 2 MiB，客户端可继续请求下一段。
// 无 Range 的成功响应必须是完整文件；大文件回 416，避免 fetch 把截断正文当作成功。
inline ByteRange SelectRange(std::string_view header, std::uint64_t size) {
    if (header.empty()) return size <= kMediaSegmentBytes ? ByteRange{200, 0, size} : ByteRange{};
    if (!header.starts_with("bytes=") || size == 0) return {};
    header.remove_prefix(6);
    const auto dash = header.find('-');
    if (dash == std::string_view::npos) return {};
    const auto first = header.substr(0, dash);
    const auto last = header.substr(dash + 1);
    std::uint64_t offset = 0;
    std::uint64_t end = size - 1;
    if (first.empty()) {
        std::uint64_t suffix = 0;
        if (!ParseByteCount(last, suffix) || suffix == 0) return {};
        offset = size - std::min(size, suffix);
    } else {
        if (!ParseByteCount(first, offset) || offset >= size) return {};
        if (!last.empty()) {
            if (!ParseByteCount(last, end) || end < offset) return {};
            end = std::min(end, size - 1);
        }
    }
    return {206, offset, std::min(end - offset + 1, kMediaSegmentBytes)};
}

}  // namespace media

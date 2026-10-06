#pragma once

#include "media/ContainerInfo.h"

#include <bit>
#include <cmath>
#include <limits>
#include <span>
#include <utility>

namespace media::detail {

using Bytes = std::vector<std::uint8_t>;
using View = std::span<const std::uint8_t>;

inline void Require(bool condition, const char* message) {
    if (!condition) throw ContainerError(message);
}

inline std::uint64_t Unsigned(View data, std::size_t offset, std::size_t length) {
    Require(length <= 8 && offset <= data.size() && length <= data.size() - offset, "Truncated metadata field");
    std::uint64_t result = 0;
    for (std::size_t i = 0; i < length; ++i) result = (result << 8) | data[offset + i];
    return result;
}

inline std::string String(View data) {
    std::string result(data.begin(), data.end());
    while (!result.empty() && result.back() == '\0') result.pop_back();
    return result;
}

inline double Float(View data) {
    Require(data.size() == 4 || data.size() == 8, "Invalid floating point metadata width");
    const auto value = Unsigned(data, 0, data.size());
    const double result = data.size() == 4 ? static_cast<double>(std::bit_cast<float>(static_cast<std::uint32_t>(value)))
                                         : std::bit_cast<double>(value);
    Require(std::isfinite(result), "Non-finite metadata value");
    return result;
}

inline std::int64_t SignedSize(std::uint64_t value) {
    Require(value <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()), "Metadata integer is out of range");
    return static_cast<std::int64_t>(value);
}

inline bool IsUtf8(std::string_view text) {
    for (std::size_t i = 0; i < text.size();) {
        const auto lead = static_cast<std::uint8_t>(text[i]);
        std::size_t length = 1;
        std::uint32_t minimum = 0;
        std::uint32_t code = lead;
        if (lead >= 0xc2 && lead <= 0xdf) { length = 2; minimum = 0x80; code = lead & 0x1f; }
        else if (lead >= 0xe0 && lead <= 0xef) { length = 3; minimum = 0x800; code = lead & 0x0f; }
        else if (lead >= 0xf0 && lead <= 0xf4) { length = 4; minimum = 0x10000; code = lead & 0x07; }
        else if (lead >= 0x80) return false;
        if (length > text.size() - i) return false;
        for (std::size_t k = 1; k < length; ++k) {
            const auto next = static_cast<std::uint8_t>(text[i + k]);
            if ((next & 0xc0) != 0x80) return false;
            code = (code << 6) | (next & 0x3f);
        }
        if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return false;
        i += length;
    }
    return true;
}

// 容器里的文本不保证是 UTF-8；回包序列化遇到非法字节会让整个调用失败，可选字段遇到就省略。
inline std::optional<std::string> OptionalText(std::string text) {
    if (text.empty() || !IsUtf8(text)) return std::nullopt;
    return text;
}

// 未配对的代理项按非法文本处理，由调用方省略该字段。
inline std::string Utf16ToUtf8(View data, bool bigEndian) {
    std::string out;
    for (std::size_t i = 0; i + 1 < data.size(); i += 2) {
        auto unit = [&data, bigEndian](std::size_t at) {
            return static_cast<std::uint32_t>(bigEndian ? (data[at] << 8) | data[at + 1] : (data[at + 1] << 8) | data[at]);
        };
        std::uint32_t code = unit(i);
        if (code >= 0xd800 && code <= 0xdbff && i + 3 < data.size() && unit(i + 2) >= 0xdc00 && unit(i + 2) <= 0xdfff) {
            code = 0x10000 + ((code - 0xd800) << 10) + (unit(i + 2) - 0xdc00);
            i += 2;
        } else if (code >= 0xd800 && code <= 0xdfff) {
            return {};
        }
        if (code < 0x80) out.push_back(static_cast<char>(code));
        else if (code < 0x800) { out.push_back(static_cast<char>(0xc0 | (code >> 6))); out.push_back(static_cast<char>(0x80 | (code & 0x3f))); }
        else if (code < 0x10000) {
            out.push_back(static_cast<char>(0xe0 | (code >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
        } else {
            out.push_back(static_cast<char>(0xf0 | (code >> 18)));
            out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3f)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
        }
    }
    return out;
}

using Chapters = std::vector<api::media::ContainerChapter>;

// 没声明终点的章节取同一套版本里下一章的起点，最后一章取容器时长；只接受晚于起点的值。
inline void FillChapterEnds(Chapters& chapters, std::optional<double> duration) {
    for (std::size_t i = 0; i < chapters.size(); ++i) {
        auto& chapter = chapters[i];
        if (chapter.end.has_value()) continue;
        if (i + 1 < chapters.size() && chapters[i + 1].edition == chapter.edition) {
            if (chapters[i + 1].start > chapter.start) chapter.end = chapters[i + 1].start;
        } else if (duration.has_value() && *duration > chapter.start) chapter.end = duration;
    }
}

// 章节结构损坏或用尽预算只省略章节，轨道照常返回；文件读取错误不是结构问题，照常抛出。
template<class Parse>
std::optional<Chapters> TryChapters(Parse parse) {
    try {
        return parse();
    } catch (const ContainerError&) {
        return std::nullopt;
    }
}

// 两种容器共用同一资源预算。跳过媒体正文仍计元素数，防止小元素或深层嵌套耗尽 CPU。
class Reader {
public:
    static constexpr std::size_t kMaxRead = 16 * 1024 * 1024;
    static constexpr std::size_t kReadBudget = 32 * 1024 * 1024;
    static constexpr std::size_t kMetadataLimit = 1024 * 1024;
    static constexpr std::size_t kEntryLimit = 1024;

    Reader(std::uint64_t size, ContainerRead read) : size_(size), read_(std::move(read)) {}
    std::uint64_t Size() const { return size_; }

    Bytes Read(std::uint64_t offset, std::size_t count) {
        Require(offset <= size_ && count <= size_ - offset, "Metadata read escapes file bounds");
        Require(count <= kMaxRead && count <= kReadBudget - bytesRead_, "Container read budget exceeded");
        // 重复读取也计入预算；预算限制解析工作的总读取量，不是文件中互不重叠的区间总长。
        bytesRead_ += count;
        if (count == 0) return {};
        auto bytes = read_(offset, count);
        Require(bytes.size() == count, "Short container read");
        return bytes;
    }

    Bytes Metadata(std::uint64_t offset, std::uint64_t count) {
        Require(count <= kMetadataLimit, "Container metadata field exceeds size limit");
        return Read(offset, static_cast<std::size_t>(count));
    }

    void Element(unsigned depth) {
        Require(depth <= 32, "Container nesting limit exceeded");
        Require(++elements_ <= 100000, "Container element limit exceeded");
    }

private:
    std::uint64_t size_;
    ContainerRead read_;
    std::size_t bytesRead_ = 0;
    std::size_t elements_ = 0;
};

api::media::GetContainerInfoResult ReadMp4(Reader& reader);
api::media::GetContainerInfoResult ReadMatroska(Reader& reader);

} // namespace media::detail

#include "pch.h"
#if __has_include("compat/fb2k_types.h")
#include "compat/fb2k_types.h"
#endif
#include "media/MediaCodecs.h"

#include <algorithm>
#include <map>
#include <set>

namespace media::detail {
namespace {

struct Element {
    std::uint32_t id;
    std::uint64_t payload;
    std::uint64_t end;
    bool unknown;
    std::uint64_t Size() const { return end - payload; }
};

struct Vint {
    std::uint64_t value;
    unsigned length;
    bool unknown;
};

Vint ReadVint(Reader& reader, std::uint64_t offset, std::uint64_t end, bool identifier) {
    Require(offset < end, "Truncated EBML integer");
    const auto first = reader.Read(offset, 1)[0];
    unsigned length = 1;
    unsigned marker = 128;
    while (marker && !(first & marker)) { marker >>= 1; ++length; }
    Require(marker && length <= (identifier ? 4u : 8u), "Invalid EBML integer width");
    Require(length <= end - offset, "Truncated EBML integer bytes");
    // ID 的长度标记属于标识本身；长度值须去掉标记，其有效位全 1 表示未知长度。
    auto value = static_cast<std::uint64_t>(identifier ? first : first & (marker - 1));
    if (length > 1) value = (value << ((length - 1) * 8)) | Unsigned(reader.Read(offset + 1, length - 1), 0, length - 1);
    const auto allOnes = (std::uint64_t{1} << (length * 7)) - 1;
    return {value, length, !identifier && value == allOnes};
}

Element NextElement(Reader& reader, std::uint64_t offset, std::uint64_t end, unsigned depth) {
    reader.Element(depth);
    const auto id = ReadVint(reader, offset, end, true);
    const auto size = ReadVint(reader, offset + id.length, end, false);
    const auto payload = offset + id.length + size.length;
    const auto identifier = static_cast<std::uint32_t>(id.value);
    Require(!size.unknown || identifier == 0x18538067 || identifier == 0x1f43b675, "Unknown EBML size is not allowed for this element");
    Require(size.unknown || size.value <= end - payload, "EBML element escapes parent");
    return {identifier, payload, size.unknown ? end : payload + size.value, size.unknown};
}

template<class Callback>
void Children(Reader& reader, const Element& parent, unsigned depth, Callback callback) {
    auto offset = parent.payload;
    while (offset < parent.end) {
        const auto element = NextElement(reader, offset, parent.end, depth);
        Require(!element.unknown, "Unexpected unknown-size EBML child");
        callback(element);
        offset = element.end;
    }
}

std::uint64_t UInt(Reader& reader, const Element& element, std::uint64_t defaultValue = 0) {
    if (element.Size() == 0) return defaultValue;
    Require(element.Size() <= 8, "Oversized EBML unsigned integer");
    const auto data = reader.Read(element.payload, static_cast<std::size_t>(element.Size()));
    return Unsigned(data, 0, data.size());
}

double Real(Reader& reader, const Element& element, double defaultValue = 0) {
    if (element.Size() == 0) return defaultValue;
    Require(element.Size() == 4 || element.Size() == 8, "Invalid EBML floating point size");
    return Float(reader.Read(element.payload, static_cast<std::size_t>(element.Size())));
}

std::string Text(Reader& reader, const Element& element, const char* defaultValue = "") {
    if (element.Size() == 0) return defaultValue;
    const auto data = reader.Metadata(element.payload, element.Size());
    // EBML 文本在首个 NUL 终止；此规则不适用于 MP4 的 FourCC 字节标识。
    const auto end = std::ranges::find(data, 0);
    return std::string(data.begin(), end);
}

bool Flag(Reader& reader, const Element& element, bool defaultValue = false) {
    const auto value = UInt(reader, element, defaultValue ? 1 : 0);
    Require(value <= 1, "Invalid Matroska boolean");
    return value != 0;
}

void Color(Reader& reader, const Element& parent, api::media::ContainerTrack& track, unsigned depth) {
    Children(reader, parent, depth, [&reader, &track](const Element& element) {
        if (element.id == 0x55b1) track.colorMatrix = SignedSize(UInt(reader, element, 2));
        else if (element.id == 0x55b2) {
            const auto bits = UInt(reader, element);
            if (bits) track.bitDepth = SignedSize(bits);
        } else if (element.id == 0x55ba) track.colorTransfer = SignedSize(UInt(reader, element, 2));
        else if (element.id == 0x55bb) track.colorPrimaries = SignedSize(UInt(reader, element, 2));
        else if (element.id == 0x55b9) {
            const auto range = UInt(reader, element);
            if (range == 1 || range == 2) track.fullRange = range == 2;
        }
    });
}

void Video(Reader& reader, const Element& parent, api::media::ContainerTrack& track, unsigned depth) {
    std::optional<std::uint64_t> displayWidth, displayHeight;
    std::uint64_t unit = 0, left = 0, right = 0, top = 0, bottom = 0;
    Children(reader, parent, depth, [&reader, &track, depth, &displayWidth, &displayHeight, &unit, &left, &right, &top, &bottom](const Element& element) {
        switch (element.id) {
        case 0xb0: track.width = SignedSize(UInt(reader, element)); break;
        case 0xba: track.height = SignedSize(UInt(reader, element)); break;
        case 0x54b0: if (element.Size()) displayWidth = UInt(reader, element); break;
        case 0x54ba: if (element.Size()) displayHeight = UInt(reader, element); break;
        case 0x54b2: unit = UInt(reader, element); break;
        case 0x54cc: left = UInt(reader, element); break;
        case 0x54dd: right = UInt(reader, element); break;
        case 0x54bb: top = UInt(reader, element); break;
        case 0x54aa: bottom = UInt(reader, element); break;
        case 0x55b0: Color(reader, element, track, depth + 1); break;
        default: break;
        }
    });
    Require(track.width.has_value() && *track.width > 0 && track.height.has_value() && *track.height > 0, "Missing Matroska video dimensions");
    const auto width = static_cast<std::uint64_t>(*track.width), height = static_cast<std::uint64_t>(*track.height);
    Require(left < width && right < width - left && top < height && bottom < height - top, "Invalid Matroska pixel crop");
    if (unit == 0) {
        if (!displayWidth.has_value()) displayWidth = width - left - right;
        if (!displayHeight.has_value()) displayHeight = height - top - bottom;
    }
    if (displayWidth.has_value() && displayHeight.has_value()) {
        Require(*displayWidth && *displayHeight, "Invalid Matroska display dimensions");
        if (unit <= 3) track.displayAspectRatio = static_cast<double>(*displayWidth) / static_cast<double>(*displayHeight);
    }
}

void Audio(Reader& reader, const Element& parent, api::media::ContainerTrack& track, unsigned depth) {
    // 8000 Hz 与单声道是 Matroska 缺省值；不同于 MP4 压缩音频头中的占位值。
    track.sampleRate = 8000;
    track.channels = 1;
    std::optional<double> outputFrequency;
    Children(reader, parent, depth, [&reader, &track, &outputFrequency](const Element& element) {
        if (element.id == 0xb5) track.sampleRate = Real(reader, element, 8000);
        else if (element.id == 0x78b5) { if (element.Size()) outputFrequency = Real(reader, element); }
        else if (element.id == 0x9f) track.channels = SignedSize(UInt(reader, element, 1));
        else if (element.id == 0x6264) track.bitDepth = SignedSize(UInt(reader, element));
    });
    if (outputFrequency.has_value()) track.sampleRate = outputFrequency;
    Require(*track.sampleRate > 0 && *track.channels > 0 && (!track.bitDepth.has_value() || *track.bitDepth > 0), "Invalid Matroska audio format");
}

std::string TrackType(std::uint64_t type) {
    Require(type != 0, "Matroska track type is zero");
    if (type == 1) return "video";
    if (type == 2) return "audio";
    if (type == 0x11) return "subtitle";
    return "other";
}

struct TrackMetadata {
    api::media::ContainerTrack track;
    Bytes privateData;
};

TrackMetadata Track(Reader& reader, const Element& parent, unsigned depth) {
    TrackMetadata metadata;
    auto& track = metadata.track;
    // 缺省标记与语言来自 Matroska 定义；LanguageIETF 存在时优先于旧语言字段。
    track.default_ = true; track.forced = false;
    track.language = "eng";
    std::optional<std::string> languageIetf;
    std::optional<double> frameRate;
    auto& privateData = metadata.privateData;
    Children(reader, parent, depth, [&reader, &track, &privateData, &languageIetf, &frameRate, depth](const Element& element) {
        switch (element.id) {
        case 0xd7: {
            const auto number = UInt(reader, element);
            Require(number > 0, "Matroska track number is zero");
            track.id = std::to_string(number); break;
        }
        case 0x83: track.type = TrackType(UInt(reader, element)); break;
        case 0x86: track.codec = Text(reader, element); break;
        case 0x63a2: privateData = reader.Metadata(element.payload, element.Size()); break;
        case 0x536e: track.name = OptionalText(Text(reader, element)); break;
        case 0x22b59c: track.language = OptionalText(Text(reader, element, "eng")); break;
        case 0x22b59d: languageIetf = OptionalText(Text(reader, element)); break;
        case 0x88: track.default_ = Flag(reader, element, true); break;
        case 0x55aa: track.forced = Flag(reader, element); break;
        case 0x23e383: {
            const auto duration = UInt(reader, element);
            Require(duration != 0, "Matroska default duration is zero");
            frameRate = 1000000000.0 / static_cast<double>(duration); break;
        }
        case 0xe0: Video(reader, element, track, depth + 1); break;
        case 0xe1: Audio(reader, element, track, depth + 1); break;
        default: break;
        }
    });
    Require(!track.id.empty() && !track.type.empty() && !track.codec.empty(), "Matroska track metadata is incomplete");
    if (languageIetf) track.language = std::move(languageIetf);
    if (track.type == "video") track.frameRate = frameRate;
    ReadMatroskaCodec(track, privateData);
    return metadata;
}

bool SegmentChild(std::uint32_t id) {
    return id == 0x114d9b74 || id == 0x1549a966 || id == 0x1654ae6b || id == 0x1f43b675
        || id == 0x1c53bb6b || id == 0x1941a469 || id == 0x1043a770 || id == 0x1254c367 || id == 0x18538067;
}

// 未知长度的 Cluster 在下一个 Segment 层元素前结束。按元素边界跳过，不能搜索媒体正文中的字节特征。
std::uint64_t SkipUnknownCluster(Reader& reader, const Element& cluster) {
    auto offset = cluster.payload;
    while (offset < cluster.end) {
        const auto child = NextElement(reader, offset, cluster.end, 2);
        if (SegmentChild(child.id)) return offset;
        Require(!child.unknown, "Unexpected unknown-size cluster child");
        offset = child.end;
    }
    return offset;
}

std::string MatroskaMimeType(std::string_view docType, bool video) {
    if (docType == "webm") return video ? "video/webm" : "audio/webm";
    return video ? "video/x-matroska" : "audio/x-matroska";
}

class MatroskaParser {
public:
    explicit MatroskaParser(Reader& reader) : reader_(reader) {}
    api::media::GetContainerInfoResult Parse() {
        const auto header = NextElement(reader_, 0, reader_.Size(), 0);
        std::string docType;
        Children(reader_, header, 1, [this, &docType](const Element& element) {
            if (element.id == 0x4282) docType = Text(reader_, element);
            else if (element.id == 0x42f2) Require(UInt(reader_, element, 4) >= 1 && UInt(reader_, element, 4) <= 4, "Unsupported EBML ID width");
            else if (element.id == 0x42f3) Require(UInt(reader_, element, 8) >= 1 && UInt(reader_, element, 8) <= 8, "Unsupported EBML size width");
        });
        if (docType != "matroska" && docType != "webm") return {};
        result_.recognized = true;
        result_.container = docType;
        bool found = false;
        auto offset = header.end;
        while (offset < reader_.Size()) {
            const auto element = NextElement(reader_, offset, reader_.Size(), 0);
            if (element.id == 0x18538067) {
                Require(!found, "Multiple Matroska segments cannot share one metadata result");
                found = true; Segment(element);
            } else Require(element.id == 0xec || element.id == 0xbf, "Expected Matroska Segment");
            offset = element.end;
        }
        Require(found, "Matroska Segment is missing");
        const bool video = std::ranges::any_of(result_.tracks, [](const auto& track) { return track.type == "video"; });
        result_.mimeType = MatroskaMimeType(docType, video);
        for (auto& track : result_.tracks) {
            track.mimeType = MatroskaMimeType(docType, track.type != "audio");
        }
        return std::move(result_);
    }

private:
    std::pair<std::uint64_t, std::optional<double>> Info(const Element& parent) {
        std::uint64_t scale = 1000000;
        std::optional<double> duration;
        Children(reader_, parent, 2, [this, &scale, &duration](const Element& element) {
            if (element.id == 0x2ad7b1) scale = UInt(reader_, element, 1000000);
            else if (element.id == 0x4489) duration = Real(reader_, element);
        });
        Require(scale != 0 && (!duration.has_value() || *duration > 0), "Invalid Matroska duration or timestamp scale");
        if (duration.has_value()) {
            const double seconds = *duration * static_cast<double>(scale) / 1000000000;
            Require(std::isfinite(seconds), "Matroska duration overflows");
            // Info 的时长属于整个 Segment，不能据此推断各轨道的时长或起点。
            result_.duration = seconds;
        }
        return {scale, duration};
    }

    void Attachment(const Element& parent) {
        Require(result_.attachments.size() < Reader::kEntryLimit, "Too many Matroska attachments");
        api::media::ContainerAttachment attachment;
        Children(reader_, parent, 3, [this, &attachment](const Element& element) {
            if (element.id == 0x46ae) {
                const auto uid = UInt(reader_, element);
                Require(uid != 0, "Matroska attachment UID is zero");
                attachment.id = std::to_string(uid);
            }
            else if (element.id == 0x466e) attachment.name = Text(reader_, element);
            else if (element.id == 0x4660) attachment.mimeType = OptionalText(Text(reader_, element));
            // 只登记 FileData 的长度，不读取附件正文，避免大附件占用解析预算。
            else if (element.id == 0x465c) attachment.size = SignedSize(element.Size());
        });
        Require(!attachment.id.empty() && !attachment.name.empty() && attachment.size.has_value(), "Matroska attachment metadata is incomplete");
        result_.attachments.push_back(std::move(attachment));
    }

    void Segment(const Element& segment) {
        auto offset = segment.payload;
        std::optional<std::pair<std::uint64_t, std::optional<double>>> info;
        std::map<std::string, std::pair<std::size_t, Bytes>, std::less<>> identifiers;
        std::vector<Element> chapters;
        bool hasTracks = false;
        while (offset < segment.end) {
            const auto element = NextElement(reader_, offset, segment.end, 1);
            if (element.id == 0x1549a966) {
                const auto timing = Info(element);
                Require(!info || *info == timing, "Conflicting Matroska Info timing");
                info = timing;
            }
            else if (element.id == 0x1654ae6b) {
                std::set<std::string, std::less<>> currentIdentifiers;
                Children(reader_, element, 2, [this, &currentIdentifiers, &identifiers, hasTracks](const Element& entry) {
                    if (entry.id != 0xae) return;
                    auto metadata = Track(reader_, entry, 3);
                    auto& track = metadata.track;
                    Require(currentIdentifiers.insert(track.id).second, "Duplicate Matroska track number within Tracks");
                    const auto existing = identifiers.find(track.id);
                    if (hasTracks) {
                        // 重复 Tracks 可以调整排列，但轨道集合、已解析属性和 CodecPrivate 必须一致。
                        Require(existing != identifiers.end(), "Recurring Matroska Tracks changes track identifiers");
                        const auto& previous = result_.tracks[existing->second.first];
                        Require(api::media::ToJson(previous) == api::media::ToJson(track) && existing->second.second == metadata.privateData,
                            "Conflicting recurring Matroska track metadata");
                        return;
                    }
                    Require(result_.tracks.size() < Reader::kEntryLimit, "Too many Matroska tracks");
                    identifiers.try_emplace(track.id, result_.tracks.size(), std::move(metadata.privateData));
                    result_.tracks.push_back(std::move(track));
                });
                Require(currentIdentifiers.size() == identifiers.size(), "Recurring Matroska Tracks omits track identifiers");
                hasTracks = true;
            } else if (element.id == 0x1941a469) {
                Children(reader_, element, 2, [this](const Element& entry) { if (entry.id == 0x61a7) Attachment(entry); });
            } else if (element.id == 0x1043a770) {
                chapters.push_back(element);
            }
            // Tracks/Attachments 也可能位于 Cluster 之后，不能读到首个 Cluster 就结束解析。
            if (element.unknown) {
                Require(element.id == 0x1f43b675, "Unexpected nested unknown-size Segment");
                offset = SkipUnknownCluster(reader_, element);
            } else offset = element.end;
        }
        Require(info.has_value(), "Matroska Info element is missing");
        // 章节可能在 Cluster 之后；读完整个 Segment、拿到时长后再解析。
        result_.chapters = TryChapters([this, &chapters] {
            Require(chapters.size() <= 1, "Multiple Matroska Chapters elements");
            Chapters list;
            if (!chapters.empty()) list = ReadChapters(chapters.front());
            FillChapterEnds(list, result_.duration);
            return list;
        });
    }

    // foobar2000 把每套版本的顶层章节都拆成子曲目，按容器顺序，不看默认、隐藏、停用标记，也不展开嵌套章节。
    Chapters ReadChapters(const Element& parent) {
        Chapters chapters;
        std::int64_t editions = 0;
        Children(reader_, parent, 2, [this, &chapters, &editions](const Element& entry) {
            if (entry.id != 0x45b9) return;
            const auto edition = editions;
            ++editions;
            Children(reader_, entry, 3, [this, &chapters, edition](const Element& atom) {
                if (atom.id != 0xb6) return;
                Require(chapters.size() < Reader::kEntryLimit, "Too many Matroska chapters");
                auto chapter = ReadChapter(atom);
                chapter.edition = edition;
                chapters.push_back(std::move(chapter));
            });
        });
        if (editions <= 1) for (auto& chapter : chapters) chapter.edition.reset();
        return chapters;
    }

    api::media::ContainerChapter ReadChapter(const Element& atom) {
        api::media::ContainerChapter chapter;
        std::optional<std::uint64_t> start;
        std::optional<std::uint64_t> end;
        bool display = false;
        Children(reader_, atom, 4, [this, &chapter, &start, &end, &display](const Element& element) {
            switch (element.id) {
            // 章节时间以纳秒计，不受 TimestampScale 缩放。
            case 0x91: start = UInt(reader_, element); break;
            case 0x92: end = UInt(reader_, element); break;
            case 0x6e67: case 0x6ebc: throw ContainerError("Linked Matroska chapters are unsupported");
            case 0x80:
                if (!display) ReadChapterDisplay(element, chapter);
                display = true;
                break;
            default: break;
            }
        });
        Require(start.has_value(), "Matroska chapter start is missing");
        chapter.start = static_cast<double>(*start) / 1000000000;
        if (end.has_value()) {
            Require(*end >= *start, "Matroska chapter ends before it starts");
            chapter.end = static_cast<double>(*end) / 1000000000;
        }
        return chapter;
    }

    // 只取第一个显示项；ChapLanguage 缺省为 eng，ChapLanguageBCP47 存在时优先，与轨道语言的规则相同。
    void ReadChapterDisplay(const Element& parent, api::media::ContainerChapter& chapter) {
        std::optional<std::string> language = "eng";
        std::optional<std::string> languageBcp47;
        Children(reader_, parent, 5, [this, &chapter, &language, &languageBcp47](const Element& element) {
            if (element.id == 0x85) chapter.title = OptionalText(Text(reader_, element));
            else if (element.id == 0x437c) language = OptionalText(Text(reader_, element, "eng"));
            else if (element.id == 0x437d) languageBcp47 = OptionalText(Text(reader_, element));
        });
        chapter.language = languageBcp47 ? languageBcp47 : language;
    }

    Reader& reader_;
    api::media::GetContainerInfoResult result_;
};

} // namespace

api::media::GetContainerInfoResult ReadMatroska(Reader& reader) { return MatroskaParser(reader).Parse(); }

} // namespace media::detail

#include "pch.h"
#if __has_include("compat/fb2k_types.h")
#include "compat/fb2k_types.h"
#endif
#include "media/MediaCodecs.h"

#include <algorithm>
#include <map>
#include <numbers>
#include <set>

namespace media::detail {
namespace {

struct Box {
    std::string type;
    std::uint64_t payload;
    std::uint64_t end;
    std::uint64_t Size() const { return end - payload; }
};

Box NextBox(Reader& reader, std::uint64_t offset, std::uint64_t end, unsigned depth) {
    reader.Element(depth);
    Require(offset <= end && end - offset >= 8, "Truncated MP4 box header");
    const auto header = reader.Read(offset, 8);
    auto size = Unsigned(header, 0, 4);
    std::uint64_t headerSize = 8;
    // size=0 延伸到当前父 box 的末端，不能越过父边界读入相邻结构。
    if (size == 1) {
        Require(end - offset >= 16, "Truncated extended MP4 box header");
        size = Unsigned(reader.Read(offset + 8, 8), 0, 8);
        headerSize = 16;
    } else if (size == 0) size = end - offset;
    Require(size >= headerSize && size <= end - offset, "MP4 box escapes parent");
    return {String(View(header).subspan(4)), offset + headerSize, offset + size};
}

template<class Callback>
void Boxes(Reader& reader, std::uint64_t offset, std::uint64_t end, unsigned depth, Callback callback) {
    while (offset < end) {
        const auto box = NextBox(reader, offset, end, depth);
        callback(box);
        offset = box.end;
    }
}

Bytes Prefix(Reader& reader, const Box& box, std::size_t size) {
    Require(box.Size() >= size, "Truncated MP4 metadata box");
    return reader.Read(box.payload, size);
}

struct TimeHeader {
    std::uint64_t scale;
    std::optional<double> duration;
    std::size_t languageOffset;
};

TimeHeader ReadTime(Reader& reader, const Box& box) {
    const auto version = Prefix(reader, box, 1)[0];
    Require(version <= 1, "Unsupported MP4 time header version");
    const auto data = Prefix(reader, box, version ? 32 : 20);
    const auto scale = Unsigned(data, version ? 20 : 12, 4);
    Require(scale != 0, "MP4 timescale is zero");
    const auto duration = Unsigned(data, version ? 24 : 16, version ? 8 : 4);
    const auto unknown = version ? UINT64_MAX : UINT32_MAX;
    return {scale, duration == unknown ? std::nullopt : std::optional<double>(static_cast<double>(duration) / static_cast<double>(scale)), version ? 32u : 20u};
}

std::string TrackType(std::string_view handler) {
    if (handler == "vide") return "video";
    if (handler == "soun") return "audio";
    if (handler == "text" || handler == "subt" || handler == "sbtl" || handler == "clcp") return "subtitle";
    if (handler == "pict") return "image";
    return "other";
}

struct TrackState {
    api::media::ContainerTrack track;
    std::optional<Box> descriptions;
    unsigned descriptionDepth = 0;
    bool hasMedia = false;
    bool hasHandler = false;
    bool hasMediaInfo = false;
    bool hasSampleTable = false;
    bool emptySampleTable = false;
    std::optional<Box> timeTable;
    std::optional<Box> sampleToChunk;
    std::optional<Box> sampleSizes;
    std::optional<Box> chunkOffsets;
    bool largeChunkOffsets = false;
    std::vector<std::uint32_t> chapterTracks;
    bool invalidChapterReference = false;
    std::optional<std::string> extendedLanguage;
    std::uint64_t timescale = 0;
};

// 章节轨的样本表；只在解析完全部轨道后，为被 tref/chap 引用的轨道读取。
struct SampleTables {
    std::uint32_t id = 0;
    std::uint64_t timescale = 0;
    std::optional<Box> times;
    std::optional<Box> chunks;
    std::optional<Box> sizes;
    std::optional<Box> offsets;
    bool largeOffsets = false;
    std::vector<std::uint32_t> chapterTracks;
    bool invalidChapterReference = false;
};

// tref 里的 chap 列出章节文本轨的轨道 ID。结构有误时只标记，由章节解析省略章节，不让整个文件失败。
void ChapterReferences(View data, TrackState& state) {
    for (std::size_t offset = 0; offset < data.size();) {
        if (data.size() - offset < 8) { state.invalidChapterReference = true; return; }
        const auto size = Unsigned(data, offset, 4);
        if (size < 8 || size > data.size() - offset) { state.invalidChapterReference = true; return; }
        if (String(data.subspan(offset + 4, 4)) == "chap") {
            if ((size - 8) % 4 != 0 || state.chapterTracks.size() + (size - 8) / 4 > Reader::kEntryLimit) {
                state.invalidChapterReference = true;
                return;
            }
            for (std::size_t id = offset + 8; id < offset + size; id += 4) {
                state.chapterTracks.push_back(static_cast<std::uint32_t>(Unsigned(data, id, 4)));
            }
        }
        offset += static_cast<std::size_t>(size);
    }
}

void TrackHeader(Reader& reader, const Box& box, api::media::ContainerTrack& track) {
    const auto version = Prefix(reader, box, 1)[0];
    Require(version <= 1, "Unsupported MP4 track header version");
    const auto data = Prefix(reader, box, version ? 96 : 84);
    track.id = std::to_string(Unsigned(data, version ? 20 : 12, 4));
    Require(track.id != "0", "MP4 track identifier is zero");
    const std::size_t matrix = version ? 52 : 40;
    const auto fixed = [&data, matrix](std::size_t index) {
        return static_cast<double>(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(Unsigned(data, matrix + index * 4, 4)))) / 65536.0;
    };
    const double a = fixed(0), b = fixed(1), c = fixed(3), d = fixed(4);
    // 只有纯旋转矩阵能转换成单个角度；缩放、镜像或透视变换不能冒充 rotation。
    const double tolerance = 0.0001;
    if (Unsigned(data, matrix + 8, 4) == 0 && Unsigned(data, matrix + 20, 4) == 0 && Unsigned(data, matrix + 32, 4) == 0x40000000
        && std::abs(a * a + b * b - 1) < tolerance && std::abs(c * c + d * d - 1) < tolerance
        && std::abs(a * c + b * d) < tolerance && a * d - b * c > 0) {
        double angle = std::atan2(b, a) * 180 / std::numbers::pi;
        if (angle < 0) angle += 360;
        track.rotation = angle;
    }
}

void Color(Reader& reader, const Box& box, api::media::ContainerTrack& track) {
    const auto data = Prefix(reader, box, 4);
    const auto type = String(data);
    if (type != "nclc" && type != "nclx") return;
    const auto fields = Prefix(reader, box, type == "nclx" ? 11 : 10);
    track.colorPrimaries = SignedSize(Unsigned(fields, 4, 2));
    track.colorTransfer = SignedSize(Unsigned(fields, 6, 2));
    track.colorMatrix = SignedSize(Unsigned(fields, 8, 2));
    if (type == "nclx") track.fullRange = (fields[10] & 128) != 0;
}

void Extensions(Reader& reader, std::uint64_t offset, std::uint64_t end, unsigned depth, api::media::ContainerTrack& track) {
    Boxes(reader, offset, end, depth, [&reader, &track, depth](const Box& box) {
        if (box.type == "avcC" || box.type == "hvcC" || box.type == "av1C" || box.type == "vpcC" || box.type == "esds") {
            const auto data = reader.Metadata(box.payload, box.Size());
            ReadCodecConfiguration(track, box.type, data);
        } else if (box.type == "pasp") {
            const auto data = Prefix(reader, box, 8);
            const auto horizontal = Unsigned(data, 0, 4), vertical = Unsigned(data, 4, 4);
            Require(horizontal && vertical, "Invalid MP4 pixel aspect ratio");
            if (track.width.has_value() && track.height.has_value() && *track.height > 0) {
                track.displayAspectRatio = static_cast<double>(*track.width) / static_cast<double>(*track.height)
                    * static_cast<double>(horizontal) / static_cast<double>(vertical);
            }
        } else if (box.type == "colr") Color(reader, box, track);
        else if (box.type == "btrt") {
            const auto bitrate = Unsigned(Prefix(reader, box, 12), 8, 4);
            if (bitrate) track.bitrate = SignedSize(bitrate);
        } else if (box.type == "wave") Extensions(reader, box.payload, box.end, depth + 1, track);
    });
}

std::size_t VideoSampleHeader(Reader& reader, const Box& box, api::media::ContainerTrack& track) {
    const auto data = Prefix(reader, box, 78);
    const auto width = Unsigned(data, 24, 2), height = Unsigned(data, 26, 2);
    if (width) track.width = SignedSize(width);
    if (height) track.height = SignedSize(height);
    if (width && height) track.displayAspectRatio = static_cast<double>(width) / static_cast<double>(height);
    return 78;
}

std::optional<std::size_t> AudioSampleHeader(Reader& reader, const Box& box, api::media::ContainerTrack& track) {
    const auto data = Prefix(reader, box, 28);
    const auto version = Unsigned(data, 8, 2);
    if (version > 2) return std::nullopt;
    std::size_t headerSize = 28;
    if (version == 1) headerSize = 44;
    else if (version == 2) headerSize = 64;
    const auto audio = Prefix(reader, box, headerSize);
    const auto channels = Unsigned(audio, version == 2 ? 40 : 16, version == 2 ? 4 : 2);
    const double frequency = version == 2 ? Float(View(audio).subspan(32, 8)) : static_cast<double>(Unsigned(audio, 24, 4)) / 65536;
    if (box.type == "lpcm" || box.type == "sowt" || box.type == "twos" || box.type == "raw ") {
        // 压缩音频的通用头可能只是占位值；仅 PCM 直接使用，AAC 等由编码配置提供实际值。
        if (channels) track.channels = SignedSize(channels);
        if (std::isfinite(frequency) && frequency > 0) track.sampleRate = frequency;
        const auto bits = Unsigned(audio, version == 2 ? 48 : 18, version == 2 ? 4 : 2);
        if (bits) track.bitDepth = SignedSize(bits);
    }
    return headerSize;
}

api::media::ContainerTrack SampleEntry(Reader& reader, const Box& box, std::string_view type, unsigned depth) {
    api::media::ContainerTrack track;
    track.codec = box.type;
    track.type = type;
    Prefix(reader, box, 8);
    std::optional<std::size_t> headerSize;
    if (type == "video") headerSize = VideoSampleHeader(reader, box, track);
    else if (type == "audio") headerSize = AudioSampleHeader(reader, box, track);
    if (!headerSize.has_value()) return track;
    if (box.type == "Opus") track.codecs = "opus";
    else if (box.type == "fLaC") track.codecs = "flac";
    else if (box.type == "ac-3" || box.type == "ec-3") track.codecs = box.type;
    Extensions(reader, box.payload + *headerSize, box.end, depth + 1, track);
    return track;
}

template<class T>
void Common(std::optional<T>& target, const std::optional<T>& other) {
    if (target != other) target.reset();
}

// 一条轨道可切换 sample description；这里只保留所有描述共有的值，避免把局部配置当作全轨属性。
void MergeDescription(api::media::ContainerTrack& target, const api::media::ContainerTrack& other) {
    if (target.codec != other.codec) target.codec = "unknown";
    Common(target.codecs, other.codecs); Common(target.width, other.width); Common(target.height, other.height);
    Common(target.displayAspectRatio, other.displayAspectRatio); Common(target.bitrate, other.bitrate);
    Common(target.bitDepth, other.bitDepth); Common(target.colorPrimaries, other.colorPrimaries);
    Common(target.colorTransfer, other.colorTransfer); Common(target.colorMatrix, other.colorMatrix);
    Common(target.fullRange, other.fullRange); Common(target.sampleRate, other.sampleRate); Common(target.channels, other.channels);
}

void Descriptions(Reader& reader, TrackState& state, unsigned depth) {
    if (!state.descriptions) return;
    const auto& box = *state.descriptions;
    const auto count = Unsigned(Prefix(reader, box, 8), 4, 4);
    Require(count <= Reader::kEntryLimit, "Too many MP4 sample descriptions");
    std::uint64_t seen = 0;
    api::media::ContainerTrack merged;
    Boxes(reader, box.payload + 8, box.end, depth, [&reader, &state, &seen, &merged, depth](const Box& sample) {
        const auto entry = SampleEntry(reader, sample, state.track.type, depth);
        if (seen++ == 0) merged = entry;
        else MergeDescription(merged, entry);
    });
    Require(seen == count, "MP4 sample description count mismatch");
    auto& track = state.track;
    track.codec = count ? merged.codec : "unknown";
    track.codecs = merged.codecs; track.width = merged.width; track.height = merged.height;
    track.displayAspectRatio = merged.displayAspectRatio; track.bitrate = merged.bitrate; track.bitDepth = merged.bitDepth;
    track.colorPrimaries = merged.colorPrimaries; track.colorTransfer = merged.colorTransfer; track.colorMatrix = merged.colorMatrix;
    track.fullRange = merged.fullRange; track.sampleRate = merged.sampleRate; track.channels = merged.channels;
}

// stts 只能给出样本总数/总时长的平均帧率，不能作为 MediaCapabilities 要求的最大帧率。
void FrameRate(Reader& reader, TrackState& state) {
    if (!state.timeTable || !state.timescale || state.track.type != "video") return;
    const auto& box = *state.timeTable;
    const auto count = Unsigned(Prefix(reader, box, 8), 4, 4);
    Require(count <= 100000 && box.Size() - 8 == count * 8, "Invalid MP4 time table");
    std::uint64_t samples = 0, ticks = 0;
    for (std::uint64_t index = 0; index < count;) {
        const auto entries = std::min<std::uint64_t>(count - index, 4096);
        const auto data = reader.Read(box.payload + 8 + index * 8, static_cast<std::size_t>(entries * 8));
        for (std::size_t offset = 0; offset < data.size(); offset += 8) {
            const auto amount = Unsigned(data, offset, 4);
            const auto delta = Unsigned(data, offset + 4, 4);
            Require(amount <= UINT64_MAX - samples && amount * delta <= UINT64_MAX - ticks, "MP4 time table overflows");
            samples += amount; ticks += amount * delta;
        }
        index += entries;
    }
    if (samples && ticks) state.track.frameRate = static_cast<double>(samples) * static_cast<double>(state.timescale) / static_cast<double>(ticks);
}

struct FileBrands {
    bool media = false;
    bool image = false;
    bool quickTime = false;
};

FileBrands ReadFileBrands(Reader& reader, const Box& box) {
    Require(box.Size() >= 8 && box.Size() % 4 == 0, "Invalid MP4 file type box");
    const auto data = reader.Metadata(box.payload, box.Size());
    FileBrands brands;
    for (std::size_t offset = 0; offset < data.size(); offset += 4) {
        if (offset == 4) continue;
        const auto brand = String(View(data).subspan(offset, 4));
        for (const auto* supported : {"isom", "iso2", "iso3", "iso4", "iso5", "iso6", "iso7", "iso8", "iso9", "isoa", "isob", "isoc",
            "mp41", "mp42", "M4A ", "M4B ", "M4P ", "M4V ", "avc1", "av01", "dash", "MSNV", "qt  "}) {
            if (brand == supported) brands.media = true;
        }
        for (const auto* image : {"avif", "avis", "heic", "heix", "hevc", "hevx", "mif1", "msf1", "avci", "avcs"}) {
            if (brand == image) brands.image = true;
        }
        if (brand == "qt  ") brands.quickTime = true;
    }
    return brands;
}

bool IsTrackLeaf(std::string_view type, std::string_view parent) {
    if (parent == "trak") return type == "tkhd" || type == "tref";
    if (parent == "mdia") return type == "hdlr" || type == "mdhd" || type == "elng";
    if (parent == "stbl") return type == "stsd" || type == "stts" || type == "stsc" || type == "stsz" || type == "stco" || type == "co64";
    return false;
}

class Mp4Parser {
public:
    explicit Mp4Parser(Reader& reader) : reader_(reader) {}
    api::media::GetContainerInfoResult Parse() {
        if (!Identify()) return {};
        Walk(0, reader_.Size(), 0, nullptr, {});
        if (!recognized_) return {};
        Require(hasMoov_ && hasMovieHeader_, "MP4 movie metadata is missing");
        result_.recognized = true;
        result_.container = quickTime_ ? "mov" : "mp4";
        const bool video = std::ranges::any_of(result_.tracks, [](const auto& track) { return track.type == "video"; });
        result_.mimeType = quickTime_ ? "video/quicktime" : video ? "video/mp4" : "audio/mp4";
        for (auto& track : result_.tracks) {
            track.mimeType = quickTime_ ? "video/quicktime" : track.type == "audio" ? "audio/mp4" : "video/mp4";
            // 分片的样本时长还在 moof 中，单靠 stts 无法代表整条轨道。
            if (fragmented_) track.frameRate.reset();
        }
        ReadChapters();
        return std::move(result_);
    }

private:
    // 只按 box 长度跳过 mdat，仍可找到文件尾的 moov；图片容器的兼容品牌不能冒充视频格式。
    bool Identify() {
        bool fileType = false;
        bool movie = false;
        FileBrands brands;
        Boxes(reader_, 0, reader_.Size(), 0, [this, &fileType, &brands, &movie](const Box& box) {
            if (box.type == "moov") movie = true;
            if (box.type != "ftyp") return;
            Require(!fileType, "Duplicate MP4 file type box");
            fileType = true;
            brands = ReadFileBrands(reader_, box);
            quickTime_ = brands.quickTime;
        });
        recognized_ = fileType ? brands.media && !brands.image : movie;
        if (!fileType && movie) quickTime_ = true;
        return recognized_;
    }

    void Cover(const Box& box) {
        const auto data = Prefix(reader_, box, 8);
        api::media::ContainerAttachment attachment;
        attachment.id = "cover:" + std::to_string(result_.attachments.size());
        attachment.name = "cover";
        const auto type = Unsigned(data, 0, 4) & 0xffffff;
        if (type == 13) attachment.mimeType = "image/jpeg";
        if (type == 14) attachment.mimeType = "image/png";
        attachment.size = SignedSize(box.Size() - 8);
        Require(result_.attachments.size() < Reader::kEntryLimit, "Too many MP4 attachments");
        result_.attachments.push_back(std::move(attachment));
    }

    void TrackLeaf(const Box& box, TrackState& state, unsigned depth) {
        auto& track = state.track;
        if (box.type == "tkhd") {
            Require(track.id.empty(), "Duplicate MP4 track header");
            TrackHeader(reader_, box, track);
        }
        else if (box.type == "hdlr") HandlerBox(box, state);
        else if (box.type == "mdhd") MediaHeaderBox(box, state);
        else if (box.type == "elng") {
            const auto data = reader_.Metadata(box.payload, box.Size());
            Require(data.size() >= 4, "Truncated MP4 extended language");
            if (auto language = OptionalText(String(View(data).subspan(4)))) state.extendedLanguage = std::move(language);
        } else if (box.type == "stsd") {
            Require(!state.descriptions, "Duplicate MP4 sample description box");
            state.descriptions = box; state.descriptionDepth = depth;
        }
        else if (box.type == "stts") state.timeTable = box;
        else ChapterTableLeaf(box, state);
    }

    void HandlerBox(const Box& box, TrackState& state) {
        state.hasHandler = true;
        const auto data = reader_.Metadata(box.payload, box.Size());
        Require(data.size() >= 24, "Truncated MP4 handler");
        state.track.type = TrackType(String(View(data).subspan(8, 4)));
        if (data.size() > 24) {
            auto name = String(View(data).subspan(24));
            if (!name.empty() && static_cast<unsigned char>(name[0]) == name.size() - 1) name.erase(0, 1);
            state.track.name = OptionalText(std::move(name));
        }
    }

    void MediaHeaderBox(const Box& box, TrackState& state) {
        const auto time = ReadTime(reader_, box);
        state.timescale = time.scale; state.track.duration = time.duration;
        const auto data = Prefix(reader_, box, time.languageOffset + 2);
        const auto language = Unsigned(data, time.languageOffset, 2);
        const unsigned a = (language >> 10) & 31, b = (language >> 5) & 31, c = language & 31;
        if (a >= 1 && a <= 26 && b >= 1 && b <= 26 && c >= 1 && c <= 26) {
            state.track.language = std::string{static_cast<char>(a + 96), static_cast<char>(b + 96), static_cast<char>(c + 96)};
        }
    }

    // 以下样本表只供章节轨读取；重复时保留第一个，不改变其余轨道的解析结果。
    void ChapterTableLeaf(const Box& box, TrackState& state) {
        if (box.type == "stsc") { if (!state.sampleToChunk) state.sampleToChunk = box; }
        else if (box.type == "stsz") { if (!state.sampleSizes) state.sampleSizes = box; }
        else if (box.type == "stco" || box.type == "co64") {
            if (!state.chunkOffsets) { state.chunkOffsets = box; state.largeChunkOffsets = box.type == "co64"; }
        } else if (box.type == "tref") {
            if (box.Size() > Reader::kMetadataLimit) state.invalidChapterReference = true;
            else ChapterReferences(reader_.Metadata(box.payload, box.Size()), state);
        }
    }

    void ReadTrack(const Box& box, unsigned depth, std::string_view parent) {
        Require(parent == "moov", "MP4 track must belong to a movie");
        Require(result_.tracks.size() < Reader::kEntryLimit, "Too many MP4 tracks");
        TrackState next;
        next.track.type = "other"; next.track.codec = "unknown";
        Walk(box.payload, box.end, depth + 1, &next, box.type);
        Require(!next.track.id.empty(), "MP4 track has no identifier");
        // QuickTime 允许物理为空的 stbl；其他缺失 stsd 的轨道仍视为结构不完整。
        Require(next.hasMedia && next.hasHandler && next.hasMediaInfo && next.hasSampleTable &&
            (next.descriptions || (quickTime_ && next.emptySampleTable)) && next.timescale > 0,
            "MP4 track media metadata is incomplete");
        Require(trackIdentifiers_.insert(next.track.id).second, "Duplicate MP4 track identifier");
        Descriptions(reader_, next, next.descriptionDepth + 1);
        FrameRate(reader_, next);
        if (next.extendedLanguage) next.track.language = std::move(next.extendedLanguage);
        Require(result_.tracks.size() < Reader::kEntryLimit, "Too many MP4 tracks");
        tables_.emplace_back(static_cast<std::uint32_t>(std::stoul(next.track.id)), next.timescale, next.timeTable,
            next.sampleToChunk, next.sampleSizes, next.chunkOffsets, next.largeChunkOffsets, std::move(next.chapterTracks),
            next.invalidChapterReference);
        result_.tracks.push_back(std::move(next.track));
    }

    void EnterContainer(const Box& box, TrackState* state, std::string_view parent) {
        if (box.type == "mdia") {
            Require(parent == "trak" && state && !state->hasMedia, "Invalid MP4 media container");
            state->hasMedia = true;
        }
        if (box.type == "minf") {
            Require(parent == "mdia" && state && !state->hasMediaInfo, "Invalid MP4 media information container");
            state->hasMediaInfo = true;
        }
        if (box.type == "stbl") {
            Require(parent == "minf" && state && !state->hasSampleTable, "Invalid MP4 sample table container");
            state->hasSampleTable = true;
            state->emptySampleTable = box.payload == box.end;
        }
        if (box.type == "moov") {
            Require(parent.empty() && !hasMoov_, "Invalid MP4 movie container");
            hasMoov_ = true;
            if (!recognized_) quickTime_ = true;
            recognized_ = true;
        }
    }

    void ReadLeaf(const Box& box, unsigned depth, TrackState* state, std::string_view parent) {
        if (box.type == "mvhd") {
            Require(parent == "moov" && !hasMovieHeader_, "Invalid MP4 movie header");
            const auto version = Prefix(reader_, box, 1)[0];
            Prefix(reader_, box, version == 1 ? 112 : 100);
            result_.duration = ReadTime(reader_, box).duration;
            hasMovieHeader_ = true;
        }
        else if (box.type == "cmov") throw ContainerError("Compressed QuickTime movie metadata is unsupported");
        else if (box.type == "rmra") throw ContainerError("QuickTime reference movie metadata is unsupported");
        else if (box.type == "mvex") fragmented_ = true;
        else if (box.type == "data" && parent == "covr") Cover(box);
        else if (box.type == "chpl" && parent == "udta" && !state) { if (!neroChapters_) neroChapters_ = box; }
        else if (state && IsTrackLeaf(box.type, parent)) TrackLeaf(box, *state, depth);
    }

    void Walk(std::uint64_t offset, std::uint64_t end, unsigned depth, TrackState* state, std::string_view parent) {
        Boxes(reader_, offset, end, depth, [this, parent, depth, state](const Box& box) {
            if (box.type == "ftyp") {
                return;
            } else if (box.type == "trak") {
                ReadTrack(box, depth, parent);
            } else if (box.type == "moov" || box.type == "mdia" || box.type == "minf" || box.type == "stbl"
                || box.type == "udta" || box.type == "ilst" || box.type == "covr") {
                EnterContainer(box, state, parent);
                Walk(box.payload, box.end, depth + 1, state, box.type);
            } else if (box.type == "meta") {
                const auto prefix = Prefix(reader_, box, 8);
                // QuickTime 的 meta 可直接以 hdlr 开始；ISO 形式在子 box 前还有版本/标志四字节。
                const bool quickTimeMeta = String(View(prefix).subspan(4, 4)) == "hdlr";
                Walk(box.payload + (quickTimeMeta ? 0 : 4), box.end, depth + 1, state, box.type);
            } else ReadLeaf(box, depth, state, parent);
        });
    }

    // QuickTime 章节轨优先于 Nero chpl，与 foobar2000 一致；被引用的文本轨只承载章节标题，不按字幕报告。
    void ReadChapters() {
        std::map<std::uint32_t, std::size_t> indexes;
        for (std::size_t i = 0; i < tables_.size(); ++i) indexes.emplace(tables_[i].id, i);
        const SampleTables* chapterTrack = nullptr;
        bool invalidReference = false;
        for (const auto& tables : tables_) {
            invalidReference = invalidReference || tables.invalidChapterReference;
            for (const auto id : tables.chapterTracks) {
                const auto found = indexes.find(id);
                if (found == indexes.end()) continue;
                result_.tracks[found->second].type = "other";
                if (!chapterTrack) chapterTrack = &tables_[found->second];
            }
        }
        result_.chapters = TryChapters([this, chapterTrack, invalidReference] {
            Require(!invalidReference, "Invalid MP4 chapter reference");
            Chapters chapters;
            if (chapterTrack) chapters = QuickTimeChapters(*chapterTrack);
            if (chapters.empty() && neroChapters_) chapters = NeroChapters(*neroChapters_);
            FillChapterEnds(chapters, result_.duration);
            return chapters;
        });
    }

    std::vector<std::uint64_t> SampleDurations(const Box& box) {
        const auto count = Unsigned(Prefix(reader_, box, 8), 4, 4);
        Require(count <= Reader::kEntryLimit && box.Size() - 8 == count * 8, "Invalid MP4 chapter time table");
        const auto data = reader_.Read(box.payload + 8, static_cast<std::size_t>(count * 8));
        std::vector<std::uint64_t> durations;
        for (std::size_t offset = 0; offset < data.size(); offset += 8) {
            const auto amount = Unsigned(data, offset, 4);
            const auto delta = Unsigned(data, offset + 4, 4);
            Require(amount <= Reader::kEntryLimit - durations.size(), "Too many MP4 chapters");
            durations.insert(durations.end(), static_cast<std::size_t>(amount), delta);
        }
        return durations;
    }

    std::vector<std::uint64_t> SampleSizes(const Box& box, std::size_t samples) {
        const auto header = Prefix(reader_, box, 12);
        const auto fixed = Unsigned(header, 4, 4);
        const auto count = Unsigned(header, 8, 4);
        Require(count == samples, "MP4 chapter sample count mismatch");
        if (fixed) return std::vector<std::uint64_t>(samples, fixed);
        Require(box.Size() - 12 >= count * 4, "Truncated MP4 chapter sample sizes");
        const auto data = reader_.Read(box.payload + 12, static_cast<std::size_t>(count * 4));
        std::vector<std::uint64_t> sizes;
        for (std::size_t offset = 0; offset < data.size(); offset += 4) sizes.push_back(Unsigned(data, offset, 4));
        return sizes;
    }

    // 按 stsc 把样本依次分进各个 chunk，chunk 内的样本首尾相接。
    std::vector<std::uint64_t> SampleOffsets(const Box& chunkOffsets, bool largeOffsets, const Box& sampleToChunk,
                                             const std::vector<std::uint64_t>& sizes) {
        const auto chunkCount = Unsigned(Prefix(reader_, chunkOffsets, 8), 4, 4);
        const std::size_t width = largeOffsets ? 8 : 4;
        Require(chunkCount <= Reader::kEntryLimit && chunkOffsets.Size() - 8 >= chunkCount * width, "Invalid MP4 chapter chunk offsets");
        const auto chunks = reader_.Read(chunkOffsets.payload + 8, static_cast<std::size_t>(chunkCount * width));
        const auto runCount = Unsigned(Prefix(reader_, sampleToChunk, 8), 4, 4);
        Require(runCount <= Reader::kEntryLimit && sampleToChunk.Size() - 8 >= runCount * 12, "Invalid MP4 chapter sample-to-chunk table");
        const auto runs = reader_.Read(sampleToChunk.payload + 8, static_cast<std::size_t>(runCount * 12));
        std::vector<std::uint64_t> offsets;
        for (std::size_t run = 0; run < runCount && offsets.size() < sizes.size(); ++run) {
            const auto first = Unsigned(runs, run * 12, 4);
            const auto perChunk = Unsigned(runs, run * 12 + 4, 4);
            const auto next = run + 1 < runCount ? Unsigned(runs, (run + 1) * 12, 4) : chunkCount + 1;
            Require(first >= 1 && first < next && next <= chunkCount + 1, "Invalid MP4 chapter chunk run");
            for (auto chunk = first; chunk < next && offsets.size() < sizes.size(); ++chunk) {
                auto offset = Unsigned(chunks, static_cast<std::size_t>((chunk - 1) * width), width);
                for (std::uint64_t sample = 0; sample < perChunk && offsets.size() < sizes.size(); ++sample) {
                    offsets.push_back(offset);
                    Require(sizes[offsets.size() - 1] <= UINT64_MAX - offset, "MP4 chapter sample offset overflows");
                    offset += sizes[offsets.size() - 1];
                }
            }
        }
        Require(offsets.size() == sizes.size(), "MP4 chapter samples exceed their chunks");
        return offsets;
    }

    // 文本样本以两字节长度开头；带 BOM 的是 UTF-16，否则按 UTF-8 读，后面的样式 box 不读。
    std::optional<std::string> ChapterTitle(std::uint64_t offset, std::uint64_t size) {
        if (size < 2) return std::nullopt;
        const auto length = Unsigned(reader_.Read(offset, 2), 0, 2);
        Require(length <= size - 2, "MP4 chapter title escapes its sample");
        const auto text = reader_.Metadata(offset + 2, length);
        if (text.size() >= 2 && ((text[0] == 0xfe && text[1] == 0xff) || (text[0] == 0xff && text[1] == 0xfe))) {
            return OptionalText(Utf16ToUtf8(View(text).subspan(2), text[0] == 0xfe));
        }
        return OptionalText(String(text));
    }

    Chapters QuickTimeChapters(const SampleTables& tables) {
        if (!tables.timescale || !tables.times || !tables.chunks || !tables.sizes || !tables.offsets) {
            throw ContainerError("MP4 chapter track tables are incomplete");
        }
        const auto durations = SampleDurations(*tables.times);
        const auto sizes = SampleSizes(*tables.sizes, durations.size());
        const auto offsets = SampleOffsets(*tables.offsets, tables.largeOffsets, *tables.chunks, sizes);
        Chapters chapters;
        std::uint64_t time = 0;
        const auto scale = static_cast<double>(tables.timescale);
        for (std::size_t i = 0; i < durations.size(); ++i) {
            Require(durations[i] <= UINT64_MAX - time, "MP4 chapter time overflows");
            api::media::ContainerChapter chapter;
            chapter.start = static_cast<double>(time) / scale;
            time += durations[i];
            chapter.end = static_cast<double>(time) / scale;
            chapter.title = ChapterTitle(offsets[i], sizes[i]);
            chapters.push_back(std::move(chapter));
        }
        return chapters;
    }

    // Nero chpl：版本 1 多 4 个保留字节；条目是 100 ns 计的起点加一字节长度的标题，没有终点。
    Chapters NeroChapters(const Box& box) {
        const auto data = reader_.Metadata(box.payload, box.Size());
        Require(data.size() >= 5, "Truncated MP4 Nero chapters");
        std::size_t offset = data[0] ? 8 : 4;
        Require(offset < data.size(), "Truncated MP4 Nero chapters");
        const std::size_t count = data[offset];
        ++offset;
        Chapters chapters;
        for (std::size_t i = 0; i < count; ++i) {
            Require(data.size() - offset >= 9, "Truncated MP4 Nero chapter");
            api::media::ContainerChapter chapter;
            chapter.start = static_cast<double>(Unsigned(data, offset, 8)) / 10000000;
            const std::size_t length = data[offset + 8];
            offset += 9;
            Require(data.size() - offset >= length, "Truncated MP4 Nero chapter title");
            chapter.title = OptionalText(String(View(data).subspan(offset, length)));
            offset += length;
            chapters.push_back(std::move(chapter));
        }
        return chapters;
    }

    Reader& reader_;
    api::media::GetContainerInfoResult result_;
    bool recognized_ = false;
    bool quickTime_ = false;
    bool hasMoov_ = false;
    bool hasMovieHeader_ = false;
    bool fragmented_ = false;
    std::set<std::string> trackIdentifiers_;
    std::vector<SampleTables> tables_;
    std::optional<Box> neroChapters_;
};

} // namespace

api::media::GetContainerInfoResult ReadMp4(Reader& reader) { return Mp4Parser(reader).Parse(); }

} // namespace media::detail

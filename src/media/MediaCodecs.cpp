#include "pch.h"
#if __has_include("compat/fb2k_types.h")
#include "compat/fb2k_types.h"
#endif
#include "media/MediaCodecs.h"

#include <array>
#include <iomanip>
#include <iterator>
#include <sstream>

namespace media::detail {
namespace {

std::string Hex(std::uint64_t value, int width = 0) {
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0') << std::setw(width) << value;
    return out.str();
}

std::string Decimal(unsigned value) {
    return (value < 10 ? "0" : "") + std::to_string(value);
}

class AudioBits {
public:
    explicit AudioBits(View data) : data_(data) {}

    std::size_t Remaining() const { return data_.size() * 8 - offset_; }

    unsigned Read(unsigned count) {
        Require(count <= 24 && count <= Remaining(), "Truncated AudioSpecificConfig");
        unsigned value = 0;
        for (unsigned index = 0; index < count; ++index, ++offset_) {
            value = (value << 1) | ((data_[offset_ / 8] >> (7 - offset_ % 8)) & 1);
        }
        return value;
    }

    bool ConsumeMarker(unsigned marker, unsigned width) {
        if (Remaining() < width) return false;
        const auto saved = offset_;
        if (Read(width) == marker) return true;
        offset_ = saved;
        return false;
    }

private:
    View data_;
    std::size_t offset_ = 0;
};

unsigned AudioObjectType(AudioBits& bits) {
    const auto type = bits.Read(5);
    return type == 31 ? 32 + bits.Read(6) : type;
}

std::optional<double> AudioFrequency(AudioBits& bits) {
    static constexpr std::array<unsigned, 13> frequencies = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350};
    const auto index = bits.Read(4);
    if (index < std::size(frequencies)) return frequencies[index];
    if (index == 15) {
        const auto explicitFrequency = bits.Read(24);
        if (explicitFrequency) return explicitFrequency;
    }
    return std::nullopt;
}

std::optional<std::int64_t> AudioChannels(unsigned configuration) {
    static constexpr std::array<unsigned, 15> channels = {0, 1, 2, 3, 4, 5, 6, 8, 0, 0, 0, 7, 8, 24, 8};
    if (configuration < std::size(channels) && channels[configuration]) return channels[configuration];
    return std::nullopt;
}

struct AudioConfiguration {
    unsigned objectType = 0;
    std::optional<double> frequency;
    std::optional<std::int64_t> channels;
};

bool SkipGeneralAudioConfig(AudioBits& bits, unsigned objectType, unsigned channels) {
    if (objectType != 1 && objectType != 2 && objectType != 3 && objectType != 4 && objectType != 6 && objectType != 7) return false;
    bits.Read(1);
    if (bits.Read(1)) bits.Read(14);
    const bool extension = bits.Read(1) != 0;
    // PCE 和扩展配置需要专门解析；不知道语法边界时不能把后续任意位串当作同步扩展标记。
    if (channels == 0) return false;
    if (objectType == 6) bits.Read(3);
    return !extension || bits.Read(1) == 0;
}

AudioConfiguration ReadAudioConfiguration(View data) {
    AudioBits bits(data);
    AudioConfiguration result;
    result.objectType = AudioObjectType(bits);
    result.frequency = AudioFrequency(bits);
    auto channelConfiguration = bits.Read(4);
    result.channels = AudioChannels(channelConfiguration);
    auto coreType = result.objectType;
    const bool explicitSbr = coreType == 5 || coreType == 29;
    bool parametricStereo = coreType == 29;
    // SBR 扩展采样率描述解码后的输出；PS 可把单声道核心扩展为双声道。
    if (explicitSbr) {
        result.frequency = AudioFrequency(bits);
        coreType = AudioObjectType(bits);
        if (coreType == 22) {
            channelConfiguration = bits.Read(4);
            result.channels = AudioChannels(channelConfiguration);
        }
    }
    const bool knownAac = coreType == 1 || coreType == 2 || coreType == 3 || coreType == 4 || coreType == 6 || coreType == 7
        || coreType == 17 || coreType == 19 || coreType == 20 || coreType == 21 || coreType == 22 || coreType == 23 || coreType == 39;
    if (!knownAac) {
        result.frequency.reset();
        result.channels.reset();
        return result;
    }
    if (SkipGeneralAudioConfig(bits, coreType, channelConfiguration) && !explicitSbr && bits.ConsumeMarker(0x2b7, 11)) {
        const auto extensionType = AudioObjectType(bits);
        if (extensionType == 5 && bits.Read(1)) {
            result.frequency = AudioFrequency(bits);
            if (bits.ConsumeMarker(0x548, 11)) parametricStereo = bits.Read(1) != 0;
        }
    }
    if (parametricStereo && result.channels == 1) result.channels = 2;
    return result;
}

void ApplyAudioConfiguration(api::media::ContainerTrack& track, View data) {
    const auto configuration = ReadAudioConfiguration(data);
    if (configuration.objectType) track.codecs = "mp4a.40." + std::to_string(configuration.objectType);
    track.sampleRate = configuration.frequency;
    track.channels = configuration.channels;
}

struct Descriptor {
    unsigned tag;
    View body;
};

Descriptor NextDescriptor(View& data) {
    Require(data.size() >= 2, "Truncated MPEG descriptor");
    const unsigned tag = data[0];
    std::size_t cursor = 1;
    std::size_t length = 0;
    for (unsigned i = 0; i < 4; ++i) {
        Require(cursor < data.size(), "Truncated MPEG descriptor length");
        const auto value = data[cursor];
        ++cursor;
        length = (length << 7) | (value & 127);
        if (!(value & 128)) {
            Require(length <= data.size() - cursor, "MPEG descriptor escapes parent");
            const auto body = data.subspan(cursor, length);
            data = data.subspan(cursor + length);
            return {tag, body};
        }
    }
    throw ContainerError("Invalid MPEG descriptor length");
}

void ReadDecoder(api::media::ContainerTrack& track, View data) {
    Require(data.size() >= 13, "Truncated MPEG decoder configuration");
    const auto object = data[0];
    const auto bitrate = Unsigned(data, 9, 4);
    if (bitrate) track.bitrate = SignedSize(bitrate);
    auto children = data.subspan(13);
    std::optional<View> audioConfiguration;
    while (!children.empty()) {
        const auto descriptor = NextDescriptor(children);
        if (descriptor.tag == 5) audioConfiguration = descriptor.body;
    }
    if (track.codec != "mp4a") return;
    if (object == 0x40) {
        if (audioConfiguration) ApplyAudioConfiguration(track, *audioConfiguration);
    } else if (object == 0x66 || object == 0x67 || object == 0x68 || object == 0x69 || object == 0x6b) {
        track.codecs = "mp4a." + Hex(object, 2);
    }
}

void ReadEsds(api::media::ContainerTrack& track, View data) {
    Require(data.size() >= 4, "Truncated ES descriptor box");
    auto descriptors = data.subspan(4);
    while (!descriptors.empty()) {
        auto descriptor = NextDescriptor(descriptors);
        if (descriptor.tag == 4) { ReadDecoder(track, descriptor.body); continue; }
        if (descriptor.tag != 3) continue;
        auto body = descriptor.body;
        Require(body.size() >= 3, "Truncated ES descriptor");
        const auto flags = body[2];
        std::size_t offset = 3 + ((flags & 128) ? 2 : 0);
        if (flags & 64) {
            Require(offset < body.size(), "Truncated ES URL");
            offset += 1 + body[offset];
        }
        if (flags & 32) offset += 2;
        Require(offset <= body.size(), "Truncated ES descriptor fields");
        body = body.subspan(offset);
        while (!body.empty()) {
            descriptor = NextDescriptor(body);
            if (descriptor.tag == 4) ReadDecoder(track, descriptor.body);
        }
    }
}

void ReadAvc(api::media::ContainerTrack& track, View data) {
    Require(data.size() >= 7, "Truncated AVC configuration");
    if (data[0] != 1) return;
    std::size_t cursor = 6;
    const auto nals = [data, &cursor](unsigned count) {
        for (unsigned index = 0; index < count; ++index) {
            const auto size = Unsigned(data, cursor, 2);
            cursor += 2;
            Require(size <= data.size() - cursor, "Truncated AVC parameter set");
            cursor += static_cast<std::size_t>(size);
        }
    };
    nals(data[5] & 31);
    Require(cursor < data.size(), "Missing AVC picture parameter set count");
    const auto pictureSets = data[cursor];
    ++cursor;
    nals(pictureSets);
    if (track.codec == "avc1" || track.codec == "avc2" || track.codec == "avc3" || track.codec == "avc4") {
        track.codecs = track.codec + "." + Hex(Unsigned(data, 1, 3), 6);
    }
}

void ReadHevc(api::media::ContainerTrack& track, View data) {
    Require(data.size() >= 23, "Truncated HEVC configuration");
    if (data[0] != 1 || (track.codec != "hvc1" && track.codec != "hev1")) return;
    std::size_t cursor = 23;
    for (unsigned array = 0; array < data[22]; ++array) {
        const auto count = Unsigned(data, cursor + 1, 2);
        cursor += 3;
        for (std::uint64_t index = 0; index < count; ++index) {
            const auto size = Unsigned(data, cursor, 2);
            cursor += 2;
            Require(size <= data.size() - cursor, "Truncated HEVC parameter set");
            cursor += static_cast<std::size_t>(size);
        }
    }
    // HEVC 的 codecs 字符串按相反位序表示 compatibility flags，不能直接输出 hvcC 原整数。
    std::uint32_t compatibility = static_cast<std::uint32_t>(Unsigned(data, 2, 4));
    std::uint32_t reversed = 0;
    for (unsigned i = 0; i < 32; ++i) { reversed = (reversed << 1) | (compatibility & 1); compatibility >>= 1; }
    const auto space = data[1] >> 6;
    std::string codec = track.codec + ".";
    if (space) codec += static_cast<char>('A' + space - 1);
    codec += std::to_string(data[1] & 31) + "." + Hex(reversed) + "." + ((data[1] & 32) ? "H" : "L") + std::to_string(data[12]);
    std::size_t end = 12;
    while (end > 6 && data[end - 1] == 0) --end;
    for (std::size_t i = 6; i < end; ++i) codec += "." + Hex(data[i], 2);
    track.codecs = std::move(codec);
    const auto lumaDepth = 8 + (data[17] & 7);
    const auto chromaDepth = 8 + (data[18] & 7);
    if (lumaDepth == chromaDepth) track.bitDepth = lumaDepth;
}

void ReadAv1(api::media::ContainerTrack& track, View data) {
    Require(data.size() >= 4, "Truncated AV1 configuration");
    if (data[0] != 0x81 || track.codec != "av01") return;
    const unsigned profile = data[1] >> 5;
    const unsigned depth = (data[2] & 64) ? ((data[2] & 32) ? 12 : 10) : 8;
    if (profile > 2 || ((data[2] & 32) && (profile != 2 || !(data[2] & 64)))) return;
    track.codecs = "av01." + std::to_string(profile) + "." + Decimal(data[1] & 31) + ((data[2] & 128) ? "H" : "M") + "." + Decimal(depth);
    track.bitDepth = depth;
}

void ReadVp(api::media::ContainerTrack& track, View data) {
    Require(data.size() >= 12, "Truncated VP configuration");
    if (data[0] != 1 || (track.codec != "vp09" && track.codec != "vp08")) return;
    const unsigned depth = data[6] >> 4;
    if (data[4] > 3 || (depth != 8 && depth != 10 && depth != 12)) return;
    Require(Unsigned(data, 10, 2) <= data.size() - 12, "Truncated VP initialization data");
    track.codecs = track.codec + "." + Decimal(data[4]) + "." + Decimal(data[5]) + "." + Decimal(depth)
        + "." + Decimal((data[6] >> 1) & 7) + "." + Decimal(data[7]) + "." + Decimal(data[8]) + "." + Decimal(data[9]) + "." + Decimal(data[6] & 1);
    track.bitDepth = depth; track.colorPrimaries = data[7]; track.colorTransfer = data[8]; track.colorMatrix = data[9]; track.fullRange = (data[6] & 1) != 0;
}

} // namespace

void ReadCodecConfiguration(api::media::ContainerTrack& track, std::string_view kind, View data) {
    if (kind == "avcC") ReadAvc(track, data);
    else if (kind == "hvcC") ReadHevc(track, data);
    else if (kind == "av1C") ReadAv1(track, data);
    else if (kind == "vpcC") ReadVp(track, data);
    else if (kind == "esds") ReadEsds(track, data);
}

void ReadMatroskaCodec(api::media::ContainerTrack& track, View privateData) {
    // 配置解析器使用 MP4 编码名；仅在副本上转换，返回的 codec 仍保留 Matroska 原标识。
    auto configured = track;
    std::string kind;
    if (track.codec == "V_MPEG4/ISO/AVC") { configured.codec = "avc1"; kind = "avcC"; }
    else if (track.codec == "V_MPEGH/ISO/HEVC") { configured.codec = "hev1"; kind = "hvcC"; }
    else if (track.codec == "V_AV1") { configured.codec = "av01"; kind = "av1C"; }
    if (!kind.empty() && !privateData.empty()) {
        ReadCodecConfiguration(configured, kind, privateData);
        track.codecs = configured.codecs;
        if (!track.bitDepth.has_value()) track.bitDepth = configured.bitDepth;
    } else if (track.codec == "A_AAC") {
        if (!privateData.empty()) ApplyAudioConfiguration(track, privateData);
    } else if (track.codec == "A_OPUS") track.codecs = "opus";
    else if (track.codec == "A_VORBIS") track.codecs = "vorbis";
    else if (track.codec == "A_FLAC") track.codecs = "flac";
    else if (track.codec == "A_AC3") track.codecs = "ac-3";
    else if (track.codec == "A_EAC3") track.codecs = "ec-3";
}

} // namespace media::detail

#include "pch.h"
#include "compat/fb2k_types.h"
#include "media/ChapterSubsongs.h"
#include "media/ContainerInfo.h"
#include "media/ContainerReader.h"

#include <algorithm>
#include <bit>
#include <limits>

namespace {
using Bytes = std::vector<std::uint8_t>;

void Append(Bytes& out, const Bytes& value) { out.insert(out.end(), value.begin(), value.end()); }
Bytes Join(std::initializer_list<Bytes> parts) {
    Bytes out;
    for (const auto& part : parts) Append(out, part);
    return out;
}
Bytes Number(std::uint64_t value, std::size_t size) {
    Bytes out(size);
    for (std::size_t i = size; i > 0; --i) { out[i - 1] = static_cast<std::uint8_t>(value); value >>= 8; }
    return out;
}
Bytes Text(std::string_view value) { return Bytes(value.begin(), value.end()); }
void Put(Bytes& out, std::size_t offset, std::uint64_t value, std::size_t size) {
    const auto bytes = Number(value, size);
    std::copy(bytes.begin(), bytes.end(), out.begin() + offset);
}
Bytes Box(std::string_view type, const Bytes& payload, int width = 4) {
    return Join({Number(width == 8 ? 1 : width == 0 ? 0 : payload.size() + 8, 4), Text(type),
        width == 8 ? Number(payload.size() + 16, 8) : Bytes{}, payload});
}
Bytes Ftyp(std::string_view brand = "isom") { return Box("ftyp", Join({Text(brand), Number(0, 4), Text(brand)})); }
Bytes MovieHeader(std::uint32_t scale = 1000, std::uint32_t duration = 2500) {
    Bytes data(100);
    Put(data, 12, scale, 4); Put(data, 16, duration, 4);
    return Box("mvhd", data);
}
Bytes Movie(const Bytes& payload, int width = 4) { return Box("moov", Join({MovieHeader(), payload}), width); }
Bytes Track(std::string_view handler, std::string_view codec, const Bytes& extensions = {}, bool rotated = false, const Bytes& extraSample = {}, std::uint32_t id = 7, const Bytes& sampleHeader = {}) {
    Bytes tkhd(84);
    Put(tkhd, 12, id, 4);
    Put(tkhd, 40, rotated ? 0 : 65536, 4);
    Put(tkhd, 44, rotated ? 65536 : 0, 4);
    Put(tkhd, 52, rotated ? 0xffff0000 : 0, 4);
    Put(tkhd, 56, rotated ? 0 : 65536, 4);
    Put(tkhd, 72, 0x40000000, 4);
    Bytes mdhd(24);
    Put(mdhd, 12, 1000, 4);
    Put(mdhd, 16, 2500, 4);
    Put(mdhd, 20, (5 << 10) | (14 << 5) | 7, 2);
    Bytes hdlr(24);
    std::copy(handler.begin(), handler.end(), hdlr.begin() + 8);
    Bytes sample(handler == "vide" ? 78 : handler == "soun" ? 28 : 8);
    if (handler == "vide") { Put(sample, 24, 1920, 2); Put(sample, 26, 1080, 2); }
    if (handler == "soun") { Put(sample, 16, 2, 2); Put(sample, 24, 48000ull << 16, 4); }
    if (!sampleHeader.empty()) sample = sampleHeader;
    Append(sample, extensions);
    return Box("trak", Join({Box("tkhd", tkhd), Box("mdia", Join({Box("mdhd", mdhd), Box("hdlr", hdlr),
        Box("minf", Box("stbl", Box("stsd", Join({Number(0, 4), Number(extraSample.empty() ? 1 : 2, 4), Box(codec, sample), extraSample}))))}))}));
}
Bytes Ebml(std::uint32_t id, const Bytes& payload) {
    std::size_t idSize = id > 0xffffff ? 4 : id > 0xffff ? 3 : id > 0xff ? 2 : 1;
    std::size_t lengthSize = 1;
    while (payload.size() >= (std::uint64_t{1} << (lengthSize * 7)) - 1) ++lengthSize;
    return Join({Number(id, idSize), Number((std::uint64_t{1} << (lengthSize * 7)) | payload.size(), lengthSize), payload});
}
Bytes UInt(std::uint32_t id, std::uint64_t value) { return Ebml(id, Number(value, 8)); }
Bytes Str(std::uint32_t id, std::string_view value) { return Ebml(id, Text(value)); }
Bytes Mkv(const Bytes& content, bool unknown = false, std::string_view docType = "matroska") {
    return Join({Ebml(0x1a45dfa3, Str(0x4282, docType)), unknown ? Join({Number(0x18538067, 4), Bytes{0xff}, content}) : Ebml(0x18538067, content)});
}
Bytes Bits(std::initializer_list<std::pair<std::uint32_t, unsigned>> fields) {
    Bytes output;
    std::size_t count = 0;
    for (const auto [value, width] : fields) {
        for (unsigned bit = width; bit > 0; --bit) {
            if (count % 8 == 0) output.push_back(0);
            output.back() |= static_cast<std::uint8_t>(((value >> (bit - 1)) & 1) << (7 - count % 8));
            ++count;
        }
    }
    return output;
}
Bytes AudioEsds(const Bytes& config) {
    const auto decoder = Join({Bytes{0x40, 0x15}, Bytes(11), Bytes{5, static_cast<std::uint8_t>(config.size())}, config});
    const auto es = Join({Bytes{0, 1, 0, 4, static_cast<std::uint8_t>(decoder.size())}, decoder});
    return Box("esds", Join({Bytes(4), Bytes{3, static_cast<std::uint8_t>(es.size())}, es}));
}
api::media::GetContainerInfoResult Inspect(const Bytes& file) {
    return media::InspectContainer(file.size(), [&](std::uint64_t offset, std::size_t size) {
        if (offset > file.size() || size > file.size() - offset || size > 16 * 1024 * 1024) throw std::runtime_error("invalid read");
        return Bytes(file.begin() + static_cast<std::size_t>(offset), file.begin() + static_cast<std::size_t>(offset) + size);
    });
}
}

TEST(MediaContainers, UnknownInputIsNotAMalformedRecognizedContainer) {
    EXPECT_FALSE(Inspect(Text("not a media file")).recognized);
    EXPECT_FALSE(Inspect(Bytes{}).recognized);
    EXPECT_FALSE(Inspect(Mkv({}, false, "other")).recognized);
}

TEST(MediaContainers, Mp4ReadsTailMoovAndAccurateVideoFields) {
    auto file = Join({Ftyp(), Box("mdat", Bytes(100)), Movie(Track("vide", "avc1", Join({
        Box("avcC", {1, 100, 0, 40, 0xff, 0xe0, 0}), Box("pasp", Join({Number(4, 4), Number(3, 4)})),
        Box("colr", Join({Text("nclx"), Number(1, 2), Number(13, 2), Number(1, 2), {0x80}})),
        Box("btrt", Join({Number(0, 4), Number(900000, 4), Number(800000, 4)}))}), true), 8)});
    const auto result = Inspect(file);
    ASSERT_TRUE(result.recognized);
    ASSERT_EQ(result.tracks.size(), 1u);
    const auto& track = result.tracks[0];
    EXPECT_EQ(track.id, "7"); EXPECT_EQ(track.type, "video"); EXPECT_EQ(track.codec, "avc1");
    EXPECT_EQ(track.codecs, "avc1.640028"); EXPECT_EQ(track.width, 1920); EXPECT_EQ(track.height, 1080);
    EXPECT_NEAR(*track.displayAspectRatio, 64.0 / 27, 0.00001); EXPECT_EQ(track.rotation, 90);
    EXPECT_EQ(track.colorTransfer, 13); EXPECT_EQ(track.fullRange, true); EXPECT_EQ(track.bitrate, 800000);
    EXPECT_EQ(track.language, "eng"); EXPECT_EQ(track.duration, 2.5);
    EXPECT_FALSE(track.startTime); EXPECT_FALSE(track.frameRate); EXPECT_FALSE(track.default_);
}

TEST(MediaContainers, Mp4ClassifiesAudioSubtitlesDataAndDoesNotGuessAac) {
    const auto result = Inspect(Join({Ftyp(), Movie(Join({Track("soun", "mp4a"), Track("subt", "tx3g", {}, false, {}, 8), Track("meta", "mett", {}, false, {}, 9)}), 0)}));
    ASSERT_EQ(result.tracks.size(), 3u);
    EXPECT_EQ(result.tracks[0].type, "audio"); EXPECT_FALSE(result.tracks[0].channels); EXPECT_FALSE(result.tracks[0].sampleRate);
    EXPECT_FALSE(result.tracks[0].codecs);
    EXPECT_EQ(result.tracks[1].type, "subtitle"); EXPECT_EQ(result.tracks[2].type, "other");
}

TEST(MediaContainers, CodecStringsComeFromConfigurationRecords) {
    Bytes hevc(23);
    hevc[0] = 1; hevc[1] = 1; hevc[2] = 0x60; hevc[6] = 0xb0; hevc[12] = 93;
    const auto asc = Bytes{0x12, 0x10};
    const auto decoder = Join({Bytes{0x40, 0x15}, Bytes(11), Bytes{5, 2}, asc});
    const auto es = Join({Bytes{0, 1, 0, 4, static_cast<std::uint8_t>(decoder.size())}, decoder});
    const auto esds = Box("esds", Join({Bytes(4), Bytes{3, static_cast<std::uint8_t>(es.size())}, es}));
    const auto result = Inspect(Join({Ftyp(), Movie(Join({
        Track("vide", "hvc1", Box("hvcC", hevc)), Track("vide", "av01", Box("av1C", {0x81, 8, 0x40, 0}), false, {}, 8),
        Track("vide", "vp09", Box("vpcC", {1, 0, 0, 0, 2, 10, 0xa3, 9, 16, 9, 0, 0}), false, {}, 9),
        Track("soun", "mp4a", esds, false, {}, 10)}))}));
    ASSERT_EQ(result.tracks.size(), 4u);
    EXPECT_EQ(result.tracks[0].codecs, "hvc1.1.6.L93.B0");
    EXPECT_EQ(result.tracks[1].codecs, "av01.0.08M.10");
    EXPECT_EQ(result.tracks[2].codecs, "vp09.02.10.10.01.09.16.09.01");
    EXPECT_EQ(result.tracks[3].codecs, "mp4a.40.2");
}

TEST(MediaContainers, Mp4CoverIsOnlyAttachmentMetadata) {
    const auto cover = Box("covr", Box("data", Join({Number(13, 4), Number(0, 4), Bytes(70, 42)})));
    const auto result = Inspect(Join({Ftyp(), Movie(Box("udta", Box("meta", Join({Number(0, 4), Box("ilst", cover)}))))}));
    EXPECT_TRUE(result.tracks.empty()); ASSERT_EQ(result.attachments.size(), 1u);
    EXPECT_EQ(result.attachments[0].mimeType, "image/jpeg"); EXPECT_EQ(result.attachments[0].size, 70);
}

TEST(MediaContainers, Mp4RejectsTruncationOverflowAndChildEscapingItsParent) {
    EXPECT_THROW(Inspect(Join({Ftyp(), Number(100, 4), Text("moov")})), media::ContainerError);
    EXPECT_THROW(Inspect(Join({Ftyp(), Number(1, 4), Text("moov"), Number(UINT64_MAX, 8)})), media::ContainerError);
    EXPECT_THROW(Inspect(Join({Ftyp(), Movie(Join({Number(20, 4), Text("trak")})), Box("free", Bytes(20))})), media::ContainerError);
}

TEST(MediaContainers, Mp4RejectsExcessiveDepthAndElementCount) {
    auto nested = Box("free", {});
    for (int i = 0; i < 40; ++i) nested = Movie(nested);
    EXPECT_THROW(Inspect(Join({Ftyp(), nested})), media::ContainerError);
    Bytes boxes;
    for (int i = 0; i < 100001; ++i) Append(boxes, Box("free", {}));
    EXPECT_THROW(Inspect(Join({Ftyp(), boxes})), media::ContainerError);
}

TEST(MediaContainers, MatroskaFindsTracksAfterClusterAndUsesDeclaredDefaults) {
    const auto video = Ebml(0xae, Join({UInt(0xd7, 3), UInt(0x83, 1), Str(0x86, "V_MPEG4/ISO/AVC"),
        Ebml(0x63a2, {1, 100, 0, 40, 0xff, 0xe0, 0}), UInt(0x23e383, 40000000), Str(0x22b59c, "eng"), Str(0x22b59d, "zh-Hant"),
        Ebml(0xe0, Join({UInt(0xb0, 1920), UInt(0xba, 1080), UInt(0x54b0, 16), UInt(0x54ba, 9), UInt(0x54b2, 3),
            Ebml(0x55b0, Join({UInt(0x55b2, 10), UInt(0x55b9, 2), UInt(0x55ba, 16), UInt(0x55bb, 9)}))}))}));
    const auto sub = Ebml(0xae, Join({UInt(0xd7, 4), UInt(0x83, 17), Str(0x86, "S_TEXT/UTF8"), UInt(0x55aa, 1)}));
    const auto result = Inspect(Mkv(Join({Ebml(0x1549a966, {}), Ebml(0x1f43b675, Ebml(0xa3, Bytes(100))), Ebml(0x1654ae6b, Join({video, sub}))}), true));
    ASSERT_TRUE(result.recognized); ASSERT_EQ(result.tracks.size(), 2u);
    const auto& track = result.tracks[0];
    EXPECT_EQ(track.id, "3"); EXPECT_EQ(track.language, "zh-Hant"); EXPECT_EQ(track.default_, true); EXPECT_EQ(track.forced, false);
    EXPECT_EQ(track.codecs, "avc1.640028"); EXPECT_EQ(track.frameRate, 25); EXPECT_EQ(track.bitDepth, 10);
    EXPECT_EQ(track.colorTransfer, 16); EXPECT_EQ(track.colorPrimaries, 9); EXPECT_EQ(track.fullRange, true);
    EXPECT_NEAR(*track.displayAspectRatio, 16.0 / 9, 0.00001); EXPECT_FALSE(track.startTime);
    EXPECT_EQ(result.tracks[1].type, "subtitle"); EXPECT_EQ(result.tracks[1].forced, true);
}

TEST(MediaContainers, MatroskaReportsAudioDurationAndAttachmentWithoutReadingData) {
    const auto attachment = Ebml(0x1941a469, Ebml(0x61a7, Join({UInt(0x46ae, 99), Str(0x466e, "cover.jpg"),
        Str(0x4660, "image/jpeg"), Ebml(0x465c, Bytes(500, 0xfa))})));
    const auto audio = Ebml(0xae, Join({UInt(0xd7, 1), UInt(0x83, 2), Str(0x86, "A_AAC"),
        Ebml(0xe1, Join({Ebml(0xb5, Number(std::bit_cast<std::uint64_t>(48000.0), 8)), UInt(0x9f, 6), UInt(0x6264, 24)}))}));
    const auto result = Inspect(Mkv(Join({Ebml(0x1549a966, Ebml(0x4489, Number(std::bit_cast<std::uint64_t>(2500.0), 8))),
        Ebml(0x1654ae6b, audio), attachment})));
    EXPECT_EQ(result.duration, 2.5); ASSERT_EQ(result.tracks.size(), 1u);
    EXPECT_EQ(result.tracks[0].channels, 6); EXPECT_EQ(result.tracks[0].sampleRate, 48000); EXPECT_FALSE(result.tracks[0].codecs);
    ASSERT_EQ(result.attachments.size(), 1u); EXPECT_EQ(result.attachments[0].id, "99"); EXPECT_EQ(result.attachments[0].size, 500);
}

TEST(MediaContainers, MatroskaUnknownClusterDoesNotConsumeFollowingTracks) {
    const auto track = Ebml(0xae, Join({UInt(0xd7, 1), UInt(0x83, 2), Str(0x86, "A_OPUS")}));
    const auto result = Inspect(Mkv(Join({Ebml(0x1549a966, {}), Number(0x1f43b675, 4), {0xff}, Ebml(0xa3, Bytes(50)), Ebml(0x1654ae6b, track)}), true, "webm"));
    EXPECT_EQ(result.container, "webm"); ASSERT_EQ(result.tracks.size(), 1u); EXPECT_EQ(result.tracks[0].codecs, "opus");
}

TEST(MediaContainers, MatroskaRejectsInvalidVintEscapingElementAndIllegalUnknownSize) {
    EXPECT_THROW(Inspect(Mkv({0})), media::ContainerError);
    EXPECT_THROW(Inspect(Mkv({0xae, 0x90, 1})), media::ContainerError);
    EXPECT_THROW(Inspect(Mkv({0x16, 0x54, 0xae, 0x6b, 0xff})), media::ContainerError);
}

TEST(MediaContainers, ShortReadCannotTurnMalformedInputIntoSuccessfulMetadata) {
    const auto file = Join({Ftyp(), Movie(Track("vide", "avc1"))});
    EXPECT_THROW(media::InspectContainer(file.size(), [&](std::uint64_t offset, std::size_t size) {
        if (offset != 0 && size > 0) --size;
        return Bytes(file.begin() + static_cast<std::size_t>(offset), file.begin() + static_cast<std::size_t>(offset) + size);
    }), media::ContainerError);
}

TEST(MediaContainers, LargeMediaPayloadIsSkippedWithRandomReads) {
    const std::uint64_t mediaSize = 8ull * 1024 * 1024 * 1024;
    const auto start = Join({Ftyp(), Number(1, 4), Text("mdat"), Number(mediaSize, 8)});
    const auto tail = Movie(Track("soun", "mp4a"));
    const auto tailOffset = Ftyp().size() + mediaSize;
    std::size_t totalRead = 0;
    const auto result = media::InspectContainer(tailOffset + tail.size(), [&](std::uint64_t offset, std::size_t size) {
        totalRead += size;
        if (offset < start.size() && size <= start.size() - offset) {
            return Bytes(start.begin() + static_cast<std::size_t>(offset), start.begin() + static_cast<std::size_t>(offset) + size);
        }
        if (offset >= tailOffset && offset - tailOffset <= tail.size() && size <= tail.size() - (offset - tailOffset)) {
            const auto local = static_cast<std::size_t>(offset - tailOffset);
            return Bytes(tail.begin() + local, tail.begin() + local + size);
        }
        throw std::runtime_error("media payload must not be read");
    });
    EXPECT_TRUE(result.recognized); ASSERT_EQ(result.tracks.size(), 1u); EXPECT_LT(totalRead, 1024u);
}

TEST(MediaContainers, MatroskaAttachmentContentsAndCyclicSeekOffsetsAreNeverFollowed) {
    const auto seek = Ebml(0x114d9b74, Ebml(0x4dbb, Join({Ebml(0x53ab, Number(0x114d9b74, 4)), UInt(0x53ac, 0)})));
    const auto body = Bytes(4096, 0xfa);
    const auto file = Mkv(Join({Ebml(0x1549a966, {}), seek, Ebml(0x1941a469, Ebml(0x61a7, Join({UInt(0x46ae, 1), Str(0x466e, "cover"), Ebml(0x465c, body)})))}));
    const auto payload = std::search(file.begin(), file.end(), body.begin(), body.end());
    ASSERT_NE(payload, file.end());
    const auto payloadOffset = static_cast<std::uint64_t>(payload - file.begin());
    const auto result = media::InspectContainer(file.size(), [&](std::uint64_t offset, std::size_t size) {
        if (offset < payloadOffset + body.size() && offset + size > payloadOffset) throw std::runtime_error("attachment contents were read");
        return Bytes(file.begin() + static_cast<std::size_t>(offset), file.begin() + static_cast<std::size_t>(offset) + size);
    });
    ASSERT_EQ(result.attachments.size(), 1u); EXPECT_EQ(result.attachments[0].size, 4096);
}

TEST(MediaContainers, ReaderEnforcesPerCallAndCumulativeBudgetsBeforeCallingIo) {
    std::size_t calls = 0;
    media::detail::Reader reader(UINT64_MAX, [&](std::uint64_t, std::size_t size) { ++calls; return Bytes(size); });
    EXPECT_THROW(reader.Read(0, media::detail::Reader::kMaxRead + 1), media::ContainerError);
    EXPECT_EQ(calls, 0u);
    EXPECT_EQ(reader.Read(0, media::detail::Reader::kMaxRead).size(), media::detail::Reader::kMaxRead);
    EXPECT_EQ(reader.Read(0, media::detail::Reader::kMaxRead).size(), media::detail::Reader::kMaxRead);
    EXPECT_THROW(reader.Read(0, 1), media::ContainerError);
    EXPECT_EQ(calls, 2u);
}

TEST(MediaContainers, MalformedCodecConfigurationIsAnErrorRatherThanGuessedMetadata) {
    EXPECT_THROW(Inspect(Join({Ftyp(), Movie(Track("vide", "avc1", Box("avcC", {1, 100, 0, 40, 255, 225, 0, 9, 1})))})), media::ContainerError);
    Bytes hevc(23); hevc[0] = 1; hevc[22] = 1;
    EXPECT_THROW(Inspect(Join({Ftyp(), Movie(Track("vide", "hvc1", Box("hvcC", hevc)))})), media::ContainerError);
}

TEST(MediaContainers, MultipleSampleDescriptionsKeepOnlyCommonFields) {
    Bytes other(78);
    Put(other, 24, 640, 2); Put(other, 26, 480, 2);
    const auto result = Inspect(Join({Ftyp(), Movie(Track("vide", "avc1", {}, false, Box("avc1", other)))}));
    ASSERT_EQ(result.tracks.size(), 1u);
    EXPECT_EQ(result.tracks[0].codec, "avc1"); EXPECT_FALSE(result.tracks[0].width); EXPECT_FALSE(result.tracks[0].height);
    EXPECT_FALSE(result.tracks[0].displayAspectRatio); EXPECT_EQ(result.tracks[0].duration, 2.5);
    const auto mixed = Inspect(Join({Ftyp(), Movie(Track("vide", "avc1", {}, false, Box("hvc1", other)))}));
    EXPECT_EQ(mixed.tracks[0].codec, "unknown"); EXPECT_FALSE(mixed.tracks[0].codecs);
}

TEST(MediaContainers, MovieAndTrackDurationsAndAverageFrameRateUseTheirOwnTimescales) {
    Bytes movie(100); Put(movie, 12, 100, 4); Put(movie, 16, 1500, 4);
    Bytes header(84); Put(header, 12, 1, 4);
    Bytes mdhd(24); Put(mdhd, 12, 1000, 4); Put(mdhd, 16, 4000, 4);
    Bytes hdlr(24); const auto kind = Text("vide"); std::copy(kind.begin(), kind.end(), hdlr.begin() + 8);
    const auto table = Join({Number(0, 4), Number(2, 4), Number(60, 4), Number(50, 4), Number(25, 4), Number(40, 4)});
    const auto variable = Box("trak", Join({Box("tkhd", header), Box("mdia", Join({Box("mdhd", mdhd), Box("hdlr", hdlr), Box("minf", Box("stbl", Join({Box("stsd", Join({Number(0, 4), Number(1, 4), Box("avc1", Bytes(78))})), Box("stts", table)})))}))}));
    const auto result = Inspect(Join({Ftyp(), Box("moov", Join({Box("mvhd", movie), variable}))}));
    EXPECT_EQ(result.duration, 15); ASSERT_EQ(result.tracks.size(), 1u);
    EXPECT_EQ(result.tracks[0].duration, 4); EXPECT_EQ(result.tracks[0].frameRate, 21.25);
    const auto fragmented = Inspect(Join({Ftyp(), Movie(Join({variable, Box("mvex", {})}))}));
    EXPECT_FALSE(fragmented.tracks[0].frameRate);
}

TEST(MediaContainers, FileTypeRequiresAMediaBrandAndExcludesImageFamilies) {
    for (const auto* brand : {"avif", "heic", "mif1", "msf1", "zzzz"}) {
        EXPECT_FALSE(Inspect(Join({Ftyp(brand), Movie({})})).recognized) << brand;
    }
    const auto compatible = Box("ftyp", Join({Text("zzzz"), Number(0, 4), Text("mp42")}));
    EXPECT_TRUE(Inspect(Join({compatible, Movie({})})).recognized);
    const auto image = Box("ftyp", Join({Text("avif"), Number(0, 4), Text("isom")}));
    EXPECT_FALSE(Inspect(Join({image, Movie({})})).recognized);
    EXPECT_THROW(Inspect(Join({Ftyp("qt  "), Movie(Box("cmov", {}))})), media::ContainerError);
}

TEST(MediaContainers, FileTypeRejectsDuplicatesAndDoesNotInterpretTheMinorVersionAsABrand) {
    EXPECT_THROW(Inspect(Join({Ftyp(), Ftyp(), Movie({})})), media::ContainerError);
    EXPECT_THROW(Inspect(Join({Box("ftyp", Text("isom")), Movie({})})), media::ContainerError);
    EXPECT_THROW(Inspect(Join({Box("ftyp", Join({Text("isom"), Bytes(5)})), Movie({})})), media::ContainerError);
    EXPECT_FALSE(Inspect(Join({Box("ftyp", Join({Text("zzzz"), Text("isom")})), Movie({})})).recognized);
    const auto quickTime = Inspect(Movie(Track("soun", "sowt")));
    EXPECT_TRUE(quickTime.recognized);
    EXPECT_EQ(quickTime.container, "mov");
}

TEST(MediaContainers, AudioSampleVersionsRetainTheirHeaderSizesAndPcmFields) {
    Bytes version1(44);
    Put(version1, 8, 1, 2); Put(version1, 16, 2, 2); Put(version1, 18, 16, 2); Put(version1, 24, 44100ull << 16, 4);
    const auto first = Inspect(Join({Ftyp("qt  "), Movie(Track("soun", "sowt", {}, false, {}, 1, version1))}));
    ASSERT_EQ(first.tracks.size(), 1u);
    EXPECT_EQ(first.tracks[0].sampleRate, 44100); EXPECT_EQ(first.tracks[0].channels, 2); EXPECT_EQ(first.tracks[0].bitDepth, 16);
    Bytes version2(64);
    Put(version2, 8, 2, 2); Put(version2, 32, std::bit_cast<std::uint64_t>(96000.0), 8);
    Put(version2, 40, 6, 4); Put(version2, 48, 24, 4);
    const auto second = Inspect(Join({Ftyp("qt  "), Movie(Track("soun", "lpcm", {}, false, {}, 1, version2))}));
    EXPECT_EQ(second.tracks[0].sampleRate, 96000); EXPECT_EQ(second.tracks[0].channels, 6); EXPECT_EQ(second.tracks[0].bitDepth, 24);
    for (unsigned version : {1u, 2u}) {
        Bytes truncated(28); Put(truncated, 8, version, 2);
        EXPECT_THROW(Inspect(Join({Ftyp(), Movie(Track("soun", "sowt", {}, false, {}, 1, truncated))})), media::ContainerError);
    }
}

TEST(MediaContainers, UnknownAudioSampleVersionSkipsCodecHintsAndExtensions) {
    Bytes header(28); Put(header, 8, 3, 2);
    const auto result = Inspect(Join({Ftyp(), Movie(Track("soun", "Opus", {0}, false, {}, 1, header))}));
    ASSERT_EQ(result.tracks.size(), 1u);
    EXPECT_EQ(result.tracks[0].codec, "Opus");
    EXPECT_FALSE(result.tracks[0].codecs); EXPECT_FALSE(result.tracks[0].sampleRate); EXPECT_FALSE(result.tracks[0].channels);
}

TEST(MediaContainers, RejectsNestedTracksRatherThanExceedingTheTrackBudget) {
    Bytes nested;
    for (unsigned i = 0; i < 1024; ++i) Append(nested, Track("soun", "mp4a"));
    Bytes header(84); Put(header, 12, 1, 4);
    EXPECT_THROW(Inspect(Join({Ftyp(), Movie(Box("trak", Join({Box("tkhd", header), nested})))})), media::ContainerError);
}

TEST(MediaContainers, EmptyMatroskaElementsUseTheirDeclaredDefaults) {
    const auto track = Ebml(0xae, Join({UInt(0xd7, 1), UInt(0x83, 2), Str(0x86, "A_AAC"),
        Ebml(0x88, {}), Ebml(0x22b59c, {}), Ebml(0xe1, Join({Ebml(0xb5, {}), Ebml(0x9f, {})}))}));
    const auto file = Mkv(Join({Ebml(0x1549a966, Ebml(0x2ad7b1, {})), Ebml(0x1654ae6b, track)}));
    const auto result = Inspect(file);
    ASSERT_EQ(result.tracks.size(), 1u);
    EXPECT_EQ(result.tracks[0].default_, true);
    EXPECT_EQ(result.tracks[0].language, "eng");
    EXPECT_EQ(result.tracks[0].sampleRate, 8000);
    EXPECT_EQ(result.tracks[0].channels, 1);
}

TEST(MediaContainers, RequiredContainerAndTrackStructuresCannotBeOmitted) {
    EXPECT_THROW(Inspect(Mkv({})), media::ContainerError);
    Bytes header(84); Put(header, 12, 1, 4);
    EXPECT_THROW(Inspect(Join({Ftyp("qt  "), Movie(Box("trak", Box("tkhd", header)))})), media::ContainerError);
}

TEST(MediaContainers, ZeroTrackTypesAndIdentifiersAreMalformed) {
    const auto info = Ebml(0x1549a966, {});
    const auto zeroType = Ebml(0xae, Join({UInt(0xd7, 1), UInt(0x83, 0), Str(0x86, "X_UNKNOWN")}));
    EXPECT_THROW(Inspect(Mkv(Join({info, Ebml(0x1654ae6b, zeroType)}))), media::ContainerError);
    const auto attachment = Ebml(0x61a7, Join({UInt(0x46ae, 0), Str(0x466e, "cover"), Ebml(0x465c, {})}));
    EXPECT_THROW(Inspect(Mkv(Join({info, Ebml(0x1941a469, attachment)}))), media::ContainerError);
    auto zeroId = Track("soun", "mp4a");
    Put(zeroId, 8 + 8 + 12, 0, 4);
    EXPECT_THROW(Inspect(Join({Ftyp(), Movie(zeroId)})), media::ContainerError);
}

TEST(MediaContainers, DeferredSampleDescriptionsRetainTheirPhysicalNestingDepth) {
    Bytes nested;
    for (unsigned i = 0; i < 30; ++i) nested = Box("wave", nested);
    EXPECT_THROW(Inspect(Join({Ftyp(), Movie(Track("soun", "mp4a", nested))})), media::ContainerError);
}

TEST(MediaContainers, RecurringMatroskaInfoAllowsEquivalentTiming) {
    const auto info = Ebml(0x1549a966, Join({Str(0x4d80, "fixture"), Str(0x5741, "fixture")}));
    EXPECT_TRUE(Inspect(Mkv(Join({info, info}))).recognized);
    const auto differentScale = Ebml(0x1549a966, UInt(0x2ad7b1, 1));
    EXPECT_THROW(Inspect(Mkv(Join({info, differentScale}))), media::ContainerError);
}

TEST(MediaContainers, AacConfigurationOverridesSampleEntryRateAndChannels) {
    struct Case { Bytes config; double rate; std::int64_t channels; const char* codecs; };
    const std::vector<Case> cases{
        {Bits({{2, 5}, {0, 4}, {1, 4}, {0, 3}}), 96000, 1, "mp4a.40.2"},
        {Bits({{2, 5}, {15, 4}, {88201, 24}, {6, 4}, {0, 3}}), 88201, 6, "mp4a.40.2"},
        {Bits({{5, 5}, {6, 4}, {1, 4}, {3, 4}, {2, 5}, {0, 3}}), 48000, 1, "mp4a.40.5"},
        {Bits({{29, 5}, {6, 4}, {1, 4}, {3, 4}, {2, 5}, {0, 3}}), 48000, 2, "mp4a.40.29"},
        {Bits({{2, 5}, {6, 4}, {1, 4}, {0, 3}, {0x2b7, 11}, {5, 5}, {1, 1}, {3, 4}, {0x548, 11}, {1, 1}}), 48000, 2, "mp4a.40.2"}
    };
    for (const auto& item : cases) {
        const auto result = Inspect(Join({Ftyp(), Movie(Track("soun", "mp4a", AudioEsds(item.config)))}));
        ASSERT_EQ(result.tracks.size(), 1u);
        EXPECT_EQ(result.tracks[0].sampleRate, item.rate);
        EXPECT_EQ(result.tracks[0].channels, item.channels);
        EXPECT_EQ(result.tracks[0].codecs, item.codecs);
    }
}

TEST(MediaContainers, AacUnresolvedFieldsDoNotReuseTheSampleEntryPlaceholder) {
    const auto programConfig = Bits({{2, 5}, {3, 4}, {0, 4}, {0, 3}});
    const auto result = Inspect(Join({Ftyp(), Movie(Track("soun", "mp4a", AudioEsds(programConfig)))}));
    EXPECT_EQ(result.tracks[0].sampleRate, 48000);
    EXPECT_FALSE(result.tracks[0].channels);
    const auto reservedRate = Bits({{2, 5}, {13, 4}, {2, 4}, {0, 3}});
    const auto reserved = Inspect(Join({Ftyp(), Movie(Track("soun", "mp4a", AudioEsds(reservedRate)))}));
    EXPECT_FALSE(reserved.tracks[0].sampleRate);
    EXPECT_THROW(Inspect(Join({Ftyp(), Movie(Track("soun", "mp4a", AudioEsds({0x10})))})), media::ContainerError);
}

TEST(MediaContainers, MatroskaAacUsesTheSameAudioSpecificConfiguration) {
    const auto config = Bits({{2, 5}, {0, 4}, {1, 4}, {0, 3}});
    const auto track = Ebml(0xae, Join({UInt(0xd7, 1), UInt(0x83, 2), Str(0x86, "A_AAC"), Ebml(0x63a2, config),
        Ebml(0xe1, Join({Ebml(0xb5, Number(std::bit_cast<std::uint64_t>(48000.0), 8)), UInt(0x9f, 2)}))}));
    const auto result = Inspect(Mkv(Join({Ebml(0x1549a966, {}), Ebml(0x1654ae6b, track)})));
    EXPECT_EQ(result.tracks[0].sampleRate, 96000);
    EXPECT_EQ(result.tracks[0].channels, 1);
}

TEST(MediaContainers, RecurringMatroskaTracksAcceptEquivalentParsedMetadata) {
    const auto base = Join({UInt(0xd7, 1), UInt(0x83, 2), Str(0x86, "A_AAC")});
    const auto first = Ebml(0x1654ae6b, Ebml(0xae, base));
    const auto repeated = Ebml(0x1654ae6b, Ebml(0xae, Join({Str(0x22b59c, "eng"), UInt(0x88, 1), base})));
    const auto result = Inspect(Mkv(Join({Ebml(0x1549a966, {}), first, Ebml(0x1f43b675, {}), repeated})));
    ASSERT_EQ(result.tracks.size(), 1u);
    EXPECT_EQ(result.tracks[0].id, "1");
    const auto conflict = Ebml(0x1654ae6b, Ebml(0xae, Join({base, UInt(0x88, 0)})));
    EXPECT_THROW(Inspect(Mkv(Join({Ebml(0x1549a966, {}), first, conflict}))), media::ContainerError);
    const auto duplicate = Ebml(0x1654ae6b, Join({Ebml(0xae, base), Ebml(0xae, base)}));
    EXPECT_THROW(Inspect(Mkv(Join({Ebml(0x1549a966, {}), duplicate}))), media::ContainerError);
    const auto extra = Ebml(0xae, Join({UInt(0xd7, 2), UInt(0x83, 2), Str(0x86, "A_AAC")}));
    const auto changedSet = Ebml(0x1654ae6b, Join({Ebml(0xae, base), extra}));
    EXPECT_THROW(Inspect(Mkv(Join({Ebml(0x1549a966, {}), first, changedSet}))), media::ContainerError);
    EXPECT_THROW(Inspect(Mkv(Join({Ebml(0x1549a966, {}), changedSet, first}))), media::ContainerError);
}

TEST(MediaContainers, MovieHeadersSampleTablesAndDescriptionsAreRequired) {
    EXPECT_THROW(Inspect(Join({Ftyp(), Box("moov", {})})), media::ContainerError);
    EXPECT_THROW(Inspect(Join({Ftyp(), Box("moov", Track("soun", "mp4a"))})), media::ContainerError);
    EXPECT_THROW(Inspect(Join({Ftyp("qt  "), Box("moov", Box("rmra", {}))})), media::ContainerError);
    for (const auto* missing : {"stbl", "stsd"}) {
        auto track = Track("soun", "mp4a");
        const auto marker = Text(missing);
        const auto found = std::search(track.begin(), track.end(), marker.begin(), marker.end());
        ASSERT_NE(found, track.end());
        const auto unused = Text("free");
        std::copy(unused.begin(), unused.end(), found);
        EXPECT_THROW(Inspect(Join({Ftyp(), Movie(track)})), media::ContainerError) << missing;
    }
}

TEST(MediaContainers, Mp4TrackIdentifiersMustBeUniqueWithinTheMovie) {
    const auto track = Track("soun", "mp4a");
    EXPECT_THROW(Inspect(Join({Ftyp(), Movie(Join({track, track}))})), media::ContainerError);
}

TEST(MediaContainers, CompressedAudioDoesNotExposeUnverifiedSampleEntryFields) {
    for (const auto* codec : {"alac", "fLaC", "Opus", "ac-3", "ec-3", "mp4a"}) {
        const auto result = Inspect(Join({Ftyp(), Movie(Track("soun", codec))}));
        ASSERT_EQ(result.tracks.size(), 1u);
        EXPECT_FALSE(result.tracks[0].sampleRate) << codec;
        EXPECT_FALSE(result.tracks[0].channels) << codec;
    }
    const auto pcm = Inspect(Join({Ftyp("qt  "), Movie(Track("soun", "sowt"))}));
    EXPECT_EQ(pcm.tracks[0].sampleRate, 48000);
    EXPECT_EQ(pcm.tracks[0].channels, 2);
}

TEST(MediaContainers, QuickTimeTrackWithoutDataAllowsAnEmptySampleTable) {
    Bytes tkhd(84), mdhd(24), hdlr(24);
    Put(tkhd, 12, 1, 4); Put(mdhd, 12, 1000, 4);
    const auto handler = Text("soun");
    std::copy(handler.begin(), handler.end(), hdlr.begin() + 8);
    const auto track = Box("trak", Join({Box("tkhd", tkhd), Box("mdia", Join({
        Box("mdhd", mdhd), Box("hdlr", hdlr), Box("minf", Box("stbl", {}))}))}));
    const auto result = Inspect(Join({Ftyp("qt  "), Movie(track)}));
    ASSERT_TRUE(result.recognized);
    ASSERT_EQ(result.tracks.size(), 1u);
    EXPECT_EQ(result.tracks[0].codec, "unknown");
    EXPECT_EQ(result.tracks[0].duration, 0);
}

TEST(MediaContainers, EbmlStringsEndAtTheFirstNullWithoutChangingMp4Fourcc) {
    const auto track = Ebml(0xae, Join({UInt(0xd7, 1), UInt(0x83, 2), Ebml(0x86, Join({Text("A_AAC"), {0}, Text("ignored")})),
        Ebml(0x536e, Join({Text("Lead"), {0}, Text("ignored")}))}));
    const auto type = Join({Text("matroska"), {0}, Text("ignored")});
    const auto file = Join({Ebml(0x1a45dfa3, Ebml(0x4282, type)), Ebml(0x18538067, Join({Ebml(0x1549a966, {}), Ebml(0x1654ae6b, track)}))});
    const auto result = Inspect(file);
    ASSERT_TRUE(result.recognized); ASSERT_EQ(result.tracks.size(), 1u);
    EXPECT_EQ(result.tracks[0].codec, "A_AAC"); EXPECT_EQ(result.tracks[0].name, "Lead");
    const std::string fourcc("a\0bc", 4);
    const auto mp4 = Inspect(Join({Ftyp(), Movie(Track("meta", fourcc))}));
    EXPECT_EQ(mp4.tracks[0].codec, fourcc);
}

namespace {
Bytes Concat(const std::vector<Bytes>& parts) {
    Bytes out;
    for (const auto& part : parts) Append(out, part);
    return out;
}
Bytes TextTrackSample(const Bytes& text) { return Join({Number(text.size(), 2), text}); }
Bytes ChapterTrack(std::uint32_t id, const std::vector<std::uint32_t>& durations, const std::vector<Bytes>& samples, std::uint64_t chunk) {
    Bytes tkhd(84), mdhd(24), hdlr(24);
    Put(tkhd, 12, id, 4); Put(mdhd, 12, 1000, 4); Put(mdhd, 16, 3000, 4);
    const auto handler = Text("text");
    std::copy(handler.begin(), handler.end(), hdlr.begin() + 8);
    Bytes stts = Join({Number(0, 4), Number(durations.size(), 4)});
    for (const auto duration : durations) Append(stts, Join({Number(1, 4), Number(duration, 4)}));
    Bytes stsz = Join({Number(0, 4), Number(0, 4), Number(samples.size(), 4)});
    for (const auto& sample : samples) Append(stsz, Number(sample.size(), 4));
    const auto stbl = Join({Box("stsd", Join({Number(0, 4), Number(1, 4), Box("text", Bytes(8))})), Box("stts", stts),
        Box("stsc", Join({Number(0, 4), Number(1, 4), Number(1, 4), Number(samples.size(), 4), Number(1, 4)})),
        Box("stsz", stsz), Box("stco", Join({Number(0, 4), Number(1, 4), Number(chunk, 4)}))});
    return Box("trak", Join({Box("tkhd", tkhd), Box("mdia", Join({Box("mdhd", mdhd), Box("hdlr", hdlr), Box("minf", Box("stbl", stbl))}))}));
}
Bytes Chpl(std::initializer_list<std::pair<std::uint64_t, std::string_view>> chapters) {
    Bytes body = Join({Bytes{1, 0, 0, 0}, Number(0, 4), Bytes{static_cast<std::uint8_t>(chapters.size())}});
    for (const auto& [start, title] : chapters) Append(body, Join({Number(start, 8), Bytes{static_cast<std::uint8_t>(title.size())}, Text(title)}));
    return Box("chpl", body);
}
// ftyp、mdat（章节样本）、moov：音轨 1 经 tref/chap 引用文本轨 2，影片时长 3 秒。
Bytes ChapterMp4(const std::vector<Bytes>& samples, const std::vector<std::uint32_t>& durations, const Bytes& udta, bool reference = true,
    const Bytes& customTref = {}) {
    const auto ftyp = Ftyp();
    const auto mdat = Box("mdat", Concat(samples));
    const auto audio = Track("soun", "mp4a", {}, false, {}, 1);
    // Track() 不生成 tref；把它插在 tkhd 之后（tkhd 占 8 + 84 字节）。
    Bytes referenced(audio.begin() + 8, audio.end());
    const auto tref = customTref.empty() ? Box("tref", Box("chap", Number(2, 4))) : customTref;
    if (reference) referenced.insert(referenced.begin() + 92, tref.begin(), tref.end());
    const auto movie = Box("moov", Join({MovieHeader(1000, 3000), Box("trak", referenced),
        ChapterTrack(2, durations, samples, ftyp.size() + 8), udta}));
    return Join({ftyp, mdat, movie});
}
Bytes Atom(std::uint64_t start, std::optional<std::uint64_t> end, const Bytes& extra = {}) {
    return Ebml(0xb6, Join({UInt(0x73c4, start + 1), UInt(0x91, start * 1000000000), end ? UInt(0x92, *end * 1000000000) : Bytes{}, extra}));
}
Bytes Display(std::string_view title, const Bytes& extra = {}) { return Ebml(0x80, Join({Str(0x85, title), extra})); }
Bytes Edition(const std::vector<Bytes>& atoms) { return Ebml(0x45b9, Join({UInt(0x45bc, 1), Concat(atoms)})); }
Bytes ChapterMkv(const Bytes& chapters, std::string_view trackName = "Main") {
    const auto track = Ebml(0xae, Join({UInt(0xd7, 1), UInt(0x83, 2), Str(0x86, "A_AAC"), Str(0x536e, trackName)}));
    const auto info = Ebml(0x1549a966, Ebml(0x4489, Number(std::bit_cast<std::uint64_t>(30000.0), 8)));
    return Mkv(Join({info, Ebml(0x1654ae6b, track), chapters}));
}
}

TEST(MediaChapters, Mp4PrefersTheQuickTimeChapterTrackAndReportsItAsOther) {
    const auto result = Inspect(ChapterMp4({TextTrackSample(Text("Alpha")), TextTrackSample(Text("Beta")), TextTrackSample(Text("Gamma"))},
        {1000, 1000, 1000}, Box("udta", Chpl({{0, "NeroA"}, {15000000, "NeroB"}}))));
    ASSERT_TRUE(result.recognized); ASSERT_EQ(result.tracks.size(), 2u);
    EXPECT_EQ(result.tracks[0].type, "audio"); EXPECT_EQ(result.tracks[1].type, "other");
    ASSERT_TRUE(result.chapters.has_value()); ASSERT_EQ(result.chapters->size(), 3u);
    const auto& chapters = *result.chapters;
    EXPECT_EQ(chapters[0].start, 0); EXPECT_EQ(chapters[0].end, 1); EXPECT_EQ(chapters[0].title, "Alpha");
    EXPECT_EQ(chapters[1].start, 1); EXPECT_EQ(chapters[1].end, 2); EXPECT_EQ(chapters[1].title, "Beta");
    EXPECT_EQ(chapters[2].start, 2); EXPECT_EQ(chapters[2].end, 3); EXPECT_EQ(chapters[2].title, "Gamma");
    EXPECT_FALSE(chapters[0].edition); EXPECT_FALSE(chapters[0].subsong); EXPECT_FALSE(chapters[0].language);
}

TEST(MediaChapters, Mp4NeroChaptersEndAtTheNextStartOrTheMovieDuration) {
    const auto result = Inspect(ChapterMp4({TextTrackSample(Text("Ignored"))}, {1000},
        Box("udta", Chpl({{0, "NeroA"}, {15000000, "NeroB"}})), false));
    ASSERT_EQ(result.tracks.size(), 2u); EXPECT_EQ(result.tracks[1].type, "subtitle");
    ASSERT_TRUE(result.chapters.has_value()); ASSERT_EQ(result.chapters->size(), 2u);
    EXPECT_EQ((*result.chapters)[0].title, "NeroA"); EXPECT_EQ((*result.chapters)[0].end, 1.5);
    EXPECT_EQ((*result.chapters)[1].start, 1.5); EXPECT_EQ((*result.chapters)[1].end, 3);
}

TEST(MediaChapters, Mp4ChapterTitlesDecodeUtf16AndOmitInvalidUtf8) {
    const auto utf16 = TextTrackSample(Bytes{0xfe, 0xff, 0x7a, 0xe0});
    const auto latin1 = TextTrackSample(Bytes{'C', 0xe9});
    const auto result = Inspect(ChapterMp4({utf16, latin1}, {1000, 2000}, {}));
    ASSERT_TRUE(result.chapters.has_value()); ASSERT_EQ(result.chapters->size(), 2u);
    EXPECT_EQ((*result.chapters)[0].title, "\xe7\xab\xa0");
    EXPECT_FALSE((*result.chapters)[1].title.has_value());
    EXPECT_EQ((*result.chapters)[1].end, 3);
}

TEST(MediaChapters, MalformedChapterStructuresOnlyOmitChapters) {
    // stts 声明三个样本、stsz 只有两个：章节省略，轨道照常。
    auto mismatched = Inspect(ChapterMp4({TextTrackSample(Text("A")), TextTrackSample(Text("B"))}, {1000, 1000, 1000}, {}));
    ASSERT_TRUE(mismatched.recognized); EXPECT_EQ(mismatched.tracks.size(), 2u); EXPECT_FALSE(mismatched.chapters.has_value());
    // 标题长度越出样本。
    auto escaping = Inspect(ChapterMp4({Join({Number(50, 2), Text("A")})}, {1000}, {}));
    EXPECT_EQ(escaping.tracks.size(), 2u); EXPECT_FALSE(escaping.chapters.has_value());
    const auto linked = Inspect(ChapterMkv(Ebml(0x1043a770, Edition({Atom(0, 10, Ebml(0x6e67, Bytes(16, 1)))}))));
    EXPECT_EQ(linked.tracks.size(), 1u); EXPECT_FALSE(linked.chapters.has_value());
    EXPECT_FALSE(Inspect(ChapterMkv(Ebml(0x1043a770, Edition({Ebml(0xb6, UInt(0x73c4, 1))})))).chapters.has_value());
    EXPECT_FALSE(Inspect(ChapterMkv(Ebml(0x1043a770, Edition({Atom(10, 5)})))).chapters.has_value());
    // chap 的长度不是 4 的倍数、tref 子 box 越界：只省略章节。
    const auto samples = std::vector<Bytes>{TextTrackSample(Text("A"))};
    for (const auto& tref : {Box("tref", Box("chap", Bytes{0, 0, 2})), Box("tref", Join({Number(40, 4), Text("chap")}))}) {
        const auto result = Inspect(ChapterMp4(samples, {1000}, Box("udta", Chpl({{0, "Nero"}})), true, tref));
        ASSERT_TRUE(result.recognized); EXPECT_EQ(result.tracks.size(), 2u); EXPECT_FALSE(result.chapters.has_value());
    }
}

TEST(MediaChapters, ContainersWithoutChaptersReportAnEmptyList) {
    const auto mp4 = Inspect(Join({Ftyp(), Movie(Track("soun", "mp4a"))}));
    ASSERT_TRUE(mp4.chapters.has_value()); EXPECT_TRUE(mp4.chapters->empty());
    const auto mkv = Inspect(ChapterMkv({}));
    ASSERT_TRUE(mkv.chapters.has_value()); EXPECT_TRUE(mkv.chapters->empty());
}

TEST(MediaChapters, MatroskaListsEveryEditionsTopLevelChaptersInContainerOrder) {
    const auto first = Edition({Atom(0, 10, Display("A")), Atom(10, std::nullopt, Join({UInt(0x98, 1), Display("Hidden")})),
        Atom(20, std::nullopt, Join({Display("Parent"), Atom(20, 25, Display("Child"))}))});
    const auto second = Edition({Atom(0, 15, Display("B", Join({Str(0x437c, "chi"), Str(0x437d, "zh-Hant")})))});
    const auto result = Inspect(ChapterMkv(Ebml(0x1043a770, Join({first, second}))));
    ASSERT_TRUE(result.chapters.has_value()); ASSERT_EQ(result.chapters->size(), 4u);
    const auto& chapters = *result.chapters;
    EXPECT_EQ(chapters[0].title, "A"); EXPECT_EQ(chapters[0].language, "eng"); EXPECT_EQ(chapters[0].edition, 0);
    EXPECT_EQ(chapters[1].title, "Hidden"); EXPECT_EQ(chapters[1].end, 20);
    EXPECT_EQ(chapters[2].title, "Parent"); EXPECT_EQ(chapters[2].end, 30);
    EXPECT_EQ(chapters[3].title, "B"); EXPECT_EQ(chapters[3].language, "zh-Hant"); EXPECT_EQ(chapters[3].edition, 1);
    EXPECT_EQ(chapters[3].start, 0); EXPECT_EQ(chapters[3].end, 15);
}

TEST(MediaChapters, MatroskaSingleEditionOmitsTheEditionAndKeepsOrderedChapters) {
    const auto result = Inspect(ChapterMkv(Ebml(0x1043a770, Ebml(0x45b9, Join({UInt(0x45dd, 1),
        Atom(20, 30, Display("Late")), Atom(0, 10)})))));
    ASSERT_TRUE(result.chapters.has_value()); ASSERT_EQ(result.chapters->size(), 2u);
    EXPECT_EQ((*result.chapters)[0].start, 20); EXPECT_EQ((*result.chapters)[1].start, 0);
    EXPECT_FALSE((*result.chapters)[0].edition); EXPECT_FALSE((*result.chapters)[1].title);
}

TEST(MediaChapters, InvalidUtf8TextIsOmittedInsteadOfFailingTheResult) {
    const auto result = Inspect(ChapterMkv(Ebml(0x1043a770, Edition({Atom(0, 10, Display("\xff"))})), "\xc0\x80"));
    ASSERT_EQ(result.tracks.size(), 1u); EXPECT_FALSE(result.tracks[0].name.has_value());
    ASSERT_TRUE(result.chapters.has_value()); EXPECT_FALSE((*result.chapters)[0].title.has_value());
    EXPECT_NO_THROW(api::media::ToJson(result).dump());
}

TEST(MediaChapters, Utf8ValidationRejectsOverlongSurrogateAndOutOfRangeSequences) {
    using media::detail::IsUtf8;
    EXPECT_TRUE(IsUtf8("abc")); EXPECT_TRUE(IsUtf8("\xe7\xab\xa0")); EXPECT_TRUE(IsUtf8("\xf0\x9f\x8e\xb5"));
    EXPECT_FALSE(IsUtf8("\xc0\x80")); EXPECT_FALSE(IsUtf8("\xed\xa0\x80")); EXPECT_FALSE(IsUtf8("\xf5\x80\x80\x80"));
    EXPECT_FALSE(IsUtf8("\xe7\xab")); EXPECT_FALSE(IsUtf8("\x80"));
    EXPECT_EQ(media::detail::Utf16ToUtf8(Bytes{0xd8, 0x3c, 0xdf, 0xb5}, true), "\xf0\x9f\x8e\xb5");
    EXPECT_EQ(media::detail::Utf16ToUtf8(Bytes{0x00, 0xd8}, false), "");
}

TEST(MediaChapterSubsongs, AssignsSubsongsWhenCountAndInnerLengthsAgree) {
    auto chapters = std::vector<api::media::ContainerChapter>(3);
    for (std::size_t i = 0; i < 3; ++i) { chapters[i].start = i * 10.0; chapters[i].end = i * 10.0 + 10; }
    EXPECT_TRUE(media::AssignChapterSubsongs(chapters, {{5, 10.0}, {9, 10.004}, {11, 10.0232}}));
    EXPECT_EQ(chapters[0].subsong, 5); EXPECT_EQ(chapters[1].subsong, 9); EXPECT_EQ(chapters[2].subsong, 11);
}

TEST(MediaChapterSubsongs, LeavesSubsongsUnsetWhenAnyCheckFails) {
    auto make = [] {
        auto chapters = std::vector<api::media::ContainerChapter>(3);
        for (std::size_t i = 0; i < 3; ++i) { chapters[i].start = i * 10.0; chapters[i].end = i * 10.0 + 10; }
        return chapters;
    };
    auto chapters = make();
    EXPECT_FALSE(media::AssignChapterSubsongs(chapters, {{1, 10.0}, {2, 20.0}}));
    EXPECT_FALSE(media::AssignChapterSubsongs(chapters, {{1, 30.0}}));
    EXPECT_FALSE(media::AssignChapterSubsongs(chapters, {{1, 10.0}, {2, 10.02}, {3, 10.0}}));
    EXPECT_FALSE(media::AssignChapterSubsongs(chapters, {{1, 10.0}, {2, std::nullopt}, {3, 10.0}}));
    chapters[1].end.reset();
    EXPECT_FALSE(media::AssignChapterSubsongs(chapters, {{1, 10.0}, {2, 10.0}, {3, 10.0}}));
    for (const auto& chapter : chapters) EXPECT_FALSE(chapter.subsong.has_value());
    auto lastOpen = make();
    lastOpen[2].end.reset();
    EXPECT_TRUE(media::AssignChapterSubsongs(lastOpen, {{1, 10.0}, {2, 10.0}, {3, 12.0}}));
}

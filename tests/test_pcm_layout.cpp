// test_pcm_layout.cpp - 共享 PCM 缓冲头的编解码、数据区偏移与容量上限
// （docs/audio-pcm/SPEC.md §4.1、D9、§10 A-B1）。
#include "pch.h"
#include "../src/api/PcmLayout.h"

#include <array>
#include <cstring>
#include <limits>
#include <vector>

using namespace fb2k_pcm;

namespace {

void PutU32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
}

void PutF64(std::vector<std::uint8_t>& out, double v) {
    std::uint64_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<std::uint8_t>(bits >> (8 * i)));
}

Header SampleHeader() {
    Header h;
    h.mode = BufferMode::Ring;
    h.sampleRate = 48000;
    h.channels = 2;
    h.capacityFrames = 48000;
    h.seq = 6;
    h.writeFrames = 0xFFFFFFF6u;
    h.flags = kFlagEnded | kFlagResampled;
    h.hostTimeMs = 1758800000123.5;
    h.startSeconds = 5.25;
    h.epoch = 3;
    h.writeSlot = 47990;
    return h;
}

}  // namespace

// 编码结果逐字节等于 §4.1 表：按表的字段顺序手工拼一份小端字节串对比。
TEST(PcmLayout, EncodedHeaderMatchesTheTableByteForByte) {
    std::vector<std::uint8_t> expected;
    PutU32(expected, 0x4D435046);  // "FPCM"
    PutU32(expected, 1);
    PutU32(expected, 64);
    PutU32(expected, 2);
    PutU32(expected, 48000);
    PutU32(expected, 2);
    PutU32(expected, 48000);
    PutU32(expected, 6);
    PutU32(expected, 0xFFFFFFF6u);
    PutU32(expected, 0b101);
    PutF64(expected, 1758800000123.5);
    PutF64(expected, 5.25);
    PutU32(expected, 3);
    PutU32(expected, 47990);
    ASSERT_EQ(expected.size(), 64u);

    std::array<std::uint8_t, 64> actual{};
    EncodeHeader(SampleHeader(), actual.data());
    EXPECT_EQ(std::vector<std::uint8_t>(actual.begin(), actual.end()), expected);
    EXPECT_EQ(std::memcmp(actual.data(), "FPCM", 4), 0);
}

TEST(PcmLayout, DecodeRoundTripsEveryField) {
    std::array<std::uint8_t, 64> bytes{};
    const Header h = SampleHeader();
    EncodeHeader(h, bytes.data());
    HeaderError error = HeaderError::BadMagic;
    const auto decoded = DecodeHeader(bytes.data(), bytes.size(), &error);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(error, HeaderError::None);
    EXPECT_EQ(decoded->mode, h.mode);
    EXPECT_EQ(decoded->sampleRate, h.sampleRate);
    EXPECT_EQ(decoded->channels, h.channels);
    EXPECT_EQ(decoded->capacityFrames, h.capacityFrames);
    EXPECT_EQ(decoded->seq, h.seq);
    EXPECT_EQ(decoded->writeFrames, h.writeFrames);
    EXPECT_EQ(decoded->flags, h.flags);
    EXPECT_EQ(decoded->hostTimeMs, h.hostTimeMs);
    EXPECT_EQ(decoded->startSeconds, h.startSeconds);
    EXPECT_EQ(decoded->epoch, h.epoch);
    EXPECT_EQ(decoded->writeSlot, h.writeSlot);
}

// 读端拒绝不认识的版本：布局一改就必须升版本号。
TEST(PcmLayout, DecodeRejectsUnknownVersionsAndForeignBuffers) {
    std::array<std::uint8_t, 64> bytes{};
    EncodeHeader(SampleHeader(), bytes.data());
    HeaderError error = HeaderError::None;

    auto v2 = bytes;
    StoreU32(v2.data(), offsets::kVersion, 2);
    EXPECT_FALSE(DecodeHeader(v2.data(), v2.size(), &error).has_value());
    EXPECT_EQ(error, HeaderError::UnsupportedVersion);

    auto magic = bytes;
    magic[0] = 'X';
    EXPECT_FALSE(DecodeHeader(magic.data(), magic.size(), &error).has_value());
    EXPECT_EQ(error, HeaderError::BadMagic);

    auto size = bytes;
    StoreU32(size.data(), offsets::kHeaderBytes, 128);
    EXPECT_FALSE(DecodeHeader(size.data(), size.size(), &error).has_value());
    EXPECT_EQ(error, HeaderError::BadHeaderSize);

    auto mode = bytes;
    StoreU32(mode.data(), offsets::kMode, 3);
    EXPECT_FALSE(DecodeHeader(mode.data(), mode.size(), &error).has_value());
    EXPECT_EQ(error, HeaderError::BadMode);

    EXPECT_FALSE(DecodeHeader(bytes.data(), 63, &error).has_value());
    EXPECT_EQ(error, HeaderError::TooSmall);
}

// 平面：声道 c 的第 i 帧在 64 + (c × capacity + i) × 4；交错：槽 s 的声道 c 在 64 + (s × channels + c) × 4。
TEST(PcmLayout, SampleOffsetsMatchTheFormulas) {
    for (std::uint32_t channels : {1u, 2u, 6u}) {
        for (std::uint32_t capacity : {1u, 7u, 65536u}) {
            for (std::uint32_t c = 0; c < channels; ++c) {
                for (std::uint32_t i : {0u, capacity - 1}) {
                    const std::uint64_t planar = 64 + (static_cast<std::uint64_t>(c) * capacity + i) * 4;
                    EXPECT_EQ(PlanarSampleOffset(capacity, c, i), planar)
                        << "channels=" << channels << " capacity=" << capacity << " c=" << c << " i=" << i;
                    const std::uint64_t interleaved = 64 + (static_cast<std::uint64_t>(i) * channels + c) * 4;
                    EXPECT_EQ(InterleavedSampleOffset(channels, i, c), interleaved);
                }
            }
            // 数据区最后一个样本正好占满 BufferBytes。
            EXPECT_EQ(PlanarSampleOffset(capacity, channels - 1, capacity - 1) + 4, BufferBytes(capacity, channels));
        }
    }
}

// 距最新一帧 d 帧的槽位：d = 1 是最新一帧，d = capacity 是环里最旧的一帧，即下一次要覆盖的槽。
TEST(PcmLayout, RingSlotBackCountsFromTheNextWriteSlot) {
    EXPECT_EQ(RingSlotBack(10, 100, 1), 9u);
    EXPECT_EQ(RingSlotBack(0, 100, 1), 99u);
    EXPECT_EQ(RingSlotBack(10, 100, 100), 10u);
    EXPECT_EQ(RingSlotBack(10, 100, 11), 99u);
    EXPECT_EQ(RingSlotBack(0, 1, 1), 0u);
}

TEST(PcmLayout, CapacityForABufferSizeRoundsDown) {
    EXPECT_EQ(CapacityFramesFor(64 + 8 * 7 + 5, 2), 7u);
    EXPECT_EQ(CapacityFramesFor(64, 2), 0u);
    EXPECT_EQ(CapacityFramesFor(1000, 0), 0u);
    EXPECT_EQ(CapacityFramesFor(BufferBytes(123456, 6), 6), 123456u);
}

// 单块上限按平台：x64 256 MiB，Win32 64 MiB，都含头部；等于上限通过，多 1 字节拒绝。
TEST(PcmLayout, OneShotLimitDependsOnThePointerWidth) {
    constexpr std::uint64_t kX64 = 256ull << 20;
    constexpr std::uint64_t kWin32 = 64ull << 20;
    EXPECT_EQ(OneShotLimitBytes(8), kX64);
    EXPECT_EQ(OneShotLimitBytes(4), kWin32);
    EXPECT_TRUE(WithinOneShotLimit(kX64, 8));
    EXPECT_FALSE(WithinOneShotLimit(kX64 + 1, 8));
    EXPECT_TRUE(WithinOneShotLimit(kWin32, 4));
    EXPECT_FALSE(WithinOneShotLimit(kWin32 + 1, 4));
    EXPECT_EQ(kOneShotLimitBytes, OneShotLimitBytes(sizeof(void*)));
}

// 一次性容量 = 区间帧数向上取整再加一秒余量。
TEST(PcmLayout, OneShotCapacityAddsOneSecondOfSlack) {
    EXPECT_EQ(OneShotCapacityFrames(5.0, 7.0, 48000), 96000u + 48000u);
    EXPECT_EQ(OneShotCapacityFrames(0.0, 1.00001, 44100), 44101u + 44100u);
    EXPECT_EQ(OneShotCapacityFrames(3.0, 3.0, 8000), 8000u);
}

// 字节数按浮点算：损坏文件报出的离谱时长比较时仍是大数，不会在整数换算里回绕成小值。
TEST(PcmLayout, OneShotBufferBytesStaysOverTheLimitForAbsurdDurations) {
    EXPECT_EQ(OneShotBufferBytes(5.0, 7.0, 48000, 2), 64.0 + 144000.0 * 2 * 4);
    EXPECT_FALSE(WithinOneShotLimit(OneShotBufferBytes(0.0, 1e30, 44100, 2)));
    EXPECT_FALSE(WithinOneShotLimit(OneShotBufferBytes(0.0, std::numeric_limits<double>::infinity(), 44100, 2)));
    EXPECT_FALSE(WithinOneShotLimit(std::numeric_limits<double>::quiet_NaN()));
    EXPECT_TRUE(WithinOneShotLimit(OneShotBufferBytes(0.0, 300.0, 44100, 2)));
}

// 环形容量按 bufferSeconds 取帧数，整块超过 64 MiB 时按上限折算。
TEST(PcmLayout, RingCapacityIsCappedAtTheRingLimit) {
    EXPECT_EQ(RingCapacityFrames(1.0, 48000, 2), 48000u);
    EXPECT_EQ(RingCapacityFrames(0.5, 48000, 2), 24000u);
    EXPECT_EQ(RingCapacityFrames(10.0, 192000, 8), 1920000u);
    const std::uint32_t capped = RingCapacityFrames(10.0, 384000, 8);
    EXPECT_EQ(capped, (kRingLimitBytes - 64) / 32);
    EXPECT_LE(BufferBytes(capped, 8), kRingLimitBytes);
    EXPECT_GT(BufferBytes(capped + 1, 8), kRingLimitBytes);
}

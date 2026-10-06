// PcmLayout.h - byte layout of the shared PCM buffers handed to pages
//
/* No foobar2000 SDK or WebView2 dependency. Every shared PCM buffer starts with
 * a 64-byte little-endian header followed by float32 samples: planar in
 * one-shot buffers (audio.decodePcm), interleaved in ring buffers
 * (audio.subscribeStream). Pages parse this layout without any help from the
 * JSON channel, so changing an offset or the meaning of a field requires a new
 * kHeaderVersion; readers reject versions they do not know.
 */
#pragma once

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>

namespace fb2k_pcm {

static_assert(std::endian::native == std::endian::little, "the buffer layout is little-endian");

constexpr std::uint32_t kMagic = 0x4D435046;  // the bytes "FPCM" in memory order
constexpr std::uint32_t kHeaderVersion = 1;
constexpr std::uint32_t kHeaderBytes = 64;
constexpr std::uint32_t kBytesPerSample = 4;

enum class BufferMode : std::uint32_t {
    OneShot = 1,  // written once, planar, posted after the last sample
    Ring = 2,     // written continuously, interleaved
};

// Bits of Header::flags.
constexpr std::uint32_t kFlagEnded = 1u << 0;      // the host no longer writes
constexpr std::uint32_t kFlagTruncated = 1u << 1;  // one-shot: the source ran past the capacity
constexpr std::uint32_t kFlagResampled = 1u << 2;  // one-shot: converted to another sample rate

// Byte offsets of the header fields.
namespace offsets {
constexpr std::size_t kMagic = 0;
constexpr std::size_t kVersion = 4;
constexpr std::size_t kHeaderBytes = 8;
constexpr std::size_t kMode = 12;
constexpr std::size_t kSampleRate = 16;
constexpr std::size_t kChannels = 20;
constexpr std::size_t kCapacityFrames = 24;
constexpr std::size_t kSeq = 28;
constexpr std::size_t kWriteFrames = 32;
constexpr std::size_t kFlags = 36;
constexpr std::size_t kHostTimeMs = 40;
constexpr std::size_t kStartSeconds = 48;
constexpr std::size_t kEpoch = 56;
constexpr std::size_t kWriteSlot = 60;
}  // namespace offsets

struct Header {
    BufferMode mode = BufferMode::OneShot;
    std::uint32_t sampleRate = 0;
    std::uint32_t channels = 0;
    std::uint32_t capacityFrames = 0;
    // Seqlock counter, odd while the host writes. One-shot buffers are posted with 2.
    std::uint32_t seq = 0;
    // Frames written so far, wrapping at 2^32. One-shot buffers: the frame count.
    std::uint32_t writeFrames = 0;
    std::uint32_t flags = 0;
    // Host Unix time in milliseconds of the latest write; one-shot buffers: decode completion.
    double hostTimeMs = 0.0;
    // One-shot: track time in seconds of data frame 0. Ring: 0.
    double startSeconds = 0.0;
    // Ring: generation, starting at 1 and bumped on every format change. One-shot: 1.
    std::uint32_t epoch = 1;
    // Ring: the slot the next frame goes to. Kept apart from writeFrames because
    // writeFrames wraps at 2^32, which the capacity rarely divides. One-shot: 0.
    std::uint32_t writeSlot = 0;
};

inline std::uint32_t LoadU32(const std::uint8_t* base, std::size_t offset) {
    std::uint32_t value;
    std::memcpy(&value, base + offset, sizeof(value));
    return value;
}

inline void StoreU32(std::uint8_t* base, std::size_t offset, std::uint32_t value) {
    std::memcpy(base + offset, &value, sizeof(value));
}

inline double LoadF64(const std::uint8_t* base, std::size_t offset) {
    double value;
    std::memcpy(&value, base + offset, sizeof(value));
    return value;
}

inline void StoreF64(std::uint8_t* base, std::size_t offset, double value) {
    std::memcpy(base + offset, &value, sizeof(value));
}

// Writes the 64 header bytes at `out`.
inline void EncodeHeader(const Header& header, std::uint8_t* out) {
    StoreU32(out, offsets::kMagic, kMagic);
    StoreU32(out, offsets::kVersion, kHeaderVersion);
    StoreU32(out, offsets::kHeaderBytes, kHeaderBytes);
    StoreU32(out, offsets::kMode, static_cast<std::uint32_t>(header.mode));
    StoreU32(out, offsets::kSampleRate, header.sampleRate);
    StoreU32(out, offsets::kChannels, header.channels);
    StoreU32(out, offsets::kCapacityFrames, header.capacityFrames);
    StoreU32(out, offsets::kSeq, header.seq);
    StoreU32(out, offsets::kWriteFrames, header.writeFrames);
    StoreU32(out, offsets::kFlags, header.flags);
    StoreF64(out, offsets::kHostTimeMs, header.hostTimeMs);
    StoreF64(out, offsets::kStartSeconds, header.startSeconds);
    StoreU32(out, offsets::kEpoch, header.epoch);
    StoreU32(out, offsets::kWriteSlot, header.writeSlot);
}

enum class HeaderError { None, TooSmall, BadMagic, UnsupportedVersion, BadHeaderSize, BadMode };

// Parses the header at `data`. Returns nullopt, with `error` set when given, for
// anything that is not a version-1 PCM buffer.
inline std::optional<Header> DecodeHeader(const std::uint8_t* data, std::size_t size,
                                          HeaderError* error = nullptr) {
    auto fail = [error](HeaderError e) -> std::optional<Header> {
        if (error) *error = e;
        return std::nullopt;
    };
    if (size < kHeaderBytes) return fail(HeaderError::TooSmall);
    if (LoadU32(data, offsets::kMagic) != kMagic) return fail(HeaderError::BadMagic);
    if (LoadU32(data, offsets::kVersion) != kHeaderVersion) return fail(HeaderError::UnsupportedVersion);
    if (LoadU32(data, offsets::kHeaderBytes) != kHeaderBytes) return fail(HeaderError::BadHeaderSize);
    const std::uint32_t mode = LoadU32(data, offsets::kMode);
    if (mode != static_cast<std::uint32_t>(BufferMode::OneShot) && mode != static_cast<std::uint32_t>(BufferMode::Ring)) {
        return fail(HeaderError::BadMode);
    }
    Header header;
    header.mode = static_cast<BufferMode>(mode);
    header.sampleRate = LoadU32(data, offsets::kSampleRate);
    header.channels = LoadU32(data, offsets::kChannels);
    header.capacityFrames = LoadU32(data, offsets::kCapacityFrames);
    header.seq = LoadU32(data, offsets::kSeq);
    header.writeFrames = LoadU32(data, offsets::kWriteFrames);
    header.flags = LoadU32(data, offsets::kFlags);
    header.hostTimeMs = LoadF64(data, offsets::kHostTimeMs);
    header.startSeconds = LoadF64(data, offsets::kStartSeconds);
    header.epoch = LoadU32(data, offsets::kEpoch);
    header.writeSlot = LoadU32(data, offsets::kWriteSlot);
    if (error) *error = HeaderError::None;
    return header;
}

// Byte offset of one-shot sample (channel, frame): planar, one run of
// capacityFrames samples per channel.
constexpr std::uint64_t PlanarSampleOffset(std::uint32_t capacityFrames, std::uint32_t channel, std::uint32_t frame) {
    return kHeaderBytes + (static_cast<std::uint64_t>(channel) * capacityFrames + frame) * kBytesPerSample;
}

// Byte offset of ring sample (slot, channel): interleaved.
constexpr std::uint64_t InterleavedSampleOffset(std::uint32_t channels, std::uint32_t slot, std::uint32_t channel) {
    return kHeaderBytes + (static_cast<std::uint64_t>(slot) * channels + channel) * kBytesPerSample;
}

// Ring slot of the frame `distance` frames back from the newest one: distance 1
// is the newest frame, distance capacityFrames the oldest still in the ring.
constexpr std::uint32_t RingSlotBack(std::uint32_t writeSlot, std::uint32_t capacityFrames, std::uint32_t distance) {
    return static_cast<std::uint32_t>(
        (static_cast<std::uint64_t>(writeSlot) + capacityFrames - distance % capacityFrames) % capacityFrames);
}

constexpr std::uint64_t BufferBytes(std::uint64_t capacityFrames, std::uint32_t channels) {
    return kHeaderBytes + capacityFrames * channels * kBytesPerSample;
}

// The most frames of `channels` channels that a buffer of `bufferBytes` bytes holds.
constexpr std::uint64_t CapacityFramesFor(std::uint64_t bufferBytes, std::uint32_t channels) {
    if (channels == 0 || bufferBytes <= kHeaderBytes) return 0;
    return (bufferBytes - kHeaderBytes) / (static_cast<std::uint64_t>(channels) * kBytesPerSample);
}

// Largest one-shot buffer, header included. A 32-bit host has to find one
// contiguous range of its own address space for the view, which a long-running
// process often cannot do at 256 MiB.
constexpr std::uint64_t OneShotLimitBytes(std::size_t pointerBytes) {
    return pointerBytes >= 8 ? (256ull << 20) : (64ull << 20);
}
constexpr std::uint64_t kOneShotLimitBytes = OneShotLimitBytes(sizeof(void*));

// Takes a double so that OneShotBufferBytes can be checked before it is converted; NaN fails.
constexpr bool WithinOneShotLimit(double bufferBytes, std::size_t pointerBytes = sizeof(void*)) {
    return bufferBytes <= static_cast<double>(OneShotLimitBytes(pointerBytes));
}

// Largest ring buffer, header included.
constexpr std::uint64_t kRingLimitBytes = 64ull << 20;

// Frames reserved for [startSeconds, endSeconds) at `rate`: the range rounded up
// plus one second, because track durations are estimates. Kept in floating point
// so that an absurd duration from a damaged file stays a huge number instead of
// overflowing an integer conversion.
inline double OneShotCapacityFrames(double startSeconds, double endSeconds, std::uint32_t rate) {
    const double span = std::max(0.0, endSeconds - startSeconds);
    return std::ceil(span * rate) + rate;
}

// Bytes of the one-shot buffer for that range, header included, also in floating
// point: check it with WithinOneShotLimit before converting it to an integer.
inline double OneShotBufferBytes(double startSeconds, double endSeconds, std::uint32_t rate, std::uint32_t channels) {
    return kHeaderBytes + OneShotCapacityFrames(startSeconds, endSeconds, rate) * channels * kBytesPerSample;
}

// Frames for `bufferSeconds` of audio, reduced so that the whole buffer stays
// within kRingLimitBytes. Never 0 for a valid format.
inline std::uint32_t RingCapacityFrames(double bufferSeconds, std::uint32_t sampleRate, std::uint32_t channels) {
    const std::uint64_t wanted = static_cast<std::uint64_t>(std::ceil(std::max(0.0, bufferSeconds) * sampleRate));
    const std::uint64_t limit = CapacityFramesFor(kRingLimitBytes, channels);
    return static_cast<std::uint32_t>(std::max<std::uint64_t>(1, std::min(wanted, limit)));
}

}  // namespace fb2k_pcm

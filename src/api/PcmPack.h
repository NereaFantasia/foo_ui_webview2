// PcmPack.h - turning decoded interleaved audio into the float32 layouts of PcmLayout.h
//
/* No foobar2000 SDK or WebView2 dependency. The sample type is a template
 * parameter because foobar2000's audio_sample is double in 64-bit builds and
 * float in 32-bit ones; PcmPack.cpp instantiates both. The decode loop owns
 * the decoder and the resampler and feeds chunks through RangeClipper and
 * PlanarPacker. Nothing here allocates.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace fb2k_pcm {

// Converts `count` samples to float32, keeping their order.
template <class Sample>
void ConvertSamples(const Sample* src, std::size_t count, float* dst);

// Tracks which decoded frames fall inside the requested range. Positions count
// source frames from the start of the track.
class RangeClipper {
public:
    // `firstFrame` is the position of the first frame the decoder delivers:
    // startFrame after a seek, 0 when decoding from the top. endFrame nullopt
    // means "to the end of the track".
    RangeClipper(std::uint64_t startFrame, std::optional<std::uint64_t> endFrame, std::uint64_t firstFrame);

    struct Span {
        std::uint64_t skip = 0;  // leading frames of the chunk that lie before the range
        std::uint64_t take = 0;  // frames right after the skipped ones that lie inside the range
        bool done = false;       // the range ends within this chunk; later chunks are not needed
    };
    // Classifies the next decoded chunk of `chunkFrames` frames and advances the position.
    Span Next(std::uint64_t chunkFrames);

    std::uint64_t position() const { return position_; }

private:
    std::uint64_t start_;
    std::optional<std::uint64_t> end_;
    std::uint64_t position_;
};

// Accumulates interleaved frames into a planar float32 data region of fixed
// capacity, optionally averaging all channels into one.
template <class Sample>
class PlanarPacker {
public:
    // `data` is the data region right after the header, sized for
    // capacityFrames x outputChannels() floats. outputChannels() is 1 when
    // `mono` is set and inputChannels otherwise.
    PlanarPacker(float* data, std::uint32_t capacityFrames, std::uint32_t inputChannels, bool mono);

    // Appends `frames` interleaved frames of inputChannels channels. Frames that
    // do not fit are dropped and mark the result truncated; from then on it
    // returns false and the caller stops decoding. A region filled exactly is
    // not truncated until another frame arrives.
    bool Append(const Sample* interleaved, std::uint64_t frames);

    std::uint32_t frames() const { return written_; }
    bool truncated() const { return truncated_; }
    std::uint32_t outputChannels() const { return mono_ ? 1 : inputChannels_; }

private:
    float* data_;
    std::uint32_t capacity_;
    std::uint32_t inputChannels_;
    bool mono_;
    std::uint32_t written_ = 0;
    bool truncated_ = false;
};

}  // namespace fb2k_pcm

#include "pch.h"
#include "api/PcmPack.h"

#include <algorithm>

namespace fb2k_pcm {

template <class Sample>
void ConvertSamples(const Sample* src, std::size_t count, float* dst) {
    for (std::size_t i = 0; i < count; ++i) dst[i] = static_cast<float>(src[i]);
}

RangeClipper::RangeClipper(std::uint64_t startFrame, std::optional<std::uint64_t> endFrame, std::uint64_t firstFrame)
    : start_(startFrame), end_(endFrame), position_(firstFrame) {}

RangeClipper::Span RangeClipper::Next(std::uint64_t chunkFrames) {
    Span span;
    const std::uint64_t chunkStart = position_;
    const std::uint64_t chunkEnd = position_ + chunkFrames;
    position_ = chunkEnd;

    const std::uint64_t from = std::max(chunkStart, start_);
    const std::uint64_t to = end_ ? std::min(chunkEnd, *end_) : chunkEnd;
    if (from < to) {
        span.skip = from - chunkStart;
        span.take = to - from;
    } else {
        // Nothing of this chunk is inside the range: all of it lies before the
        // start, or the range already ended.
        span.skip = chunkFrames;
    }
    span.done = end_.has_value() && chunkEnd >= *end_;
    return span;
}

template <class Sample>
PlanarPacker<Sample>::PlanarPacker(float* data, std::uint32_t capacityFrames, std::uint32_t inputChannels, bool mono)
    : data_(data), capacity_(capacityFrames), inputChannels_(inputChannels), mono_(mono) {}

template <class Sample>
bool PlanarPacker<Sample>::Append(const Sample* interleaved, std::uint64_t frames) {
    const std::uint32_t room = capacity_ - written_;
    if (frames > room) truncated_ = true;
    const auto count = static_cast<std::uint32_t>(std::min<std::uint64_t>(frames, room));
    const std::uint32_t channels = inputChannels_;
    if (mono_) {
        float* out = data_ + written_;
        const double scale = channels > 0 ? 1.0 / channels : 0.0;
        for (std::uint32_t i = 0; i < count; ++i) {
            const Sample* frame = interleaved + static_cast<std::size_t>(i) * channels;
            double sum = 0.0;
            for (std::uint32_t c = 0; c < channels; ++c) sum += frame[c];
            out[i] = static_cast<float>(sum * scale);
        }
    } else {
        for (std::uint32_t c = 0; c < channels; ++c) {
            float* out = data_ + static_cast<std::size_t>(c) * capacity_ + written_;
            for (std::uint32_t i = 0; i < count; ++i) {
                out[i] = static_cast<float>(interleaved[static_cast<std::size_t>(i) * channels + c]);
            }
        }
    }
    written_ += count;
    return !truncated_;
}

template void ConvertSamples<float>(const float*, std::size_t, float*);
template void ConvertSamples<double>(const double*, std::size_t, float*);
template class PlanarPacker<float>;
template class PlanarPacker<double>;

}  // namespace fb2k_pcm

#include "pch.h"
#include "api/WaveformAccumulator.h"

#include <algorithm>
#include <cmath>

namespace fb2k_waveform {

template <typename Sample>
Accumulator<Sample>::Accumulator(int resolution, uint64_t samplesPerWindow)
    : resolution_(std::max(0, resolution)),
      samplesPerWindow_(samplesPerWindow == 0 ? 1 : samplesPerWindow) {
    const auto count = static_cast<size_t>(resolution_);
    out_.rms.assign(count, 0.0f);
    out_.peak.assign(count, 0.0f);
    out_.signedPeak.assign(count, 0.0f);
}

template <typename Sample>
void Accumulator<Sample>::Feed(const Sample* data, size_t frames, unsigned channels) {
    if (channels == 0) {
        processed_ += frames;
        return;
    }
    const uint64_t limit = static_cast<uint64_t>(resolution_) * samplesPerWindow_;
    for (size_t i = 0; i < frames; ++i) {
        const uint64_t global = processed_ + i;
        if (global >= limit) break;
        const int window = static_cast<int>(global / samplesPerWindow_);

        // Per-frame channel reductions, in float like the single-method loop.
        float sum = 0.0f;
        float maxAbs = 0.0f;
        float sumSq = 0.0f;
        for (unsigned ch = 0; ch < channels; ++ch) {
            const float s = static_cast<float>(data[i * channels + ch]);
            sum += s;
            maxAbs = std::max(maxAbs, std::abs(s));
            sumSq += s * s;
        }
        const float mean = sum / channels;
        const float meanSq = sumSq / channels;

        if (windowOpen_ && window != currentWindow_) {
            CloseWindow();
        }
        if (!windowOpen_) {
            currentWindow_ = window;
            windowOpen_ = true;
            rmsSum_ = 0.0f;
            rmsCount_ = 0;
            peakMax_ = 0.0f;
            signedBest_ = 0.0f;
        }
        rmsSum_ += meanSq;
        ++rmsCount_;
        peakMax_ = std::max(peakMax_, maxAbs);
        if (std::abs(mean) > std::abs(signedBest_)) signedBest_ = mean;
    }
    processed_ += frames;
}

template <typename Sample>
void Accumulator<Sample>::CloseWindow() {
    if (!windowOpen_) return;
    if (currentWindow_ >= 0 && currentWindow_ < resolution_) {
        const auto w = static_cast<size_t>(currentWindow_);
        out_.rms[w] = std::sqrt(rmsSum_ / static_cast<float>(rmsCount_));
        out_.peak[w] = peakMax_;
        out_.signedPeak[w] = signedBest_;
    }
    windowOpen_ = false;
}

template <typename Sample>
RawWindows Accumulator<Sample>::Finish() {
    CloseWindow();
    return std::move(out_);
}

template class Accumulator<float>;
template class Accumulator<double>;

Rendered Render(const RawWindows& raw, const RenderOptions& options) {
    Rendered out;
    if (options.signedOutput) {
        out.waveform = raw.signedPeak;
        float absMax = 0.0f;
        for (float v : out.waveform) absMax = std::max(absMax, std::abs(v));
        out.maxAmplitude = absMax;
        if (absMax > 0.0f) {
            for (float& v : out.waveform) v /= absMax;
        }
        return out;
    }

    out.waveform = options.method == Method::Peak ? raw.peak : raw.rms;
    float maxValue = 0.0f;
    for (float v : out.waveform) maxValue = std::max(maxValue, v);
    out.maxAmplitude = maxValue;
    if (maxValue > 0.0f) {
        for (float& v : out.waveform) v /= maxValue;
    }

    if (options.scale == Scale::Db) {
        const float dbFloor = -60.0f;
        for (float& v : out.waveform) {
            if (v <= 0.0f) { v = 0.0f; continue; }
            const float db = 20.0f * std::log10f(v);
            v = std::max(0.0f, (db - dbFloor) / (-dbFloor));
        }
    }
    return out;
}

}  // namespace fb2k_waveform

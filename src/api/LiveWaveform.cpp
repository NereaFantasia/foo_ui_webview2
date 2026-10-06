#include "pch.h"
#include "api/LiveWaveform.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace fb2k_waveform {

namespace {

// 单个线性值 → 应答值。表达式与类型照搬原 GetWaveform 循环，'mix' 的逐位一致靠它。
float ToResponseValue(float linear, bool signedOutput) {
    if (signedOutput) {
        return std::max(-1.0f, std::min(1.0f, linear));
    }
    float db = 20.0f * log10f(std::max(fabsf(linear), 1e-10f));
    float normalized = (db + 70.0f) / 70.0f;
    return std::max(0.0f, std::min(1.0f, normalized));
}

}  // namespace

template <typename Sample>
void MixWindow(const Sample* data, size_t frames, unsigned channels, bool signedOutput,
               std::vector<float>& out) {
    if (channels == 0) {
        out.clear();
        return;
    }
    out.resize(frames);
    for (size_t i = 0; i < frames; ++i) {
        // 先逐声道收窄成 float 再求和，顺序与原循环相同
        float sum = 0;
        for (unsigned ch = 0; ch < channels; ++ch) {
            sum += static_cast<float>(data[i * channels + ch]);
        }
        out[i] = ToResponseValue(sum / channels, signedOutput);
    }
}

template <typename Sample>
void SplitStereoWindow(const Sample* data, size_t frames, unsigned channels, bool signedOutput,
                       std::vector<float>& left, std::vector<float>& right) {
    if (channels == 0) {
        left.clear();
        right.clear();
        return;
    }
    left.resize(frames);
    for (size_t i = 0; i < frames; ++i) {
        left[i] = ToResponseValue(static_cast<float>(data[i * channels]), signedOutput);
    }
    if (channels == 1) {
        right = left;
        return;
    }
    right.resize(frames);
    for (size_t i = 0; i < frames; ++i) {
        right[i] = ToResponseValue(static_cast<float>(data[i * channels + 1]), signedOutput);
    }
}

void PickEvenly(std::vector<float>& values, size_t points) {
    const size_t n = values.size();
    if (points == 0 || n <= points) return;
    // 下标不小于 i，原地前移不会读到已覆盖的位置。乘积按 64 位算：Win32 的 size_t
    // 只有 32 位，65536 点乘上一秒 192 kHz 双声道的样本数就会溢出。
    for (size_t i = 0; i < points; ++i) {
        values[i] = values[static_cast<size_t>(static_cast<uint64_t>(i) * n / points)];
    }
    values.resize(points);
}

template void MixWindow<float>(const float*, size_t, unsigned, bool, std::vector<float>&);
template void MixWindow<double>(const double*, size_t, unsigned, bool, std::vector<float>&);
template void SplitStereoWindow<float>(const float*, size_t, unsigned, bool, std::vector<float>&,
                                       std::vector<float>&);
template void SplitStereoWindow<double>(const double*, size_t, unsigned, bool, std::vector<float>&,
                                        std::vector<float>&);

}  // namespace fb2k_waveform

#include "pch.h"
#include "api/SpectrumBands.h"

#include <algorithm>
#include <cmath>

namespace fb2k_spectrum {

// 数值流水线从 SpectrumState::GetSpectrum 原样搬出，逐表达式保持 float 精度与运算顺序：
// 快照单测按逐位相同守着它，改任何一步都会让已发布的 'weighted' 输出变样。
template <typename Sample>
void ComputeWeightedBands(const Sample* data, size_t binCount, unsigned channels,
                          unsigned sampleRate, int fftSize, int bands,
                          std::vector<float>& out, double minFrequency, double maxFrequency) {
    const size_t count = binCount;
    if (sampleRate == 0) sampleRate = 44100;

    int numBands = bands;
    if (numBands <= 0 || numBands > (int)count) numBands = (int)count;

    out.resize(numBands);

    // --- 处理流水线 ---
    // 1. 对数频率映射 (log-spaced bands)
    // 2. Sub-bin 插值 + 三角滤波器 RMS
    // 3. 频率倾斜补偿 (乘法域)
    // 4. dB 归一化 + gamma

    // 端点按 float 算，与分带循环同精度：缺省下限 20.0 收窄成 20.0f 无损，上限是 sampleRate / 2.0f
    float minFreq = static_cast<float>(minFrequency);
    float maxFreq = (float)sampleRate / 2.0f;
    if (maxFrequency > 0.0 && static_cast<float>(maxFrequency) < maxFreq) {
        maxFreq = static_cast<float>(maxFrequency);
    }
    if (!(maxFreq > minFreq)) {
        std::fill(out.begin(), out.end(), 0.0f);
        return;
    }
    float binWidth = (float)sampleRate / (float)fftSize;
    float logMin = log10f(minFreq);
    float logMax = log10f(maxFreq);

    // 辅助: 对 FFT bin 做线性插值, 支持小数位置
    // 让共享相邻 bin 的频段有不同的插值结果
    auto interpBin = [&](float binPos) -> float {
        binPos = std::max(0.0f, std::min(binPos, (float)(count - 1)));
        int b0 = (int)floorf(binPos);
        int b1 = std::min(b0 + 1, (int)(count - 1));
        float frac = binPos - (float)b0;
        float v0 = 0, v1 = 0;
        for (unsigned ch = 0; ch < channels; ++ch) {
            v0 += static_cast<float>(data[b0 * channels + ch]);
            v1 += static_cast<float>(data[b1 * channels + ch]);
        }
        v0 /= channels; v1 /= channels;
        return v0 + (v1 - v0) * frac;
    };

    // dB 归一化参数
    constexpr float dbFloor = -50.0f;
    constexpr float dbCeil  =   0.0f;
    constexpr float dbRange = dbCeil - dbFloor;

    // 频率倾斜补偿: 只提升中高频, 不衰减低频
    constexpr float slopeAmount = 1.5f;
    constexpr float slopeOffset = 20.0f;
    constexpr float slopeExp = slopeAmount / 6.0f;

    // Bass shelf: 衰减 200Hz 以下频段，避免 bass 能量饱和导致视觉粘连
    // 20Hz→0.55, 100Hz→~0.78, 200Hz→1.0  (smoothstep S 曲线)
    constexpr float bassShelfFloor = 0.55f;   // 20Hz 处衰减保留比
    constexpr float bassShelfFreq  = 200.0f;  // shelf 拐点频率

    constexpr float gamma = 0.8f;

    // 宽频带三角滤波器 RMS 辅助（消除 4-5 层嵌套）
    auto computeWideBandRMS = [&](float fMinBin, float fMidBin, float fMaxBin) -> float {
        int loIdx = std::max(1, (int)floorf(fMinBin));
        int hiIdx = std::min((int)count, (int)ceilf(fMaxBin));
        // 先收敛到合法范围，再保证 hiIdx > loIdx
        if (loIdx >= (int)count) loIdx = (int)count - 1;
        if (hiIdx > (int)count) hiIdx = (int)count;
        if (hiIdx <= loIdx) hiIdx = loIdx + 1;
        if (hiIdx > (int)count) hiIdx = (int)count;

        float sumSq = 0.0f;
        for (int i = loIdx; i < hiIdx; ++i) {
            float fi = (float)i;
            float weight = (fi <= fMidBin)
                ? ((fMidBin > fMinBin) ? (fi - fMinBin) / (fMidBin - fMinBin) : 1.0f)
                : ((fMaxBin > fMidBin) ? (fMaxBin - fi) / (fMaxBin - fMidBin) : 1.0f);
            weight = std::max(weight, 0.05f);

            float mag = 0.0f;
            for (unsigned ch = 0; ch < channels; ++ch)
                mag += static_cast<float>(data[i * channels + ch]);
            mag /= channels;
            float wm = mag * weight;
            sumSq += wm * wm;
        }
        return sqrtf(sumSq);
    };

    for (int b = 0; b < numBands; ++b) {
        float f0 = powf(10.0f, logMin + (logMax - logMin) * b / numBands);
        float f1 = powf(10.0f, logMin + (logMax - logMin) * (b + 1) / numBands);
        float fCenter = sqrtf(f0 * f1);

        float fMinBin = f0 / binWidth;
        float fMidBin = fCenter / binWidth;
        float fMaxBin = f1 / binWidth;

        float bandWidthBins = fMaxBin - fMinBin;
        float rmsVal = (bandWidthBins < 2.0f)
            ? interpBin(fMidBin)
            : computeWideBandRMS(fMinBin, fMidBin, fMaxBin);

        // 频率倾斜补偿
        float tilt = powf(fCenter / slopeOffset, slopeExp);

        // Bass shelf 衰减: smoothstep 从 20Hz (0.55) 平滑过渡到 200Hz (1.0)
        float bassShelf = 1.0f;
        if (fCenter < bassShelfFreq) {
            float t = (fCenter - 20.0f) / (bassShelfFreq - 20.0f);
            t = std::max(0.0f, std::min(1.0f, t));
            bassShelf = bassShelfFloor + (1.0f - bassShelfFloor) * t * t * (3.0f - 2.0f * t);
        }
        rmsVal *= tilt * bassShelf;

        // dB + 归一化 + gamma
        float db = 20.0f * log10f(std::max(rmsVal, 1e-10f));
        float normalized = (db - dbFloor) / dbRange;
        normalized = std::max(0.0f, std::min(1.0f, normalized));
        out[b] = powf(normalized, gamma);
    }
}

template void ComputeWeightedBands<float>(const float*, size_t, unsigned, unsigned, int, int,
                                          std::vector<float>&, double, double);
template void ComputeWeightedBands<double>(const double*, size_t, unsigned, unsigned, int, int,
                                           std::vector<float>&, double, double);

// 'db' 档不受 'weighted' 的逐位约束，全程用 double：频带边界、频点功率与求和都在
// 双精度下算，只在写出时转 float。
template <typename Sample>
void ComputeDbBands(const Sample* data, size_t binCount, unsigned channels,
                    unsigned sampleRate, int fftSize, int bands,
                    std::vector<float>& out, double minFrequency, double maxFrequency) {
    if (sampleRate == 0) sampleRate = 44100;

    int numBands = bands;
    if (numBands <= 0 || numBands > (int)binCount) numBands = (int)binCount;
    out.assign(static_cast<size_t>(std::max(0, numBands)), kDbBandsFloor);

    // 频点 1 到 fftSize/2 - 1；少于两个频点时没有可用的非直流频点
    const size_t binEnd = std::min(binCount, static_cast<size_t>(std::max(0, fftSize / 2)));
    const double minFreq = minFrequency;
    const double nyquist = sampleRate / 2.0;
    const double maxFreq = (maxFrequency > 0.0 && maxFrequency < nyquist) ? maxFrequency : nyquist;
    if (numBands <= 0 || channels == 0 || binEnd < 2 || !(maxFreq > minFreq)) return;

    const double binWidth = static_cast<double>(sampleRate) / fftSize;
    const double logMin = std::log10(minFreq);
    const double logSpan = std::log10(maxFreq) - logMin;
    std::vector<double> edges(static_cast<size_t>(numBands) + 1);
    for (int b = 0; b <= numBands; ++b) {
        edges[b] = std::pow(10.0, logMin + logSpan * b / numBands);
    }

    auto binPower = [&](size_t k) {
        double sum = 0.0;
        for (unsigned ch = 0; ch < channels; ++ch) {
            const double m = static_cast<double>(data[k * channels + ch]);
            sum += m * m;
        }
        return sum / channels;
    };

    // 频点与频带都按频率升序，一趟归入：每个频点只进它中心频率所在的那一带
    std::vector<double> power(static_cast<size_t>(numBands), 0.0);
    std::vector<char> hasBin(static_cast<size_t>(numBands), 0);
    int band = 0;
    for (size_t k = 1; k < binEnd; ++k) {
        const double freq = static_cast<double>(k) * binWidth;
        if (freq < minFreq) continue;
        // 缺省上限是 Nyquist，最高频点也低于它，这一条只在调用方收窄上限时生效
        if (freq >= maxFreq) break;
        while (band + 1 < numBands && freq >= edges[band + 1]) ++band;
        power[band] += binPower(k);
        hasBin[band] = 1;
    }

    for (int b = 0; b < numBands; ++b) {
        double p = power[b];
        if (!hasBin[b]) {
            // 空带：在几何中心处对左右相邻频点的功率线性插值，左邻是直流时换成频点 1
            const double pos = std::sqrt(edges[b] * edges[b + 1]) / binWidth;
            const double leftPos = std::floor(pos);
            const double frac = pos - leftPos;
            const size_t last = binEnd - 1;
            const size_t left = std::min(last, std::max<size_t>(1, static_cast<size_t>(leftPos)));
            const size_t right = std::min(last, std::max<size_t>(1, static_cast<size_t>(leftPos) + 1));
            const double pl = binPower(left);
            const double pr = binPower(right);
            p = pl + (pr - pl) * frac;
        }
        if (p > 0.0) {
            const double db = 10.0 * std::log10(p) + kDbBandsCalibration;
            out[b] = static_cast<float>(std::max(db, static_cast<double>(kDbBandsFloor)));
        }
    }
}

template void ComputeDbBands<float>(const float*, size_t, unsigned, unsigned, int, int,
                                    std::vector<float>&, double, double);
template void ComputeDbBands<double>(const double*, size_t, unsigned, unsigned, int, int,
                                     std::vector<float>&, double, double);

// 选点规则逐项照搬 ComputeDbBands：同一个频率公式、同样的左闭右开比较，
// bins 与 'db' 档对同一个频点的归属才不会因浮点边界而不同。
DbBinSpan FindDbBinSpan(size_t binCount, unsigned sampleRate, int fftSize,
                        double minFrequency, double maxFrequency) {
    if (sampleRate == 0) sampleRate = 44100;

    DbBinSpan span;
    const size_t binEnd = std::min(binCount, static_cast<size_t>(std::max(0, fftSize / 2)));
    const double nyquist = sampleRate / 2.0;
    const double maxFreq = (maxFrequency > 0.0 && maxFrequency < nyquist) ? maxFrequency : nyquist;
    if (fftSize <= 0 || binEnd < 2 || !(maxFreq > minFrequency)) return span;

    const double binWidth = static_cast<double>(sampleRate) / fftSize;
    for (size_t k = 1; k < binEnd; ++k) {
        const double freq = static_cast<double>(k) * binWidth;
        if (freq < minFrequency) continue;
        if (freq >= maxFreq) break;
        if (span.count == 0) span.first = k;
        ++span.count;
    }
    return span;
}

namespace {

// 功率换成 dB：先加标定、压到下限，再按 kDbBinsStepsPerDb 取整。取整用除法：
// round(x * 100) / 100 得到的是离两位小数最近的 double，JSON 写出来就是两位小数；
// 乘 0.01 得不到这个保证。
double DbBinValue(double power) {
    if (!(power > 0.0)) return static_cast<double>(kDbBandsFloor);
    const double db = std::max(10.0 * std::log10(power) + kDbBandsCalibration,
                               static_cast<double>(kDbBandsFloor));
    return std::round(db * kDbBinsStepsPerDb) / kDbBinsStepsPerDb;
}

}  // namespace

template <typename Sample>
void ComputeDbBins(const Sample* data, size_t binCount, unsigned channels,
                   unsigned sampleRate, int fftSize, SpectrumChannels mode, DbBins& out,
                   double minFrequency, double maxFrequency) {
    out.firstBin = 0;
    out.mix.clear();
    out.left.clear();
    out.right.clear();
    if (channels == 0 || data == nullptr) return;

    const DbBinSpan span = FindDbBinSpan(binCount, sampleRate, fftSize, minFrequency, maxFrequency);
    if (span.count == 0) return;
    out.firstBin = span.first;
    const size_t end = span.first + span.count;

    if (mode == SpectrumChannels::Mix) {
        out.mix.reserve(span.count);
        for (size_t k = span.first; k < end; ++k) {
            double sum = 0.0;
            for (unsigned ch = 0; ch < channels; ++ch) {
                const double m = static_cast<double>(data[k * channels + ch]);
                sum += m * m;
            }
            out.mix.push_back(DbBinValue(sum / channels));
        }
        return;
    }

    // 单声道时右路读同一路，与 getWaveform 的 'stereo' 一致
    const unsigned rightChannel = channels >= 2 ? 1 : 0;
    out.left.reserve(span.count);
    out.right.reserve(span.count);
    for (size_t k = span.first; k < end; ++k) {
        const double l = static_cast<double>(data[k * channels]);
        const double r = static_cast<double>(data[k * channels + rightChannel]);
        out.left.push_back(DbBinValue(l * l));
        out.right.push_back(DbBinValue(r * r));
    }
}

template void ComputeDbBins<float>(const float*, size_t, unsigned, unsigned, int, SpectrumChannels,
                                   DbBins&, double, double);
template void ComputeDbBins<double>(const double*, size_t, unsigned, unsigned, int, SpectrumChannels,
                                    DbBins&, double, double);

}  // namespace fb2k_spectrum

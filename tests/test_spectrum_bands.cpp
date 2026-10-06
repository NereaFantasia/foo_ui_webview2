// test_spectrum_bands.cpp - 频谱数组到对数频带的纯值计算
// （docs/audio-visualization/SPEC.md §4.2 / §5.1 / §10 A1.6）。
//
// 取频谱那一步（visualisation_stream::get_spectrum_absolute）要 SDK，本测试工程不链接 SDK；
// 这里只测喂进幅度数组之后的分带、加权与映射。快照一节按逐位相同锁住 'weighted' 档，
// 它是已发布的输出，改动须同时改快照并在 changelog 里说明。'db' 档另起一组，
// 按功率求和的定义逐条核对：频点归属、声道取均方、空带插值与下限。
#include "pch.h"
#include "../src/api/SpectrumBands.h"

#include <algorithm>
#include <cmath>
#include <cstring>

using fb2k_spectrum::ComputeDbBands;
using fb2k_spectrum::ComputeWeightedBands;
using fb2k_spectrum::kDbBandsCalibration;
using fb2k_spectrum::kDbBandsFloor;

namespace {

// 构造确定的幅度数组：平滑下降的谱加上几处尖峰，交错存放 channels 路。
std::vector<double> MakeMagnitudes(size_t bins, unsigned channels) {
    std::vector<double> data(bins * channels);
    for (size_t i = 0; i < bins; ++i) {
        for (unsigned ch = 0; ch < channels; ++ch) {
            double v = 0.4 / (1.0 + static_cast<double>(i) * 0.02);
            v += 0.05 * std::sin(static_cast<double>(i) * 0.37 + ch);
            if (i % 97 == 0) v += 0.3;
            data[i * channels + ch] = std::fabs(v);
        }
    }
    return data;
}

// FNV-1a 64 位，按 float 的位模式求：任何一位变化都会改哈希。
uint64_t HashFloatBits(const std::vector<float>& values) {
    uint64_t h = 1469598103934665603ULL;
    for (float f : values) {
        uint32_t bits = 0;
        std::memcpy(&bits, &f, sizeof bits);
        for (int k = 0; k < 4; ++k) {
            h ^= static_cast<uint8_t>(bits >> (k * 8));
            h *= 1099511628211ULL;
        }
    }
    return h;
}

}  // namespace

// 采样率 0 按 44100 处理，与宿主取不到采样率时的回退一致。
TEST(SpectrumBands, ZeroSampleRateFallsBackTo44100) {
    const auto data = MakeMagnitudes(4096, 2);
    std::vector<float> a, b;
    ComputeWeightedBands(data.data(), 4096, 2, 0, 8192, 96, a);
    ComputeWeightedBands(data.data(), 4096, 2, 44100, 8192, 96, b);
    EXPECT_EQ(a, b);
}

// 频带数不大于 0 或超过频点数时，输出频点数个频带。
TEST(SpectrumBands, BandCountClampsToBinCount) {
    const auto data = MakeMagnitudes(128, 1);
    std::vector<float> out;
    ComputeWeightedBands(data.data(), 128, 1, 44100, 256, 0, out);
    EXPECT_EQ(out.size(), 128u);
    ComputeWeightedBands(data.data(), 128, 1, 44100, 256, -5, out);
    EXPECT_EQ(out.size(), 128u);
    ComputeWeightedBands(data.data(), 128, 1, 44100, 256, 500, out);
    EXPECT_EQ(out.size(), 128u);
    ComputeWeightedBands(data.data(), 128, 1, 44100, 256, 48, out);
    EXPECT_EQ(out.size(), 48u);
}

// 全零输入落在 -50 dB 以下，映射后全为 0。
TEST(SpectrumBands, SilenceMapsToZero) {
    std::vector<double> data(4096 * 2, 0.0);
    std::vector<float> out;
    ComputeWeightedBands(data.data(), 4096, 2, 48000, 8192, 256, out);
    ASSERT_EQ(out.size(), 256u);
    for (float v : out) EXPECT_EQ(v, 0.0f);
}

// 单个尖峰放在某个频带的几何中心：最大值出现在该频带。尖峰不能放在频带交界，
// 那里两侧三角窗都只剩最低权重，归属由倾斜补偿决定，测不出分带是否正确。
TEST(SpectrumBands, SinglePeakLandsInItsBand) {
    const unsigned sampleRate = 48000;
    const int fftSize = 8192;
    const size_t bins = fftSize / 2;
    const int bands = 96;
    const double binWidth = static_cast<double>(sampleRate) / fftSize;
    const double logMin = std::log10(20.0);
    const double logMax = std::log10(sampleRate / 2.0);

    for (int target : {30, 50, 80}) {
        const double center =
            std::pow(10.0, logMin + (target + 0.5) / bands * (logMax - logMin));
        std::vector<double> data(bins, 1e-6);
        data[static_cast<size_t>(std::lround(center / binWidth))] = 0.5;

        std::vector<float> out;
        ComputeWeightedBands(data.data(), bins, 1, sampleRate, fftSize, bands, out);
        const auto maxIt = std::max_element(out.begin(), out.end());
        EXPECT_EQ(static_cast<int>(maxIt - out.begin()), target) << "center=" << center << " Hz";
    }
}

// 多声道按平均处理：两路 (a, b) 与单路 (a+b)/2 给出同一结果。
TEST(SpectrumBands, ChannelsAreAveraged) {
    const size_t bins = 2048;
    std::vector<double> stereo(bins * 2), mono(bins);
    for (size_t i = 0; i < bins; ++i) {
        const double a = 0.25 / (1.0 + i * 0.01);
        const double b = 0.75 / (1.0 + i * 0.01);
        stereo[i * 2] = a;
        stereo[i * 2 + 1] = b;
        mono[i] = (static_cast<float>(a) + static_cast<float>(b)) / 2.0f;
    }
    std::vector<float> fromStereo, fromMono;
    ComputeWeightedBands(stereo.data(), bins, 2, 44100, 4096, 64, fromStereo);
    ComputeWeightedBands(mono.data(), bins, 1, 44100, 4096, 64, fromMono);
    ASSERT_EQ(fromStereo.size(), fromMono.size());
    for (size_t i = 0; i < fromStereo.size(); ++i) {
        EXPECT_FLOAT_EQ(fromStereo[i], fromMono[i]) << "band " << i;
    }
}

// 输出是显示曲线，恒在 [0, 1]。
TEST(SpectrumBands, OutputStaysInUnitRange) {
    auto data = MakeMagnitudes(8192, 2);
    for (auto& v : data) v *= 40.0;  // 推过 0 dB 上限，检验截顶
    std::vector<float> out;
    ComputeWeightedBands(data.data(), 8192, 2, 96000, 16384, 256, out);
    for (float v : out) {
        EXPECT_GE(v, 0.0f);
        EXPECT_LE(v, 1.0f);
    }
}

// float 与 double 两种样本类型：数值能被 float 精确表示时结果逐位相同
// （流水线一进来就转成 float）。
TEST(SpectrumBands, FloatAndDoubleSamplesAgree) {
    const auto wide = MakeMagnitudes(4096, 2);
    std::vector<float> narrow(wide.size());
    std::vector<double> roundTripped(wide.size());
    for (size_t i = 0; i < wide.size(); ++i) {
        narrow[i] = static_cast<float>(wide[i]);
        roundTripped[i] = narrow[i];
    }
    std::vector<float> a, b;
    ComputeWeightedBands(narrow.data(), 4096, 2, 44100, 8192, 256, a);
    ComputeWeightedBands(roundTripped.data(), 4096, 2, 44100, 8192, 256, b);
    EXPECT_EQ(a, b);
}

// 逐位快照：'weighted' 档是已发布的输出，任何数值变化都要在这里显式改快照。
// 四组参数覆盖窄带插值、宽带三角滤波、低频衰减区和不同采样率。
TEST(SpectrumBands, WeightedSnapshot) {
    struct Case {
        size_t bins;
        unsigned channels;
        unsigned sampleRate;
        int fftSize;
        int bands;
        uint64_t hash;
    };
    const Case cases[] = {
        {4096, 2, 44100, 8192, 256, 0x516dccaad3b02dadULL},
        {4096, 2, 48000, 8192, 96, 0x28797261d521df6fULL},
        {8192, 1, 44100, 16384, 48, 0x87e9ab5c19b73d96ULL},
        {512, 2, 96000, 1024, 32, 0x2c2fa5938ec4a922ULL},
    };
    for (const auto& c : cases) {
        const auto data = MakeMagnitudes(c.bins, c.channels);
        std::vector<float> out;
        ComputeWeightedBands(data.data(), c.bins, c.channels, c.sampleRate, c.fftSize, c.bands, out);
        EXPECT_EQ(HashFloatBits(out), c.hash)
            << "bins=" << c.bins << " channels=" << c.channels << " sampleRate=" << c.sampleRate
            << " fftSize=" << c.fftSize << " bands=" << c.bands << " actual=0x" << std::hex
            << HashFloatBits(out);
    }
}

// ---- 'db' 档 ----

namespace {

// 与实现同一公式的频带边界：20 Hz 到 sampleRate / 2 的对数等分。
double DbBandEdge(unsigned sampleRate, int bands, int b) {
    const double logMin = std::log10(20.0);
    const double logSpan = std::log10(sampleRate / 2.0) - logMin;
    return std::pow(10.0, logMin + logSpan * b / bands);
}

// 频率落在哪一带（左闭右开）。
int DbBandOf(unsigned sampleRate, int bands, double freq) {
    int b = 0;
    while (b + 1 < bands && freq >= DbBandEdge(sampleRate, bands, b + 1)) ++b;
    return b;
}

// 去掉标定常数，把读数还原成频带功率。
double DbToPower(float db) {
    return std::pow(10.0, (static_cast<double>(db) - kDbBandsCalibration) / 10.0);
}

double PowerToDb(double power) {
    return 10.0 * std::log10(power) + kDbBandsCalibration;
}

}  // namespace

// 全零输入：每一带都是下限。
TEST(SpectrumDbBands, SilenceIsFloor) {
    std::vector<double> data(4096 * 2, 0.0);
    std::vector<float> out;
    ComputeDbBands(data.data(), 4096, 2, 48000, 8192, 256, out);
    ASSERT_EQ(out.size(), 256u);
    for (float v : out) EXPECT_EQ(v, kDbBandsFloor);
}

// 一个频点的功率只进它中心频率所在的那一带，读数就是它的功率 dB，其余各带是下限。
TEST(SpectrumDbBands, SingleBinReadsItsPower) {
    const unsigned sampleRate = 48000;
    const int fftSize = 8192;
    const size_t bins = fftSize / 2;
    const int bands = 48;
    const double binWidth = static_cast<double>(sampleRate) / fftSize;
    const size_t k = 171;  // 1001.95 Hz

    std::vector<double> data(bins, 0.0);
    data[k] = 0.5;
    std::vector<float> out;
    ComputeDbBands(data.data(), bins, 1, sampleRate, fftSize, bands, out);

    const int target = DbBandOf(sampleRate, bands, k * binWidth);
    for (int b = 0; b < bands; ++b) {
        if (b == target) {
            EXPECT_NEAR(out[b], PowerToDb(0.25), 1e-4);
        } else {
            EXPECT_EQ(out[b], kDbBandsFloor) << "band " << b;
        }
    }
}

// 功率守恒：能量都在没有空带的频段时，各带功率之和等于各频点功率之和，与频带数无关。
TEST(SpectrumDbBands, TotalPowerDoesNotDependOnBandCount) {
    const unsigned sampleRate = 48000;
    const int fftSize = 8192;
    const size_t bins = fftSize / 2;
    const double binWidth = static_cast<double>(sampleRate) / fftSize;

    std::vector<double> data(bins, 0.0);
    double expected = 0.0;
    for (size_t k = 1; k < bins; ++k) {
        if (k * binWidth < 1000.0) continue;
        data[k] = 0.1 + 0.05 * std::sin(static_cast<double>(k) * 0.7);
        expected += data[k] * data[k];
    }

    for (int bands : {48, 64, 256}) {
        std::vector<float> out;
        ComputeDbBands(data.data(), bins, 1, sampleRate, fftSize, bands, out);
        double total = 0.0;
        for (float v : out) {
            if (v > kDbBandsFloor) total += DbToPower(v);
        }
        // 读数写出时转成 float，逐带约有 1e-7 的相对误差
        EXPECT_NEAR(total / expected, 1.0, 1e-5) << "bands=" << bands;
    }
}

// 声道取功率平均：只有一侧有信号时比两侧同信号低 3 dB。
TEST(SpectrumDbBands, ChannelPowersAreAveraged) {
    const size_t bins = 4096;
    std::vector<double> both(bins * 2, 0.0), oneSide(bins * 2, 0.0);
    const size_t k = 400;
    both[k * 2] = 0.2;
    both[k * 2 + 1] = 0.2;
    oneSide[k * 2] = 0.2;

    std::vector<float> a, b;
    ComputeDbBands(both.data(), bins, 2, 48000, 8192, 64, a);
    ComputeDbBands(oneSide.data(), bins, 2, 48000, 8192, 64, b);
    const auto peakA = std::max_element(a.begin(), a.end());
    const auto peakB = std::max_element(b.begin(), b.end());
    ASSERT_EQ(peakA - a.begin(), peakB - b.begin());
    // 两侧都是 0.2：均方 0.04。求和而不取平均会读成 0.08，差 3 dB。
    EXPECT_NEAR(*peakA, PowerToDb(0.04), 1e-4);
    EXPECT_NEAR(*peakA - *peakB, 10.0 * std::log10(2.0), 1e-4);
}

// 直流与 20 Hz 以下的频点不进任何一带；它们只在空带插值时可能被当作邻点，
// 这里只给直流和 20 Hz 以下最低的两个频点，离所有空带的插值邻点都远。
TEST(SpectrumDbBands, DcAndSubAudioBinsAreDropped) {
    const size_t bins = 4096;
    std::vector<double> data(bins, 0.0);
    data[0] = 1.0;  // 直流
    data[1] = 0.8;  // 5.86 Hz
    data[2] = 0.8;  // 11.7 Hz
    std::vector<float> out;
    ComputeDbBands(data.data(), bins, 1, 48000, 8192, 48, out);
    for (float v : out) EXPECT_EQ(v, kDbBandsFloor);
}

// 空带在几何中心处对左右相邻频点的功率线性插值。48 kHz、8192 点、48 带时第 0 带
// [20, 23.18) Hz 里没有频点（频点 4 在 23.44 Hz），中心落在频点 3 与 4 之间。
TEST(SpectrumDbBands, EmptyBandInterpolatesNeighbourPower) {
    const unsigned sampleRate = 48000;
    const int fftSize = 8192;
    const size_t bins = fftSize / 2;
    const int bands = 48;
    const double binWidth = static_cast<double>(sampleRate) / fftSize;
    ASSERT_GE(4 * binWidth, DbBandEdge(sampleRate, bands, 1));

    std::vector<double> data(bins, 0.0);
    data[3] = 0.1;
    data[4] = 0.3;
    std::vector<float> out;
    ComputeDbBands(data.data(), bins, 1, sampleRate, fftSize, bands, out);

    const double pos =
        std::sqrt(DbBandEdge(sampleRate, bands, 0) * DbBandEdge(sampleRate, bands, 1)) / binWidth;
    ASSERT_GT(pos, 3.0);
    ASSERT_LT(pos, 4.0);
    const double expected = 0.01 + (0.09 - 0.01) * (pos - 3.0);
    EXPECT_NEAR(out[0], PowerToDb(expected), 1e-4);
    // 频点 4 本身归第 1 带
    EXPECT_NEAR(out[1], PowerToDb(0.09), 1e-4);
    // 第 2 带有频点（29.3 Hz）但功率为 0：它不是空带，不插值，读下限
    EXPECT_EQ(out[2], kDbBandsFloor);
}

// 空带的左邻是直流时换成频点 1：256 点、48 kHz 下频点宽 187.5 Hz，第 0 带的中心低于它。
TEST(SpectrumDbBands, EmptyBandSkipsDcNeighbour) {
    const size_t bins = 128;
    std::vector<double> data(bins, 0.0);
    data[0] = 1.0;
    data[1] = 0.1;
    std::vector<float> out;
    ComputeDbBands(data.data(), bins, 1, 48000, 256, 64, out);
    EXPECT_NEAR(out[0], PowerToDb(0.01), 1e-4);
}

// 最高频点 fftSize/2 - 1 归最高一带，不丢。
TEST(SpectrumDbBands, TopBinLandsInLastBand) {
    const size_t bins = 4096;
    std::vector<double> data(bins, 0.0);
    data[bins - 1] = 0.5;
    std::vector<float> out;
    ComputeDbBands(data.data(), bins, 1, 48000, 8192, 48, out);
    EXPECT_NEAR(out.back(), PowerToDb(0.25), 1e-4);
    for (size_t b = 0; b + 1 < out.size(); ++b) EXPECT_EQ(out[b], kDbBandsFloor) << "band " << b;
}

// 频带数与采样率的回退与 'weighted' 档一致。
TEST(SpectrumDbBands, FallbacksMatchWeightedScale) {
    const auto data = MakeMagnitudes(4096, 2);
    std::vector<float> a, b;
    ComputeDbBands(data.data(), 4096, 2, 0, 8192, 96, a);
    ComputeDbBands(data.data(), 4096, 2, 44100, 8192, 96, b);
    EXPECT_EQ(a, b);

    std::vector<float> out;
    ComputeDbBands(data.data(), 128, 2, 44100, 256, 0, out);
    EXPECT_EQ(out.size(), 128u);
    ComputeDbBands(data.data(), 128, 2, 44100, 256, -5, out);
    EXPECT_EQ(out.size(), 128u);
    ComputeDbBands(data.data(), 128, 2, 44100, 256, 500, out);
    EXPECT_EQ(out.size(), 128u);
    ComputeDbBands(data.data(), 128, 2, 44100, 256, 48, out);
    EXPECT_EQ(out.size(), 48u);
}

// float 与 double 两种样本类型：数值能被 float 精确表示时结果逐位相同（进来就转 double）。
TEST(SpectrumDbBands, FloatAndDoubleSamplesAgree) {
    const auto wide = MakeMagnitudes(4096, 2);
    std::vector<float> narrow(wide.size());
    std::vector<double> roundTripped(wide.size());
    for (size_t i = 0; i < wide.size(); ++i) {
        narrow[i] = static_cast<float>(wide[i]);
        roundTripped[i] = narrow[i];
    }
    std::vector<float> a, b;
    ComputeDbBands(narrow.data(), 4096, 2, 44100, 8192, 256, a);
    ComputeDbBands(roundTripped.data(), 4096, 2, 44100, 8192, 256, b);
    EXPECT_EQ(a, b);
}

// ---- 频率范围 ----

namespace {

// 与实现同一公式的范围内频带边界：[minFreq, maxFreq] 的对数等分。
double RangeBandEdge(double minFreq, double maxFreq, int bands, int b) {
    const double logMin = std::log10(minFreq);
    return std::pow(10.0, logMin + (std::log10(maxFreq) - logMin) * b / bands);
}

}  // namespace

// 显式传入缺省范围（20 Hz 与 0 或 sampleRate / 2）与不传时两档逐位相同，'weighted' 仍是快照值。
TEST(SpectrumBandsRange, DefaultRangeMatchesImplicitDefault) {
    struct Case {
        size_t bins;
        unsigned channels;
        unsigned sampleRate;
        int fftSize;
        int bands;
    };
    const Case cases[] = {
        {4096, 2, 44100, 8192, 256},
        {4096, 2, 48000, 8192, 96},
        {8192, 1, 44100, 16384, 48},
        {512, 2, 96000, 1024, 32},
    };
    for (const auto& c : cases) {
        const auto data = MakeMagnitudes(c.bins, c.channels);
        std::vector<float> implicitW, zeroW, nyquistW, implicitD, zeroD, nyquistD;
        ComputeWeightedBands(data.data(), c.bins, c.channels, c.sampleRate, c.fftSize, c.bands, implicitW);
        ComputeWeightedBands(data.data(), c.bins, c.channels, c.sampleRate, c.fftSize, c.bands, zeroW, 20.0, 0.0);
        ComputeWeightedBands(data.data(), c.bins, c.channels, c.sampleRate, c.fftSize, c.bands, nyquistW, 20.0,
                             c.sampleRate / 2.0);
        ComputeDbBands(data.data(), c.bins, c.channels, c.sampleRate, c.fftSize, c.bands, implicitD);
        ComputeDbBands(data.data(), c.bins, c.channels, c.sampleRate, c.fftSize, c.bands, zeroD, 20.0, 0.0);
        ComputeDbBands(data.data(), c.bins, c.channels, c.sampleRate, c.fftSize, c.bands, nyquistD, 20.0,
                       c.sampleRate / 2.0);
        EXPECT_EQ(HashFloatBits(zeroW), HashFloatBits(implicitW)) << "sampleRate=" << c.sampleRate;
        EXPECT_EQ(HashFloatBits(nyquistW), HashFloatBits(implicitW)) << "sampleRate=" << c.sampleRate;
        EXPECT_EQ(HashFloatBits(zeroD), HashFloatBits(implicitD)) << "sampleRate=" << c.sampleRate;
        EXPECT_EQ(HashFloatBits(nyquistD), HashFloatBits(implicitD)) << "sampleRate=" << c.sampleRate;
    }
}

// 上限高于 Nyquist 时按 Nyquist 截，与不给上限相同。
TEST(SpectrumBandsRange, UpperEdgeAboveNyquistIsClamped) {
    const auto data = MakeMagnitudes(4096, 2);
    std::vector<float> w, wClamped, d, dClamped;
    ComputeWeightedBands(data.data(), 4096, 2, 48000, 8192, 64, w);
    ComputeWeightedBands(data.data(), 4096, 2, 48000, 8192, 64, wClamped, 20.0, 30000.0);
    ComputeDbBands(data.data(), 4096, 2, 48000, 8192, 64, d);
    ComputeDbBands(data.data(), 4096, 2, 48000, 8192, 64, dClamped, 20.0, 30000.0);
    EXPECT_EQ(HashFloatBits(wClamped), HashFloatBits(w));
    EXPECT_EQ(HashFloatBits(dClamped), HashFloatBits(d));
}

// 'db'：区间内各带功率之和等于区间内频点功率之和，区间外的频点不计入任何一带。
TEST(SpectrumBandsRange, DbSumsOnlyBinsInsideTheRange) {
    const unsigned sampleRate = 48000;
    const int fftSize = 8192;
    const size_t bins = fftSize / 2;
    const double binWidth = static_cast<double>(sampleRate) / fftSize;
    const double minFreq = 500.0, maxFreq = 2000.0;
    const int bands = 32;  // 最窄的一带约 22 Hz，宽于频点（5.86 Hz），没有空带

    std::vector<double> data(bins, 0.0);
    double inside = 0.0;
    for (size_t k = 1; k < bins; ++k) {
        data[k] = 0.05 + 0.02 * std::sin(static_cast<double>(k) * 0.3);
        const double f = k * binWidth;
        if (f >= minFreq && f < maxFreq) inside += data[k] * data[k];
    }
    // 区间外放两个很强的频点，不应影响任何一带
    data[static_cast<size_t>(400.0 / binWidth)] = 5.0;
    data[static_cast<size_t>(3000.0 / binWidth)] = 5.0;

    std::vector<float> out;
    ComputeDbBands(data.data(), bins, 1, sampleRate, fftSize, bands, out, minFreq, maxFreq);
    ASSERT_EQ(out.size(), static_cast<size_t>(bands));
    double total = 0.0;
    for (float v : out) {
        ASSERT_GT(v, kDbBandsFloor);
        total += DbToPower(v);
    }
    EXPECT_NEAR(total / inside, 1.0, 1e-5);
}

// 'db'：单个频点落在按区间算出的那一带，其余各带为下限。
TEST(SpectrumBandsRange, DbPeakLandsInTheRangeBand) {
    const unsigned sampleRate = 48000;
    const int fftSize = 8192;
    const size_t bins = fftSize / 2;
    const double binWidth = static_cast<double>(sampleRate) / fftSize;
    const double minFreq = 500.0, maxFreq = 2000.0;
    const int bands = 32;
    const size_t k = 171;  // 1001.95 Hz

    std::vector<double> data(bins, 0.0);
    data[k] = 0.5;
    std::vector<float> out;
    ComputeDbBands(data.data(), bins, 1, sampleRate, fftSize, bands, out, minFreq, maxFreq);

    int target = 0;
    while (target + 1 < bands && k * binWidth >= RangeBandEdge(minFreq, maxFreq, bands, target + 1)) ++target;
    for (int b = 0; b < bands; ++b) {
        if (b == target) {
            EXPECT_NEAR(out[b], PowerToDb(0.25), 1e-4);
        } else {
            EXPECT_EQ(out[b], kDbBandsFloor) << "band " << b;
        }
    }
}

// 'weighted'：频带数不变，尖峰放在某带几何中心时最大值落在该带。
TEST(SpectrumBandsRange, WeightedPeakLandsInTheRangeBand) {
    const unsigned sampleRate = 48000;
    const int fftSize = 16384;
    const size_t bins = fftSize / 2;
    const int bands = 48;
    const double binWidth = static_cast<double>(sampleRate) / fftSize;
    const double minFreq = 200.0, maxFreq = 8000.0;

    for (int target : {5, 24, 40}) {
        const double center = std::sqrt(RangeBandEdge(minFreq, maxFreq, bands, target) *
                                        RangeBandEdge(minFreq, maxFreq, bands, target + 1));
        std::vector<double> data(bins, 1e-6);
        data[static_cast<size_t>(std::lround(center / binWidth))] = 0.5;

        std::vector<float> out;
        ComputeWeightedBands(data.data(), bins, 1, sampleRate, fftSize, bands, out, minFreq, maxFreq);
        ASSERT_EQ(out.size(), static_cast<size_t>(bands));
        const auto maxIt = std::max_element(out.begin(), out.end());
        EXPECT_EQ(static_cast<int>(maxIt - out.begin()), target) << "center=" << center << " Hz";
    }
}

// 截后的上限不大于下限（区间整个高于这条流的 Nyquist）：两档整帧填静音值。
TEST(SpectrumBandsRange, RangeAboveNyquistYieldsSilence) {
    const auto data = MakeMagnitudes(4096, 2);
    std::vector<float> w, d;
    ComputeWeightedBands(data.data(), 4096, 2, 48000, 8192, 32, w, 30000.0, 40000.0);
    ComputeDbBands(data.data(), 4096, 2, 48000, 8192, 32, d, 30000.0, 40000.0);
    ASSERT_EQ(w.size(), 32u);
    ASSERT_EQ(d.size(), 32u);
    for (float v : w) EXPECT_EQ(v, 0.0f);
    for (float v : d) EXPECT_EQ(v, kDbBandsFloor);
}

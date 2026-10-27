// test_waveform_accumulator.cpp - 整轨波形一次解码三组窗口值的纯值计算
// （docs/audio-visualization/SPEC.md D6、§5.1、§10 A2.3）。
//
// 解码在宿主里；这里喂进已解码的交错样本块，测三组原始窗口值的累加与按请求
// 形态现算。参照实现是 b571932c 的 ExecuteWaveformGeneration 里那段单一形态的
// 解码循环、归一化与 dB 映射的逐字复制（只把 decoder->run 换成遍历给定的块，
// 并记下归一化除数），三种请求序列 × 两种 scale × signed 的输出须与它逐位相同。
#include "pch.h"
#include "../src/api/WaveformAccumulator.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>

using fb2k_waveform::Accumulator;
using fb2k_waveform::DecodedAccumulator;
using fb2k_waveform::DecodedWindows;
using fb2k_waveform::Method;
using fb2k_waveform::RawWindows;
using fb2k_waveform::Render;
using fb2k_waveform::Rendered;
using fb2k_waveform::RenderOptions;
using fb2k_waveform::Scale;

namespace {

// 交错样本块，chCount 路；一个块对应 decoder->run 的一次输出。
using Chunks = std::vector<std::vector<double>>;

struct ReferenceResult {
    std::vector<float> waveform;
    float divisor = 0.0f;
};

// 从 b571932c 的 ExecuteWaveformGeneration 逐字复制（从 samplesPerWindow 的零值守卫
// 起，到 dB 映射止）：变量名、浮点类型与运算顺序不变。改动：样本指针写成 double（x64
// 的 audio_sample）；decoder->run(chunk, abort) 换成遍历 chunks；归一化时把除数记进
// divisor（原代码里它只是局部量 maxValue / absMax）；原有注释删去。
ReferenceResult ReferenceGenerate(const Chunks& chunks, unsigned chCount, int resolution,
                                  uint64_t samplesPerWindow, const std::string& method,
                                  const std::string& scale, bool signedOutput) {
    ReferenceResult result;
    if (samplesPerWindow == 0) samplesPerWindow = 1;

    std::vector<float> waveform(resolution, 0.0f);
    std::vector<float> windowBuffer;
    int currentWindow = 0;
    float maxValue = 0.0f;
    uint64_t totalProcessedSamples = 0;  // 全局已处理样本帧计数

    bool isPeak = (method == "peak");

    auto computeSampleValue = [signedOutput](const double* samples, size_t i,
                                 unsigned chCount, bool peak) -> float {
        if (signedOutput) {
            float sum = 0.0f;
            for (unsigned ch = 0; ch < chCount; ch++) {
                sum += static_cast<float>(samples[i * chCount + ch]);
            }
            return sum / chCount;
        }
        if (peak) {
            float value = 0.0f;
            for (unsigned ch = 0; ch < chCount; ch++) {
                float s = static_cast<float>(samples[i * chCount + ch]);
                value = std::max(value, std::abs(s));
            }
            return value;
        }
        float sumSq = 0.0f;
        for (unsigned ch = 0; ch < chCount; ch++) {
            float s = static_cast<float>(samples[i * chCount + ch]);
            sumSq += s * s;
        }
        return sumSq / chCount;
    };

    auto finalizeWindow = [signedOutput](const std::vector<float>& buf, bool peak) -> float {
        if (signedOutput) {
            float best = 0.0f;
            for (float v : buf) {
                if (std::abs(v) > std::abs(best)) best = v;
            }
            return best;
        }
        if (peak)
            return *std::max_element(buf.begin(), buf.end());
        float sum = std::accumulate(buf.begin(), buf.end(), 0.0f);
        return std::sqrt(sum / buf.size());
    };

    for (const auto& chunk : chunks) {
        const double* samples = chunk.data();
        size_t sampleCount = chunk.size() / chCount;

        for (size_t i = 0; i < sampleCount; i++) {
            uint64_t globalSampleIndex = totalProcessedSamples + i;
            int windowIndex = static_cast<int>(globalSampleIndex / samplesPerWindow);
            if (windowIndex >= resolution) break;

            float value = computeSampleValue(samples, i, chCount, isPeak);

            if (windowIndex == currentWindow) {
                windowBuffer.push_back(value);
                continue;
            }
            if (!windowBuffer.empty()) {
                float windowValue = finalizeWindow(windowBuffer, isPeak);
                waveform[currentWindow] = windowValue;
                maxValue = std::max(maxValue, windowValue);
            }
            currentWindow = windowIndex;
            windowBuffer.clear();
            windowBuffer.push_back(value);
        }

        totalProcessedSamples += sampleCount;
    }

    if (!windowBuffer.empty() && currentWindow < resolution) {
        float windowValue = finalizeWindow(windowBuffer, isPeak);
        waveform[currentWindow] = windowValue;
        maxValue = std::max(maxValue, windowValue);
    }

    if (signedOutput) {
        float absMax = 0.0f;
        for (float v : waveform) {
            absMax = std::max(absMax, std::abs(v));
        }
        result.divisor = absMax;
        if (absMax > 0.0f) {
            for (float& v : waveform) {
                v /= absMax;
            }
        }
    } else {
        result.divisor = maxValue;
        if (maxValue > 0.0f) {
            for (float& v : waveform) {
                v /= maxValue;
            }
        }
    }

    if (scale == "db" && !signedOutput) {
        const float dbFloor = -60.0f;
        for (float& v : waveform) {
            if (v <= 0.0f) { v = 0.0f; continue; }
            float db = 20.0f * std::log10f(v);
            v = std::max(0.0f, (db - dbFloor) / (-dbFloor));
        }
    }

    result.waveform = std::move(waveform);
    return result;
}

// 把同一批块喂进累加器，得到三组原始窗口值。
RawWindows Accumulate(const Chunks& chunks, unsigned chCount, int resolution, uint64_t samplesPerWindow) {
    Accumulator<double> acc(resolution, samplesPerWindow);
    for (const auto& chunk : chunks) acc.Feed(chunk.data(), chunk.size() / chCount, chCount);
    return acc.Finish();
}

bool SameBits(float a, float b) {
    uint32_t x = 0, y = 0;
    std::memcpy(&x, &a, sizeof x);
    std::memcpy(&y, &b, sizeof y);
    return x == y;
}

// 逐位比较两条序列，返回第一个不同的下标，全同返回 -1。
int FirstBitDifference(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size()) return static_cast<int>(std::min(a.size(), b.size()));
    for (size_t i = 0; i < a.size(); ++i) {
        if (!SameBits(a[i], b[i])) return static_cast<int>(i);
    }
    return -1;
}

// 16 位 PCM 网格上的样本：整数 / 32768。真实解码常见正负幅值恰好相等，signed 序列的
// 并列裁决（先到者胜）只有在这种输入上才测得到，随机 double 几乎不会并列。
Chunks QuantizedSineChunks(unsigned channels, size_t frames, double hz, double sampleRate) {
    Chunks chunks;
    std::vector<double> chunk;
    const double step = 2.0 * 3.14159265358979323846 * hz / sampleRate;
    for (size_t i = 0; i < frames; ++i) {
        const double v = std::round(32767.0 * std::sin(step * static_cast<double>(i)));
        for (unsigned ch = 0; ch < channels; ++ch) chunk.push_back(v / 32768.0);
        if (chunk.size() >= 480 * channels) {
            chunks.push_back(std::move(chunk));
            chunk.clear();
        }
    }
    if (!chunk.empty()) chunks.push_back(std::move(chunk));
    return chunks;
}

Chunks SmallIntegerChunks(unsigned channels, size_t frames, uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> value(-3, 3);
    Chunks chunks{std::vector<double>()};
    for (size_t i = 0; i < frames * channels; ++i) chunks.back().push_back(value(rng) / 32768.0);
    return chunks;
}

struct Fixture {
    const char* name;
    unsigned channels;
    int resolution;
    uint64_t samplesPerWindow;
    Chunks chunks;
};

// 确定性随机样本，切成大小不一的块；`frames` 是总帧数。
Chunks RandomChunks(unsigned channels, size_t frames, uint32_t seed, double amplitude = 1.0) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> value(-amplitude, amplitude);
    std::uniform_int_distribution<size_t> chunkFrames(1, 700);
    Chunks chunks;
    size_t produced = 0;
    while (produced < frames) {
        const size_t n = std::min(chunkFrames(rng), frames - produced);
        std::vector<double> chunk(n * channels);
        for (double& s : chunk) s = value(rng);
        chunks.push_back(std::move(chunk));
        produced += n;
    }
    return chunks;
}

std::vector<Fixture> Fixtures() {
    std::vector<Fixture> out;
    // 立体声，帧数是窗口数与窗口长的整倍数。
    out.push_back({"stereo exact", 2, 64, 37, RandomChunks(2, 64 * 37, 1)});
    // 单声道，最后一个窗口只有一部分帧。
    out.push_back({"mono partial last window", 1, 50, 100, RandomChunks(1, 50 * 100 - 41, 2)});
    // 六声道，帧比窗口数还少：大部分窗口没有样本，保持 0。
    out.push_back({"6ch fewer frames than windows", 6, 256, 1, RandomChunks(6, 30, 3)});
    // 解码出的帧多于 duration 估算：超出的帧全部丢弃。
    out.push_back({"stereo audio beyond the last window", 2, 32, 25, RandomChunks(2, 32 * 25 + 913, 4)});
    // 幅度大于 1 的样本（浮点解码器可能给出）：不截顶，按实际最大值归一化。
    out.push_back({"stereo hot signal", 2, 16, 300, RandomChunks(2, 16 * 300, 5, 3.5)});
    // 全零。
    out.push_back({"silence", 2, 40, 10, Chunks{std::vector<double>(2 * 400, 0.0)}});
    // 窗口长 0 按 1 处理。
    out.push_back({"zero samplesPerWindow", 1, 20, 0, RandomChunks(1, 60, 6)});
    // 16 位网格上的满幅正弦：每窗都有恰好相等的正负峰值，窗口长不是周期的整数倍，
    // 先到的是正是负逐窗不同。
    out.push_back({"quantized sine ties", 1, 64, 125, QuantizedSineChunks(1, 64 * 125, 1000.0, 48000.0)});
    // 立体声小整数噪声：声道均值大量并列，且正负都有。
    out.push_back({"small integer ties", 2, 32, 40, SmallIntegerChunks(2, 32 * 40, 9)});
    return out;
}

}  // namespace

// A2.3：两种 method × 两种 scale × signed，输出与参照实现逐位相同，除数即 maxAmplitude。
TEST(WaveformAccumulator, MatchesReferenceBitForBit) {
    for (const Fixture& f : Fixtures()) {
        const RawWindows raw = Accumulate(f.chunks, f.channels, f.resolution, f.samplesPerWindow);
        for (const char* method : {"rms", "peak"}) {
            for (const char* scale : {"linear", "db"}) {
                for (bool signedOutput : {false, true}) {
                    const ReferenceResult ref = ReferenceGenerate(f.chunks, f.channels, f.resolution,
                                                                  f.samplesPerWindow, method, scale,
                                                                  signedOutput);
                    RenderOptions options;
                    options.method = std::string(method) == "peak" ? Method::Peak : Method::Rms;
                    options.scale = std::string(scale) == "db" ? Scale::Db : Scale::Linear;
                    options.signedOutput = signedOutput;
                    const Rendered got = Render(raw, options);

                    const int diff = FirstBitDifference(got.waveform, ref.waveform);
                    const auto at = [diff](const std::vector<float>& v) {
                        return diff >= 0 && diff < static_cast<int>(v.size()) ? v[static_cast<size_t>(diff)] : 0.0f;
                    };
                    EXPECT_EQ(diff, -1) << f.name << " method=" << method << " scale=" << scale
                                        << " signed=" << signedOutput << " at window " << diff
                                        << " got=" << at(got.waveform) << " ref=" << at(ref.waveform);
                    EXPECT_TRUE(SameBits(got.maxAmplitude, ref.divisor))
                        << f.name << " method=" << method << " signed=" << signedOutput
                        << " maxAmplitude=" << got.maxAmplitude << " divisor=" << ref.divisor;
                }
            }
        }
    }
}

// 原始值不依赖块边界：一次喂完与按任意大小分块喂，三组值逐位相同。
TEST(WaveformAccumulator, ChunkingDoesNotChangeRawValues) {
    const Chunks chunked = RandomChunks(2, 5000, 7);
    std::vector<double> whole;
    for (const auto& c : chunked) whole.insert(whole.end(), c.begin(), c.end());

    const RawWindows a = Accumulate(chunked, 2, 48, 100);
    const RawWindows b = Accumulate(Chunks{whole}, 2, 48, 100);
    EXPECT_EQ(FirstBitDifference(a.rms, b.rms), -1);
    EXPECT_EQ(FirstBitDifference(a.peak, b.peak), -1);
    EXPECT_EQ(FirstBitDifference(a.signedPeak, b.signedPeak), -1);

    // 单帧一块的极端切法。
    Chunks single;
    for (size_t i = 0; i + 1 < whole.size(); i += 2) single.push_back({whole[i], whole[i + 1]});
    const RawWindows c = Accumulate(single, 2, 48, 100);
    EXPECT_EQ(FirstBitDifference(a.rms, c.rms), -1);
    EXPECT_EQ(FirstBitDifference(a.peak, c.peak), -1);
    EXPECT_EQ(FirstBitDifference(a.signedPeak, c.signedPeak), -1);
}

// float 与 double 两种样本类型：数值能被 float 精确表示时结果逐位相同（进来就转 float）。
TEST(WaveformAccumulator, FloatAndDoubleSamplesAgree) {
    const Chunks wide = RandomChunks(2, 3000, 8);
    Accumulator<float> narrow(30, 100);
    Accumulator<double> roundTripped(30, 100);
    for (const auto& chunk : wide) {
        std::vector<float> f(chunk.size());
        std::vector<double> d(chunk.size());
        for (size_t i = 0; i < chunk.size(); ++i) {
            f[i] = static_cast<float>(chunk[i]);
            d[i] = f[i];
        }
        narrow.Feed(f.data(), f.size() / 2, 2);
        roundTripped.Feed(d.data(), d.size() / 2, 2);
    }
    const RawWindows a = narrow.Finish();
    const RawWindows b = roundTripped.Finish();
    EXPECT_EQ(FirstBitDifference(a.rms, b.rms), -1);
    EXPECT_EQ(FirstBitDifference(a.peak, b.peak), -1);
    EXPECT_EQ(FirstBitDifference(a.signedPeak, b.signedPeak), -1);
}

// 静音：三组值全 0，现算不除以 0，maxAmplitude 为 0，输出全 0。
TEST(WaveformAccumulator, SilenceRendersZeroWithoutDividing) {
    const RawWindows raw = Accumulate(Chunks{std::vector<double>(2 * 500, 0.0)}, 2, 25, 20);
    for (bool signedOutput : {false, true}) {
        for (Scale scale : {Scale::Linear, Scale::Db}) {
            RenderOptions options;
            options.scale = scale;
            options.signedOutput = signedOutput;
            const Rendered got = Render(raw, options);
            EXPECT_EQ(got.maxAmplitude, 0.0f);
            ASSERT_EQ(got.waveform.size(), 25u);
            for (float v : got.waveform) EXPECT_EQ(v, 0.0f);
        }
    }
}

// maxAmplitude 是所选原始序列归一化前的最大值：rms 取窗口 RMS 最大值、peak 取峰值
// 最大值、signed 取绝对值最大值；waveform[i] * maxAmplitude 还原原始值。
TEST(WaveformAccumulator, MaxAmplitudeRestoresTheSelectedSequence) {
    RawWindows raw;
    raw.rms = {0.1f, 0.25f, 0.05f, 0.0f};
    raw.peak = {0.4f, 0.9f, 0.2f, 0.0f};
    raw.signedPeak = {0.3f, -0.8f, 0.5f, 0.0f};

    RenderOptions rms;
    const Rendered r = Render(raw, rms);
    EXPECT_EQ(r.maxAmplitude, 0.25f);
    EXPECT_EQ(r.waveform[1], 1.0f);
    EXPECT_FLOAT_EQ(r.waveform[0] * r.maxAmplitude, 0.1f);

    RenderOptions peak;
    peak.method = Method::Peak;
    const Rendered p = Render(raw, peak);
    EXPECT_EQ(p.maxAmplitude, 0.9f);
    EXPECT_FLOAT_EQ(p.waveform[2] * p.maxAmplitude, 0.2f);

    RenderOptions signedPeak;
    signedPeak.signedOutput = true;
    signedPeak.method = Method::Peak;  // signed 忽略 method
    signedPeak.scale = Scale::Db;      // 与 scale
    const Rendered s = Render(raw, signedPeak);
    EXPECT_EQ(s.maxAmplitude, 0.8f);
    EXPECT_EQ(s.waveform[1], -1.0f);
    EXPECT_FLOAT_EQ(s.waveform[2] * s.maxAmplitude, 0.5f);
    for (float v : s.waveform) {
        EXPECT_GE(v, -1.0f);
        EXPECT_LE(v, 1.0f);
    }
}

// 'db' 档：归一化后 v 映射为 (20·log10 v + 60) / 60，截到 [0, 1]；低于 −60 dB 的读 0。
TEST(WaveformAccumulator, DbScaleMapsSixtyDecibelsOntoUnitRange) {
    RawWindows raw;
    raw.rms = {1.0f, 0.1f, 0.001f, 0.0001f, 0.0f};
    raw.peak = raw.rms;
    raw.signedPeak = raw.rms;
    RenderOptions options;
    options.scale = Scale::Db;
    const Rendered got = Render(raw, options);
    EXPECT_FLOAT_EQ(got.waveform[0], 1.0f);
    EXPECT_NEAR(got.waveform[1], 40.0f / 60.0f, 1e-6f);
    EXPECT_NEAR(got.waveform[2], 0.0f, 1e-6f);  // 恰在 −60 dB
    EXPECT_EQ(got.waveform[3], 0.0f);           // 低于下限
    EXPECT_EQ(got.waveform[4], 0.0f);
}

namespace {

void ExpectRawBitsEqual(const RawWindows& actual, const RawWindows& expected) {
    EXPECT_EQ(FirstBitDifference(actual.rms, expected.rms), -1);
    EXPECT_EQ(FirstBitDifference(actual.peak, expected.peak), -1);
    EXPECT_EQ(FirstBitDifference(actual.signedPeak, expected.signedPeak), -1);
}

void ExpectConstantWindow(const RawWindows& raw, size_t window, float amplitude) {
    ASSERT_LT(window, raw.rms.size());
    ASSERT_LT(window, raw.peak.size());
    ASSERT_LT(window, raw.signedPeak.size());
    EXPECT_FLOAT_EQ(raw.rms[window], std::abs(amplitude));
    EXPECT_FLOAT_EQ(raw.peak[window], std::abs(amplitude));
    EXPECT_FLOAT_EQ(raw.signedPeak[window], amplitude);
}

template <typename Sample>
void ExpectFourSecondTimeline(unsigned sampleRate) {
    DecodedAccumulator<Sample> accumulator(1024, 4.0);
    // 四个一秒区间的幅度已知；文件标称格式不参与构造或喂入。
    for (float amplitude : {0.125f, -0.25f, 0.5f, -1.0f}) {
        const std::vector<Sample> second(sampleRate, static_cast<Sample>(amplitude));
        accumulator.Feed(second.data(), second.size(), 1, sampleRate);
    }
    const DecodedWindows decoded = accumulator.Finish();
    EXPECT_EQ(decoded.sampleRate, sampleRate);
    EXPECT_EQ(decoded.channels, 1u);
    ASSERT_EQ(decoded.raw.rms.size(), 1024u);
    ASSERT_EQ(decoded.raw.peak.size(), 1024u);
    ASSERT_EQ(decoded.raw.signedPeak.size(), 1024u);
    ExpectConstantWindow(decoded.raw, 0, 0.125f);
    ExpectConstantWindow(decoded.raw, 128, 0.125f);
    ExpectConstantWindow(decoded.raw, 384, -0.25f);
    ExpectConstantWindow(decoded.raw, 640, 0.5f);
    ExpectConstantWindow(decoded.raw, 896, -1.0f);
    ExpectConstantWindow(decoded.raw, 1023, -1.0f);
}

template <typename Sample>
class DecodedWaveformAccumulator : public ::testing::Test {};

using DecodedSampleTypes = ::testing::Types<float, double>;
TYPED_TEST_CASE(DecodedWaveformAccumulator, DecodedSampleTypes);

TYPED_TEST(DecodedWaveformAccumulator, Dsd64DecodedAt44100HzFillsTrackTimeline) {
    // DSD64 的文件标签为 2822400 Hz，实际 PCM 为 44100 Hz。
    ExpectFourSecondTimeline<TypeParam>(44100);
}

TYPED_TEST(DecodedWaveformAccumulator, OutputAt88200HzPreservesAudioBeyond44100HzFileLabel) {
    ExpectFourSecondTimeline<TypeParam>(88200);
}

TYPED_TEST(DecodedWaveformAccumulator, OrdinaryPcmMatchesLegacyRawValuesAndEveryRenderMode) {
    for (const Fixture& fixture : Fixtures()) {
        SCOPED_TRACE(fixture.name);
        // 四分之一帧的余量避开浮点表示恰落整数边界，取整后仍是既定窗口总帧数。
        const double duration = (static_cast<double>(fixture.samplesPerWindow) *
                                 fixture.resolution + 0.25) / 48000.0;
        DecodedAccumulator<TypeParam> decoded(fixture.resolution, duration);
        Accumulator<TypeParam> legacy(fixture.resolution, fixture.samplesPerWindow);
        for (const auto& chunk : fixture.chunks) {
            std::vector<TypeParam> samples;
            samples.reserve(chunk.size());
            for (double sample : chunk) samples.push_back(static_cast<TypeParam>(sample));
            const size_t frames = samples.size() / fixture.channels;
            decoded.Feed(samples.data(), frames, fixture.channels, 48000);
            legacy.Feed(samples.data(), frames, fixture.channels);
        }
        const DecodedWindows actual = decoded.Finish();
        const RawWindows expected = legacy.Finish();
        EXPECT_EQ(actual.sampleRate, 48000u);
        EXPECT_EQ(actual.channels, fixture.channels);
        ExpectRawBitsEqual(actual.raw, expected);
        for (Method method : {Method::Rms, Method::Peak}) {
            for (Scale scale : {Scale::Linear, Scale::Db}) {
                for (bool signedOutput : {false, true}) {
                    const RenderOptions options{method, scale, signedOutput};
                    const Rendered rendered = Render(actual.raw, options);
                    const Rendered reference = Render(expected, options);
                    EXPECT_EQ(FirstBitDifference(rendered.waveform, reference.waveform), -1);
                    EXPECT_TRUE(SameBits(rendered.maxAmplitude, reference.maxAmplitude));
                }
            }
        }
    }
}

TYPED_TEST(DecodedWaveformAccumulator, FirstBlockIsConsumedOnceAndLastWindowIsClosed) {
    std::vector<TypeParam> samples(16, TypeParam{0});
    samples.front() = TypeParam{1};
    samples.back() = TypeParam{-0.5};
    DecodedAccumulator<TypeParam> accumulator(4, 2.0);
    accumulator.Feed(samples.data(), 1, 1, 8);
    accumulator.Feed(samples.data() + 1, 15, 1, 8);
    const DecodedWindows decoded = accumulator.Finish();
    const RawWindows expected{{0.5f, 0.0f, 0.0f, 0.25f},
                              {1.0f, 0.0f, 0.0f, 0.5f},
                              {1.0f, 0.0f, 0.0f, -0.5f}};
    ExpectRawBitsEqual(decoded.raw, expected);
}

TYPED_TEST(DecodedWaveformAccumulator, ChunkBoundariesDoNotMoveKnownPulses) {
    std::vector<TypeParam> samples(16, TypeParam{0});
    samples[0] = TypeParam{1};
    samples[5] = TypeParam{-0.5};
    samples[10] = TypeParam{0.25};
    samples[15] = TypeParam{-1};
    DecodedAccumulator<TypeParam> whole(4, 2.0);
    DecodedAccumulator<TypeParam> chunked(4, 2.0);
    DecodedAccumulator<TypeParam> singleFrames(4, 2.0);
    whole.Feed(samples.data(), samples.size(), 1, 8);
    chunked.Feed(samples.data(), 3, 1, 8);
    chunked.Feed(samples.data() + 3, 7, 1, 8);
    chunked.Feed(samples.data() + 10, 6, 1, 8);
    for (size_t frame = 0; frame < samples.size(); ++frame) {
        singleFrames.Feed(samples.data() + frame, 1, 1, 8);
    }
    const RawWindows expected{{0.5f, 0.25f, 0.125f, 0.5f},
                              {1.0f, 0.5f, 0.25f, 1.0f},
                              {1.0f, -0.5f, 0.25f, -1.0f}};
    ExpectRawBitsEqual(whole.Finish().raw, expected);
    ExpectRawBitsEqual(chunked.Finish().raw, expected);
    ExpectRawBitsEqual(singleFrames.Finish().raw, expected);
}

TYPED_TEST(DecodedWaveformAccumulator, ValidSilenceReturnsZeroWithDecodedFormat) {
    const std::vector<TypeParam> silence(32, TypeParam{0});
    DecodedAccumulator<TypeParam> accumulator(4, 2.0);
    accumulator.Feed(silence.data(), 16, 2, 8);
    const DecodedWindows decoded = accumulator.Finish();
    EXPECT_EQ(decoded.sampleRate, 8u);
    EXPECT_EQ(decoded.channels, 2u);
    const RawWindows expected{{0.0f, 0.0f, 0.0f, 0.0f},
                              {0.0f, 0.0f, 0.0f, 0.0f},
                              {0.0f, 0.0f, 0.0f, 0.0f}};
    ExpectRawBitsEqual(decoded.raw, expected);
}

TYPED_TEST(DecodedWaveformAccumulator, EmptyChunksDoNotSetFormatOrAdvanceTime) {
    DecodedAccumulator<TypeParam> accumulator(2, 1.0);
    accumulator.Feed(nullptr, 0, 0, 0);
    accumulator.Feed(nullptr, 0, 6, 2822400);
    const TypeParam first[] = {TypeParam{1}, TypeParam{1}};
    accumulator.Feed(first, 2, 1, 4);
    accumulator.Feed(nullptr, 0, 2, 48000);
    accumulator.Feed(nullptr, 0, 0, 0);
    const TypeParam last[] = {TypeParam{-0.5}, TypeParam{-0.5}};
    accumulator.Feed(last, 2, 1, 4);
    const DecodedWindows decoded = accumulator.Finish();
    EXPECT_EQ(decoded.sampleRate, 4u);
    EXPECT_EQ(decoded.channels, 1u);
    const RawWindows expected{{1.0f, 0.5f}, {1.0f, 0.5f}, {1.0f, -0.5f}};
    ExpectRawBitsEqual(decoded.raw, expected);
}

TYPED_TEST(DecodedWaveformAccumulator, NoFramesCannotProduceSuccessfulWaveform) {
    DecodedAccumulator<TypeParam> untouched(4, 1.0);
    EXPECT_THROW(untouched.Finish(), std::runtime_error);
    DecodedAccumulator<TypeParam> empty(4, 1.0);
    empty.Feed(nullptr, 0, 2, 44100);
    empty.Feed(nullptr, 0, 0, 0);
    EXPECT_THROW(empty.Finish(), std::runtime_error);
}

TYPED_TEST(DecodedWaveformAccumulator, RejectsNonemptyChunksWithZeroSampleRate) {
    const TypeParam samples[] = {TypeParam{1}, TypeParam{-1}};
    DecodedAccumulator<TypeParam> accumulator(4, 1.0);
    EXPECT_THROW(accumulator.Feed(samples, 1, 2, 0), std::runtime_error);
}

TYPED_TEST(DecodedWaveformAccumulator, RejectsNonemptyChunksWithZeroChannels) {
    const TypeParam sample = TypeParam{1};
    DecodedAccumulator<TypeParam> accumulator(4, 1.0);
    EXPECT_THROW(accumulator.Feed(&sample, 1, 0, 44100), std::runtime_error);
}

TYPED_TEST(DecodedWaveformAccumulator, RejectsSampleRateChangesAfterFirstValidChunk) {
    const TypeParam samples[] = {TypeParam{1}, TypeParam{-1}};
    DecodedAccumulator<TypeParam> accumulator(4, 1.0);
    accumulator.Feed(samples, 1, 2, 44100);
    EXPECT_THROW(accumulator.Feed(samples, 1, 2, 48000), std::runtime_error);
}

TYPED_TEST(DecodedWaveformAccumulator, RejectsChannelChangesAfterFirstValidChunk) {
    const TypeParam samples[] = {TypeParam{1}, TypeParam{-1}};
    DecodedAccumulator<TypeParam> accumulator(4, 1.0);
    accumulator.Feed(samples, 1, 2, 44100);
    EXPECT_THROW(accumulator.Feed(samples, 1, 1, 44100), std::runtime_error);
}

TYPED_TEST(DecodedWaveformAccumulator, FormatChangesFailEvenAfterAllWindowsAreFilled) {
    const TypeParam samples[] = {TypeParam{1}, TypeParam{1}, TypeParam{1}, TypeParam{1}};
    DecodedAccumulator<TypeParam> accumulator(4, 1.0);
    accumulator.Feed(samples, 4, 1, 4);
    EXPECT_THROW(accumulator.Feed(samples, 1, 1, 8), std::runtime_error);
}

TYPED_TEST(DecodedWaveformAccumulator, DecodedStereoUsesBothChannelsAndReturnsStereoFormat) {
    const TypeParam samples[] = {TypeParam{0.75}, TypeParam{-0.25},
                                 TypeParam{-0.5}, TypeParam{-1}};
    DecodedAccumulator<TypeParam> accumulator(2, 1.0);
    accumulator.Feed(samples, 2, 2, 2);
    const DecodedWindows decoded = accumulator.Finish();
    EXPECT_EQ(decoded.sampleRate, 2u);
    EXPECT_EQ(decoded.channels, 2u);
    ASSERT_EQ(decoded.raw.rms.size(), 2u);
    EXPECT_FLOAT_EQ(decoded.raw.rms[0], std::sqrt(0.3125f));
    EXPECT_FLOAT_EQ(decoded.raw.rms[1], std::sqrt(0.625f));
    EXPECT_EQ(decoded.raw.peak, (std::vector<float>{0.75f, 1.0f}));
    EXPECT_EQ(decoded.raw.signedPeak, (std::vector<float>{0.25f, -0.75f}));
}

}  // namespace

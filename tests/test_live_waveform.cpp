// test_live_waveform.cpp - audio.getWaveform 取样窗口的纯值计算
// （docs/audio-visualization/SPEC.md D14、§4.9、§10 A4.3）。
//
// 取可视化流的样本在宿主里；这里喂进交错样本，测 'mix' 与 'stereo' 两种应答值与
// 等距取样。参照实现是 6cb14fdd 的 SpectrumState::GetWaveform 里那段逐样本循环的
// 逐字复制，'mix' 的输出须与它逐位相同。
#include "pch.h"
#include "../src/api/LiveWaveform.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>

using fb2k_waveform::MixWindow;
using fb2k_waveform::PickEvenly;
using fb2k_waveform::SplitStereoWindow;

namespace {

// 从 6cb14fdd 的 SpectrumState::GetWaveform 逐字复制（outData.resize 起到循环止）。
// 改动：样本指针写成 double（x64 的 audio_sample），chunk 的三个取值换成参数。
std::vector<float> ReferenceWaveform(const double* data, size_t count, unsigned channels,
                                     bool signedOutput) {
    std::vector<float> outData;
    outData.resize(count);
    for (size_t i = 0; i < count; ++i) {
        float sum = 0;
        for (unsigned ch = 0; ch < channels; ++ch) {
            sum += static_cast<float>(data[i * channels + ch]);
        }
        float linear = sum / channels;
        if (signedOutput) {
            outData[i] = std::max(-1.0f, std::min(1.0f, linear));
        } else {
            float db = 20.0f * log10f(std::max(fabsf(linear), 1e-10f));
            float normalized = (db + 70.0f) / 70.0f;
            outData[i] = std::max(0.0f, std::min(1.0f, normalized));
        }
    }
    return outData;
}

bool BitEqual(const std::vector<float>& a, const std::vector<float>& b) {
    return a.size() == b.size() &&
           (a.empty() || std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0);
}

// 交错样本，frames 帧 × channels 路；幅度超过 ±1，覆盖夹取分支。
std::vector<double> RandomInterleaved(size_t frames, unsigned channels, uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(-1.5, 1.5);
    std::vector<double> data(frames * channels);
    for (double& v : data) v = dist(rng);
    return data;
}

}  // namespace

TEST(LiveWaveform, MixMatchesReferenceBitForBit) {
    const unsigned channelCounts[] = {1, 2, 6};
    const size_t frameCounts[] = {1, 3, 2400};
    uint32_t seed = 1;
    for (unsigned channels : channelCounts) {
        for (size_t frames : frameCounts) {
            const auto data = RandomInterleaved(frames, channels, seed++);
            for (bool signedOutput : {false, true}) {
                std::vector<float> out;
                MixWindow(data.data(), frames, channels, signedOutput, out);
                EXPECT_TRUE(BitEqual(out, ReferenceWaveform(data.data(), frames, channels, signedOutput)))
                    << "channels=" << channels << " frames=" << frames << " signed=" << signedOutput;
            }
        }
    }
}

TEST(LiveWaveform, MixOfSilenceAndEmptyInput) {
    const std::vector<double> zeros(2 * 16, 0.0);
    for (bool signedOutput : {false, true}) {
        std::vector<float> out;
        MixWindow(zeros.data(), 16, 2, signedOutput, out);
        EXPECT_TRUE(BitEqual(out, ReferenceWaveform(zeros.data(), 16, 2, signedOutput)));
        EXPECT_EQ(out.front(), 0.0f);

        MixWindow(zeros.data(), 0, 2, signedOutput, out);
        EXPECT_TRUE(out.empty());
    }
}

TEST(LiveWaveform, ZeroChannelsYieldEmptyResults) {
    const double sample = 0.5;
    std::vector<float> out{1.0f}, left{1.0f}, right{1.0f};
    MixWindow(&sample, 1, 0, true, out);
    SplitStereoWindow(&sample, 1, 0, true, left, right);
    EXPECT_TRUE(out.empty());
    EXPECT_TRUE(left.empty());
    EXPECT_TRUE(right.empty());
}

TEST(LiveWaveform, StereoTakesTheFirstTwoChannels) {
    // 六路：每路一个可辨认的常数，左取第 1 路、右取第 2 路，其余不混入
    const size_t frames = 4;
    std::vector<double> data(frames * 6);
    for (size_t i = 0; i < frames; ++i) {
        for (unsigned ch = 0; ch < 6; ++ch) data[i * 6 + ch] = 0.1 * (ch + 1);
    }
    std::vector<float> left, right;
    SplitStereoWindow(data.data(), frames, 6, true, left, right);
    ASSERT_EQ(left.size(), frames);
    ASSERT_EQ(right.size(), frames);
    for (size_t i = 0; i < frames; ++i) {
        EXPECT_EQ(left[i], static_cast<float>(0.1));
        EXPECT_EQ(right[i], static_cast<float>(0.2));
    }
}

TEST(LiveWaveform, StereoOfOneChannelCopiesLeftAndMatchesMix) {
    const auto data = RandomInterleaved(64, 1, 7);
    for (bool signedOutput : {false, true}) {
        std::vector<float> left, right, mix;
        SplitStereoWindow(data.data(), 64, 1, signedOutput, left, right);
        MixWindow(data.data(), 64, 1, signedOutput, mix);
        EXPECT_TRUE(BitEqual(left, right));
        // 一路时混合只是逐样本收窄再除以 1，与左路逐位相同
        EXPECT_TRUE(BitEqual(left, mix));
    }
}

TEST(LiveWaveform, StereoChannelsFollowTheMixValueRules) {
    // 左声道单独成一路时，应答值与把它当单声道混合的结果相同
    const auto data = RandomInterleaved(128, 2, 11);
    std::vector<double> leftOnly(128), rightOnly(128);
    for (size_t i = 0; i < 128; ++i) {
        leftOnly[i] = data[i * 2];
        rightOnly[i] = data[i * 2 + 1];
    }
    for (bool signedOutput : {false, true}) {
        std::vector<float> left, right, refLeft, refRight;
        SplitStereoWindow(data.data(), 128, 2, signedOutput, left, right);
        MixWindow(leftOnly.data(), 128, 1, signedOutput, refLeft);
        MixWindow(rightOnly.data(), 128, 1, signedOutput, refRight);
        EXPECT_TRUE(BitEqual(left, refLeft));
        EXPECT_TRUE(BitEqual(right, refRight));
    }
}

TEST(LiveWaveform, FloatAndDoubleSamplesAgree) {
    const auto wide = RandomInterleaved(256, 2, 23);
    std::vector<float> narrow(wide.size());
    for (size_t i = 0; i < wide.size(); ++i) narrow[i] = static_cast<float>(wide[i]);
    std::vector<float> fromDouble, fromFloat;
    MixWindow(wide.data(), 256, 2, false, fromDouble);
    MixWindow(narrow.data(), 256, 2, false, fromFloat);
    EXPECT_TRUE(BitEqual(fromDouble, fromFloat));
}

TEST(LiveWaveform, PickEvenlyUsesFloorIndices) {
    std::vector<float> values(10);
    for (size_t i = 0; i < values.size(); ++i) values[i] = static_cast<float>(i);
    PickEvenly(values, 4);
    // floor(i * 10 / 4) = 0, 2, 5, 7
    EXPECT_EQ(values, (std::vector<float>{0.0f, 2.0f, 5.0f, 7.0f}));
}

TEST(LiveWaveform, PickEvenlyLeavesShortInputsAlone) {
    const std::vector<float> original{3.0f, 1.0f, 2.0f};
    for (size_t points : {size_t{0}, size_t{3}, size_t{8}}) {
        std::vector<float> values = original;
        PickEvenly(values, points);
        EXPECT_EQ(values, original) << "points=" << points;
    }
}

TEST(LiveWaveform, PickEvenlyKeepsLeftAndRightAligned) {
    // 两路各自取样时取同一组下标：左右配对不错位
    const size_t n = 2400, points = 256;
    std::vector<float> left(n), right(n);
    for (size_t i = 0; i < n; ++i) {
        left[i] = static_cast<float>(i);
        right[i] = static_cast<float>(i) + 0.5f;
    }
    PickEvenly(left, points);
    PickEvenly(right, points);
    ASSERT_EQ(left.size(), points);
    ASSERT_EQ(right.size(), points);
    for (size_t i = 0; i < points; ++i) {
        EXPECT_EQ(left[i], static_cast<float>(i * n / points));
        EXPECT_EQ(right[i] - left[i], 0.5f);
    }
}

TEST(LiveWaveform, PickEvenlyHandlesLargeProductsWithoutOverflow) {
    // i * n 超过 2^32：Win32 上 32 位 size_t 直接相乘会溢出
    const size_t n = 384000, points = 65536;
    std::vector<float> values(n);
    for (size_t i = 0; i < n; ++i) values[i] = static_cast<float>(i);
    PickEvenly(values, points);
    ASSERT_EQ(values.size(), points);
    EXPECT_EQ(values.back(), static_cast<float>(static_cast<uint64_t>(points - 1) * n / points));
}

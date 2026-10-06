// test_pcm_pack.cpp - 解码块转成平面 f32、混单声道、区间裁剪与截断
// （docs/audio-pcm/SPEC.md §4.2、D8、D16、§10 A-B2）。
//
// 样本类型在 x64 是 double、Win32 是 float，两种都跑一遍。
#include "pch.h"
#include "../src/api/PcmPack.h"

#include <vector>

using fb2k_pcm::ConvertSamples;
using fb2k_pcm::PlanarPacker;
using fb2k_pcm::RangeClipper;

namespace {

// 交错样本：第 i 帧声道 c 的值 = i + c × 0.25，float 与 double 都能精确表示。
template <class Sample>
std::vector<Sample> Interleaved(std::uint32_t frames, std::uint32_t channels, std::uint32_t firstFrame = 0) {
    std::vector<Sample> out;
    for (std::uint32_t i = 0; i < frames; ++i) {
        for (std::uint32_t c = 0; c < channels; ++c) {
            out.push_back(static_cast<Sample>((firstFrame + i) + c * 0.25));
        }
    }
    return out;
}

constexpr float kSentinel = -12345.0f;

}  // namespace

template <class Sample>
class PcmPackTest : public ::testing::Test {};

using SampleTypes = ::testing::Types<float, double>;
TYPED_TEST_CASE(PcmPackTest, SampleTypes);

TYPED_TEST(PcmPackTest, SixChannelsBecomePlanarRuns) {
    constexpr std::uint32_t kChannels = 6;
    constexpr std::uint32_t kCapacity = 10;
    std::vector<float> region(kChannels * kCapacity, kSentinel);
    PlanarPacker<TypeParam> packer(region.data(), kCapacity, kChannels, false);

    // 奇数帧的两块：3 帧 + 4 帧。
    const auto a = Interleaved<TypeParam>(3, kChannels, 0);
    const auto b = Interleaved<TypeParam>(4, kChannels, 3);
    EXPECT_TRUE(packer.Append(a.data(), 3));
    EXPECT_TRUE(packer.Append(b.data(), 4));
    EXPECT_EQ(packer.frames(), 7u);
    EXPECT_FALSE(packer.truncated());
    EXPECT_EQ(packer.outputChannels(), kChannels);

    for (std::uint32_t c = 0; c < kChannels; ++c) {
        for (std::uint32_t i = 0; i < 7; ++i) {
            EXPECT_EQ(region[c * kCapacity + i], static_cast<float>(i + c * 0.25)) << "c=" << c << " i=" << i;
        }
        // 没写到的尾部保持原样。
        for (std::uint32_t i = 7; i < kCapacity; ++i) EXPECT_EQ(region[c * kCapacity + i], kSentinel);
    }
}

TYPED_TEST(PcmPackTest, MonoIsTheArithmeticMeanOfAllChannels) {
    const std::vector<TypeParam> opposite{TypeParam(1), TypeParam(-1), TypeParam(0.5), TypeParam(-0.5)};
    std::vector<float> region(4, kSentinel);
    PlanarPacker<TypeParam> packer(region.data(), 4, 2, true);
    EXPECT_EQ(packer.outputChannels(), 1u);
    packer.Append(opposite.data(), 2);
    EXPECT_EQ(region[0], 0.0f);
    EXPECT_EQ(region[1], 0.0f);

    const std::vector<TypeParam> three{TypeParam(1), TypeParam(2), TypeParam(3)};
    PlanarPacker<TypeParam> triple(region.data() + 2, 2, 3, true);
    triple.Append(three.data(), 1);
    EXPECT_EQ(region[2], 2.0f);
    EXPECT_EQ(region[3], kSentinel);
}

// 写满容量后多出的帧丢掉、标 truncated，且不越界写到区域外。
TYPED_TEST(PcmPackTest, OverflowIsDroppedAndMarksTruncated) {
    constexpr std::uint32_t kCapacity = 5;
    std::vector<float> region(2 * kCapacity + 4, kSentinel);  // 末尾 4 个是哨兵
    PlanarPacker<TypeParam> packer(region.data(), kCapacity, 2, false);
    const auto chunk = Interleaved<TypeParam>(8, 2);
    EXPECT_FALSE(packer.Append(chunk.data(), 8));
    EXPECT_TRUE(packer.truncated());
    EXPECT_EQ(packer.frames(), kCapacity);
    for (std::uint32_t i = 0; i < kCapacity; ++i) {
        EXPECT_EQ(region[i], static_cast<float>(i));
        EXPECT_EQ(region[kCapacity + i], static_cast<float>(i + 0.25));
    }
    for (std::size_t i = 2 * kCapacity; i < region.size(); ++i) EXPECT_EQ(region[i], kSentinel);
}

// 恰好写满不算截断，再来一帧才算。
TYPED_TEST(PcmPackTest, AnExactlyFullRegionIsNotTruncatedUntilMoreArrives) {
    std::vector<float> region(4, kSentinel);
    PlanarPacker<TypeParam> packer(region.data(), 4, 1, false);
    const auto chunk = Interleaved<TypeParam>(4, 1);
    EXPECT_TRUE(packer.Append(chunk.data(), 4));
    EXPECT_FALSE(packer.truncated());
    const auto more = Interleaved<TypeParam>(1, 1, 4);
    EXPECT_FALSE(packer.Append(more.data(), 1));
    EXPECT_TRUE(packer.truncated());
    EXPECT_EQ(region[3], 3.0f);
}

TYPED_TEST(PcmPackTest, ConvertSamplesKeepsOrder) {
    const std::vector<TypeParam> src{TypeParam(0.5), TypeParam(-1), TypeParam(0.125)};
    std::vector<float> dst(3, kSentinel);
    ConvertSamples(src.data(), src.size(), dst.data());
    EXPECT_EQ(dst, (std::vector<float>{0.5f, -1.0f, 0.125f}));
}

// 从头解码时，start 落在块中间：前面的整块全丢，跨 start 的那块只丢到 start 为止。
TEST(PcmRangeClipper, StartInsideAChunkSkipsExactlyTheFramesBeforeIt) {
    RangeClipper clip(150, 260, 0);
    auto s = clip.Next(100);  // [0, 100)
    EXPECT_EQ(s.skip, 100u);
    EXPECT_EQ(s.take, 0u);
    EXPECT_FALSE(s.done);
    s = clip.Next(100);  // [100, 200)
    EXPECT_EQ(s.skip, 50u);
    EXPECT_EQ(s.take, 50u);
    EXPECT_FALSE(s.done);
    s = clip.Next(100);  // [200, 300)，end 在 260
    EXPECT_EQ(s.skip, 0u);
    EXPECT_EQ(s.take, 60u);
    EXPECT_TRUE(s.done);
    EXPECT_EQ(clip.position(), 300u);
}

// seek 成功后第一块就从 start 开始，没有要丢的帧；不给 end 时一直取到曲目结束。
TEST(PcmRangeClipper, AfterASeekNothingIsSkippedAndNoEndMeansToTheEnd) {
    RangeClipper clip(48000, std::nullopt, 48000);
    auto s = clip.Next(4096);
    EXPECT_EQ(s.skip, 0u);
    EXPECT_EQ(s.take, 4096u);
    EXPECT_FALSE(s.done);
    s = clip.Next(10);
    EXPECT_EQ(s.take, 10u);
    EXPECT_FALSE(s.done);
}

// 区间正好在块边界结束：这块全取、done；区间已结束后再来的块一帧不取。
TEST(PcmRangeClipper, RangeEndingOnAChunkBoundary) {
    RangeClipper clip(0, 200, 0);
    auto s = clip.Next(100);
    EXPECT_EQ(s.take, 100u);
    EXPECT_FALSE(s.done);
    s = clip.Next(100);
    EXPECT_EQ(s.take, 100u);
    EXPECT_TRUE(s.done);
    s = clip.Next(100);
    EXPECT_EQ(s.take, 0u);
    EXPECT_EQ(s.skip, 100u);
    EXPECT_TRUE(s.done);
}

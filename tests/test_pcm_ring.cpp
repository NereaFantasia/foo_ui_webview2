// test_pcm_ring.cpp - 环形缓冲写入的分段、块比环长、2^32 回绕与 seqlock 计数
// （docs/audio-pcm/SPEC.md §4.1、D10、D11、§10 A-B3）。
//
// 写端直接操作裸内存：这里用堆上按 PcmLayout 布好的缓冲代替共享缓冲。
#include "pch.h"
#include "../src/api/PcmLayout.h"
#include "../src/api/PcmRing.h"

#include <cstring>
#include <vector>

using namespace fb2k_pcm;

namespace {

std::vector<std::uint8_t> MakeRing(std::uint32_t capacity, std::uint32_t channels, std::uint32_t writeFrames = 0,
                                   std::uint32_t writeSlot = 0) {
    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(BufferBytes(capacity, channels)), 0);
    Header h;
    h.mode = BufferMode::Ring;
    h.sampleRate = 48000;
    h.channels = channels;
    h.capacityFrames = capacity;
    h.writeFrames = writeFrames;
    h.writeSlot = writeSlot;
    EncodeHeader(h, buffer.data());
    return buffer;
}

// 交错块：帧号 n 的每个声道都等于 n + c × 0.25。
template <class Sample>
std::vector<Sample> Chunk(std::uint32_t firstFrame, std::uint32_t frames, std::uint32_t channels) {
    std::vector<Sample> out;
    for (std::uint32_t i = 0; i < frames; ++i) {
        for (std::uint32_t c = 0; c < channels; ++c) out.push_back(static_cast<Sample>((firstFrame + i) + c * 0.25));
    }
    return out;
}

float SampleAt(const std::vector<std::uint8_t>& ring, std::uint32_t slot, std::uint32_t channel) {
    const std::uint32_t channels = LoadU32(ring.data(), offsets::kChannels);
    float value;
    std::memcpy(&value, ring.data() + InterleavedSampleOffset(channels, slot, channel), sizeof(value));
    return value;
}

}  // namespace

// 容量 100 连写 40、40、40：前两块不跨界，第三块拆成 20 + 20。
TEST(PcmRingPlan, ChunksSplitOnlyWhereTheyCrossTheEnd) {
    auto p = PlanRingWrite(0, 0, 100, 40);
    ASSERT_EQ(p.segmentCount, 1u);
    EXPECT_EQ(p.segments[0].slot, 0u);
    EXPECT_EQ(p.segments[0].frames, 40u);
    p = PlanRingWrite(p.nextWriteFrames, p.nextWriteSlot, 100, 40);
    ASSERT_EQ(p.segmentCount, 1u);
    EXPECT_EQ(p.segments[0].slot, 40u);
    p = PlanRingWrite(p.nextWriteFrames, p.nextWriteSlot, 100, 40);
    ASSERT_EQ(p.segmentCount, 2u);
    EXPECT_EQ(p.segments[0].sourceFrame, 0u);
    EXPECT_EQ(p.segments[0].slot, 80u);
    EXPECT_EQ(p.segments[0].frames, 20u);
    EXPECT_EQ(p.segments[1].sourceFrame, 20u);
    EXPECT_EQ(p.segments[1].slot, 0u);
    EXPECT_EQ(p.segments[1].frames, 20u);
    EXPECT_EQ(p.nextWriteFrames, 120u);
    EXPECT_EQ(p.nextWriteSlot, 20u);
}

// 块比环长（interval 1 s 配 bufferSeconds 0.1 s 就会这样）：只落最后 capacity 帧，
// 计数照加整块，槽位照整块推进。
TEST(PcmRingPlan, AChunkLongerThanTheRingKeepsOnlyItsTail) {
    const auto p = PlanRingWrite(5, 5, 100, 250);
    EXPECT_EQ(p.skippedFrames, 150u);
    ASSERT_EQ(p.segmentCount, 2u);
    EXPECT_EQ(p.segments[0].sourceFrame, 150u);
    EXPECT_EQ(p.segments[0].slot, 55u);
    EXPECT_EQ(p.segments[0].frames, 45u);
    EXPECT_EQ(p.segments[1].sourceFrame, 195u);
    EXPECT_EQ(p.segments[1].slot, 0u);
    EXPECT_EQ(p.segments[1].frames, 55u);
    EXPECT_EQ(p.nextWriteFrames, 255u);
    EXPECT_EQ(p.nextWriteSlot, 55u);
}

// writeFrames 在 2^32 − 10 处写 20 帧回绕到 10，槽位不跟着跳：48000 不整除 2^32。
TEST(PcmRingPlan, SlotsStayContinuousAcrossTheCounterWrap) {
    const std::uint32_t nearWrap = 0xFFFFFFFFu - 9;
    const auto p = PlanRingWrite(nearWrap, 47990, 48000, 20);
    EXPECT_EQ(p.nextWriteFrames, 10u);
    EXPECT_EQ(p.nextWriteSlot, 10u);
    ASSERT_EQ(p.segmentCount, 2u);
    EXPECT_EQ(p.segments[0].slot, 47990u);
    EXPECT_EQ(p.segments[0].frames, 10u);
    EXPECT_EQ(p.segments[1].slot, 0u);
    EXPECT_EQ(p.segments[1].frames, 10u);
}

template <class Sample>
class PcmRingWriteTest : public ::testing::Test {};

using SampleTypes = ::testing::Types<float, double>;
TYPED_TEST_CASE(PcmRingWriteTest, SampleTypes);

// 连写多块、总量超过容量后，最近 capacity 帧都能按 writeSlot 往回数取到；每次写后 seq 加 2。
TYPED_TEST(PcmRingWriteTest, TheLastCapacityFramesAreReadableBackFromWriteSlot) {
    constexpr std::uint32_t kCapacity = 100;
    constexpr std::uint32_t kChannels = 2;
    auto ring = MakeRing(kCapacity, kChannels);
    std::uint32_t total = 0;
    std::uint32_t expectedSeq = 0;
    for (std::uint32_t frames : {40u, 40u, 40u, 7u, 250u, 13u}) {
        const auto chunk = Chunk<TypeParam>(total, frames, kChannels);
        ASSERT_TRUE(WriteRingChunk(ring.data(), chunk.data(), frames, kChannels, 1000.0 + total));
        total += frames;
        expectedSeq += 2;
        EXPECT_EQ(LoadU32(ring.data(), offsets::kSeq), expectedSeq);
    }
    EXPECT_EQ(LoadU32(ring.data(), offsets::kWriteFrames), total);
    EXPECT_EQ(LoadF64(ring.data(), offsets::kHostTimeMs), 1000.0 + (total - 13));

    const std::uint32_t writeSlot = LoadU32(ring.data(), offsets::kWriteSlot);
    for (std::uint32_t d = 1; d <= kCapacity; ++d) {
        const std::uint32_t slot = RingSlotBack(writeSlot, kCapacity, d);
        EXPECT_EQ(SampleAt(ring, slot, 0), static_cast<float>(total - d)) << "d=" << d;
        EXPECT_EQ(SampleAt(ring, slot, 1), static_cast<float>(total - d + 0.25)) << "d=" << d;
    }
}

// 计数跨过 2^32 时写入的帧按 writeSlot 仍落在连续的槽里。
TYPED_TEST(PcmRingWriteTest, WritesAcrossTheCounterWrapLandInOrder) {
    constexpr std::uint32_t kCapacity = 48000;
    auto ring = MakeRing(kCapacity, 1, 0xFFFFFFFFu - 9, 47990);
    const auto chunk = Chunk<TypeParam>(0, 20, 1);
    ASSERT_TRUE(WriteRingChunk(ring.data(), chunk.data(), 20, 1, 0.0));
    EXPECT_EQ(LoadU32(ring.data(), offsets::kWriteFrames), 10u);
    const std::uint32_t writeSlot = LoadU32(ring.data(), offsets::kWriteSlot);
    EXPECT_EQ(writeSlot, 10u);
    for (std::uint32_t d = 1; d <= 20; ++d) {
        EXPECT_EQ(SampleAt(ring, RingSlotBack(writeSlot, kCapacity, d), 0), static_cast<float>(20 - d));
    }
}

// 块的声道数与缓冲不一致时不动缓冲：由调用方换代。
TYPED_TEST(PcmRingWriteTest, AChannelMismatchLeavesTheBufferUntouched) {
    auto ring = MakeRing(10, 2);
    const auto before = ring;
    const auto chunk = Chunk<TypeParam>(0, 4, 6);
    EXPECT_FALSE(WriteRingChunk(ring.data(), chunk.data(), 4, 6, 1.0));
    EXPECT_EQ(ring, before);
}

TEST(PcmRingEnded, MarkingEndedSetsTheFlagUnderTheSeqlock) {
    auto ring = MakeRing(10, 2);
    StoreU32(ring.data(), offsets::kFlags, kFlagResampled);
    MarkRingEnded(ring.data());
    EXPECT_EQ(LoadU32(ring.data(), offsets::kFlags), kFlagResampled | kFlagEnded);
    EXPECT_EQ(LoadU32(ring.data(), offsets::kSeq), 2u);
}

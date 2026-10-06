#include "pch.h"
#include "api/PcmRing.h"

#include "api/PcmLayout.h"
#include "api/PcmPack.h"

#include <algorithm>
#include <atomic>

namespace fb2k_pcm {

namespace {

std::atomic_ref<std::uint32_t> SeqRef(std::uint8_t* base) {
    static_assert(std::atomic_ref<std::uint32_t>::required_alignment <= alignof(std::uint32_t));
    return std::atomic_ref<std::uint32_t>(*reinterpret_cast<std::uint32_t*>(base + offsets::kSeq));
}

// Opens a write: seq turns odd. The release fence keeps the stores that follow
// from being moved before it; a release or seq_cst store alone only orders the
// stores in front of it.
std::uint32_t BeginWrite(std::uint8_t* base) {
    auto seq = SeqRef(base);
    const std::uint32_t even = seq.load(std::memory_order_relaxed);
    seq.store(even + 1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    return even;
}

void EndWrite(std::uint8_t* base, std::uint32_t even) {
    SeqRef(base).store(even + 2, std::memory_order_release);
}

}  // namespace

RingWritePlan PlanRingWrite(std::uint32_t writeFrames, std::uint32_t writeSlot, std::uint32_t capacityFrames,
                            std::uint32_t chunkFrames) {
    RingWritePlan plan;
    plan.nextWriteFrames = writeFrames + chunkFrames;  // wraps at 2^32 by design
    if (capacityFrames == 0) return plan;
    plan.nextWriteSlot = static_cast<std::uint32_t>((static_cast<std::uint64_t>(writeSlot) + chunkFrames) % capacityFrames);

    plan.skippedFrames = chunkFrames > capacityFrames ? chunkFrames - capacityFrames : 0;
    const std::uint32_t kept = chunkFrames - plan.skippedFrames;
    if (kept == 0) return plan;

    const auto firstSlot =
        static_cast<std::uint32_t>((static_cast<std::uint64_t>(writeSlot) + plan.skippedFrames) % capacityFrames);
    const std::uint32_t head = std::min(kept, capacityFrames - firstSlot);
    plan.segments[0] = {plan.skippedFrames, firstSlot, head};
    plan.segmentCount = 1;
    if (head < kept) {
        plan.segments[1] = {plan.skippedFrames + head, 0, kept - head};
        plan.segmentCount = 2;
    }
    return plan;
}

template <class Sample>
bool WriteRingChunk(std::uint8_t* base, const Sample* interleaved, std::uint32_t frames, std::uint32_t channels,
                    double hostTimeMs) {
    const std::uint32_t ringChannels = LoadU32(base, offsets::kChannels);
    const std::uint32_t capacity = LoadU32(base, offsets::kCapacityFrames);
    if (channels != ringChannels || channels == 0 || capacity == 0) return false;

    const RingWritePlan plan =
        PlanRingWrite(LoadU32(base, offsets::kWriteFrames), LoadU32(base, offsets::kWriteSlot), capacity, frames);
    auto* data = reinterpret_cast<float*>(base + kHeaderBytes);

    const std::uint32_t even = BeginWrite(base);
    for (std::uint32_t i = 0; i < plan.segmentCount; ++i) {
        const RingSegment& segment = plan.segments[i];
        ConvertSamples(interleaved + static_cast<std::size_t>(segment.sourceFrame) * channels,
                       static_cast<std::size_t>(segment.frames) * channels,
                       data + static_cast<std::size_t>(segment.slot) * channels);
    }
    StoreU32(base, offsets::kWriteFrames, plan.nextWriteFrames);
    StoreU32(base, offsets::kWriteSlot, plan.nextWriteSlot);
    StoreF64(base, offsets::kHostTimeMs, hostTimeMs);
    EndWrite(base, even);
    return true;
}

void MarkRingEnded(std::uint8_t* base) {
    const std::uint32_t even = BeginWrite(base);
    StoreU32(base, offsets::kFlags, LoadU32(base, offsets::kFlags) | kFlagEnded);
    EndWrite(base, even);
}

template bool WriteRingChunk<float>(std::uint8_t*, const float*, std::uint32_t, std::uint32_t, double);
template bool WriteRingChunk<double>(std::uint8_t*, const double*, std::uint32_t, std::uint32_t, double);

}  // namespace fb2k_pcm

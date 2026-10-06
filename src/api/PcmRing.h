// PcmRing.h - writing interleaved frames into a shared PCM ring buffer
//
/* No foobar2000 SDK or WebView2 dependency: the writer works on raw memory, so
 * the host passes the shared buffer's address and tests pass a heap block laid
 * out by PcmLayout.h. Every write follows the seqlock order the page's reader
 * relies on: seq turns odd, then the samples, writeFrames, writeSlot and
 * hostTimeMs, then seq turns even again. The page gets the buffer read-only and
 * cannot report how far it has read, so the writer never waits: it overwrites
 * the oldest frames and the reader works out what it missed.
 */
#pragma once

#include <cstdint>

namespace fb2k_pcm {

// One contiguous copy: frames [sourceFrame, sourceFrame + frames) of the chunk
// go to ring slots [slot, slot + frames).
struct RingSegment {
    std::uint32_t sourceFrame = 0;
    std::uint32_t slot = 0;
    std::uint32_t frames = 0;
};

struct RingWritePlan {
    RingSegment segments[2];
    std::uint32_t segmentCount = 0;
    // Leading frames of a chunk longer than the ring. They are not copied, since
    // later frames of the same chunk would overwrite them, but still count in
    // writeFrames and writeSlot.
    std::uint32_t skippedFrames = 0;
    std::uint32_t nextWriteFrames = 0;
    std::uint32_t nextWriteSlot = 0;
};

// Plans writing a chunk of `chunkFrames` frames into a ring of `capacityFrames`
// frames whose header currently holds writeFrames and writeSlot.
RingWritePlan PlanRingWrite(std::uint32_t writeFrames, std::uint32_t writeSlot, std::uint32_t capacityFrames,
                            std::uint32_t chunkFrames);

// Writes one chunk of interleaved samples into the ring buffer at `base`, whose
// header was encoded with BufferMode::Ring. Returns false without touching the
// buffer when `channels` differs from the buffer's channel count; the caller
// starts a new epoch instead.
template <class Sample>
bool WriteRingChunk(std::uint8_t* base, const Sample* interleaved, std::uint32_t frames, std::uint32_t channels,
                    double hostTimeMs);

// Sets the ended flag under the same seqlock order, so a reader that sees it
// also sees the final writeFrames.
void MarkRingEnded(std::uint8_t* base);

}  // namespace fb2k_pcm

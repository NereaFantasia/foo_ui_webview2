// WaveformAccumulator.h - one-pass window accumulation for full-track waveforms
//
/* No foobar2000 SDK dependency. The decode loop feeds interleaved chunks in;
 * the accumulator keeps, for every window, the three raw values the host used
 * to obtain from three separate decodes (window RMS, peak, signed peak).
 * Render() then derives the waveform for any method / signed / scale
 * combination, so a cache entry holding the raw values answers every variant
 * of a request without decoding the track again.
 *
 * The arithmetic reproduces the previous single-method decode loop bit for
 * bit: samples are narrowed to float first, per-frame channel reductions and
 * per-window sums happen in float in arrival order, and normalisation divides
 * by the same maximum. The unit test compares against a verbatim copy of that
 * loop. The one deliberate difference is a chunk reporting zero channels: the
 * old loop divided by zero and counted NaN frames into the window, this one
 * skips them.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace fb2k_waveform {

enum class Method { Rms, Peak };
enum class Scale { Linear, Db };

// Per-window values in linear full-scale units, `resolution` entries each.
// Windows the decoded audio never reached stay 0.
struct RawWindows {
    // sqrt of the window mean of the per-frame channel mean square.
    std::vector<float> rms;
    // Largest |sample| over the window and all channels.
    std::vector<float> peak;
    // Per-frame channel mean with the largest magnitude in the window, sign
    // kept; the first such frame wins on ties.
    std::vector<float> signedPeak;
};

// `Sample` follows audio_sample: double on x64, float on Win32. Both are
// instantiated.
template <typename Sample>
class Accumulator {
public:
    // `resolution` windows of `samplesPerWindow` frames each. A
    // samplesPerWindow of 0 is treated as 1, as the host does when a track is
    // shorter than the requested resolution; a resolution below 1 yields no
    // windows.
    Accumulator(int resolution, uint64_t samplesPerWindow);

    // `data` holds `frames` interleaved frames of `channels` samples. Frames
    // past resolution * samplesPerWindow are ignored: the window count is
    // fixed by the caller's duration estimate, and audio beyond it would have
    // no window to land in.
    void Feed(const Sample* data, size_t frames, unsigned channels);

    // Closes the window still open and hands the raw values out. Feed() must
    // not be called afterwards.
    RawWindows Finish();

private:
    void CloseWindow();

    int resolution_;
    uint64_t samplesPerWindow_;
    uint64_t processed_ = 0;
    int currentWindow_ = 0;
    bool windowOpen_ = false;
    float rmsSum_ = 0.0f;
    size_t rmsCount_ = 0;
    float peakMax_ = 0.0f;
    float signedBest_ = 0.0f;
    RawWindows out_;
};

struct RenderOptions {
    Method method = Method::Rms;
    Scale scale = Scale::Linear;
    // Takes the signed peak sequence regardless of `method`; `scale` is then
    // ignored, as before.
    bool signedOutput = false;
};

struct Rendered {
    // Normalised to [0, 1], or to [-1, 1] for signed output; on the 'db'
    // scale each value is (dB + 60) / 60 clamped to [0, 1].
    std::vector<float> waveform;
    // The divisor used for normalisation: the largest value of the selected
    // raw sequence (largest magnitude for signed output), in linear full-scale
    // units. 0 when the sequence is silent, in which case nothing was divided.
    float maxAmplitude = 0.0f;
};

Rendered Render(const RawWindows& raw, const RenderOptions& options);

}  // namespace fb2k_waveform

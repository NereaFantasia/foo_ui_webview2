// LiveWaveform.h - sample window of the visualisation stream for audio.getWaveform
//
/* No foobar2000 SDK dependency. The host hands over the interleaved samples of
 * the chunk visualisation_stream::get_chunk_absolute() returned; this unit turns
 * them into the per-sample response values of the 'mix' or 'stereo' shape and
 * thins them out, so the value rules run in tests without a host.
 */
#pragma once

#include <cstddef>
#include <vector>

namespace fb2k_waveform {

// Mono mix: the channels of each frame are averaged. With signedOutput the mean
// is clamped to [-1, 1]; otherwise |mean| is converted to dB and mapped from
// -70..0 dB onto [0, 1]. Reproduces the host's earlier loop bit for bit; the one
// deliberate difference is `channels` 0, which that loop divided by and which
// yields an empty result here. `Sample` follows audio_sample (double on x64,
// float on Win32); both are instantiated.
template <typename Sample>
void MixWindow(const Sample* data, size_t frames, unsigned channels, bool signedOutput,
               std::vector<float>& out);

// First two channels, each sample converted as MixWindow converts a mono frame.
// With one channel `right` is a copy of `left`; channels beyond the second are
// ignored rather than mixed in. `channels` 0 empties both.
template <typename Sample>
void SplitStereoWindow(const Sample* data, size_t frames, unsigned channels, bool signedOutput,
                       std::vector<float>& left, std::vector<float>& right);

// Even thinning without averaging: averaging would blur the sample-by-sample
// relation between the two channels that stereo displays are built on. Output i
// is input floor(i * n / points), n being the input length; inputs no longer
// than `points`, and `points` 0, are left untouched.
void PickEvenly(std::vector<float>& values, size_t points);

}  // namespace fb2k_waveform

// SpectrumBands.h - map an FFT magnitude spectrum to log-spaced display bands
//
/* No foobar2000 SDK dependency. The caller hands over the interleaved buffer
 * that visualisation_stream::get_spectrum_absolute() wrote into its
 * audio_chunk, so the band mapping runs in tests without a host.
 */
#pragma once

#include <cstddef>
#include <vector>

namespace fb2k_spectrum {

// Frequency range of the bands in Hz. The upper edge is
// min(maxFrequency, sampleRate / 2), with maxFrequency 0 meaning sampleRate / 2;
// both scales default to 20 Hz .. sampleRate / 2, the range the snapshot tests
// lock bit for bit. When the resulting upper edge is not above minFrequency
// (a range above this stream's Nyquist frequency), every band gets the
// scale's silence value: 0 for 'weighted', kDbBandsFloor for 'db'.
constexpr double kDefaultMinFrequency = 20.0;

// Weighted display bands: log-spaced over [minFrequency, upper edge], triangular
// band filter, +1.5 dB/octave tilt, bass shelf below 200 Hz, then mapped from
// -50..0 dB onto [0, 1] with gamma 0.8. The values are a display curve, not dB.
// Tilt and shelf follow each band's absolute centre frequency, so narrowing the
// range does not shift the curve.
//
// `data` holds `binCount` bins of `channels` interleaved magnitudes each (bin i
// of channel ch at data[i * channels + ch]); channels are averaged. `Sample`
// follows audio_sample: double on x64, float on Win32. Both are instantiated.
//
// `sampleRate` 0 is treated as 44100. `fftSize` must be the FFT size the
// spectrum was actually computed with, after any automatic raise, so that
// binCount == fftSize / 2; passing the requested size instead silently changes
// the bin width. `bands` <= 0 or larger than `binCount` yields `binCount` bands.
// `out` is resized to the band count.
template <typename Sample>
void ComputeWeightedBands(const Sample* data, size_t binCount, unsigned channels,
                          unsigned sampleRate, int fftSize, int bands,
                          std::vector<float>& out,
                          double minFrequency = kDefaultMinFrequency, double maxFrequency = 0.0);

// Calibration constant C of the 'db' scale, added to every band value: a
// full-scale sine reads 0 dB summed over the bands its main lobe falls into,
// which is a single band unless the sine sits near a band edge. Measured on the
// host's KStreamFlagNewFFT spectrum: a 0 dBFS 1 kHz sine at 48 kHz summed to
// +2.75 dB before calibration at 48, 64 and 256 bands and at every FFT size
// from 2048 to 65536 (spread 0.003 dB).
constexpr double kDbBandsCalibration = -2.75;

// Floor of the 'db' scale; also the value of a band that holds no power.
constexpr float kDbBandsFloor = -160.0f;

// Power bands in dB: value = 10 * log10(sum of bin powers in the band) + C,
// floored at kDbBandsFloor. No weighting and no display curve: wherever no band
// is empty (see below), the total power of a signal does not depend on the
// band count.
//
// Bands divide the frequency range logarithmically, as the weighted scale
// does; band b is the half-open range [f_b, f_b+1). Bin k (centre frequency
// k * sampleRate / fftSize) falls into exactly one band; bins below
// minFrequency or at and above the upper edge, and the DC bin, are not summed
// into any band. With the default upper edge no bin reaches it, since the
// highest bin sits below sampleRate / 2. The power of a bin is the mean over
// channels of the squared magnitude.
//
// A band narrower than a bin can end up with no bin at all. Its power is then
// interpolated linearly between the two bins around the band's geometric
// centre, with bin 1 standing in for the DC bin; a neighbour below 20 Hz is
// used as is. Such bands do not conserve power, and they only occur where
// bands are narrower than a bin: at the low end, or across a narrow range.
//
// Arguments follow ComputeWeightedBands: same data layout, same fftSize
// precondition, same fallbacks for sampleRate 0 and for the band count.
template <typename Sample>
void ComputeDbBands(const Sample* data, size_t binCount, unsigned channels,
                    unsigned sampleRate, int fftSize, int bands,
                    std::vector<float>& out,
                    double minFrequency = kDefaultMinFrequency, double maxFrequency = 0.0);

}  // namespace fb2k_spectrum

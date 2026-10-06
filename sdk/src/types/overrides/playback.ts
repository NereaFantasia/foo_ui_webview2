/**
 * Response types for the `playback.*` namespace.
 *
 * `playback.getCurrentTrack` returns {@link TrackInfo} while a track is
 * loaded, or {@link PlaybackNoTrackResponse} when no track is loaded.
 * Test for `found` before reading track metadata; neither response is `null`.
 */

import type { TrackInfo } from '../responses.js';

/**
 * Resolved by `playback.getCurrentTrack` when no track is loaded.
 *
 * `found: false` is the discriminant: {@link TrackInfo} never carries a
 * `found` key, so `'found' in result` narrows the union on both sides. The
 * host never resolves this call with `null`.
 */
export interface PlaybackNoTrackResponse {
    success: true;
    found: false;
    playing: false;
}

/**
 * Response from `playback.getCurrentTrack`.
 *
 * Either the current track or {@link PlaybackNoTrackResponse}; see that type
 * for how to tell the two apart.
 *
 * @codegen-override response:playback.getCurrentTrack
 * @codegen-snapshot found:primitive,playing:primitive,success:primitive
 */
export type PlaybackGetCurrentTrackResponse = TrackInfo | PlaybackNoTrackResponse;

/**
 * `state` — reactive playback state mirror.
 *
 * The exported {@link state} object is a plain mutable container that
 * the SDK keeps in sync with host-emitted playback events. Consumers
 * read it imperatively (e.g. `if (fb.state.isPlaying) ...`) — there is
 * no reactivity framework attached; the contract is "fields update
 * before the next animation frame after every relevant event".
 *
 * Five host events are wired:
 *
 * | Event                       | Effect on `state`                            |
 * |-----------------------------|----------------------------------------------|
 * | `playback:stateChanged`     | sync `isPlaying` + `position` (when present) |
 * | `playback:trackChanged`     | sync `currentTrack`; force `isPlaying = true` |
 * | `playback:stopped`          | reset `isPlaying = false`, `position = 0`    |
 * | `playback:volumeChanged`    | sync `volume`                                |
 * | `playback:time`             | sync `position` (high-frequency tick)        |
 */

import { bridge } from './Bridge.js';
import type { TrackInfo } from '../types/responses.js';

/**
 * Mutable shape exposed via `fb.state` / `import { state } from
 * 'foo-webview-sdk/bridge'`.
 */
export interface ReactiveState {
    /** Output volume, linear 0-100 scale. */
    volume: number;
    /** `true` when playback is `'playing'`; `false` for paused/stopped. */
    isPlaying: boolean;
    /**
     * Most recently announced now-playing track. `null` when no track
     * has played since the SDK loaded.
     */
    currentTrack: TrackInfo | null;
    /** Current playback position in seconds. */
    position: number;
}

/**
 * Singleton reactive playback-state mirror. Mutated by the event
 * handlers wired below.
 */
export const state: ReactiveState = {
    volume: 0,
    isPlaying: false,
    currentTrack: null,
    position: 0,
};

// ── Event wiring ────────────────────────────────────────────

bridge.on('playback:stateChanged', (data) => {
    if (data.state !== undefined) {
        state.isPlaying = data.state === 'playing';
    }
    if (data.position !== undefined) {
        state.position = data.position;
    }
});

bridge.on('playback:trackChanged', (data) => {
    state.currentTrack = data;
    state.isPlaying = true;
});

bridge.on('playback:stopped', () => {
    state.isPlaying = false;
    state.position = 0;
});

bridge.on('playback:volumeChanged', (data) => {
    if (data.volume !== undefined) {
        state.volume = data.volume;
    }
});

bridge.on('playback:time', (data) => {
    if (data.position !== undefined) {
        state.position = data.position;
    }
});

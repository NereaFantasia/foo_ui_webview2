import { call } from '../call.js';
import type {
    PlaybackPlayPathsResponse,
} from '../../types/generated/responses.js';
import type { PlaybackOrder } from '../../types/responses.js';

/**
 * `player` — playback control namespace.
 */
export const player = {
    /** Start playback, or resume when paused. */
    play: () => call('playback.play'),
    /** Pause playback. Does nothing when stopped. */
    pause: () => call('playback.pause'),
    /** Stop playback. */
    stop: () => call('playback.stop'),
    /** Skip to the next track under the current playback order. */
    next: () => call('playback.next'),
    /** Skip to the previous track under the current playback order. */
    prev: () => call('playback.previous'),
    /** Start a random track of the active playlist. */
    random: () => call('playback.random'),
    /** Toggle between playing and paused; starts playback when stopped. */
    toggle: () =>
        call('playback.playOrPause'),
    /**
     * Seek within the current track. The host clamps the position to the
     * track and reports both the requested and the applied value.
     */
    seek: (seconds: number) =>
        call('playback.setPosition', {
            position: seconds,
        }),
    /** Report the output volume on both scales the host uses, and whether it is muted. */
    getVolume: () =>
        call('playback.getVolume'),
    /** Set the output volume as a percentage; values outside `0` to `100` are clamped. `0` mutes. */
    setVolume: (volume: number) =>
        call('playback.setVolume', {
            volume,
        }),
    /** Mute by default; pass `false` to unmute. */
    mute: (muted = true) =>
        call('playback.mute', {
            muted,
        }),
    /** Flip the mute state and report the new one. */
    toggleMute: () =>
        call('playback.toggleMute'),
    /** Report the transport state and what the current track allows. */
    getState: () =>
        call('playback.getState'),
    /**
     * Resolves with `found` and, when a track is loaded, the shared `track`
     * row. The host never answers `null`.
     */
    getCurrentTrack: () =>
        call(
            'playback.getCurrentTrack',
        ),
    /**
     * Report the playback position together with the length and identity of the track.
     * All values are zero or empty when nothing is playing.
     */
    getPosition: () =>
        call('playback.getPosition'),
    /** Report the active playback order as its index and its name. */
    getOrder: () =>
        call(
            'playback.getPlaybackOrder',
        ),
    /** Set the active playback order by index or by name; the host reports what took effect. */
    setOrder: (order: PlaybackOrder | number) =>
        call(
            'playback.setPlaybackOrder',
            (typeof order === 'number'
                ? { order }
                : { name: order }),
        ),
    /** Report whether playback stops after the current track. */
    getStopAfterCurrent: () =>
        call(
            'playback.getStopAfterCurrent',
        ),
    /** Arm or disarm stopping after the current track. */
    setStopAfterCurrent: (enabled: boolean) =>
        call(
            'playback.setStopAfterCurrent',
            { enabled },
        ),
    /**
     * Locate the playing track in its playlist.
     *
     * @param includeTrackInfo - When `true`, also returns track metadata.
     */
    getCurrentTrackIndex: (includeTrackInfo?: boolean) =>
        call(
            'playback.getCurrentTrackIndex',
            {
                includeTrackInfo: !!includeTrackInfo,
            },
        ),
    /** Report the playlist playback was last started from. */
    getPlayingPlaylist: () =>
        call(
            'playback.getPlayingPlaylist',
        ),
    /**
     * Append one file to the active playlist and play it; creates a playlist when none exists.
     * A `|subsong:N` suffix selects a subsong of the file.
     */
    playPath: (path: string) =>
        call('playback.playPath', {
            path,
        }),
    /**
     * Begin playback of the supplied paths.
     *
     * Accepts either a single numeric `startIndex` matching the
     * original `(paths, startIndex)` signature or an options object
     * `{ startIndex?, replace? }` that additionally controls whether
     * the active playlist is cleared before insertion.
     *
     * @param paths - Absolute file paths. Each entry may carry a
     *                `|subsong:N` suffix for CUE tracks.
     * @param options - Numeric `startIndex` (0-based offset into the
     *                  active playlist), **or** an options object:
     *                  - `startIndex` selects the entry to start
     *                    playback from (0-based);
     *                  - `replace: true` clears the active playlist
     *                    before inserting; `false` (default) appends.
     */
    playPaths: ((
        paths: string[],
        options?:
            | number
            | { startIndex?: number; replace?: boolean },
    ): Promise<PlaybackPlayPathsResponse> => {
        const opts =
            typeof options === 'number' || options === undefined
                ? { startIndex: options }
                : options;
        return call(
            'playback.playPaths',
            {
                paths,
                ...(opts.startIndex != null
                    ? { startIndex: opts.startIndex }
                    : {}),
                ...(opts.replace != null ? { replace: opts.replace } : {}),
            },
        );
    }) as {
        (paths: string[], startIndex?: number): Promise<PlaybackPlayPathsResponse>;
        (
            paths: string[],
            opts: { startIndex?: number; replace?: boolean },
        ): Promise<PlaybackPlayPathsResponse>;
    },
    /** Toggle between playing and paused; starts playback when stopped. */
    playPause: () =>
        call('playback.playPause'),
    /** Flip the stop-after-current flag and report the new value. */
    toggleStopAfterCurrent: () =>
        call(
            'playback.toggleStopAfterCurrent',
        ),
    /** Raise the volume by one step (1 dB). */
    volumeUp: () => call('playback.volumeUp'),
    /** Lower the volume by one step (1 dB). */
    volumeDown: () =>
        call('playback.volumeDown'),
};

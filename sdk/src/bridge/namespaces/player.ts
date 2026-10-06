import { call } from '../call.js';
import type {
    PlaybackPlayPathsResponse,
} from '../../types/generated/responses.js';
import type { PlaybackOrder } from '../../types/responses.js';

/**
 * `player` — playback control namespace.
 */
export const player = {
    play: () => call('playback.play'),
    pause: () => call('playback.pause'),
    stop: () => call('playback.stop'),
    next: () => call('playback.next'),
    prev: () => call('playback.previous'),
    random: () => call('playback.random'),
    toggle: () =>
        call('playback.playOrPause'),
    /**
     * Seeks within the current track. The host clamps the position to the
     * track and reports both the requested and the applied value.
     */
    seek: (seconds: number) =>
        call('playback.setPosition', {
            position: seconds,
        }),
    getVolume: () =>
        call('playback.getVolume'),
    setVolume: (volume: number) =>
        call('playback.setVolume', {
            volume,
        }),
    /** Mutes by default; pass `false` to unmute. */
    mute: (muted = true) =>
        call('playback.mute', {
            muted,
        }),
    toggleMute: () =>
        call('playback.toggleMute'),
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
    getPosition: () =>
        call('playback.getPosition'),
    getOrder: () =>
        call(
            'playback.getPlaybackOrder',
        ),
    /** Takes the order's index or its name; the host reports what took effect. */
    setOrder: (order: PlaybackOrder | number) =>
        call(
            'playback.setPlaybackOrder',
            (typeof order === 'number'
                ? { order }
                : { name: order }),
        ),
    getStopAfterCurrent: () =>
        call(
            'playback.getStopAfterCurrent',
        ),
    setStopAfterCurrent: (enabled: boolean) =>
        call(
            'playback.setStopAfterCurrent',
            { enabled },
        ),
    getCurrentTrackIndex: (includeTrackInfo?: boolean) =>
        call(
            'playback.getCurrentTrackIndex',
            {
                includeTrackInfo: !!includeTrackInfo,
            },
        ),
    getPlayingPlaylist: () =>
        call(
            'playback.getPlayingPlaylist',
        ),
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
    playPause: () =>
        call('playback.playPause'),
    toggleStopAfterCurrent: () =>
        call(
            'playback.toggleStopAfterCurrent',
        ),
    volumeUp: () => call('playback.volumeUp'),
    volumeDown: () =>
        call('playback.volumeDown'),
};

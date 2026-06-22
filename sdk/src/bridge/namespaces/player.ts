/**
 * `player` — playback control namespace.
 */

import { bridge } from '../Bridge.js';
import type {
    BaseResponse,
    PlaybackOrder,
    PlaybackOrderInfo,
    PlaybackPlayPathResponse,
    PlaybackPlayPathsResponse,
    PlaybackSetOrderResponse,
    PlaybackSetPositionResponse,
    PlaybackState,
    PlaybackStopAfterCurrentState,
    PlaybackToggleMuteResponse,
    PlaybackToggleResponse,
    PositionResponse,
    TrackInfo,
    VolumeResponse,
} from '../../types/responses.js';

export const player = {
    play: () => bridge.invoke<BaseResponse>('playback.play'),
    pause: () => bridge.invoke<BaseResponse>('playback.pause'),
    stop: () => bridge.invoke<BaseResponse>('playback.stop'),
    next: () => bridge.invoke<BaseResponse>('playback.next'),
    prev: () => bridge.invoke<BaseResponse>('playback.previous'),
    random: () => bridge.invoke<BaseResponse>('playback.random'),
    toggle: () =>
        bridge.invoke<PlaybackToggleResponse>('playback.playOrPause'),
    seek: (seconds: number) =>
        bridge.invoke<PlaybackSetPositionResponse>('playback.setPosition', {
            seconds,
        }),
    getVolume: () => bridge.invoke<VolumeResponse>('playback.getVolume'),
    setVolume: (volume: number) =>
        bridge.invoke<BaseResponse>('playback.setVolume', { volume }),
    mute: () => bridge.invoke<BaseResponse>('playback.mute'),
    toggleMute: () =>
        bridge.invoke<PlaybackToggleMuteResponse>('playback.toggleMute'),
    getState: () => bridge.invoke<PlaybackState>('playback.getState'),
    getCurrentTrack: () =>
        bridge.invoke<TrackInfo | null>('playback.getCurrentTrack'),
    getPosition: () => bridge.invoke<PositionResponse>('playback.getPosition'),
    getOrder: () => bridge.invoke<PlaybackOrderInfo>('playback.getPlaybackOrder'),
    setOrder: (order: PlaybackOrder | number) =>
        bridge.invoke<PlaybackSetOrderResponse>(
            'playback.setPlaybackOrder',
            { order },
        ),
    getStopAfterCurrent: () =>
        bridge.invoke<PlaybackStopAfterCurrentState>(
            'playback.getStopAfterCurrent',
        ),
    setStopAfterCurrent: (enabled: boolean) =>
        bridge.invoke<BaseResponse>('playback.setStopAfterCurrent', { enabled }),
    getCurrentTrackIndex: (includeTrackInfo?: boolean) =>
        bridge.invoke<{ index: number; track?: TrackInfo }>(
            'playback.getCurrentTrackIndex',
            { includeTrackInfo: !!includeTrackInfo },
        ),
    getPlayingPlaylist: () =>
        bridge.invoke<{ playlist: number }>('playback.getPlayingPlaylist'),
    playPath: (path: string) =>
        bridge.invoke<PlaybackPlayPathResponse>('playback.playPath', { path }),
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
        return bridge.invoke<PlaybackPlayPathsResponse>(
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
        bridge.invoke<PlaybackToggleResponse>('playback.playPause'),
    toggleStopAfterCurrent: () =>
        bridge.invoke<PlaybackStopAfterCurrentState>(
            'playback.toggleStopAfterCurrent',
        ),
    volumeUp: () => bridge.invoke<BaseResponse>('playback.volumeUp'),
    volumeDown: () => bridge.invoke<BaseResponse>('playback.volumeDown'),
};

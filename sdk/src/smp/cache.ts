/**
 * SMP cache state + bridge event wiring.
 *
 * Owns the {@link SmpCompatCache} literal and the array of subscribed
 * `fb.on(...)` handlers that keep the cache in sync with playback /
 * playlist / config events emitted by the C++ host.
 *
 * Migrated from `sdk/smp-compat.js:121-275`.
 */

import type { SmpBridgeShape } from './bridgeShape.js';
import type { PlaylistInfo, TrackInfo } from '../types/responses.js';
import type {
    PlaybackBooleanPayload,
    PlaybackOrderChangedPayload,
    PlaybackPausedPayload,
    PlaybackStateChangedPayload,
    PlaybackTimePayload,
    PlaybackVolumeChangedPayload,
} from '../types/events.js';
import type { SmpCompatCache } from './types.js';

const LOG_PREFIX = '[SMP-Compat]';

/** Default-initialised cache literal. */
export function createInitialCache(): SmpCompatCache {
    return {
        isPlaying: false,
        isPaused: false,
        volumeDb: -100,
        muted: false,
        playbackTime: 0,
        playbackLength: 0,
        playbackOrder: 0,
        stopAfterCurrent: false,
        alwaysOnTop: false,
        cursorFollowPlayback: false,
        playbackFollowCursor: false,
        replaygainMode: 0,
        componentPath: '',
        foobarPath: '',
        profilePath: '',
        currentTrack: null,
        playlists: [],
        activePlaylist: 0,
        playingPlaylist: -1,
        playlistCount: 0,
        version: '',
    };
}

/** Schedule-once playlist refresh helper. Returns a `{ schedule, refresh }` pair. */
export function createPlaylistRefresher(
    fb: SmpBridgeShape,
    cache: SmpCompatCache,
): {
    refresh: () => Promise<void>;
    schedule: () => void;
} {
    let scheduled = false;

    const refresh = async (): Promise<void> => {
        try {
            const all = (await fb.invoke('playlist.getAll', {})) as PlaylistInfo[] | undefined;
            if (!Array.isArray(all)) return;

            cache.playlists = all;
            cache.playlistCount = all.length;

            const active = all.find((p) => p && p.isActive);
            const playing = all.find((p) => p && p.isPlaying);

            cache.activePlaylist =
                typeof active?.index === 'number'
                    ? active.index
                    : Math.max(0, all.findIndex((p) => p && p.isActive));
            cache.playingPlaylist =
                typeof playing?.index === 'number'
                    ? playing.index
                    : all.findIndex((p) => p && p.isPlaying);
        } catch (e) {
            // Silent: only emitted in debug mode.
            void e;
        }
    };

    const schedule = (): void => {
        if (scheduled) return;
        scheduled = true;
        setTimeout(async () => {
            try {
                await refresh();
            } finally {
                scheduled = false;
            }
        }, 0);
    };

    return { refresh, schedule };
}

/**
 * Wire `fb.on(...)` subscriptions that mirror C++ events into the
 * SMP cache. Returns the array of unsubscribe callbacks the bootstrap
 * later iterates inside `smp.dispose()`.
 */
export function attachCacheEventSubscriptions(
    fb: SmpBridgeShape,
    cache: SmpCompatCache,
    schedulePlaylistRefresh: () => void,
): Array<() => void> {
    const unsubs: Array<() => void> = [];

    unsubs.push(
        fb.on('playback:stateChanged', (d: PlaybackStateChangedPayload) => {
            const state = d?.state;
            const paused = state === 'paused';
            const playing = state === 'playing' || paused;
            cache.isPaused = paused;
            cache.isPlaying = playing;
            if (typeof d?.position === 'number') cache.playbackTime = d.position;
            if (typeof d?.duration === 'number') cache.playbackLength = d.duration;
        }),
    );

    unsubs.push(
        fb.on('playback:paused', (d: PlaybackPausedPayload) => {
            const paused = !!d?.paused;
            cache.isPaused = paused;
            cache.isPlaying = paused ? true : cache.isPlaying;
        }),
    );

    unsubs.push(
        fb.on('playback:volumeChanged', (d: PlaybackVolumeChangedPayload) => {
            if (typeof d?.volumeDb === 'number') cache.volumeDb = d.volumeDb;
            if (typeof d?.muted === 'boolean') cache.muted = d.muted;
        }),
    );

    unsubs.push(
        fb.on('playback:time', (d: PlaybackTimePayload) => {
            if (typeof d?.position === 'number') cache.playbackTime = d.position;
        }),
    );

    unsubs.push(
        fb.on('playback:trackChanged', (d: TrackInfo) => {
            cache.currentTrack = d ?? null;
            if (typeof d?.duration === 'number') cache.playbackLength = d.duration;
            cache.isPlaying = true;
        }),
    );

    unsubs.push(
        fb.on('playback:stopped', () => {
            cache.isPlaying = false;
            cache.isPaused = false;
            cache.playbackTime = 0;
            cache.playbackLength = 0;
            cache.currentTrack = null;
        }),
    );

    unsubs.push(
        fb.on('playback:orderChanged', (d: PlaybackOrderChangedPayload) => {
            if (typeof d?.orderIndex === 'number') {
                cache.playbackOrder = d.orderIndex | 0;
            }
        }),
    );

    unsubs.push(
        fb.on('playback:stopAfterCurrentChanged', (d: PlaybackBooleanPayload) => {
            cache.stopAfterCurrent = !!d?.enabled;
        }),
    );

    unsubs.push(
        fb.on('window:alwaysOnTopChanged', (d: PlaybackBooleanPayload) => {
            cache.alwaysOnTop = !!d?.enabled;
        }),
    );

    unsubs.push(
        fb.on('playback:cursorFollowChanged', (d: PlaybackBooleanPayload) => {
            cache.cursorFollowPlayback = !!d?.enabled;
        }),
    );

    unsubs.push(
        fb.on('playback:followCursorChanged', (d: PlaybackBooleanPayload) => {
            cache.playbackFollowCursor = !!d?.enabled;
        }),
    );

    unsubs.push(
        fb.on('audio:replaygainModeChanged', (d: { mode?: number }) => {
            if (typeof d?.mode === 'number') cache.replaygainMode = d.mode;
        }),
    );

    // Playlist events that can affect active / playing / count / name / lock / trackCount
    const playlistEvents = [
        'playlist:activated',
        'playlist:created',
        'playlist:removed',
        'playlist:reordered',
        'playlist:renamed',
        'playlist:lockChanged',
        'playlist:itemsAdded',
        'playlist:itemsRemoved',
        'playlist:itemsReordered',
    ] as const;

    for (const evt of playlistEvents) {
        unsubs.push(fb.on(evt, schedulePlaylistRefresh));
    }

    return unsubs;
}

/**
 * Pull the full set of cache fields from the bridge in parallel.
 *
 * Mirrors `_populateCache(opts)` from `sdk/smp-compat.js:1710-1795`.
 */
export async function populateCache(
    fb: SmpBridgeShape,
    cache: SmpCompatCache,
    opts: { includePaths?: boolean } = {},
): Promise<void> {
    const includePaths = !!opts.includePaths;

    type StateResp = { state?: string };
    type VolResp = { volumeDb?: number; muted?: boolean };
    type PosResp = { position?: number; duration?: number };
    type TrackResp = TrackInfo & { found?: boolean };
    type OrderResp = { orderIndex?: number; order?: number };
    type StopAfterResp = { enabled?: boolean };
    type WinResp = { alwaysOnTop?: boolean; isAlwaysOnTop?: boolean };
    type RgResp = { mode?: number };
    type PathResp = { path?: string };
    type VersionResp = { version?: string };

    // Deliberately untyped: the queries are heterogeneous and each result
    // is narrowed via the local response interfaces (`StateResp`,
    // `VolResp`, ...) at destructure-time. Letting TS infer the array
    // type keeps the audit's `Promise<unknown>` text-probe clean while
    // preserving call-site type precision.
    const queries = [
        fb.invoke('playback.getState', {}),
        fb.invoke('playback.getVolume', {}),
        fb.invoke('playback.getPosition', {}),
        fb.invoke('playback.getCurrentTrack', {}),
        fb.invoke('playlist.getAll', {}),
        fb.invoke('playback.getPlaybackOrder', {}),
        fb.invoke('playback.getStopAfterCurrent', {}).catch(() => ({ enabled: false }) as StopAfterResp),
        fb.invoke('window.getState', {}).catch(() => ({ alwaysOnTop: false }) as WinResp),
        fb.invoke('config.getCursorFollowPlayback', {}).catch(() => ({ enabled: false }) as StopAfterResp),
        fb.invoke('config.getPlaybackFollowCursor', {}).catch(() => ({ enabled: false }) as StopAfterResp),
        fb.invoke('config.getReplaygainMode', {}).catch(() => ({ mode: 0 }) as RgResp),
    ];

    if (includePaths) {
        queries.push(
            fb.invoke('misc.getComponentPath', {}).catch(() => ({}) as PathResp),
            fb.invoke('misc.getFoobarPath', {}).catch(() => ({}) as PathResp),
            fb.invoke('misc.getProfilePath', {}).catch(() => ({}) as PathResp),
            fb.invoke('config.getVersionInfo', {}).catch(() => ({}) as VersionResp),
        );
    }

    const results = await Promise.all(queries);
    const [
        rawState,
        rawVol,
        rawPos,
        rawTrack,
        rawPlaylists,
        rawOrder,
        rawStopAfter,
        rawWin,
        rawCursorFollow,
        rawPlaybackFollow,
        rawRgMode,
    ] = results;

    const state = rawState as StateResp | undefined;
    const vol = rawVol as VolResp | undefined;
    const pos = rawPos as PosResp | undefined;
    const track = rawTrack as TrackResp | null | undefined;
    const playlists = rawPlaylists as PlaylistInfo[] | undefined;
    const order = rawOrder as OrderResp | undefined;
    const stopAfter = rawStopAfter as StopAfterResp | undefined;
    const winState = rawWin as WinResp | undefined;
    const cursorFollow = rawCursorFollow as StopAfterResp | undefined;
    const playbackFollow = rawPlaybackFollow as StopAfterResp | undefined;
    const rgMode = rawRgMode as RgResp | undefined;

    if (typeof state?.state === 'string') {
        const paused = state.state === 'paused';
        cache.isPaused = paused;
        cache.isPlaying = state.state === 'playing' || paused;
    }

    if (typeof vol?.volumeDb === 'number') cache.volumeDb = vol.volumeDb;
    if (typeof vol?.muted === 'boolean') cache.muted = vol.muted;

    if (typeof pos?.position === 'number') cache.playbackTime = pos.position;
    if (typeof pos?.duration === 'number') cache.playbackLength = pos.duration;

    if (track && track.found === false) {
        cache.currentTrack = null;
    } else if (track && typeof track === 'object') {
        cache.currentTrack = track;
        if (typeof track.duration === 'number') cache.playbackLength = track.duration;
    }

    if (Array.isArray(playlists)) {
        cache.playlists = playlists;
        cache.playlistCount = playlists.length;
        const active = playlists.find((p) => p && p.isActive);
        const playing = playlists.find((p) => p && p.isPlaying);
        cache.activePlaylist =
            typeof active?.index === 'number'
                ? active.index
                : Math.max(0, playlists.findIndex((p) => p && p.isActive));
        cache.playingPlaylist =
            typeof playing?.index === 'number'
                ? playing.index
                : playlists.findIndex((p) => p && p.isPlaying);
    }

    if (typeof order?.orderIndex === 'number') {
        cache.playbackOrder = order.orderIndex | 0;
    } else if (typeof order?.order === 'number') {
        cache.playbackOrder = order.order | 0;
    }

    if (typeof stopAfter?.enabled === 'boolean') cache.stopAfterCurrent = stopAfter.enabled;
    if (winState) cache.alwaysOnTop = !!(winState.alwaysOnTop ?? winState.isAlwaysOnTop);
    if (typeof cursorFollow?.enabled === 'boolean') cache.cursorFollowPlayback = cursorFollow.enabled;
    if (typeof playbackFollow?.enabled === 'boolean') {
        cache.playbackFollowCursor = playbackFollow.enabled;
    }
    if (typeof rgMode?.mode === 'number') cache.replaygainMode = rgMode.mode | 0;

    if (includePaths) {
        const [rawComponentPath, rawFoobarPath, rawProfilePath, rawVersionInfo] = results.slice(11);
        const componentPath = rawComponentPath as PathResp | string | undefined;
        const foobarPath = rawFoobarPath as PathResp | string | undefined;
        const profilePath = rawProfilePath as PathResp | string | undefined;
        const versionInfo = rawVersionInfo as VersionResp | undefined;

        const compPath =
            typeof (componentPath as PathResp)?.path === 'string'
                ? (componentPath as PathResp).path!
                : typeof componentPath === 'string'
                    ? componentPath
                    : '';
        if (typeof compPath === 'string') cache.componentPath = compPath;

        const fbPath =
            typeof (foobarPath as PathResp)?.path === 'string'
                ? (foobarPath as PathResp).path!
                : typeof foobarPath === 'string'
                    ? foobarPath
                    : '';
        if (typeof fbPath === 'string') cache.foobarPath = fbPath;

        const profPath =
            typeof (profilePath as PathResp)?.path === 'string'
                ? (profilePath as PathResp).path!
                : typeof profilePath === 'string'
                    ? profilePath
                    : '';
        if (typeof profPath === 'string') cache.profilePath = profPath;

        if (versionInfo && typeof versionInfo === 'object') {
            cache.version = typeof versionInfo.version === 'string' ? versionInfo.version : '';
        }
    }

    void LOG_PREFIX; // silence unused-symbol warning for the shared prefix
}

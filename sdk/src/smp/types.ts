import type { JsonValue } from '../types/json.js';
import type { JsonObject } from '../types/json.js';
/**
 * `foo-webview-sdk/smp` — Spider Monkey Panel compatibility types.
 *
 * Public type definitions for the SMP compatibility layer. Consumers
 * porting Spider Monkey Panel scripts can rely on these types when
 * they import from `foo-webview-sdk/smp`.
 *
 * Sub-modules:
 *
 * - {@link SmpEventName}         — SMP event-name union accepted by
 *                                  `fb.onSMP(event, callback)`.
 * - {@link SmpCompatCache}       — Mutable cache shape exposed via
 *                                  `window.smp.cache`.
 * - {@link SmpCompatApi}         — Public surface of `window.smp`.
 * - {@link SmpPlaybackQueueItem} — Element shape returned by
 *                                  `plman.GetPlaybackQueueContents()`.
 * - {@link SMPPlman}             — Public surface of `window.plman`.
 * - {@link SmpHandleId}          — Discriminated handle-id parts.
 * - {@link SmpHandleLike}        — Loose union accepted by SMP wrapper
 *                                  factories.
 *
 * The SMP wrapper classes (`FbMetadbHandle`, `FbMetadbHandleList`,
 * `FbFileInfo`, `FbProfiler`, `FbTitleFormat`, `FbUiSelectionHolder`,
 * `ContextMenuManager`, `MainMenuManager`) live in `./classes/*.ts`;
 * their constructor / method signatures are emitted by tsup from the
 * runtime classes themselves.
 */

import type { PlaylistInfo, TrackInfo } from '../types/responses.js';
import type { FbMetadbHandleList } from './classes/FbMetadbHandleList.js';

// ---------------------------------------------------------------------------
// Event names
// ---------------------------------------------------------------------------

/**
 * SMP-style event name union accepted by `fb.onSMP(event, callback)`.
 *
 * Mirrors the original Spider Monkey Panel event vocabulary; these
 * names are translated to the canonical `FBEventName` colon-format
 * inside `SMP_EVENT_MAP` before reaching `bridge.on(...)`.
 */
export type SmpEventName =
    // Playback
    | 'on_playback_starting'
    | 'on_playback_new_track'
    | 'on_playback_stop'
    | 'on_playback_seek'
    | 'on_playback_pause'
    | 'on_playback_time'
    | 'on_playback_edited'
    | 'on_playback_dynamic_info'
    | 'on_playback_dynamic_info_track'
    | 'on_playback_order_changed'
    | 'on_playback_queue_changed'
    // Playback stats
    | 'on_item_played'
    // Volume
    | 'on_volume_change'
    // Selection
    | 'on_selection_changed'
    // Library / Metadb
    | 'on_library_items_added'
    | 'on_library_items_removed'
    | 'on_library_items_changed'
    | 'on_metadb_changed'
    // Playlist
    | 'on_playlist_switch'
    | 'on_playlist_items_added'
    | 'on_playlist_items_removed'
    | 'on_playlist_items_reordered'
    | 'on_playlist_items_selection_change'
    | 'on_item_focus_change'
    | 'on_playlists_changed'
    // Audio / DSP
    | 'on_dsp_preset_changed'
    | 'on_output_device_changed'
    | 'on_replaygain_mode_changed'
    // UI
    | 'on_colours_changed'
    | 'on_font_changed'
    // App state
    | 'on_always_on_top_changed'
    | 'on_cursor_follow_playback_changed'
    | 'on_playback_follow_cursor_changed'
    | 'on_playlist_stop_after_current_changed'
    // Special
    | 'on_focus';

/**
 * SMP callback signature accepted by {@link SmpCompatApi.onSMP}.
 *
 * Argument count and shape vary per event — the parameter adapter
 * inside `smp-compat.ts` decides what to pass through. Keeping it
 * untyped on purpose; per-event narrowing belongs to consumer code.
 */
export type SmpEventCallback = (...args: unknown[]) => void;

// ---------------------------------------------------------------------------
// Handle-id helpers
// ---------------------------------------------------------------------------

/**
 * Parsed components of an SMP-flavoured handle id (e.g.
 * `"C:\Music\song.flac|subsong:2"`).
 */
export interface SmpHandleId {
    /** File system path with no `|subsong:N` suffix. */
    path: string;
    /** Sub-song index (0 for single-stream files). */
    subsong: number;
    /** Round-trip-stable id string (`path` + optional suffix). */
    id: string;
}

/**
 * Loose union of inputs accepted by every SMP wrapper that needs to
 * derive a handle-id from a value.
 *
 * Includes raw strings, `FbMetadbHandle`-shaped objects (Path/SubSong),
 * and plain track-info shapes (absolutePath / path).
 */
export type SmpHandleLike =
    | string
    | null
    | undefined
    | {
        Path?: string;
        SubSong?: number;
        HandleId?: string;
        absolutePath?: string;
        path?: string;
        [key: string]: JsonValue;
    };

// ---------------------------------------------------------------------------
// Cache + main API
// ---------------------------------------------------------------------------

/**
 * Mutable cache backing the SMP synchronous property surface.
 *
 * Updated by event subscriptions inside `smp-compat.ts` and by the
 * initial `_populateCache()` bootstrap. Consumers access via
 * `window.smp.cache.*`.
 */
export interface SmpCompatCache {
    isPlaying: boolean;
    isPaused: boolean;
    /** Volume in dB (-100..0). */
    volumeDb: number;
    muted: boolean;
    /** Current playback position in seconds. */
    playbackTime: number;
    /** Current track duration in seconds. */
    playbackLength: number;
    playbackOrder: number;
    stopAfterCurrent: boolean;
    alwaysOnTop: boolean;
    cursorFollowPlayback: boolean;
    playbackFollowCursor: boolean;
    replaygainMode: number;
    componentPath: string;
    foobarPath: string;
    profilePath: string;
    currentTrack: TrackInfo | null;
    playlists: PlaylistInfo[];
    activePlaylist: number;
    playingPlaylist: number;
    playlistCount: number;
    /** foobar2000 application version string (`fb.Version`). */
    version: string;
}

/**
 * Public surface installed onto `window.smp` by the SMP compatibility
 * IIFE bundle. Matches the Spider Monkey Panel contract one-to-one.
 */
export interface SmpCompatApi {
    /** Resolves once the bootstrap has populated the cache. */
    ready: Promise<boolean>;

    /** Reactive-ish cache of synchronous playback / playlist state. */
    cache: SmpCompatCache;

    /** Low-level invoke escape hatch (delegates to `window.fb2k.invoke`). */
    invoke<TResp = unknown>(
        method: string,
        params?: JsonObject,
    ): Promise<TResp>;

    /** Parse `path|subsong:N` ids into `{ path, subsong, id }`. */
    parseHandleId(handleId: string): SmpHandleId;

    /** Inverse of {@link SmpCompatApi.parseHandleId}. */
    formatHandleId(path: string, subsong?: number): string;

    /** Drop the `|subsong:N` suffix from a handle id (no-op on plain paths). */
    stripSubsongSuffix(pathOrHandleId: string): string;

    /** Refresh the entire cache (playback + playlists + paths). */
    refreshCache(): Promise<void>;

    /** Detach every event subscription wired by the bootstrap. */
    dispose(): void;

    /** Internal load guard; consumers should not depend on this flag. */
    __smpCompatLoaded?: boolean;
}

// ---------------------------------------------------------------------------
// plman (playlist manager)
// ---------------------------------------------------------------------------

/**
 * Element shape returned by `plman.GetPlaybackQueueContents()`. Matches
 * the SMP queue-item layout (`Handle` + playlist coordinates).
 */
export interface SmpPlaybackQueueItem {
    /** Underlying track handle (typed as `unknown` here to avoid a
     *  forward dependency on the runtime class). The bundle installs
     *  the concrete `FbMetadbHandle` instance at runtime. */
    Handle: unknown;
    PlaylistIndex: number;
    PlaylistItemIndex: number;
    QueueIndex?: number;
}

/**
 * Public surface installed onto `window.plman`. Async methods return
 * `Promise<...>`; sync helpers read from the cache.
 *
 * Wrapper classes (`FbMetadbHandleList` etc.) are referenced as
 * `unknown` to keep the type self-contained; the runtime IIFE installs
 * the concrete classes onto `window.*` so theme code that types them
 * via global ambient declarations continues to work unchanged.
 */
export interface SMPPlman {
    ActivePlaylist: number;
    readonly PlayingPlaylist: number;
    readonly PlaylistCount: number;
    PlaybackOrder: number;

    // Sync helpers (cache-backed)
    GetPlaylistName(playlistIdx: number): string;
    PlaylistItemCount(playlistIdx: number): number;
    FindPlaylist(name: string): number;
    IsAutoPlaylist(playlistIdx: number): boolean;
    IsPlaylistLocked(playlistIdx: number): boolean;

    // Async methods
    RenamePlaylist(playlistIdx: number, name: string): Promise<boolean>;
    CreatePlaylist(position: number, name: string): Promise<number>;
    RemovePlaylist(playlistIdx: number): Promise<boolean>;
    ClearPlaylist(playlistIdx: number): Promise<boolean>;

    /** Returns an `FbMetadbHandleList` instance at runtime. */
    GetPlaylistItems(playlistIdx: number): Promise<FbMetadbHandleList>;
    /** Returns an `FbMetadbHandleList` instance at runtime. */
    GetPlaylistSelectedItems(
        playlistIdx: number,
    ): Promise<FbMetadbHandleList>;

    GetPlaylistFocusItemIndex(playlistIdx: number): Promise<number>;
    SetPlaylistFocusItem(playlistIdx: number, itemIndex: number): Promise<boolean>;

    SetPlaylistSelection(
        playlistIdx: number,
        indices: number[] | Iterable<number>,
        state: boolean,
    ): Promise<boolean>;
    ClearPlaylistSelection(playlistIdx: number): Promise<boolean>;

    InsertPlaylistItems(
        playlistIdx: number,
        base: number,
        handles: Iterable<unknown> | unknown,
        select?: boolean,
    ): Promise<number>;
    RemovePlaylistSelection(playlistIdx: number, crop?: boolean): Promise<boolean>;

    SortByFormat(
        playlistIdx: number,
        pattern: string,
        selectedOnly?: boolean,
    ): Promise<boolean>;
    UndoBackup(playlistIdx: number): boolean | Promise<boolean>;
    MovePlaylistSelection(playlistIdx: number, delta: number): Promise<boolean>;

    CreateAutoPlaylist(
        playlistIdx: number,
        name: string,
        query: string,
        sort?: string,
        flags?: number,
    ): Promise<number>;
    DuplicatePlaylist(from: number, name?: string): Promise<number>;
    AddLocations(
        playlistIdx: number,
        locations: string[] | Iterable<string>,
        select?: boolean,
    ): Promise<number>;

    AddItemToPlaybackQueue(handleLike: SmpHandleLike): Promise<number>;
    AddPlaylistItemToPlaybackQueue?(
        playlistIdx: number,
        playlistItemIdx: number,
    ): Promise<boolean>;
    FlushPlaybackQueue(): Promise<boolean>;
    GetPlaybackQueueContents(): Promise<SmpPlaybackQueueItem[]>;
    GetPlaybackQueueHandles?(): Promise<FbMetadbHandleList>;

    IsPlaylistItemSelected?(playlistIdx: number, itemIdx: number): Promise<boolean>;
    FindOrCreatePlaylist?(name: string, unlocked?: boolean): Promise<number>;
    MovePlaylist?(from: number, to: number): Promise<boolean>;
}

// ---------------------------------------------------------------------------
// Menu builder helpers (shared by ContextMenuManager / MainMenuManager)
// ---------------------------------------------------------------------------

/** Bit-flag constants used by the C++ menu API for item state. */
export const SMP_MENU_FLAGS = {
    disabled: 1 << 0,
    checked: 1 << 1,
    radiochecked: 1 << 2,
    defaulthidden: 1 << 3,
    disclosure: 1 << 4,
} as const;

/**
 * Raw menu item returned by the C++ menu API (`menu.getMainMenu` /
 * `menu.getContextMenu`). Recursive: `submenu` items carry a
 * `children` array of the same shape.
 */
export interface SmpRawMenuItem {
    type?: 'command' | 'separator' | 'submenu';
    label?: string;
    flags?: number;
    /** Context menu uses `commandId`; main menu uses `path` or `guid`. */
    commandId?: number;
    path?: string;
    guid?: string;
    children?: SmpRawMenuItem[];
}

/** Element of the structured tree returned by `BuildMenu(...)`. */
export type SmpStructuredMenuItem =
    | { type: 'separator' }
    | {
        label: string;
        submenu: SmpStructuredMenuItem[];
    }
    | {
        id: number;
        label: string;
        enabled: boolean;
        checked: boolean;
    };

/** Stateful counter passed to the recursive menu builder. */
export interface SmpMenuBuildState {
    nextId: number;
    /** Upper bound on auto-allocated `id`s; `null` = unlimited. */
    limit: number | null;
    /** Maps allocated `id` ↔ the C++ command identifier. */
    idMap: Map<number, number | string>;
}

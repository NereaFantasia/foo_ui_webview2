/**
 * `foo-webview-sdk` - API response and domain value types.
 *
 * Covers base type aliases (`PlaybackStateValue`, `PlaybackOrder`,
 * `AlbumArtType`), domain models (`TrackInfo`, `PlaylistInfo`, `AlbumInfo`,
 * ...), window/configuration value types, the unified error envelope
 * (`BaseResponse`, `ErrorEnvelope`, `ApiErrorCode`) and every per-API
 * response interface.
 *
 * Event payload shapes live in {@link "./events"}.
 */

import type { JsonObject, JsonValue } from './json.js';
import type {
    AdvancedConfigItem, ArtworkAvailableEntry, ArtworkUrlRow, ConfigComponentInfo, ConfigDspPreset,
    AlbumInfo, ArtistAlbumRef, ArtistInfo, ConfigLibraryFilePattern, ConfigOutputDevice, ConfigPreferencesPage, KeyboardHotkey,
    LibraryRootInfo, LibraryTrack, MenuTreeNode, PanelConfig, PlaylistColumnDefinition, SystemApiInfo, SystemPluginInfo,
} from './generated/schema-types.js';
import type {
    ArtworkGetAvailableArtworkResponse, ArtworkGetByPathResponse, ArtworkGetByPlaylistItemResponse,
    ArtworkGetCurrentResponse, ArtworkGetFb2kUrlByPathBatchResponse, ArtworkGetForTrackResponse,
    ArtworkGetLyricsResponse, ArtworkGetMetadataResponse, AudioGetOutputInfoResponse,
    AudioGetSpectrumDebugStateResponse, AudioGetStreamInfoResponse, ConfigGetActiveDspPresetResponse,
    ConfigGetAdvancedConfigResponse, ConfigGetAdvancedConfigValueResponse, ConfigGetAllResponse,
    ConfigGetLibraryFilePatternsResponse, ConfigGetOutputConfigResponse, ConfigGetPreferencesPagesResponse,
    ConfigGetPreferencesStandardGuidsResponse, ConfigGetVersionInfoResponse, DndGetCapabilitiesResponse,
    DndGetPathsAsyncResponse, DndPrepareDragResponse, LibraryBrowseTreeResponse, LibraryGetAlbumTracksResponse,
    LibraryGetAlbumsResponse,
    LibraryGetArtistAlbumsResponse, LibraryGetArtistTracksResponse, LibraryGetArtistsResponse,
    LibraryGetCacheStatsResponse, LibraryGetFieldValuesResponse, LibraryGetGenresResponse,
    LibraryGetRandomTracksResponse, LibraryGetRecentlyAddedResponse,
    LibraryGetStatsResponse, LibraryGetStatusResponse, LyricsExistsResponse,
    LyricsGetResponse, LyricsSaveResponse, PlaylistGetAutoplaylistInfoResponse, PlaylistGetGroupRunsResponse,
    PlaylistGetLockInfoResponse, PlaylistGetSelectedTracksResponse, PlaylistGetSelectionResponse,
    PlaylistGetTrackCountResponse, PlaylistGetTracksResponse, PlaylistGetAvailableColumnsResponse,
    SystemGetApiStatsResponse,
    MenuGetContextMenuSuccess as MenuGetContextMenuWireSuccess,
    MenuGetMainMenuSuccess as MenuGetMainMenuWireSuccess,
    LibraryBrowseTreeSuccess,
} from './generated/responses.js';
import type {
    AudioGenerateFullWaveformParams, HttpGetParams, LyricsGetParams, LyricsSaveParams, ShellExecParams,
    ShellSpawnParams, WindowSetPopupBehaviorParams,
} from './generated/params.js';
import type { SpectrumChannels, SpectrumOutput, SpectrumScale } from './overrides/audio.js';
import type {
    JitQueueGetStateResponse,
    PlaybackGetPlaybackOrderResponse,
    PlaybackGetPositionResponse,
    PlaybackGetStateResponse,
    PlaybackGetStopAfterCurrentResponse,
    PlaybackGetVolumeResponse,
    PlaybackPlayPauseResponse,
    PlaybackSetPlaybackOrderResponse,
    PlaycountGetResponse,
    PlaycountGetStatsResponse,
    ReplaygainGetModeResponse,
    ReplaygainGetPreampResponse,
    ReplaygainGetResponse,
    ReplaygainGetSettingsResponse,
    SelectionGetResponse,
    ShellExecResponse,
    ShellSpawnResponse,
    TitleformatEvalBatchResponse,
    TitleformatEvalFieldsBatchResponse,
    TitleformatEvalFieldsResponse,
    TitleformatEvalResponse,
    TitleformatGetBuiltinFieldsResponse,
    WindowGetAllWindowsResponse,
    WindowGetDevServerConfigResponse,
    WindowGetDpiScaleResponse,
    WindowGetStateResponse,
    WindowGetTitlebarInfoResponse,
    WindowSetBackdropPolicyResponse,
    WindowSetPopupBehaviorResponse,
    PlaycountGetSuccess,
    WindowGetAllWindowsSuccess,
} from './generated/responses.js';
import type { WindowCreatePopupParams } from './generated/params.js';
import type { WindowInfo } from './generated/schema-types.js';

// ============================================================================
// Base type aliases
// ============================================================================

/** Discrete playback engine states. */
export type PlaybackStateValue = 'stopped' | 'playing' | 'paused';

/** Playback order modes recognised by `playlist.setPlaybackOrder`. */
export type PlaybackOrder =
    | 'default'
    | 'repeat-playlist'
    | 'repeat-track'
    | 'random'
    | 'shuffle-tracks'
    | 'shuffle-albums'
    | 'shuffle-folders';

/** Album-art type discriminator accepted by `artwork.*` APIs. */
export type AlbumArtType = 'front' | 'back' | 'disc' | 'icon' | 'artist';

// ============================================================================
// Track information
// ============================================================================

/** Canonical track metadata returned by playback / library / metadata APIs. */
export interface TrackInfo {
    /** Unique track identifier (canonical absolute path). */
    id?: string;
    /** Native filesystem absolute path. */
    absolutePath?: string;
    /** Library index (returned by some APIs). */
    index?: number;
    title: string;
    artist: string;
    /**
     * Atomic values behind `artist`, in tag order, neither de-duplicated nor
     * stripped of empty values: `artists.join(', ')` is exactly `artist`, so a
     * multi-value tag can be recovered from this field and not from `artist`.
     * Every shared `Track` row carries it (library, playlist, queue and
     * playback rows, and the track events); it is optional here because
     * objects outside that row, such as the answer of `artwork.getMetadata`,
     * do not.
     */
    artists?: string[];
    album: string;
    albumArtist?: string;
    /**
     * Atomic values behind `albumArtist`, with the same rules as `artists`:
     * `albumArtists.join(', ')` is exactly `albumArtist`. Present wherever
     * `artists` is.
     */
    albumArtists?: string[];
    genre?: string;
    date?: string;
    trackNumber?: number;
    discNumber?: number;
    duration: number;
    path: string;
    subsong?: number;
    /** File size in bytes. */
    fileSize?: number;
    /** Bitrate in kbps. */
    bitrate?: number;
    /** Sample rate in Hz. */
    sampleRate?: number;
    /** Channel count. */
    channels?: number;
    /** Codec identifier. */
    codec?: string;
    /** Rating from 0 to 5. */
    rating?: number;
    /** Date-added timestamp used when `sortBy='added'`. */
    added?: string;
    /** Modified-time fallback when sort-by-added is unavailable (Unix seconds). */
    modified?: number;
}

/**
 * One playlist row, from the declaration in src/api/schema/playlist.ts; `PlaylistTrackPartial` is
 * the same row narrowed to the `fields` a `playlist.getTracks` call asked for.
 */
export type { PlaylistTrack, PlaylistTrackPartial } from './generated/schema-types.js';

// ============================================================================
// Playlist
// ============================================================================

/** One entry of `playlist.getAll`, from the declaration in src/api/schema/playlist.ts. */
export type { PlaylistInfo } from './generated/schema-types.js';

/** `playlist.getSelection` response; the generated {@link PlaylistGetSelectionResponse} under its earlier name. */
export type SelectionInfo = PlaylistGetSelectionResponse;

/**
 * A track reference `playlist.addHandles` and `playlist.insertTracks` accept:
 * a path with an optional `|subsong:N` suffix, or the path and subsong apart.
 */
export type PlaylistHandleRef = string | { path: string; subsong?: number };

/**
 * Result of `selection.get`. The selection-manager API works on
 * metadb handles rather than playlist indices, so the envelope is
 * structurally distinct from {@link SelectionInfo}: it carries
 * `handles[]` (with `|subsong:N` suffixes for CUE rows), the typed
 * selection-source string, and pagination metadata. The shape is declared in
 * src/api/schema/selection.ts; this name stays as an alias.
 */
export type SelectionGetResult = SelectionGetResponse;

// ============================================================================
// Library
// ============================================================================

/** One album row of `library.getAlbums` and `library.getArtistAlbums`, from the declaration. */
export type { AlbumInfo };

/** One album an artist is credited on, from the declaration. */
export type { ArtistAlbumRef };

/** One artist row of `library.getArtists`, from the declaration. */
export type { ArtistInfo };

/** `library.getStats` counters; the generated {@link LibraryGetStatsResponse} under its earlier name. */
export type LibraryStats = LibraryGetStatsResponse;

/** `library.getStatus` response; the generated {@link LibraryGetStatusResponse} under its earlier name. */
export type LibraryStatus = LibraryGetStatusResponse;

// ============================================================================
// Playback state
// ============================================================================

/** Result of `playback.getState`: the generated response under its older name. */
export type PlaybackState = PlaybackGetStateResponse;

/** Result of `playback.getPlaybackOrder`: the generated response under its older name. */
export type PlaybackOrderInfo = PlaybackGetPlaybackOrderResponse;

/** Result of `playback.setPlaybackOrder`: the generated response under its older name. */
export type PlaybackSetOrderResponse = PlaybackSetPlaybackOrderResponse;

/** Result of `playback.playPause` and `playback.playOrPause`: the generated response under its older name. */
export type PlaybackToggleResponse = PlaybackPlayPauseResponse;

/** Result of `playback.getStopAfterCurrent` and `playback.toggleStopAfterCurrent`: the generated response under its older name. */
export type PlaybackStopAfterCurrentState = PlaybackGetStopAfterCurrentResponse;

/** Result of `playback.getVolume`: the generated response under its older name. */
export type VolumeResponse = PlaybackGetVolumeResponse;

/** Result of `playback.getPosition`: the generated response under its older name. */
export type PositionResponse = PlaybackGetPositionResponse;

// ============================================================================
// Window
// ============================================================================

/**
 * Result of `window.getState` (`ui.getState`). Every flag is sent twice, bare and
 * `is`-prefixed, with the same value.
 */
export type WindowState = WindowGetStateResponse;

/** Result of `window.getDpiScale` (`ui.getDpiScale`). */
export type DpiScaleResponse = WindowGetDpiScaleResponse;

/** Allowed values for the `activeEffect` slot in `window.backdropPolicy.*`. */
export type WindowActiveBackdropEffect =
    | 'inherit'
    | 'none'
    | 'mica'
    | 'mica-alt'
    | 'acrylic';

/**
 * Allowed values for the `inactiveEffect` slot in `window.backdropPolicy.*`.
 *
 * `'inherit'` (the host default) keeps the resolved `activeEffect` while the
 * window is unfocused and lets DWM apply its own inactive dimming, so a
 * theme does not have to re-apply the backdrop on every focus change.
 * `'system'` hands the inactive frame back to the platform backdrop.
 */
export type WindowInactiveBackdropEffect =
    | 'inherit'
    | 'system'
    | 'none'
    | 'mica'
    | 'mica-alt'
    | 'acrylic';

/** Behavior presets `window.createPopup` and `window.setPopupBehavior` accept. */
export type WindowPopupProfile = 'standard' | 'miniPlayer' | 'desktopLyrics';

// The window list and the resolved policy shapes declared in src/api/schema/window.ts.
export type {
    WindowBackdropPolicyState,
    WindowBounds,
    WindowInfo,
    WindowObservationCapabilities,
    WindowPopupBehaviorState,
} from './generated/schema-types.js';

/** Patch shape accepted by `window.setBackdropPolicy`. */
export type WindowBackdropPolicyPatch = {
    activeEffect?: WindowActiveBackdropEffect | null;
    inactiveEffect?: WindowInactiveBackdropEffect | null;
    darkMode?: boolean | null;
    reapplyOnActivate?: boolean | null;
};

/** Patch shape accepted by `window.setPopupBehavior`. */
export type WindowPopupBehaviorPatch = {
    showInTaskbar?: boolean | null;
    showInAltTab?: boolean | null;
    keepVisibleOnShowDesktop?: boolean | null;
    allowMinimize?: boolean | null;
    owner?: 'none' | 'main' | null;
    noActivate?: boolean | null;
};

/** Fields every entry of `window.getAllWindows` carries. */
export type WindowInfoBase = Pick<WindowInfo, 'windowId' | 'isMain' | 'title' | 'bounds' | 'capabilities'>;

/** The main window's entry of `window.getAllWindows`, without the popup-only fields. */
export type MainWindowInfo = Omit<WindowInfo, 'url' | 'profile' | 'behavior' | 'resolvedBehavior' | 'isMain'> & {
    isMain: true;
};

/** A popup's entry of `window.getAllWindows`; the popup-only fields are always present. */
export type PopupWindowInfo = Omit<WindowInfo, 'isMain'> &
    Required<Pick<WindowInfo, 'url' | 'profile' | 'behavior' | 'resolvedBehavior'>> & {
        isMain: false;
    };

/** An entry of `window.getAllWindows`, discriminated by `isMain`. */
export type WindowInfoItem = MainWindowInfo | PopupWindowInfo;

/** Result of `window.getAllWindows` (`ui.getAllWindows`), with the entries discriminated by `isMain`. */
export type WindowListResponse = (Omit<WindowGetAllWindowsSuccess, 'items'> & { items: WindowInfoItem[] }) | ApiFailure;

/** Result of `window.setPopupBehavior` (`ui.setPopupBehavior`). */
export type WindowPopupBehaviorResponse = WindowSetPopupBehaviorResponse;

/** Result of `window.setBackdropPolicy` (`ui.setBackdropPolicy`). */
export type WindowBackdropPolicyResponse = WindowSetBackdropPolicyResponse;

/**
 * Options of `ui.createPopup`: the declared parameters, with the preset and the two override
 * objects typed.
 */
export type WindowCreatePopupOptions = Omit<WindowCreatePopupParams, 'profile' | 'behavior' | 'backdropPolicy'> & {
    /** Behavior preset; omitted, none. */
    profile?: WindowPopupProfile;
    /** Behavior overrides on top of the preset. */
    behavior?: WindowPopupBehaviorPatch;
    /** Backdrop policy overrides on top of the preset. */
    backdropPolicy?: WindowBackdropPolicyPatch;
};

/**
 * Options of `ui.setPopupBehavior`: the host parameters, with `profile` and
 * `behavior` typed. The two are independent.
 */
export interface WindowSetPopupBehaviorOptions extends Omit<WindowSetPopupBehaviorParams, 'profile' | 'behavior'> {
    /** New preset. */
    profile?: WindowPopupProfile;
    /** Overrides merged into the current ones; a `null` field removes that override. */
    behavior?: WindowPopupBehaviorPatch;
}

/** Result of `window.getTitlebarInfo` (`ui.getTitlebarInfo`); physical pixels of the main window. */
export type WindowTitlebarInfo = WindowGetTitlebarInfoResponse;

/** Result of `window.getDevServerConfig` (`ui.getDevServerConfig`). */
export type WindowDevServerConfig = WindowGetDevServerConfigResponse;

// A drag or no-drag rectangle declared in src/api/schema/window.ts.
export type { WindowRegion } from './generated/schema-types.js';

// ============================================================================
// Configuration
// ============================================================================

// The config shapes come from the declarations in src/api/schema/config.ts; the names this
// module exported before stay available. The listing methods of the `config` facade resolve
// with the arrays themselves, so the array-shaped names below describe what those methods
// resolve with, not the host's `{ ..., count }` envelope.
export type {
    AdvancedConfigItem,
    ConfigComponentInfo,
    ConfigDspPreset,
    ConfigLibraryFilePattern,
    ConfigOutputDevice,
    ConfigPreferencesPage,
};

/** A foobar2000 audio output device entry from `config.getOutputDevices`. */
export type OutputDevice = ConfigOutputDevice;

/** Active audio output configuration as reported by `config.getOutputConfig`. */
export type OutputConfig = ConfigGetOutputConfigResponse;

/** A single DSP preset entry returned by `config.getDspPresets`. */
export type DspPreset = ConfigDspPreset;

/**
 * State returned by `config.getActiveDspPreset`. When no preset is
 * currently active, `index` and `name` are `null` and `isActive` is `false`.
 */
export type ActiveDspPresetInfo = ConfigGetActiveDspPresetResponse;

/** A foobar2000 component descriptor returned by `config.getComponents`. */
export type ComponentInfo = ConfigComponentInfo;

/** Snapshot returned by `config.getAll`; see {@link ConfigGetAllResponse}. */
export type ConfigSnapshot = ConfigGetAllResponse;

/** Composite version information returned by `config.getVersionInfo`. */
export type VersionInfo = ConfigGetVersionInfoResponse;

/** Kind of an entry in the advanced-config tree; see {@link AdvancedConfigItem}. */
export type AdvancedConfigItemType = NonNullable<AdvancedConfigItem['type']>;

/** `config.getAdvancedConfig` response; the generated {@link ConfigGetAdvancedConfigResponse} under its earlier name. */
export type AdvancedConfigResponse = ConfigGetAdvancedConfigResponse;

/** Result shape returned by `config.getAdvancedConfigValue` for a single entry. */
export type AdvancedConfigValueResponse = ConfigGetAdvancedConfigValueResponse;

/** Preferences page or preferences branch returned by `config.getPreferencesPages`. */
export type PreferencesPage = ConfigPreferencesPage;

/** `config.getPreferencesPages` response; the generated {@link ConfigGetPreferencesPagesResponse} under its earlier name. */
export type PreferencesPagesResponse = ConfigGetPreferencesPagesResponse;

/** Standard preferences GUID table returned by `config.getPreferencesStandardGuids`. */
export type PreferencesStandardGuids = ConfigGetPreferencesStandardGuidsResponse;

/** Where foobar2000 puts one kind of new file; see `config.getLibraryFilePatterns`. */
export type LibraryFilePattern = ConfigLibraryFilePattern;

/** Result shape returned by `config.getLibraryFilePatterns`; an unconfigured section is absent. */
export type LibraryFilePatternsResponse = ConfigGetLibraryFilePatternsResponse;

// ============================================================================
// Artwork
// ============================================================================

/**
 * Any picture reader's response under the older shared name: read `available` first and,
 * when it is true, `dataUrl`.
 */
export type ArtworkResponse =
    | ArtworkGetCurrentResponse
    | ArtworkGetByPathResponse
    | ArtworkGetForTrackResponse
    | ArtworkGetByPlaylistItemResponse;

/** One `items` entry of `artwork.getFb2kUrlByPathBatch`, from the declaration. */
export type { ArtworkBatchItem } from './generated/schema-types.js';

/** One row of `artwork.getFb2kUrlByPathBatch`: the generated row under its older name. */
export type ArtworkBatchEntry = ArtworkUrlRow;

/** The `artwork.getFb2kUrlByPathBatch` response under its older name. */
export type ArtworkBatchResponse = ArtworkGetFb2kUrlByPathBatchResponse;

/** The `artwork.getLyrics` response under its older name. */
export type ArtworkLyricsResult = ArtworkGetLyricsResponse;

/** One embedded picture of `artwork.getAvailableArtwork`: the generated row under its older name. */
export type ArtworkAvailableArtworkEntry = ArtworkAvailableEntry;

/** The `artwork.getAvailableArtwork` response under its older name. */
export type ArtworkAvailableArtworkResponse = ArtworkGetAvailableArtworkResponse;

/** The `artwork.getMetadata` response under its older name. */
export type ArtworkMetadataResponse = ArtworkGetMetadataResponse;

// ============================================================================
// System / API discovery
// ============================================================================

/** One row of `system.listApis` / `system.searchApis`: the generated row under its older name. */
export type ApiInfo = SystemApiInfo;

/** One row of `system.getRegisteredPlugins`: the generated row under its older name. */
export type PluginInfo = SystemPluginInfo;

/** The `system.getApiStats` response under its older name. */
export type ApiStats = SystemGetApiStatsResponse;

// ============================================================================
// Error envelope
// ============================================================================

/**
 * Unified error code enumeration.
 *
 * Every API failure (sync or async) reports a `code` value drawn from this
 * enumeration. Naming convention is `UPPER_SNAKE_CASE` so the value is
 * machine-readable.
 *
 * The trailing `(string & {})` branch is the canonical TypeScript escape
 * hatch for "any other string"; it allows third-party plugins to report
 * custom codes without losing the literal-completion benefits for the
 * shipped values.
 */
export type ApiErrorCode =
    // Framework-level
    | 'INVALID_REQUEST'   // Request is missing `method`.
    | 'METHOD_NOT_FOUND'  // No handler registered for the method.
    | 'INTERNAL_ERROR'    // Uncaught exception inside the handler.
    // Parameter errors
    | 'REQUIRED_PARAM'    // Required parameter missing.
    | 'INVALID_PARAMS'    // Parameter value rejected.
    | 'INVALID_INDEX'     // Index out of range.
    // State / resource errors
    | 'NOT_FOUND'         // Target resource does not exist.
    | 'LOCKED'            // Playlist locked, etc.
    | 'NOT_SUPPORTED'     // The environment or the data does not support the operation.
    | 'PANEL_MODE_UNSUPPORTED' // Called from a DUI/CUI panel; the method needs the standalone window.
    | 'LIBRARY_DISABLED'  // Media library not enabled.
    | 'NO_ACTIVE_ITEM'    // No active playlist or track.
    // Operation failures
    | 'OPERATION_FAILED'  // Generic operation failure.
    | 'CANCELLED'         // The caller cancelled an async task.
    // Path / media specific
    | 'PERMISSION_DENIED' // Path-validation refused (PathSecuritySpec decorator).
    | 'ORIGIN_DENIED'     // Document origin not trusted for this capability.
    | 'MISSING_PATH'
    | 'INVALID_PATH'
    | 'INVALID_HANDLE'
    | 'NO_INFO'
    | 'DECODER_FAILED'
    | 'DECODE_FAILED'
    | 'UNKNOWN_ERROR'
    | 'EXCEPTION'
    // Menu and port
    | 'MENU_ITEM_DISABLED'     // The menu command exists but is disabled.
    | 'MENU_MATCH_AMBIGUOUS'   // A menu command name matched several commands; see `candidates`.
    | 'MENU_COMMAND_NOT_FOUND' // No menu command has the name or path.
    | 'PORT_NOT_FOUND'         // No open port has the id.
    | 'TARGET_NOT_FOUND'       // The target port of `port.postMessageTo` does not exist.
    // Allow third-party extensions
    | (string & {});

/**
 * What a failed call resolves with. Every generated `XxxResponse` is `XxxSuccess | ApiFailure`,
 * so check `success` before reading the result fields.
 *
 * A few methods add fields of their own to the failure, such as `candidates` on
 * `MENU_MATCH_AMBIGUOUS` or the window state after a failed window call. Test for them with
 * `in` (`'candidates' in r`) and check the value before use.
 */
export interface ApiFailure {
    success: false;
    /** Why the call failed. */
    error: string;
    /** Machine-readable reason; see {@link ApiErrorCode}. */
    code: ApiErrorCode;
    /** Extra context some failures carry, such as the offending parameter. */
    details?: JsonValue;
}

/**
 * Unified error envelope — minimum shape returned by failed sync API calls.
 *
 * All `invoke()` rejections satisfy this interface. Older APIs may only
 * carry `success` + `error` (back-compat), but newly migrated APIs MUST
 * include `code`.
 */
export interface ErrorEnvelope {
    success: false;
    error: string;
    /** Machine-readable error code; required on newly migrated APIs. */
    code?: ApiErrorCode;
    /** Additional context, e.g. `{ playlist: 3, isLocked: true }`. */
    details?: JsonObject;
}

/** Base envelope for every API response (success or failure). */
export interface BaseResponse {
    success: boolean;
    /** Error message when `success` is `false`. */
    error?: string;
    /** Machine-readable error code on migrated APIs. */
    code?: ApiErrorCode;
}

// ============================================================================
// Playlist responses
// ============================================================================

/** `playlist.getTrackCount` response; the generated {@link PlaylistGetTrackCountResponse} under its earlier name. */
export type TrackCountResponse = PlaylistGetTrackCountResponse;

/** `playlist.getTracks` page envelope; the generated {@link PlaylistGetTracksResponse} under its earlier name. */
export type PlaylistTracksResponse = PlaylistGetTracksResponse;

/** Runs of `playlist.getGroupRuns`, from the declaration. */
export type { PlaylistGroupRun, PlaylistGroupSubRun } from './generated/schema-types.js';

/** `playlist.getGroupRuns` response; the generated {@link PlaylistGetGroupRunsResponse} under its earlier name. */
export type PlaylistGroupRunsResponse = PlaylistGetGroupRunsResponse;

/** `playlist.getSelectedTracks` response; the generated {@link PlaylistGetSelectedTracksResponse} under its earlier name. */
export type PlaylistSelectedTracksResponse = PlaylistGetSelectedTracksResponse;

/** `playlist.getAutoplaylistInfo` response; the generated {@link PlaylistGetAutoplaylistInfoResponse} under its earlier name. */
export type PlaylistAutoplaylistInfoResponse = PlaylistGetAutoplaylistInfoResponse;

/** `playlist.getLockInfo` response; the generated {@link PlaylistGetLockInfoResponse} under its earlier name. */
export type PlaylistLockInfoResponse = PlaylistGetLockInfoResponse;

/** One column of `playlist.getAvailableColumns`, from the declaration. */
export type { PlaylistColumnDefinition } from './generated/schema-types.js';

/** `playlist.getAvailableColumns` response; the generated {@link PlaylistGetAvailableColumnsResponse} under its earlier name. */
export type PlaylistAvailableColumnsResponse = PlaylistGetAvailableColumnsResponse;

// ============================================================================
// Queue responses
// ============================================================================

// `queue.get` is declared in src/api/schema/queue.ts; each entry is the shared `Track`
// row plus `queueIndex`, `playlist` and `playlistItem`.
export type { QueueItem } from './generated/schema-types.js';

// ============================================================================
// Library responses
// ============================================================================

/**
 * One page of `library.getAll`, as `library.getAll()` resolves it: the
 * synchronous answer, or the `library:getAllResult` payload when the host
 * built the page off the main thread.
 */
export interface LibraryPagedTracksResponse {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The page in library order; `index` is the position in the library. */
    tracks: LibraryTrack[];
    /** The same list as `tracks`. */
    items: LibraryTrack[];
    /** Tracks in the library. */
    total: number;
    /** The `offset` applied. */
    offset?: number;
    /** The `limit` applied. */
    limit?: number;
    /** `true` when the page came from the list the host keeps. */
    fromCache?: boolean;
}

/** One track row of a library list, from the declaration in src/api/schema/library.ts. */
export type { LibraryTrack, RecentLibraryTrack } from './generated/schema-types.js';

/** A library track row narrowed to the `fields` a `library.query` or `library.search` call asked for. */
export type { LibraryTrackPartial } from './generated/schema-types.js';

/** `library.getAlbums` response; the generated {@link LibraryGetAlbumsResponse} under its earlier name. */
export type LibraryAlbumsResponse = LibraryGetAlbumsResponse;

/** `library.getArtists` response; the generated {@link LibraryGetArtistsResponse} under its earlier name. */
export type LibraryArtistsResponse = LibraryGetArtistsResponse;

/** `library.getAlbumTracks` response; the generated {@link LibraryGetAlbumTracksResponse} under its earlier name. */
export type LibraryAlbumTracksResponse = LibraryGetAlbumTracksResponse;

/** `library.getArtistTracks` response; the generated {@link LibraryGetArtistTracksResponse} under its earlier name. */
export type LibraryArtistTracksResponse = LibraryGetArtistTracksResponse;

// Every `library.*` method comes from the declarations in src/api/schema/library.ts; the names
// below keep their earlier spellings, and the generated names are re-exported at the end of this
// file.

/** `library.getGenres` response; the generated {@link LibraryGetGenresResponse} under its earlier name. */
export type LibraryGenresResponse = LibraryGetGenresResponse;

/** `library.getArtistAlbums` response; the generated {@link LibraryGetArtistAlbumsResponse} under its earlier name. */
export type LibraryArtistAlbumsResponse = LibraryGetArtistAlbumsResponse;

/** `library.getFieldValues` response; the generated {@link LibraryGetFieldValuesResponse} under its earlier name. */
export type LibraryFieldValuesResponse = LibraryGetFieldValuesResponse;

/** One library root folder, from the declaration. */
export type { LibraryRootInfo };

/** One folder of the directory tree index, from the declaration. */
export type { LibraryDirectoryNodeInfo } from './generated/schema-types.js';

/** `library.getRecentlyAdded` response; the generated {@link LibraryGetRecentlyAddedResponse} under its earlier name. */
export type LibraryRecentlyAddedResponse = LibraryGetRecentlyAddedResponse;

/** `library.getCacheStats` response; the generated {@link LibraryGetCacheStatsResponse} under its earlier name. */
export type LibraryCacheStatsResponse = LibraryGetCacheStatsResponse;

/** `library.getRandomTracks` response; the generated {@link LibraryGetRandomTracksResponse} under its earlier name. */
export type LibraryRandomTracksResponse = LibraryGetRandomTracksResponse;

/** `library.getArtistTracks` response; the generated {@link LibraryGetArtistTracksResponse} under another earlier name. */
export type LibraryArtistTracksFlatResponse = LibraryGetArtistTracksResponse;

/** High-level options for `library.enumerateFieldValues`. */
export interface LibraryEnumerateFieldValuesOptions {
    limit?: number;
    separator?: string;
}

/** High-level options for `library.enumerateTracks`. */
export interface LibraryEnumerateTracksOptions {
    pageSize?: number;
    start?: number;
    useCache?: boolean;
    signal?: AbortSignal;
    onProgress?: (info: {
        fetched: number;
        total: number;
        pages: number;
        offset: number;
        limit: number;
    }) => void;
}

/** Single page yielded by `library.enumerateTracks`. */
export interface LibraryEnumerateTracksPage extends Omit<LibraryPagedTracksResponse, 'success'> {
    fetched: number;
    pages: number;
}

/** Final summary returned by `library.enumerateTracks`. */
export interface LibraryEnumerateTracksSummary {
    total: number;
    fetched: number;
    pages: number;
    fromCacheHits: number;
    aborted: boolean;
}

/** @deprecated Options for `library.enumerateDirectories`; prefer {@link LibraryEnumerateTreeOptions} with `library.enumerateTree`. */
export interface LibraryEnumerateDirectoriesOptions {
    rootPath?: string;
    includeFiles?: boolean;
    strategy?: 'bfs' | 'dfs';
    signal?: AbortSignal;
    onProgress?: (info: {
        visited: number;
        pending: number;
        path: string;
    }) => void;
}

/** @deprecated Single batch yielded by `library.enumerateDirectories`; prefer {@link LibraryTreeBatch} from `library.enumerateTree`. */
export interface LibraryDirectoryBatch {
    path: string;
    directories: string[];
    files: LibraryTrack[];
    visited: number;
    pending: number;
    success: boolean;
    error?: string;
}

/** @deprecated Final summary returned by `library.enumerateDirectories`; prefer {@link LibraryEnumerateTreeSummary} from `library.enumerateTree`. */
export interface LibraryEnumerateDirectoriesSummary {
    visited: number;
    aborted: boolean;
}

/** Root-aware tree-walk options for `library.enumerateTree`. */
export interface LibraryEnumerateTreeOptions {
    /** Required: library-root id (from `getRoots().roots[].id`). */
    rootId: string;
    /** Starting `pathId`; defaults to `""` (root node). */
    pathId?: string;
    /** Include direct files in each batch; defaults to `false`. */
    includeFiles?: boolean;
    /** Traversal strategy: `'bfs'` (default) or `'dfs'`. */
    strategy?: 'bfs' | 'dfs';
    /** Abort signal that cancels the walk. */
    signal?: AbortSignal;
    /** Per-batch progress callback. */
    onProgress?: (info: {
        rootId: string;
        pathId: string;
        absolutePath: string;
        visited: number;
        pending: number;
    }) => void;
}

/** Single batch yielded by `library.enumerateTree`. */
export interface LibraryTreeBatch extends LibraryBrowseTreeSuccess {
    /** Number of nodes already visited. */
    visited: number;
    /** Number of nodes still queued for traversal. */
    pending: number;
}

/** Final summary returned by `library.enumerateTree`. */
export interface LibraryEnumerateTreeSummary {
    /** Root id that was traversed. */
    rootId: string;
    /** Number of nodes visited overall. */
    visited: number;
    /** True when the walk was cancelled via the abort signal. */
    aborted: boolean;
}

// ============================================================================
// Metadata responses
// ============================================================================

// The `metadata.*` responses come from the declarations in src/api/schema/metadata.ts.

/** Technical info of `metadata.read`, `metadata.readRaw` and the probe events, from the declaration. */
export type { TrackTechnicalInfo } from './generated/schema-types.js';

/** One row of `metadata.readBatch`, from the declaration. */
export type { MetadataReadBatchItem } from './generated/schema-types.js';

// ============================================================================
// Misc playlist / utility responses
// ============================================================================

/** Focus-track descriptor returned by `playlist.getFocusTrack`. */
export interface FocusedTrackResponse {
    index: number;
    track: TrackInfo | null;
}

/**
 * Response from `test.echo`. `test.echo` and `test.ping` are diagnostic methods without a
 * declaration, so their types are written here rather than generated.
 */
export interface TestEchoResponse {
    success: boolean;
    /** `message` when the call passed one, otherwise the whole params object. */
    echo: unknown;
    /** The params object the host received. */
    input: JsonObject;
}

/** Response from `test.ping`. */
export interface TestPingResponse {
    pong: boolean;
    /** The host's Unix time in seconds. */
    timestamp: number;
}

/** The `test.ping` response (what `utils.ping` resolves to) under its earlier name. */
export type PingResponse = TestPingResponse;

/** Result shape returned by `utils.formatTitle`. */
export interface FormatTitleResponse {
    result: string;
}

// ============================================================================
// Lyrics
// ============================================================================

/** Source filter for `lyrics.get`. */
export type LyricsSource = 'embedded' | 'file' | 'any';

/** Save target for `lyrics.save`. */
export type LyricsSaveTarget = 'file' | 'embedded' | 'all';

/** Type filter for `lyrics.get`. */
export type LyricsType = 'synced' | 'unsynced' | 'any';

/** File-format filter for `lyrics.get`. */
export type LyricsFileFormat = 'lrc' | 'txt' | 'any';

/** `lyrics.get` return value; the generated {@link LyricsGetResponse} under its earlier name. */
export type LyricsGetResult = LyricsGetResponse;

/** `lyrics.save` return value; the generated {@link LyricsSaveResponse} under its earlier name. */
export type LyricsSaveResult = LyricsSaveResponse;

/**
 * `lyrics.save` return value with several targets; the generated
 * {@link LyricsSaveResponse} under its earlier name, with each target's
 * outcome under `results`.
 */
export type LyricsSaveAllResult = LyricsSaveResponse;

/**
 * `lyrics.save` options: the host parameters other than the track path and
 * the text, which the method takes positionally, with `target` also
 * accepting a single destination.
 */
export interface LyricsSaveOptions extends Omit<LyricsSaveParams, 'path' | 'lyrics' | 'target'> {
    /** Save destination(s): `file` (default), `embedded`, `all`, or an array such as `['file', 'embedded']`. */
    target?: LyricsSaveTarget | LyricsSaveTarget[];
}

/** `lyrics.get` options: the host parameters other than the track path, which the method takes positionally. */
export type LyricsGetOptions = Omit<LyricsGetParams, 'path'>;

/** `lyrics.exists` return value; the generated {@link LyricsExistsResponse} under its earlier name. */
export type LyricsExistsResult = LyricsExistsResponse;

// ============================================================================
// Titleformat
// ============================================================================

/** `titleformat.eval` response; the generated {@link TitleformatEvalResponse} under its earlier name. */
export type TitleformatEvalResult = TitleformatEvalResponse;

/** `titleformat.evalBatch` response; the generated {@link TitleformatEvalBatchResponse} under its earlier name. */
export type TitleformatBatchResult = TitleformatEvalBatchResponse;

/** `titleformat.evalFields` response; the generated {@link TitleformatEvalFieldsResponse} under its earlier name. */
export type TitleformatFieldsResult = TitleformatEvalFieldsResponse;

/** `titleformat.evalFieldsBatch` response; the generated {@link TitleformatEvalFieldsBatchResponse} under its earlier name. */
export type TitleformatFieldsBatchResult = TitleformatEvalFieldsBatchResponse;

/** `titleformat.getBuiltinFields` response; the generated {@link TitleformatGetBuiltinFieldsResponse} under its earlier name. */
export type TitleformatBuiltinFields = TitleformatGetBuiltinFieldsResponse;

// ============================================================================
// Shell
// ============================================================================

/**
 * `shell.exec` options: the host parameters other than the command, which the
 * method takes positionally. `shell.spawn` takes these and `waitForExitMs`;
 * see {@link ShellSpawnParams}.
 */
export type ShellExecOptions = Omit<ShellExecParams, 'command'>;

/** `shell.exec` response; the generated {@link ShellExecResponse} under its earlier name. */
export type ShellExecResult = ShellExecResponse;

// ============================================================================
// Audio (waveform / spectrum)
// ============================================================================

/** The `audio.getOutputInfo` response under its earlier name. */
export type AudioOutputInfoResponse = AudioGetOutputInfoResponse;

/** The `audio.getStreamInfo` response under its earlier name. */
export type AudioStreamInfoResponse = AudioGetStreamInfoResponse;

/**
 * `audio.generateFullWaveform` options: the host parameters without `path`,
 * which the method takes positionally.
 */
export type FullWaveformOptions = Omit<AudioGenerateFullWaveformParams, 'path'>;

/** `audio.generateFullWaveform` synchronous result shape. */
export interface FullWaveformResult {
    success: boolean;
    /** Status: `ready` (waveform available) or `pending` (background job dispatched). */
    status?: 'ready' | 'pending';
    /** Waveform data; only present when `status === 'ready'`. */
    waveform?: number[];
    /** Background-task identifier; only present when `status === 'pending'`. */
    taskId?: string;
    /** @deprecated The host never sends it. */
    cacheKey?: string;
    /** True when the response was served from cache. */
    cached?: boolean;
    /** @deprecated The host never sends it; read {@link cached}. */
    fromCache?: boolean;
    /** Source file path. */
    path?: string;
    /** Sample resolution. */
    resolution?: number;
    /** Aggregation method used for the waveform points. */
    method?: 'peak' | 'rms';
    /** Error message when `success === false`. */
    error?: string;
    /** `ApiErrorCode` when `success === false`. */
    code?: string;
    /** Track duration in seconds. */
    duration?: number;
    /** Sample rate in Hz. */
    sampleRate?: number;
    /** Channel count. */
    channels?: number;
    /** True when waveform samples are signed (PCM convention). */
    signed?: boolean;
    /** Scale of the waveform points. */
    scale?: 'linear' | 'db';
    /**
     * The largest value of the selected sequence before normalisation, in
     * linear full-scale units (window RMS for `rms`, peak for `peak`,
     * absolute value for `signed`). On the `linear` scale
     * `waveform[i] * maxAmplitude` restores the level; on `db`, dBFS is
     * `(v * 60 - 60) + 20 * log10(maxAmplitude)`. Hosts before 1.14 omit it.
     */
    maxAmplitude?: number;
}

/**
 * Settled value of {@link SpectrumSubscription.ready}: whether the host
 * registered the subscription, with the values it registered.
 */
export type SpectrumSubscribeOutcome =
    | {
          ok: true;
          subscriptionId: string;
          /** Requested FFT size; frames report the size actually used. */
          fftSize: number;
          /** Band count after clamping to `[8, fftSize / 2]`. */
          bands: number;
          /** Frame rate after clamping to `[1, 60]`. */
          fps: number;
          scale: SpectrumScale;
          /**
           * Output the host registered. Hosts without bin output report
           * `'bands'` whatever was requested; the subscription then receives
           * no frames when bins were asked for.
           */
          output: SpectrumOutput;
          channels: SpectrumChannels;
          backgroundThrottle: boolean;
          /** Lower edge of the band range in Hz, as requested. */
          minFrequency: number;
          /**
           * Requested upper edge in Hz; `null` when the range follows half the
           * stream's sample rate. Frames report the edge actually used.
           */
          maxFrequency: number | null;
          /**
           * `true` once the visualisation stream exists, which includes the
           * stopped state; whether audio flows shows in the frames' `state`.
           */
          streamReady: boolean;
      }
    | {
          ok: false;
          /**
           * `INVALID_PARAMS` for rejected parameters, `NOT_SUPPORTED` when no
           * host is available, `UNKNOWN_ERROR` when the call itself failed.
           */
          code: string;
          error?: string;
      };

/**
 * Unsubscribe callback returned by `audio.subscribeSpectrum`. Calling it
 * detaches the listener and removes the subscription on the host.
 */
export interface SpectrumSubscription {
    (): void;
    /** Settles once the host has answered the registration; never rejects. */
    readonly ready: Promise<SpectrumSubscribeOutcome>;
}

/** The `audio.getSpectrumDebugState` response under its earlier name. */
export type SpectrumDebugState = AudioGetSpectrumDebugStateResponse;

// ============================================================================
// Reactive state
// ============================================================================

/**
 * Reactive bridge state object exposed as `fb.state`.
 *
 * Bridge subscribes to playback events and keeps these four fields up to
 * date so consumers can read them synchronously.
 */
export interface FBReactiveState {
    volume: number;
    isPlaying: boolean;
    currentTrack: TrackInfo | null;
    position: number;
}

// ============================================================================
// Port hub (cross-window messaging)
// ============================================================================

// The port row declared in src/api/schema/port.ts.
export type { PortInfo } from './generated/schema-types.js';

// ============================================================================
// Shared state
// ============================================================================

/** `sharedState.get` return value. */
export interface SharedStateValue<T = any> {
    key: string;
    value: T;
    exists: boolean;
}

// ============================================================================
// Playcount statistics
// ============================================================================

/**
 * Per-track playcount entry returned by `playcount.get` / `playcount.getBatch`;
 * the row type of the generated {@link PlaycountGetResponse} under its earlier name.
 */
export type PlaycountInfo = PlaycountGetSuccess['results'][number];

/** `playcount.getStats` response; the generated {@link PlaycountGetStatsResponse} under its earlier name. */
export type PlaycountStats = PlaycountGetStatsResponse;

// ============================================================================
// HTTP
// ============================================================================

/**
 * Request options shared by every `fb.http.*` verb: the {@link HttpGetParams}
 * other than `url`. Defaults: async dispatch with a 30 s host-side timeout,
 * `follow` redirects, `text` response type.
 */
export interface HttpRequestOptions extends Omit<HttpGetParams, 'url'> {
    /**
     * Response decoding hint:
     * - `'text'` (default) — UTF-8 string body. Rejects when the response
     *   contains non-UTF-8 bytes; use a binary mode for arbitrary bytes
     *   such as `.wasm` modules or images.
     * - `'base64'` — body returned as a base64-encoded string.
     * - `'arraybuffer'` / `'binary'` — `body` is decoded into an
     *   `ArrayBuffer` (transported as base64 under the hood).
     */
    responseType?: 'text' | 'base64' | 'arraybuffer' | 'binary';
    /**
     * Skip TLS certificate validation (self-signed, expired, host
     * mismatch, untrusted CA). Modeled after `curl --insecure`.
     *
     * Requires the host-side advanced setting "Allow self-signed /
     * invalid TLS certificates" to be enabled; ignored otherwise.
     * Do not enable for requests carrying credentials — the connection
     * becomes vulnerable to MITM.
     */
    insecureTls?: boolean;
    /** @deprecated Has no effect; use `insecureTls` to accept invalid certificates. */
    verifyTls?: boolean;
    /** @deprecated Has no effect; use `async: false`. */
    sync?: boolean;
}

// ============================================================================
// Menu (main / context menu tree)
// ============================================================================

// The menu tree is declared in src/api/schema/menu.ts as one flat MenuTreeNode.
// MenuSeparator, MenuCommand and MenuSubmenu narrow it by `type`, so code can
// pick a shape with `Extract<MenuItem, { type: 'command' }>`.
export type { MenuAvailability, MenuTreeNode } from './generated/schema-types.js';

/** Menu separator — marks a visual gap in the menu tree. */
export interface MenuSeparator {
    type: 'separator';
}

/**
 * Menu command — invokable leaf node.
 *
 * `flags` and `commandId` depend on the menu tier. The v2 tree and the v1
 * HMENU tier emit both,
 * but `flags` means different bits on each: raw SDK display flags on the v2
 * tree, raw Win32 `MENUITEMINFO` state bits on the HMENU tier. The flat
 * fallback tier (`fallback: true`) emits `flags` and no `commandId`, because
 * it enumerates commands without creating a menu. Check these optional
 * fields before using them.
 *
 * `guid` is the ONLY stable way to run the command. `commandId` is a Win32
 * menu id that dies with the transient menu it was generated from, so it must
 * never be persisted or passed back as an address.
 *
 * `source` is decided per leaf: on the tree tiers a node that carries
 * `subGuid` reports the family's dynamic value (`mainmenu_dynamic` /
 * `contextmenu_dynamic`), one without it reports the static value. Every
 * leaf of the v1 HMENU tier reports `hmenu_fallback` whether or not a
 * `subGuid` was back-filled for it.
 */
export type MenuCommand = Omit<MenuTreeNode, 'type' | 'children' | 'availability' | MenuNodeLabelKey | 'available'> &
    Required<Pick<MenuTreeNode, MenuNodeLabelKey | 'available'>> & { type: 'command' };

/** Menu submenu — container with child nodes. */
export type MenuSubmenu = Pick<MenuTreeNode, 'flags' | 'availability'> &
    Required<Pick<MenuTreeNode, MenuNodeLabelKey>> & { type: 'submenu'; children: MenuItem[] };

/** The label and path fields every command and submenu carries. */
type MenuNodeLabelKey = 'label' | 'displayLabel' | 'path' | 'displayPath';

/** Recursive menu-tree union node returned by `menu.getMainMenu` / `menu.getContextMenu`. */
export type MenuItem = MenuSeparator | MenuCommand | MenuSubmenu;

/** Response shape returned by `menu.getMainMenu`, with `items` narrowed to {@link MenuItem}. */
export type MenuGetMainMenuResponse = (Omit<MenuGetMainMenuWireSuccess, 'items'> & { items: MenuItem[] }) | ApiFailure;

/** Response shape returned by `menu.getContextMenu`, with `items` narrowed to {@link MenuItem}. */
export type MenuGetContextMenuResponse = (Omit<MenuGetContextMenuWireSuccess, 'items'> & { items: MenuItem[] }) | ApiFailure;

/**
 * A single item in a self-drawn (WebView-rendered) popup menu.
 *
 * This is the *input* shape accepted by `menu.show` / `menu.popup`,
 * distinct from the read-only {@link MenuItem} tree returned by
 * `menu.getMainMenu` / `menu.getContextMenu`.
 */
export type MenuPopupItem = {
    /** Stable id echoed back through `menu:select` when the row is chosen. */
    id?: string;
    /** Visible row text; omitted for separators. */
    label?: string;
    /**
     * Row kind. `'separator'` renders a divider; any other value is a normal row.
     *
     * The rich kinds (`'nowplaying'` / `'rating'` / `'slider'` / `'segmented'`)
     * draw an inline control instead of a plain label row. A rating, slider, or
     * segmented change reports through `menu:valueChanged` and **keeps the menu
     * open**; a now-playing card is an ordinary selection that reports through
     * `menu:select` and closes the menu.
     */
    type?: 'normal' | 'separator' | 'nowplaying' | 'rating' | 'slider' | 'segmented';
    /** Disabled rows are greyed out and cannot be chosen. Defaults to `true`. */
    enabled?: boolean;
    /** Renders a check mark to the left of the label. */
    checked?: boolean;
    /**
     * Inline monochrome SVG icon, drawn before the label. `content` is the SVG
     * inner markup (e.g. `"<path d=\"...\"/>"`). The overlay runtime parses it
     * with `DOMParser` and clones only an allowlisted set of shape elements /
     * attributes into the live document; raw `innerHTML` injection is not used.
     * An illegal or oversized (>32 KiB) icon is dropped silently and the row
     * continues without an icon.
     */
    iconSvg?: { viewBox: string; content: string };

    // ── Rich-item payload ─────────────────────────────────────────────────

    /**
     * `type: 'nowplaying'` album art, drawn at 40x40 CSS px by the default
     * stylesheet. Accepts three forms: a full data URL
     * (`data:image/jpeg;base64,...`), an `http(s)://` URL (used directly), or
     * raw base64 (decoded as JPEG).
     */
    cover?: string;
    /** `type: 'nowplaying'` primary line (track title). Falls back to `label`. */
    title?: string;
    /** `type: 'nowplaying'` secondary line (artist / album). */
    subtitle?: string;
    /**
     * Current value of a rich value control. `type: 'rating'` → integer stars
     * `0..5`; `type: 'slider'` → integer within `[min, max]`;
     * `type: 'segmented'` → zero-based index of the selected segment. Changes
     * are reported through `menu:valueChanged` as `{ menuId, itemId, value }`
     * without closing the menu, so the page owns what a value means.
     */
    value?: number;
    /** `type: 'slider'` range minimum (default `0`). */
    min?: number;
    /** `type: 'slider'` range maximum (default `100`). */
    max?: number;
    /**
     * `type: 'slider'` axis only (`'horizontal'` | `'vertical'`). Default when
     * omitted: `'horizontal'`. Only the exact value `'vertical'` is vertical;
     * unknown strings fall back to horizontal, and non-slider types ignore the
     * field. Vertical semantics: min at bottom, max at top; ArrowUp / ArrowRight
     * increase, ArrowDown / ArrowLeft decrease, Home=min, End=max. A constant
     * slider (`min === max` after normalization) displays its value and never
     * emits a value change.
     */
    orientation?: 'horizontal' | 'vertical';
    /**
     * `type: 'segmented'` segments — an inline single-select control (one row of
     * mutually exclusive options). The selected segment is {@link value}, a
     * zero-based index. Each segment shows its `iconSvg` when present, otherwise
     * its `label`; a segment with `enabled: false` is greyed out and cannot be
     * picked. Picking one reports `{ menuId, itemId, value }` through
     * `menu:valueChanged` and keeps the menu open (Left/Right also move the
     * selection). Segment icons go through the same allowlist sanitizer as item
     * icons, so an illegal icon is dropped per segment.
     */
    segments?: { label?: string; iconSvg?: { viewBox: string; content: string }; enabled?: boolean }[];
    /** Nested child items; presence renders a flyout arrow. */
    submenu?: MenuPopupItem[];
};

/**
 * Optional presentation configuration for `menu.show` / `menu.popup`.
 *
 * Every field is applied per call, so a page can follow its own theme state on
 * each right-click. Omitted fields keep their documented defaults.
 */
export interface MenuPopupOptions {
    /**
     * Window geometry model for the self-drawn menu (default `'fullscreen'`).
     *
     * - `'contentSized'`: the menu is drawn in a compact window measured to the
     *   menu content, with the root and its first-level submenu in separate,
     *   tightly sized popup windows. Each panel therefore carries the real DWM
     *   {@link backdrop} material across its own surface plus the system window
     *   shadow. This is the recommended model for a context menu.
     * - `'fullscreen'`: the compatibility default — one fullscreen overlay
     *   window hosting the menu DOM.
     */
    windowModel?: 'fullscreen' | 'contentSized';

    /**
     * Frontend style takeover. The CSS string (at most 256 KiB) is injected into
     * the overlay's dedicated `<style>` layer and applied on every open.
     *
     * By default this is **override / append** mode: your rules sit on top of the
     * built-in styles, so target the menu's stable class names — `.fb-menu`,
     * `.fb-item` (with `.nrm` / `.disabled` / `.active` / `.checked` / `.has-sub`),
     * `.fb-item-ico`, `.fb-arrow`, `.fb-sep`, the now-playing `.fb-np*`, rating
     * `.fb-rating*` / `.fb-star`, slider `.fb-slider*`, and segmented `.fb-seg*` —
     * and win by source order or `!important`. (The overlay is an isolated
     * top-level document, so a host page's `::part()` cannot reach it; the stable
     * class hooks are the supported styling contract.) A small protected
     * structural layer (`#viewport`, menu box-sizing / fixed positioning /
     * overflow, and the hidden-state fallback) is always force-applied last.
     *
     * For a translucent menu, keep the background alpha around `0.75`–`0.9`:
     * that preserves text contrast, and Windows 11's own system menus are
     * similarly restrained about how much they let through.
     */
    css?: string;

    /**
     * When `true`, switches {@link css} from override/append to **replace** mode:
     * the built-in default styles are disabled and only your `css` plus the
     * protected structural layer remain, so the menu's entire look (including the
     * entry animation) is yours to define. Default `false`.
     */
    cssReplace?: boolean;

    /**
     * DWM system backdrop for the menu window, sharing the effect vocabulary of
     * the main / tabbed windows (mapped to DWM `DWMSBT_*`): `'none'` (no
     * backdrop), `'mica'`, `'mica-alt'` and `'acrylic'`.
     *
     * Default `'acrylic'` — the transient-surface material, which is the correct
     * default for a pop-up menu. `'mica'` / `'mica-alt'` are designed as
     * main-window / tabbed-window backgrounds and may look off on a transient
     * menu. The material is only visible through a translucent menu background,
     * so pair it with a translucent `.fb-menu` background via {@link css}.
     * Acrylic needs Windows 11 22H2 or newer; Windows 10 degrades to whatever
     * backdrop the system supports.
     *
     * Warning: the DWM backdrop is a window-level, all-or-nothing effect: it
     * appears / disappears the instant the window is shown / hidden and **cannot
     * fade with CSS animations** (see {@link closeAnimationMs}). For an animated
     * open / close, set this to `'none'` and give `.fb-menu` a translucent
     * `background` via {@link css} — a CSS translucent background has no real
     * blur, which is the trade-off.
     */
    backdrop?: 'acrylic' | 'mica' | 'mica-alt' | 'none';

    /**
     * Dark tint for the {@link backdrop}. Default `true`; pass `false` to follow
     * a light theme. Applied together with {@link backdrop} on every open.
     */
    backdropDarkMode?: boolean;

    /**
     * Exit (fade-out) animation duration in milliseconds.
     *
     * Default `0` — the menu hides immediately on close with no exit animation.
     * When `> 0` (clamped to `0..1000`), a user-initiated close (clicking
     * outside, `Escape`, selecting an item, or losing focus) first plays an exit
     * transition for this many milliseconds before the window is hidden. Set it
     * to roughly your own `#menu.out` transition duration so the fade finishes
     * just as the window disappears.
     *
     * On close the renderer toggles the root menu's class from `#menu.in` to
     * `#menu.out`; the built-in `#menu.out` rule mirrors the entry animation, and
     * you can override it via {@link css}. The `replaced` (a new menu opening
     * over this one) and internal timeout close paths always hide immediately,
     * regardless of this value.
     *
     * Warning: this animates the web content only — **not** the DWM
     * {@link backdrop}, a window-level effect that snaps in / out with it. With
     * `acrylic` / `mica` enabled, the backdrop pops while the content fades, so
     * the transition is out of sync. For a fully smooth fade, use a CSS
     * translucent background ({@link backdrop} `'none'` plus a translucent
     * `.fb-menu` background via {@link css}) rather than the DWM backdrop.
     */
    closeAnimationMs?: number;
}

/**
 * Anchor position for a self-drawn popup menu, in screen pixels.
 * Either coordinate may be omitted to fall back to the cursor position.
 */
export interface MenuPopupPosition {
    /** Screen-space X anchor; defaults to the current cursor X. */
    x?: number;
    /** Screen-space Y anchor; defaults to the current cursor Y. */
    y?: number;
}

// ============================================================================
// Panel (webview-level config)
// ============================================================================

/** The configuration `panel.getConfig` reports: the generated shape under its older name. */
export type PanelConfigShape = PanelConfig;
export type { PanelConfig } from './generated/schema-types.js';

// ============================================================================
// Keyboard (hotkeys)
// ============================================================================

/** One entry of `keyboard.getRegisteredHotkeys`: the generated row under its older name. */
export type HotkeyInfo = KeyboardHotkey;
export type { KeyboardHotkey } from './generated/schema-types.js';

// ============================================================================
// Selection — viewing track
// ============================================================================

/** Source that provided the viewing track. */
export type ViewingTrackSource = 'now_playing' | 'selection';

/** Resolution mode requested / used by `selection.getViewingTrack`. */
export type ViewingTrackMode = 'prefer_playing' | 'prefer_selection';

// `selection.getViewingTrack` is declared in src/api/schema/selection.ts; its `track`
// is the shared `Track` row rather than the playback namespace's `TrackInfo`.

// ============================================================================
// Drag-and-Drop
// ============================================================================

/** Why the real-path side channel is unavailable for the current window. */
export type DndPathsUnavailableReason =
    /** `RegisterDragDrop` failed on the host window. */
    | 'register-failed'
    /** The composition controller needed to forward drag events was missing. */
    | 'forward-unavailable'
    /** No descendant window carried an OLE drop target to chain into. */
    | 'inner-target-not-found'
    /** The revoke/register transaction that installs the chain failed. */
    | 'chain-failed'
    /** Chromium re-registered its own drop target over the host's. */
    | 'displaced'
    /** The document origin is not trusted with real filesystem paths. */
    | 'origin-untrusted';

/** Why dragging files out of the window is unavailable for the current window. */
export type DndDragOutUnavailableReason =
    /**
     * The window hosts its WebView in standard controller mode, where Chromium
     * owns the drag source and the host has no way to take a drag over.
     */
    | 'not-visual-hosting'
    /**
     * The installed WebView2 Runtime predates the drag-start event this
     * feature is built on (Edge 144.0.3712.0).
     */
    | 'runtime-too-old'
    /** Subscribing to the drag-start event failed on this window. */
    | 'register-failed';

/** `dnd.prepareDrag` response; the generated {@link DndPrepareDragResponse} under its earlier name. */
export type DndDragToken = DndPrepareDragResponse;

/** `dnd.getPathsAsync` response; the generated {@link DndGetPathsAsyncResponse} under its earlier name. */
export type DndSessionPaths = DndGetPathsAsyncResponse;

/** `dnd.getCapabilities` response; the generated {@link DndGetCapabilitiesResponse} under its earlier name. */
export type DndCapabilities = DndGetCapabilitiesResponse;

// ============================================================================
// Discovery (services / menu / components / formats)
// ============================================================================

/**
 * Why a listed menu node cannot be executed. Empty string when it can.
 *
 * - `separator` — nothing to invoke.
 * - `dynamicParent` — a container slot; the SDK leaves `execute()` on one
 *   undefined, so its expanded children must be used instead.
 * - `noStableIdentifier` — the tier exposes no GUID for the node.
 * - `emptyNode` — a degenerate registration with neither name nor children.
 */
export type MenuUnaddressableReason =
    | ''
    | 'separator'
    | 'dynamicParent'
    | 'noStableIdentifier'
    | 'emptyNode';

/** Which registration tier a menu node was enumerated from. */
export type MenuNodeSource =
    | 'mainmenu_static'
    | 'mainmenu_dynamic'
    | 'contextmenu_static'
    | 'contextmenu_dynamic'
    | 'hmenu_fallback';

/**
 * State vocabulary shared by every menu enumeration endpoint.
 *
 * `flags` keeps the untranslated SDK bits, so an advanced caller can still tell
 * apart states that normalize to the same boolean — `flag_defaulthidden`
 * ("hidden unless Shift is held") and `FORCE_OFF` ("keyboard-shortcut list
 * only") both surface as `hidden: true`.
 *
 * `stateKnown` is not decoration: the context-menu tier can only report
 * `enabled` / `checked` when a track selection exists, because the SDK evaluates
 * display data against a track set. When it is false those two fields carry no
 * observation and must not be filtered on.
 */
export interface MenuNodeState {
    enabled?: boolean;
    checked?: boolean;
    /** Set for a radio-group member; implies `checked`. */
    radioChecked?: boolean;
    /** The host would not draw this entry — `get_display()` false, or `FORCE_OFF`. */
    hidden?: boolean;
    /** False when `enabled` / `checked` could not be observed. */
    stateKnown?: boolean;
    /** Raw SDK display flags. */
    flags?: number;
}

// The discovery shapes come from src/api/schema/discovery.ts; the names this module
// exported before stay available. `MenuNodeState`, `MenuNodeSource` and
// `MenuUnaddressableReason` above stay hand-written under their earlier names.
export type {
    DiscoveryComponentInfo,
    DiscoveryContextMenuCommand,
    DiscoveryContextMenuTreeNode,
    DiscoveryDspEntryInfo,
    DiscoveryInputFormatType,
    DiscoveryMainMenuCommand,
    DiscoveryMainMenuGroup,
    DiscoveryOutputDeviceEntry,
    DiscoveryPreferencePageInfo,
    DiscoverySearchResult,
    DiscoveryServiceCounts,
    DiscoveryUIElementInfo,
} from './generated/schema-types.js';

// ============================================================================
// ReplayGain
// ============================================================================

// The replaygain shapes come from the declarations in src/api/schema/replaygain.ts; the earlier
// names stay as aliases.

/** Per-track row of `replaygain.get`; the generated {@link ReplayGainTrackInfo}. */
export type { ReplayGainTrackInfo } from './generated/schema-types.js';

/** `replaygain.getSettings` response; the generated {@link ReplaygainGetSettingsResponse} under its earlier name. */
export type ReplayGainGetSettingsResponse = ReplaygainGetSettingsResponse;

/** `replaygain.getMode` response; the generated {@link ReplaygainGetModeResponse} under its earlier name. */
export type ReplayGainGetModeResponse = ReplaygainGetModeResponse;

/** `replaygain.getPreamp` response; the generated {@link ReplaygainGetPreampResponse} under its earlier name. */
export type ReplayGainGetPreampResponse = ReplaygainGetPreampResponse;

/** `replaygain.get` response; the generated {@link ReplaygainGetResponse} under its earlier name. */
export type ReplayGainGetResponse = ReplaygainGetResponse;

// ============================================================================
// Output (modules / settings)
// ============================================================================

/** Output-module descriptor returned by `output.getEntries`. */
export interface OutputEntryInfo {
    guid: string;
    name: string;
    needsBitdepthConfig: boolean;
    needsDitherConfig: boolean;
    supportsMultipleStreams: boolean;
    isHighLatency: boolean;
    isLowLatency: boolean;
}

/** Response shape returned by `output.getEntries`. */
export interface OutputGetEntriesResponse {
    entries?: OutputEntryInfo[];
    count?: number;
    success?: boolean;
    error?: string;
}

/** Response shape returned by `output.getSettings`. */
export interface OutputGetSettingsResponse {
    note?: string;
    /** Names of every available output module. */
    availableOutputs?: string[];
    success?: boolean;
    error?: string;
}

// ============================================================================
// JitQueue (just-in-time playback queue)
// ============================================================================

/**
 * Response shape returned by `jitQueue.getState`; declared in
 * src/api/schema/jitQueue.ts, this name stays as an alias.
 */
export type JitQueueStateInfo = JitQueueGetStateResponse;

// ============================================================================
// File (directory listing / metadata)
// ============================================================================

// `file.list` and `file.getInfo` responses come from the declarations in src/api/schema/file.ts;
// both names are re-exported with the generated per-API types at the end of this file.

// ============================================================================
// Shell (process execution)
// ============================================================================

// `shell.exec` and `shell.spawn` responses come from the declarations in src/api/schema/shell.ts.
export type { ShellExecResponse, ShellSpawnResponse };

// ============================================================================
// System (API discovery / plugin registry)
// ============================================================================

export type { SystemApiInfo, SystemPluginInfo } from './generated/schema-types.js';

/** The `system.getApiStats` response under its older name. */
export type SystemApiStatsResponse = SystemGetApiStatsResponse;

// ============================================================================
// Generated per-API response types
// ============================================================================

export type {
    ApiResponseMap, ArtworkGetAvailableArtworkResponse, ArtworkGetAvailableTypesResponse, ArtworkGetBatchResponse, ArtworkGetByPathResponse, ArtworkGetByPlaylistItemResponse,
    ArtworkGetCurrentResponse, ArtworkGetFb2kUrlByPathBatchResponse, ArtworkGetFb2kUrlByPathResponse, ArtworkGetFb2kUrlResponse, ArtworkGetFolderImagesResponse, ArtworkGetForTrackResponse,
    ArtworkGetLyricsResponse, ArtworkGetMetadataResponse, AudioAnalyzeBPMResponse, AudioGenerateFullWaveformResponse, AudioGetOutputInfoResponse, AudioGetSpectrumDebugStateResponse,
    AudioGetSpectrumResponse, AudioGetStreamInfoResponse, AudioGetWaveformResponse, AudioIsVisualizationAvailableResponse, AudioSetChannelModeResponse, AudioSubscribeSpectrumResponse, AudioSubscribeStreamResponse,
    AudioUnsubscribeSpectrumResponse, AudioUnsubscribeStreamResponse, ClipboardReadResponse, ClipboardWriteFilesResponse, ClipboardWriteHTMLResponse, ClipboardWriteResponse,
    ConfigExportResponse, ConfigGetActiveDspPresetResponse, ConfigGetAdvancedConfigResponse, ConfigGetAdvancedConfigValueResponse, ConfigGetAllResponse, ConfigGetComponentsResponse, ConfigGetCursorFollowPlaybackResponse, ConfigGetDspPresetsResponse,
    ConfigGetLibraryFilePatternsResponse, ConfigGetLibraryStatusResponse, ConfigGetOutputConfigResponse, ConfigGetOutputDevicesResponse, ConfigGetPlaybackFollowCursorResponse, ConfigGetPreferencesPagesResponse,
    ConfigGetPreferencesStandardGuidsResponse, ConfigGetReplaygainModeResponse, ConfigGetResponse, ConfigGetVersionInfoResponse, ConfigRemoveResponse, ConfigResetAdvancedConfigResponse,
    ConfigSetActiveDspPresetResponse, ConfigSetAdvancedConfigValueResponse, ConfigSetCursorFollowPlaybackResponse, ConfigSetOutputBufferResponse, ConfigSetOutputDeviceResponse, ConfigSetPlaybackFollowCursorResponse,
    ConfigSetReplaygainModeResponse, ConfigSetResponse, ConfigShowLibraryPreferencesResponse, ConsoleErrorResponse, ConsoleLogResponse, ConsoleWarnResponse,
    CursorIsHiddenResponse, CursorSetHiddenResponse,
    DialogConfirmResponse, DialogOpenFileResponse, DialogOpenFolderResponse, DialogSaveFileResponse,
    DiscoveryExecuteContextMenuByPathResponse, DiscoveryExecuteContextMenuCommandResponse, DiscoveryExecuteMainMenuCommandResponse, DiscoveryGetAllServicesResponse,
    DiscoveryGetComponentsResponse, DiscoveryGetContextMenuCommandsResponse, DiscoveryGetContextMenuTreeResponse, DiscoveryGetDspEntriesResponse, DiscoveryGetInputFormatsResponse,
    DiscoveryGetMainMenuCommandsResponse, DiscoveryGetMainMenuGroupsResponse, DiscoveryGetOutputDevicesResponse, DiscoveryGetPreferencePagesResponse, DiscoveryGetUIElementsResponse,
    DiscoverySearchCommandsResponse, DndGetCapabilitiesResponse, DndGetPathsAsyncResponse, DndPrepareDragResponse,
    DndStartDragResponse, DspAddDspResponse, DspApplyPresetResponse, DspGetAvailableResponse, DspGetChainResponse, DspGetPresetsResponse,
    DspMoveDspResponse, DspRemoveDspResponse, DspSetChainResponse, EventEmitResponse, EventEmitToResponse, FileCancelOpResponse,
    FileCopyAsyncResponse, FileCopyResponse, FileDeleteAsyncResponse, FileDeleteResponse, FileExistsResponse, FileGetInfoResponse, FileListResponse, FileMkdirResponse,
    FileMoveAsyncResponse, FileMoveResponse, FileReadResponse, FileRenameResponse, FileWriteResponse, HttpAbortResponse,
    HttpDeleteResponse, HttpDownloadResponse, HttpGetResponse, HttpHeadResponse,
    HttpPatchResponse, HttpPostResponse, HttpPutResponse, JitQueueClearResponse, JitQueueEnqueueNextResponse, JitQueueGetStateResponse,
    JitQueueNotifyEmptyResponse, JitQueuePlayNowResponse, JitQueuePreloadBatchResponse, JitQueueSkipResponse, JitQueueStopResponse, KeyboardGetRegisteredHotkeysResponse, KeyboardRegisterHotkeyResponse,
    KeyboardRegisterShortcutResponse, KeyboardUnregisterHotkeyResponse, LibraryAddToPlaylistResponse, LibraryBrowseDirectoryResponse, LibraryBrowseTreeResponse, LibraryGetAlbumTracksResponse, LibraryGetAlbumsResponse, LibraryGetAllResponse,
    LibraryGetArtistAlbumsResponse, LibraryGetArtistTracksResponse, LibraryGetArtistsResponse, LibraryGetByPathResponse, LibraryGetCacheStatsResponse, LibraryGetCountResponse,
    LibraryGetFieldValuesResponse, LibraryGetGenresResponse, LibraryGetRandomTracksResponse, LibraryGetRecentlyAddedResponse, LibraryGetRootsResponse, LibraryGetStatsResponse,
    LibraryGetStatusResponse, LibraryInvalidateCacheResponse, LibraryIsEnabledResponse, LibraryQueryResponse, LibraryRefreshResponse, LibraryRescanResponse, LibrarySearchResponse, LogClearResponse, LogReadResponse, LogWriteResponse, LyricsExistsResponse, LyricsGetResponse, LyricsSaveResponse,
    MenuRunContextCommandByIdResponse, MenuRunContextCommandResponse, MenuRunMainMenuCommandResponse, MenuShowNativePopupResponse, MetadataCancelProbeResponse, MetadataEmbedArtworkResponse,
    MetadataProbeBatchAsyncResponse, MetadataReadBatchResponse, MetadataReadByPathResponse, MetadataReadRawResponse, MetadataReadResponse, MetadataRemoveEmbeddedArtResponse,
    MetadataRemoveFieldResponse, MetadataRemoveTagResponse, MetadataWriteBatchResponse, MetadataWriteResponse, MiscExitResponse, MiscGetComponentPathResponse,
    MiscGetFoobarPathResponse, MiscGetProfilePathResponse, MiscRestartResponse, MiscShowConsoleResponse, MiscShowLibrarySearchResponse, MiscShowPopupMessageResponse,
    MiscShowPreferencesResponse, OutputGetDevicesResponse, PanelGetConfigResponse, PanelSetConfigResponse, PlaybackGetCurrentTrackIndexResponse, PlaybackGetCurrentTrackResponse, PlaybackGetPlaybackOrderResponse,
    PlaybackGetPlayingPlaylistResponse, PlaybackGetPositionResponse, PlaybackGetStateResponse, PlaybackGetStopAfterCurrentResponse, PlaybackGetVolumeResponse, PlaybackMuteResponse,
    PlaybackNextResponse, PlaybackPauseResponse, PlaybackPlayOrPauseResponse, PlaybackPlayPauseResponse, PlaybackPlayResponse, PlaybackPreviousResponse,
    PlaybackRandomResponse, PlaybackSetPlaybackOrderResponse, PlaybackSetStopAfterCurrentResponse, PlaybackSetVolumeResponse, PlaybackStopResponse, PlaybackToggleStopAfterCurrentResponse,
    PlaybackPlayPathResponse, PlaybackPlayPathsResponse, PlaybackSetPositionResponse, PlaybackToggleMuteResponse,
    PlaybackVolumeDownResponse, PlaybackVolumeUpResponse, PlaycountGetBatchResponse, PlaycountGetResponse, PlaycountGetStatsResponse, PlaycountSetResponse, PlaylistAddHandlesResponse, PlaylistAddPathsAsyncResponse,
    PlaylistAddPathsResponse, PlaylistAddPathsSequentialResponse, PlaylistClearResponse, PlaylistConvertToAutoplaylistResponse,
    PlaylistCreateAutoplaylistResponse, PlaylistCreateResponse, PlaylistDuplicateResponse, PlaylistDeselectAllResponse, PlaylistFocusTrackResponse, PlaylistGetActiveResponse, PlaylistGetAllResponse, PlaylistGetAutoplaylistInfoResponse,
    PlaylistGetAutoplaylistQueryResponse, PlaylistGetAvailableColumnsResponse, PlaylistGetCountResponse, PlaylistGetFocusTrackResponse, PlaylistGetFocusedTrackResponse, PlaylistGetLockInfoResponse,
    PlaylistGetMatchingRowsResponse, PlaylistGetPlayingResponse, PlaylistGetSelectedTracksResponse, PlaylistGetSelectionResponse, PlaylistGetTrackCountResponse, PlaylistGetTracksAtResponse, PlaylistGetTracksResponse, PlaylistInsertTracksResponse,
    PlaylistIsAutoplaylistResponse, PlaylistIsLockedResponse, PlaylistMoveTracksResponse, PlaylistPlayTrackResponse, PlaylistRedoResponse, PlaylistRemoveResponse,
    PlaylistRemoveAutoplaylistResponse, PlaylistRemoveSelectedTracksResponse, PlaylistRemoveTracksResponse, PlaylistRenameResponse,
    PlaylistReorderPlaylistsResponse, PlaylistReorderResponse, PlaylistReplaceAllAndPlayResponse, PlaylistReverseResponse, PlaylistSelectAllResponse, PlaylistSetActiveResponse,
    PlaylistSetFocusedTrackResponse, PlaylistSetSelectionResponse, PlaylistShuffleResponse, PlaylistSortResponse, PlaylistUndoResponse, PortConnectResponse,
    PortDisconnectResponse, PortGetPortsResponse, PortPostMessageResponse, PortPostMessageToResponse, QueueAddPathsResponse, QueueAddResponse,
    QueueClearResponse, QueueFlushResponse, QueueGetCountResponse, QueueInsertNextResponse, QueueMoveToTopResponse, QueuePlayNowResponse,
    QueueRemoveResponse, QueueSetContentsResponse, RatingGetResponse,
    RatingSetResponse, ReplaygainClearResponse, ReplaygainGetModeResponse, ReplaygainGetPreampResponse, ReplaygainGetResponse, ReplaygainGetSettingsResponse,
    ReplaygainScanResponse, ReplaygainSetModeResponse, ReplaygainSetPreampResponse, SelectionGetResponse, SelectionGetTypeResponse, SelectionGetViewerModeResponse, SelectionGetViewingTrackResponse,
    SelectionSetPlaylistTrackingResponse, SelectionSetResponse, ShellOpenExternalResponse, ShellOpenWithResponse, ShellShowInExplorerResponse, StateDeleteResponse,
    StateGetResponse, StateKeysResponse, StateSetResponse, SystemGetApiStatsResponse, SystemGetApisByNamespaceResponse, SystemGetDPIResponse,
    SystemGetLocaleResponse, SystemGetRegisteredPluginsResponse, SystemGetThemeResponse, SystemIsPluginRegisteredResponse, SystemListAvailableApisResponse, SystemSearchApisResponse,
    TitleformatEvalBatchResponse, TitleformatEvalFieldsBatchResponse, TitleformatEvalFieldsResponse, TitleformatEvalResponse,
    TitleformatGetBuiltinFieldsResponse, UiHideNotificationResponse, UiShowContextMenuResponse, UiShowCustomMenuResponse, UiShowNotificationResponse, UiShowToastResponse, WindowBlurResponse,
    WindowBroadcastResponse, WindowCancelCloseResponse, WindowCenterResponse, WindowClearClickThroughExcludeRegionsResponse, WindowClearDragRegionsResponse, WindowClearNoDragRegionsResponse,
    WindowCloseAllPopupsResponse, WindowClosePopupResponse, WindowCloseResponse, WindowConfirmCloseResponse, WindowCreatePopupResponse, WindowEnterFullscreenResponse,
    WindowExitFullscreenResponse, WindowFlashResponse, WindowFlashTaskbarResponse, WindowFocusResponse, WindowGetAllWindowsResponse, WindowGetBackdropPolicyResponse,
    WindowGetBoundsResponse, WindowGetCaptionButtonsWidthResponse, WindowGetCornerPreferenceResponse, WindowGetCurrentWindowIdResponse, WindowGetDevServerConfigResponse, WindowGetDpiScaleResponse,
    WindowGetMaxSizeResponse, WindowGetMinSizeResponse, WindowGetModeResponse, WindowGetPopupBehaviorResponse, WindowGetStateResponse, WindowGetTitleResponse,
    WindowGetTitlebarHeightResponse, WindowGetTitlebarInfoResponse, WindowGetZoomResponse, WindowHasSavedBoundsResponse, WindowIsAlwaysOnTopResponse, WindowIsClickThroughResponse,
    WindowIsFullscreenResponse, WindowIsMaximizedResponse, WindowIsMinimizedResponse, WindowIsResizableResponse, WindowMaximizeResponse, WindowMinimizeResponse,
    WindowRefreshWebViewResponse, WindowReloadResponse, WindowResetZoomResponse, WindowRestoreResponse, WindowSendMessageResponse, WindowSetAcrylicResponse,
    WindowSetAlwaysOnTopResponse, WindowSetBackdropPolicyResponse, WindowSetBackgroundTransparencyResponse, WindowSetBlurResponse, WindowSetBoundsResponse, WindowSetClickThroughExcludeRegionsResponse,
    WindowSetClickThroughResponse, WindowSetCornerPreferenceResponse, WindowSetDarkModeResponse, WindowSetDevServerConfigResponse, WindowSetDragRegionsResponse, WindowSetFramelessResponse,
    WindowSetFullscreenResponse, WindowSetMaxSizeResponse, WindowSetMicaEffectResponse, WindowSetMicaResponse, WindowSetMinSizeResponse, WindowSetNoDragRegionsResponse,
    WindowSetPopupBehaviorResponse, WindowSetPositionResponse, WindowSetResizableResponse, WindowSetSizeResponse, WindowSetTitleResponse, WindowSetTitlebarHeightResponse,
    WindowSetZoomForDpiResponse, WindowSetZoomResponse, WindowShowSystemMenuResponse, WindowStartDragResponse, WindowStartResizeResponse, WindowToggleAlwaysOnTopResponse,
    WindowToggleFullscreenResponse, WindowToggleMaximizeResponse,
} from './generated/responses.js';

// Every generated `XxxResponse` and its `XxxSuccess` half, including the ones the list above
// does not name. A type of the same name declared in this file takes precedence.
export type * from './generated/responses.js';

export type {
    ReplaygainSourceMode,
    ReplaygainSourceModeName,
} from './overrides/config.js';
export { REPLAYGAIN_SOURCE_MODE } from './overrides/config.js';
export type {
    SpectrumBeatSource,
    SpectrumBinsFrame,
    SpectrumChannels,
    SpectrumFrameInfo,
    SpectrumFrameState,
    SpectrumMixBinsFrame,
    SpectrumOutput,
    SpectrumScale,
    SpectrumStereoBinsFrame,
    WaveformChannels,
} from './overrides/audio.js';

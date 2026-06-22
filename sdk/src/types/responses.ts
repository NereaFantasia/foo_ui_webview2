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

import type { JsonObject } from './json.js';

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
    album: string;
    albumArtist?: string;
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

/** A single track occupying a slot in a foobar2000 playlist. */
export interface PlaylistTrack extends TrackInfo {
    index: number;
    isPlaying: boolean;
    isSelected: boolean;
}

// ============================================================================
// Playlist
// ============================================================================

/** Top-level metadata for a foobar2000 playlist. */
export interface PlaylistInfo {
    index: number;
    name: string;
    trackCount: number;
    isActive: boolean;
    isPlaying: boolean;
    isLocked: boolean;
    isAutoplaylist?: boolean;
}

/** Selection set descriptor returned by `playlist.getSelection`. */
export interface SelectionInfo {
    /** Indices of the selected playlist items. */
    items: number[];
    count: number;
    playlist?: number;
}

/**
 * Result of `selection.get`. The selection-manager API works on
 * metadb handles rather than playlist indices, so the envelope is
 * structurally distinct from {@link SelectionInfo}: it carries
 * `handles[]` (with `|subsong:N` suffixes for CUE rows), the typed
 * selection-source string, and pagination metadata.
 */
export interface SelectionGetResult {
    count: number;
    type: string;
    /** Stringified `path[|subsong:N]` for each selected metadb handle. */
    handles: string[];
    offset: number;
    /** True when the slice is shorter than the full selection. */
    hasMore: boolean;
    /** True when the host capped the page at the implicit 100-item limit. */
    truncated?: boolean;
}

// ============================================================================
// Library
// ============================================================================

/** Album-level aggregate row used by `library.getAlbums` etc. */
export interface AlbumInfo {
    name: string;
    artist: string;
    albumArtist?: string;
    trackCount: number;
    discCount?: number;
    duration: number;
    year?: string;
    genre?: string;
    label?: string;
    firstTrackPath?: string;
    firstTrackAbsolutePath?: string;
    coverDataUrl?: string;
    tracks?: TrackInfo[];
}

/** Artist-level aggregate row used by `library.getArtists`. */
export interface ArtistInfo {
    name: string;
    albumCount: number;
    trackCount: number;
    duration: number;
}

/**
 * Coarse-grained snapshot of the media-library subsystem state,
 * returned by `config.getLibraryStatus` and `library.getStatus`.
 *
 * Per-entity counters (albums / artists / …) live in {@link LibraryStats}
 * (`library.getStats`).
 */
export interface LibraryStatus {
    enabled?: boolean;
    initialized: boolean;
    /** True while a scan is in progress. */
    scanning?: boolean;
    /** Total enumerated items in the library (foobar2000 calls them "items"). */
    itemCount?: number;
    /** Alias of {@link itemCount}; the host writes both. */
    count?: number;
}

/** Aggregate counters exposed by `library.getStats`. */
export interface LibraryStats {
    totalTracks: number;
    totalAlbums: number;
    totalArtists: number;
    totalDuration: number;
    totalSize: number;
    averageBitrate?: number;
    /** Last cache invalidation timestamp (epoch ms). */
    lastModified?: number;
    /** True when the cached snapshot is still valid. */
    cacheValid?: boolean;
}

// ============================================================================
// Playback state
// ============================================================================

/** Result of `playback.getState` calls. */
export interface PlaybackState {
    state: PlaybackStateValue;
    canSeek: boolean;
    /** Whether the current track can be paused (always `true` today). */
    canPause: boolean;
}

/** Single entry from the playback-order enumeration. */
export interface PlaybackOrderInfo {
    order: number;
    name: string;
    /** Alias of {@link order}; the host writes both. */
    orderIndex?: number;
    /** Alias of {@link name}; the host writes both. */
    orderName?: string;
}

/**
 * Receipt returned by `playback.setPlaybackOrder`. The handler echoes
 * the resolved `orderName` so callers know which mode landed when they
 * passed a string alias.
 */
export interface PlaybackSetOrderResponse extends BaseResponse {
    /** Resolved canonical order name. */
    orderName?: string;
}

/**
 * Result of `playback.playPause` and `playback.playOrPause`. Reports
 * the post-toggle play/pause state so callers don't need to follow up
 * with a `getState`.
 */
export interface PlaybackToggleResponse extends BaseResponse {
    isPlaying: boolean;
}

/** Result of `playback.toggleMute`. */
export interface PlaybackToggleMuteResponse extends BaseResponse {
    muted: boolean;
}

/**
 * Result of `playback.toggleStopAfterCurrent` and
 * `playback.getStopAfterCurrent`. No `success` field is written; a
 * successful call just returns the new `enabled` flag.
 */
export interface PlaybackStopAfterCurrentState {
    enabled: boolean;
}

/**
 * Result of `playback.setPosition`. Surfaces the requested seek position
 * alongside the actual landing position and track context so callers
 * can validate clamp / snap behaviour.
 */
export interface PlaybackSetPositionResponse extends BaseResponse {
    /** Position the caller asked for (echoed for sanity). */
    requestedPosition?: number;
    /** Position after clamping to track bounds. */
    actualPosition?: number;
    /** Position before the seek. */
    oldPosition?: number;
    /** Position after the seek (post-engine reaction). */
    newPosition?: number;
    /** Active track duration in seconds. */
    duration?: number;
    /** Active subsong index. */
    subsong?: number;
}

/** Result of `playback.playPath`. */
export interface PlaybackPlayPathResponse extends BaseResponse {
    path?: string;
    /** Resolved subsong index after `|subsong:N` parsing. */
    subsong?: number;
    /** Number of tracks added to the active playlist. */
    tracksAdded?: number;
}

/** Result of `playback.playPaths`. */
export interface PlaybackPlayPathsResponse extends BaseResponse {
    /** Number of tracks added to the active playlist. */
    tracksAdded?: number;
    /** Index where playback actually started. */
    startedAt?: number;
}

/** Result shape returned by `playback.getVolume`. */
export interface VolumeResponse {
    /** Volume on a 0-100 linear scale. */
    volume: number;
    /** Volume in dB. */
    volumeDb: number;
    /** True when output is muted. */
    muted: boolean;
    /** Alias of {@link muted}; the host writes both. */
    isMuted?: boolean;
}

/** Result shape returned by `playback.getPosition`. */
export interface PositionResponse {
    /** Current playback position in seconds. */
    position: number;
    /** Total track duration in seconds. */
    duration?: number;
    /** Subsong index. */
    subsong?: number;
    /** File path of the current track. */
    path?: string;
}

// ============================================================================
// Window
// ============================================================================

/**
 * Result shape returned by `ui.getWindowState` / `window.getState`.
 *
 * Every boolean is written twice - once with the `is*` prefix and once
 * without - so callers can use whichever style fits. Both flavours are
 * declared here as optional aliases.
 */
export interface WindowState {
    isMaximized: boolean;
    isMinimized: boolean;
    isFullscreen: boolean;
    isAlwaysOnTop?: boolean;
    isFocused?: boolean;
    /** Bare-name alias of {@link isMaximized}. */
    maximized?: boolean;
    /** Bare-name alias of {@link isMinimized}. */
    minimized?: boolean;
    /** Bare-name alias of {@link isFullscreen}. */
    fullscreen?: boolean;
    /** Bare-name alias of {@link isAlwaysOnTop}. */
    alwaysOnTop?: boolean;
    /** Bare-name alias of {@link isFocused}. */
    focused?: boolean;
    width: number;
    height: number;
    x: number;
    y: number;
}

/** DPI scale factor reported by `ui.getDpiScale` / `window.getDpiScale`. */
export interface DpiScaleResponse {
    scale: number;
    /** Raw DPI value (96 = 100% scale). */
    dpi?: number;
}

/** Allowed values for the `activeEffect` slot in `window.backdropPolicy.*`. */
export type WindowActiveBackdropEffect =
    | 'inherit'
    | 'none'
    | 'mica'
    | 'mica-alt'
    | 'acrylic';

/** Allowed values for the `inactiveEffect` slot in `window.backdropPolicy.*`. */
export type WindowInactiveBackdropEffect =
    | 'system'
    | 'none'
    | 'mica'
    | 'mica-alt'
    | 'acrylic';

/** Built-in popup behaviour profiles. */
export type WindowPopupProfile = 'legacy' | 'standard' | 'miniPlayer';

/** Rectangle in screen-space pixels. */
export interface WindowBounds {
    x: number;
    y: number;
    width: number;
    height: number;
}

/** Authoritative backdrop policy reported by the host. */
export interface WindowBackdropPolicyState {
    activeEffect: WindowActiveBackdropEffect;
    inactiveEffect: WindowInactiveBackdropEffect;
    darkMode: boolean;
    reapplyOnActivate: boolean;
}

/** Patch shape accepted by `window.setBackdropPolicy`. */
export interface WindowBackdropPolicyPatch {
    activeEffect?: WindowActiveBackdropEffect | null;
    inactiveEffect?: WindowInactiveBackdropEffect | null;
    darkMode?: boolean | null;
    reapplyOnActivate?: boolean | null;
}

/** Authoritative popup behaviour reported by the host. */
export interface WindowPopupBehaviorState {
    showInTaskbar: boolean;
    showInAltTab: boolean;
    keepVisibleOnShowDesktop: boolean;
    allowMinimize: boolean;
    owner: 'none' | 'main';
    noActivate: boolean;
}

/** Patch shape accepted by `window.setPopupBehavior`. */
export interface WindowPopupBehaviorPatch {
    showInTaskbar?: boolean | null;
    showInAltTab?: boolean | null;
    keepVisibleOnShowDesktop?: boolean | null;
    allowMinimize?: boolean | null;
    owner?: 'none' | 'main' | null;
    noActivate?: boolean | null;
}

/** Capability bitmap describing which window features the host supports. */
export interface WindowObservationCapabilities {
    supportsBackdropPolicy: boolean;
    supportsFrameless: boolean;
    supportsCornerPreference: boolean;
    supportsPopupBehavior: boolean;
    supportsMicaAlt: boolean;
    supportsFullscreen: boolean;
}

/** Common fields shared by all window-info entries. */
export interface WindowInfoBase {
    windowId: string;
    isMain: boolean;
    title: string;
    bounds: WindowBounds;
    capabilities: WindowObservationCapabilities;
}

/** Window-info entry for the foobar2000 main window. */
export interface MainWindowInfo extends WindowInfoBase {
    isMain: true;
    backdropPolicy: Partial<WindowBackdropPolicyState>;
    resolvedBackdropPolicy: WindowBackdropPolicyState;
}

/** Window-info entry for a popup window. */
export interface PopupWindowInfo extends WindowInfoBase {
    isMain: false;
    url: string;
    profile: WindowPopupProfile;
    behavior: Partial<WindowPopupBehaviorState>;
    resolvedBehavior: WindowPopupBehaviorState;
    backdropPolicy: Partial<WindowBackdropPolicyState>;
    resolvedBackdropPolicy: WindowBackdropPolicyState;
}

/** Discriminated union of window-info entries. */
export type WindowInfoItem = MainWindowInfo | PopupWindowInfo;

/** Result shape returned by `window.list`. */
export interface WindowListResponse {
    items: WindowInfoItem[];
}

/** Result shape returned by `window.setPopupBehavior`. */
export interface WindowPopupBehaviorResponse extends BaseResponse {
    windowId: string;
    profile: WindowPopupProfile;
    behavior: Partial<WindowPopupBehaviorState>;
    resolvedBehavior: WindowPopupBehaviorState;
}

/** Result shape returned by `window.setBackdropPolicy`. */
export interface WindowBackdropPolicyResponse extends BaseResponse {
    windowId: string;
    backdropPolicy: Partial<WindowBackdropPolicyState>;
    resolvedBackdropPolicy: WindowBackdropPolicyState;
}

/** Result shape returned by `window.getTitlebarInfo`. */
export interface WindowTitlebarInfo {
    /** Titlebar height in DIPs. */
    height: number;
    /** Combined width of the three caption buttons (min/max/close). */
    captionButtonsWidth: number;
    /** Width of a single caption button. */
    captionButtonWidth: number;
    /** True when the window is currently maximised (zoomed). */
    isMaximized: boolean;
}

/** Result shape returned by `window.getDevServerConfig`. */
export interface WindowDevServerConfig extends BaseResponse {
    /** True when the dev-server route is active. */
    useDevServer: boolean;
    /** Dev-server URL (e.g. `http://localhost:5173`). */
    devServerUrl: string;
}

// ============================================================================
// Configuration
// ============================================================================

/** A foobar2000 audio output device entry from `config.getOutputDevices`. */
export interface OutputDevice {
    /** Device GUID string (alias of {@link deviceId}). */
    id: string;
    /** Human-readable device name (often `"Output Driver: Device Name"`). */
    name: string;
    /** True when this device is the currently selected output. */
    isCurrent: boolean;
    /** Output-driver GUID. */
    outputId: string;
    /** Device GUID (mirror of {@link id}). */
    deviceId: string;
}

/** Active audio output configuration as reported by `config.getOutputConfig`. */
export interface OutputConfig {
    /** Selected device GUID. */
    deviceId: string;
    /** Device display name; only present when the entry resolves. */
    deviceName?: string;
    /** Output-driver GUID. */
    outputId: string;
    /** Output-driver display name; only present when the entry resolves. */
    outputName?: string;
    /** Buffer length in seconds. */
    bufferLength: number;
    /** Effective output bit depth. */
    bitDepth: number;
    /** Whether dithering is enabled. */
    useDither: boolean;
    /** Whether fade in/out is enabled. */
    useFades: boolean;
}

/** A single DSP preset entry returned by `config.getDspPresets`. */
export interface DspPreset {
    /** Index in the global DSP-preset list; required for `setActiveDspPreset`. */
    index: number;
    name: string;
}

/**
 * State block returned by `config.getActiveDspPreset`. When no preset is
 * currently active, `index` and `name` are `null` and `isActive` is `false`.
 */
export interface ActiveDspPresetInfo {
    index: number | null;
    name: string | null;
    isActive: boolean;
}

/** A foobar2000 component descriptor returned by `config.getComponents`. */
export interface ComponentInfo {
    name: string;
    version: string;
    /** File name (lower-case canonical key). */
    filename?: string;
    /** Alias of {@link filename}; the host writes both. */
    fileName?: string;
}

/**
 * Result shape returned by `config.getAll`. Full snapshot of the
 * portable-config cache; the same `Record<string, unknown>` map is
 * exposed under both `items` and `configs` - use whichever is more
 * convenient.
 */
export interface ConfigGetAllResponse extends BaseResponse {
    /** Key-value snapshot of every config entry. */
    items: JsonObject;
    /** Alias of {@link items} — same map, identical reference. */
    configs: JsonObject;
    /** Number of keys in the cache. */
    count: number;
}

/** Snapshot returned by `config.getAll`; see {@link ConfigGetAllResponse}. */
export type ConfigSnapshot = ConfigGetAllResponse;

/**
 * Composite version information returned by `config.getVersionInfo`.
 */
export interface VersionInfo {
    /** Short foobar2000 version string. */
    version: string;
    /** Alias of {@link version}; the host writes both. */
    foobar2000: string;
    /** Full version string (`Service Pack` etc. included). */
    versionFull: string;
    /** True for the 64-bit build. */
    is64bit: boolean;
    /** True when running in portable mode. */
    isPortable: boolean;
    /** Plugin self-identification block. */
    plugin: {
        name: string;
        version: string;
    };
    /** Resolved profile directory (display path). */
    profilePath: string;
}

/**
 * Discriminator describing how an `AdvancedConfigItem` should be
 * rendered:
 *
 * - `branch`: container; carries `children`.
 * - `checkbox` / `radio`: boolean leaf; `value` is `boolean`.
 * - `integer`: integer leaf; `value` is the string form (preserves
 *   sign / overflow handling delegated to consumers).
 * - `string`: text / path leaf; `value` is the string form.
 */
export type AdvancedConfigItemType =
    | 'branch'
    | 'checkbox'
    | 'radio'
    | 'integer'
    | 'string';

/**
 * Single entry in the advanced-config tree returned by
 * `config.getAdvancedConfig`. Branch entries carry `children`; leaf
 * entries carry `value` plus optional flags (string subtype, default
 * value).
 */
export interface AdvancedConfigItem {
    /** Display name. */
    name: string;
    /** Entry GUID rendered as `{...}`. */
    guid: string;
    /** Sort priority; lower values come first. */
    sortPriority: number;
    /** Entry kind; controls which optional fields are present. */
    type: AdvancedConfigItemType;
    /** Boolean state for `checkbox` / `radio`; string form for `integer` / `string`; absent for `branch`. */
    value?: boolean | string;
    /** Default value (only string-shape entries expose it). */
    defaultValue?: boolean | string;
    /** Sub-entries; only present when `type === 'branch'`. */
    children?: AdvancedConfigItem[];
    /** True when an integer string entry is signed (string subtype only). */
    isSigned?: boolean;
    /** True when a string entry is intended to hold a file path. */
    isFilePath?: boolean;
    /** True when a string entry is intended to hold a folder path. */
    isFolderPath?: boolean;
}

/**
 * Result shape returned by `config.getAdvancedConfig`. A flat top-level
 * array; nested levels are reached through each item's `children` field.
 */
export type AdvancedConfigResponse = AdvancedConfigItem[];

/**
 * Result shape returned by `config.getAdvancedConfigValue` for a single
 * entry. The shape mirrors {@link AdvancedConfigItem} except branches
 * always return `value: null` (and never have children at this level).
 */
export interface AdvancedConfigValueResponse {
    /** Display name of the entry. */
    name: string;
    /** Entry GUID rendered as `{...}` (echo of the request). */
    guid: string;
    /** Entry kind. */
    type: AdvancedConfigItemType;
    /** Current value; `null` for branches. */
    value: boolean | string | null;
    /** Default value; only present on string-shape entries. */
    defaultValue?: boolean | string;
    /** True when an integer string entry is signed. */
    isSigned?: boolean;
    /** True when a string entry is intended to hold a file path. */
    isFilePath?: boolean;
    /** True when a string entry is intended to hold a folder path. */
    isFolderPath?: boolean;
}

/**
 * Preferences page descriptor returned by `config.getPreferencesPages`.
 * Both real preferences pages and branch-only nodes are flattened into
 * a single array; branch-only entries carry `isBranch === true`.
 */
export interface PreferencesPage {
    /** Display name of the page. */
    name: string;
    /** Page GUID rendered as `{...}`. */
    guid: string;
    /** Parent GUID; the root page uses `00000000-0000-0000-0000-000000000000`. */
    parentGuid: string;
    /** Sort priority; defaults to `0.0` when not exposed by the host. */
    sortPriority: number;
    /** True when the entry is a branch (folder) rather than a real page. */
    isBranch?: boolean;
}

/** Result shape returned by `config.getPreferencesPages`. */
export type PreferencesPagesResponse = PreferencesPage[];

/**
 * Standard preferences-page GUID lookup table returned by
 * `config.getPreferencesStandardGuids`.
 */
export interface PreferencesStandardGuids {
    root: string;
    hidden: string;
    tools: string;
    core: string;
    display: string;
    playback: string;
    visualisations: string;
    input: string;
    tagWriting: string;
    mediaLibrary: string;
    tagging: string;
    output: string;
    advanced: string;
    components: string;
    dsp: string;
    shell: string;
    keyboardShortcuts: string;
}

/** Per-section file pattern descriptor returned by `library_manager_v3`. */
export interface LibraryFilePattern {
    /** Target directory (foobar2000-style path with `$expand` macros). */
    directory: string;
    /** Title-format pattern applied to each item. */
    format: string;
}

/**
 * Result shape returned by `config.getLibraryFilePatterns`. Both
 * sections are optional because newer foobar2000 builds may omit
 * either if the corresponding feature is disabled.
 */
export interface LibraryFilePatternsResponse {
    /** New-track pattern. */
    tracks?: LibraryFilePattern;
    /** Image-import pattern. */
    images?: LibraryFilePattern;
}

/**
 * Result shape returned by `config.export`. Wraps the same map as
 * {@link ConfigGetAllResponse} but additionally exposes a serialised
 * `json` blob for tests / persistence.
 */
export interface ConfigExportResponse extends BaseResponse {
    /** Snapshot of every config key (same map as {@link ConfigGetAllResponse.items}). */
    data: ConfigGetAllResponse['items'];
    /** `JSON.stringify`-style serialisation of {@link data}. */
    json: string;
    /** Number of keys in the snapshot. */
    count: number;
}

// ============================================================================
// Artwork
// ============================================================================

/**
 * Unified artwork response shape returned by `artwork.*` APIs.
 *
 * Read `available` first; when true, consume the renderable URL fields
 * (`dataUrl`, `url`).
 */
export interface ArtworkResponse {
    /** True when album art is available. */
    available: boolean;

    /** Resolved artwork type. */
    type?: AlbumArtType;

    /** Source track path, useful for diagnostics. */
    path?: string;

    /** Provider label that supplied the artwork. */
    source?: string;

    /** MIME type of the image bytes. */
    mimeType?: string;

    /** Image size in bytes. */
    size?: number;

    /** Image width in pixels, when available. */
    width?: number;
    /** Image height in pixels, when available. */
    height?: number;

    /** True when the host resized the image before returning. */
    resized?: boolean;

    /** Base64 data URL suitable for direct rendering. */
    dataUrl?: string;

    /** `fb2k://` protocol URL referencing the artwork. */
    url?: string;

    /** Reason artwork is unavailable (e.g. `no_track`, `not_found`, `invalid_path`). */
    reason?: string;

    /** Error message when the request failed. */
    error?: string;
}

/**
 * Item shape accepted by `artwork.getFb2kUrlByPathBatch` when the
 * caller wants to override per-track `type` / `maxSize`. The `items`
 * array accepts either bare path strings or `{ path, type?, maxSize? }`
 * objects.
 */
export interface ArtworkBatchItem {
    path: string;
    type?: AlbumArtType;
    maxSize?: number;
}

/**
 * Per-track entry inside `ArtworkBatchResponse.artworks`. Available
 * tracks carry `dataUrl`; failures only carry `available: false` plus
 * an `error` reason.
 */
export interface ArtworkBatchEntry {
    path?: string;
    available: boolean;
    type?: AlbumArtType;
    /** `fb2k://artwork/?path=...` URL ready for `<img src>`. */
    dataUrl?: string;
    error?: string;
}

/** Result shape returned by `artwork.getFb2kUrlByPathBatch`. */
export interface ArtworkBatchResponse extends BaseResponse {
    artworks: ArtworkBatchEntry[];
}

/** `artwork.getLyrics` result. */
export interface ArtworkLyricsResult {
    available: boolean;
    /** Tag name that supplied the lyrics (e.g. `LYRICS`, `UNSYNCED LYRICS`). */
    tag?: string;
    lyrics?: string;
    synced?: boolean;
    error?: string;
}

/** Single descriptor in `ArtworkAvailableArtworkResponse.artworks`. */
export interface ArtworkAvailableArtworkEntry {
    /** Artwork variant the entry describes. */
    type: AlbumArtType;
    /** Where the artwork came from; currently always `'embedded'` for entries. */
    source: 'embedded';
}

/**
 * Result shape returned by `artwork.getAvailableArtwork`.
 *
 * `artworks` enumerates the embedded artwork variants the file actually
 * carries (front / back / disc / icon / artist). `sources` is the
 * union of providers found: `'embedded'` is added once when any
 * embedded entry exists; folder-level sidecar files are added as
 * `'folder:<filename>'` strings.
 */
export interface ArtworkAvailableArtworkResponse extends BaseResponse {
    /** True when at least one embedded artwork variant was found. */
    available: boolean;
    /** Embedded artwork descriptors (one per variant the file carries). */
    artworks: ArtworkAvailableArtworkEntry[];
    /** Provider tags found, e.g. `['embedded', 'folder:cover.jpg']`. */
    sources: string[];
}

/**
 * Result shape returned by `artwork.getMetadata`. Failure (no track,
 * handle creation failed, exception) returns `{ available: false, error }`.
 */
export interface ArtworkMetadataResponse {
    /** True when metadata was successfully read. */
    available: boolean;
    /** Only present on failure. */
    error?: string;
    /** Album-level fields read from the file's tags. */
    album?: string;
    artist?: string;
    albumArtist?: string;
    title?: string;
    /** Year in raw tag form (typically `YYYY`). */
    year?: string;
    genre?: string;
    /** Raw track number tag value (string because tags allow `1/12`). */
    trackNumber?: string;
    /** Raw disc number tag value. */
    discNumber?: string;
    /** True when the file carries any embedded artwork. */
    hasEmbedded?: boolean;
    /** True when any LYRICS/SYNCED LYRICS-style tag is present. */
    hasLyrics?: boolean;
}

/**
 * Result shape returned by `lyrics.get`. The host always returns
 * `{ success, available, path, ... }`; lyrics-specific fields are only
 * present when `available === true`.
 */
export interface LyricsGetResponse extends BaseResponse {
    /** True when lyrics were successfully located. */
    available: boolean;
    /** Track path the lyrics belong to (echo of the request, or active-track path). */
    path?: string;
    /** Provider that supplied the lyrics. */
    source?: 'embedded' | 'file';
    /** Absolute path of the .lrc / .txt file when `source === 'file'`. */
    sourcePath?: string;
    /** Tag name when type-filtered embedded lookup hit a specific tag (e.g. `SYNCEDLYRICS`). */
    tagName?: string;
    /** Raw lyrics text. */
    lyrics?: string;
    /** True when the lyrics carry LRC-style timecodes. */
    synced?: boolean;
}

// ============================================================================
// System / API discovery
// ============================================================================

/** Single API descriptor returned by `system.listApis` / `system.searchApis`. */
export interface ApiInfo {
    fullName: string;
    plugin: string;
    namespace: string;
    method: string;
    description: string;
    version: string;
    isExternal: boolean;
}

/** Plugin-level descriptor returned by `system.getRegisteredPlugins`. */
export interface PluginInfo {
    name: string;
    namespace: string;
    version: string;
    author: string;
    description: string;
    apiCount: number;
    apis: string[];
}

/** Aggregate counters returned by `system.getApiStats`. */
export interface ApiStats {
    totalApis: number;
    internalApis: number;
    externalApis: number;
    pluginCount: number;
    byNamespace: Record<string, number>;
}

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
    | 'NOT_SUPPORTED'     // Operation not supported in current panel mode.
    | 'LIBRARY_DISABLED'  // Media library not enabled.
    | 'NO_ACTIVE_ITEM'    // No active playlist or track.
    // Operation failures
    | 'OPERATION_FAILED'  // Generic operation failure.
    // Path / media specific
    | 'PERMISSION_DENIED' // Path-validation refused (PathSecuritySpec decorator).
    | 'MISSING_PATH'
    | 'INVALID_PATH'
    | 'INVALID_HANDLE'
    | 'NO_INFO'
    | 'DECODER_FAILED'
    | 'DECODE_FAILED'
    | 'UNKNOWN_ERROR'
    | 'EXCEPTION'
    // Allow third-party extensions
    | (string & {});

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

/** Result shape returned by `playlist.clear`. */
export interface PlaylistClearResponse extends BaseResponse {
    playlist: number;
    clearedCount: number;
    remainingCount: number;
}

/** Result shape returned by `playlist.addPaths` / `playlist.addPath`. */
export interface PlaylistAddPathsResponse extends BaseResponse {
    playlist: number;
    requestedPaths: number;
    addedCount: number;
    countBefore: number;
    totalCount: number;
    /** Number of paths that failed to resolve into a metadb handle. */
    invalidCount?: number;
}

/**
 * Result shape returned by `playlist.addPathsAsync`. The handler
 * returns immediately with an `operationId` and signals completion via
 * the `playlist:addComplete` event — the eventual added count arrives
 * on the event payload, not here.
 */
export interface PlaylistAddPathsAsyncResponse extends BaseResponse {
    /** Correlation ID matching the `playlist:addComplete` event. */
    operationId?: string;
    /** Always `'pending'` on success. */
    status?: 'pending';
    /** Number of valid paths that started processing. */
    totalCount?: number;
    /** Number of paths rejected upfront before dispatch. */
    invalidCount?: number;
}

/**
 * Result shape returned by `playlist.addPathsSequential`. Unlike
 * `playlist.addPaths` this variant omits `requestedPaths` /
 * `countBefore` / `totalCount` and instead exposes the resolved `order`
 * mapping (one entry per requested path).
 */
export interface PlaylistAddPathsSequentialResponse extends BaseResponse {
    playlist: number;
    addedCount: number;
    /** Insertion index per resolved path (length matches the request). */
    order: number[];
}

/**
 * Result shape returned by `playlist.addHandles` and
 * `playlist.insertTracks`. Same envelope as {@link
 * PlaylistAddPathsResponse} but counts come from `handles[]` rather
 * than `paths[]`, plus `insertIndex` for the `insertTracks` variant.
 */
export interface PlaylistAddHandlesResponse extends BaseResponse {
    playlist: number;
    requestedCount: number;
    addedCount: number;
    invalidCount?: number;
    countBefore: number;
    totalCount: number;
    /** Only present on `insertTracks`. */
    insertIndex?: number;
}

/** Result shape returned by `playlist.reorder`. */
export interface PlaylistReorderResponse extends BaseResponse {
    playlist?: number;
    itemCount?: number;
    /** Length-mismatch error context. */
    expected?: number;
    got?: number;
    /** Out-of-range error context. */
    index?: number;
}

/** Result shape returned by `playlist.reorderPlaylists`. */
export interface PlaylistReorderPlaylistsResponse extends BaseResponse {
    /** Total number of playlists reordered (echo of the count argument). */
    count?: number;
    /** Length-mismatch error context. */
    expected?: number;
    got?: number;
    index?: number;
}

/** Result shape returned by `playlist.removeAutoplaylist`. */
export interface PlaylistRemoveAutoplaylistResponse extends BaseResponse {
    playlist?: number;
    /** Where the autoplaylist marker came from. */
    source?: 'sdk' | 'dui';
    /** Free-form remediation hint when `source === 'dui'`. */
    note?: string;
}

/** Result shape returned by atomic `playlist.replaceAllAndPlay`. */
export interface PlaylistReplaceAllAndPlayResponse extends BaseResponse {
    playlist: number;
    clearedCount: number;
    addedCount: number;
    totalCount: number;
    playIndex: number;
    /** Number of paths rejected during the bulk add. */
    invalidCount?: number;
}

/** Result shape returned by `playlist.create`. */
export interface PlaylistCreateResponse {
    index: number;
}

/** Result shape returned by `playlist.duplicate`. */
export interface PlaylistDuplicateResponse {
    index: number;
}

/** Result shape returned by `playlist.getTrackCount`. */
export interface TrackCountResponse {
    count: number;
}

/**
 * Raw page envelope returned by `playlist.getTracks`.
 *
 * The SDK wrapper `fb.playlist.getTracks(...)` unwraps `tracks` and
 * resolves with `PlaylistTrack[]` for ergonomic iteration; this
 * interface exists for callers that hit the bridge directly via
 * `bridge.invoke('playlist.getTracks', …)` and need access to the
 * pagination metadata.
 */
export interface PlaylistTracksResponse {
    playlist: number;
    start: number;
    count: number;
    total: number;
    tracks: PlaylistTrack[];
}

/** Result shape returned by `playlist.getSelectedTracks`. */
export interface PlaylistSelectedTracksResponse extends BaseResponse {
    playlist: number;
    count: number;
    tracks: PlaylistTrack[];
}

/**
 * Result shape returned by `playlist.getAutoplaylistInfo`.
 *
 * Either a regular `{ isAutoplaylist, playlist, ... }` envelope (success
 * path, no `success` field) or `{ success: false, error }` (invalid
 * index). Check `success !== false` first, then read `isAutoplaylist`.
 */
export interface PlaylistAutoplaylistInfoResponse {
    /** True when the playlist is recognised as autoplaylist (DUI or SDK). */
    isAutoplaylist?: boolean;
    /** Playlist index echoed from the request (or active playlist when omitted). */
    playlist?: number;
    /** Whether the autoplaylist keeps results sorted across rebuilds. */
    keepSorted?: boolean;
    /** Origin of the autoplaylist marker. */
    source?: 'dui' | 'sdk';
    /** Internal lock-name marker, only when present. */
    lockName?: string;
    /** Only present on failure. */
    success?: false;
    error?: string;
}

/**
 * Result shape returned by `playlist.getLockInfo`.
 *
 * The success branch returns `{ playlist, isLocked }` (no `success` field);
 * the failure branch returns `{ success: false, error }`.
 */
export interface PlaylistLockInfoResponse {
    /** Playlist index echoed from the request. */
    playlist?: number;
    /** True when the playlist has any active lock owner. */
    isLocked?: boolean;
    /** Only present on failure. */
    success?: false;
    error?: string;
}

/**
 * Single DUI-column definition entry returned by
 * `playlist.getAvailableColumns`.
 */
export interface PlaylistColumnDefinition {
    /** Column GUID rendered as `{...}`. */
    id: string;
    /** Display name of the column. */
    name: string;
    /** Default title-format pattern for cell text. */
    pattern: string;
    /** Cell-content alignment. */
    alignment: 'left' | 'right' | 'center';
    /** True when the column should be rendered as numeric (right-aligned by default). */
    numeric: boolean;
    /** Override sort-time title-format pattern; absent when no specific sort script is set. */
    sortPattern?: string;
}

/**
 * Result shape returned by `playlist.getAvailableColumns`. The handler
 * returns a flat array of {@link PlaylistColumnDefinition}.
 */
export type PlaylistAvailableColumnsResponse = PlaylistColumnDefinition[];

// ============================================================================
// Queue responses
// ============================================================================

/** Single queue entry returned by `queue.get`. */
export interface QueueItem extends TrackInfo {
    /** Source playlist index for the queued item. */
    playlist?: number;
    /** Source playlist track index. */
    playlistItem?: number;
}

/** Result shape returned by `queue.get`. */
export interface QueueGetResponse {
    items: QueueItem[];
    count: number;
}

// ============================================================================
// Library responses
// ============================================================================

/**
 * Paged-tracks response shared by several `library.*` endpoints.
 *
 * `offset` / `limit` are the *requested* page parameters and are not
 * always echoed back — some handlers (`library.getAlbumTracks`,
 * `library.getArtistTracks`) only return tracks + total without the
 * pagination metadata. Mark them optional so callers can rely on the
 * core `tracks` / `total` triple.
 */
export interface LibraryPagedTracksResponse {
    tracks: TrackInfo[];
    /** Compatibility alias usually identical to `tracks`. */
    items: TrackInfo[];
    total: number;
    offset?: number;
    limit?: number;
    fromCache?: boolean;
}

/** Result shape returned by `library.search`. */
export interface LibrarySearchResponse extends LibraryPagedTracksResponse {
    hasMore: boolean;
    success?: boolean;
    error?: string;
}

/** Result shape returned by `library.getAlbums`. */
export interface LibraryAlbumsResponse {
    albums: AlbumInfo[];
    total: number;
    offset: number;
    limit: number;
    hasMore: boolean;
    includeCover?: boolean;
    fromCache?: boolean;
}

/** Result shape returned by `library.getArtists`. */
export interface LibraryArtistsResponse {
    success: boolean;
    items: ArtistInfo[];
    count: number;
    error?: string;
}

/** Result shape returned by `library.getGenres`. */
export interface LibraryGenresResponse {
    success: boolean;
    genres: Array<{ name: string; trackCount: number }>;
    error?: string;
}

/** Result shape returned by `library.getAlbumTracks`. */
export interface LibraryAlbumTracksResponse {
    items: TrackInfo[];
    /** Compatibility alias usually identical to `items`. */
    tracks: TrackInfo[];
    total: number;
    album: string;
    artist?: string;
}

/** Result shape returned by `library.getArtistTracks`. */
export interface LibraryArtistTracksResponse {
    items: TrackInfo[];
    /** Compatibility alias usually identical to `items`. */
    tracks: TrackInfo[];
    total: number;
    count: number;
    artist: string;
}

/** Result shape returned by `library.getArtistAlbums`. */
export interface LibraryArtistAlbumsResponse {
    success: boolean;
    albums: AlbumInfo[];
    error?: string;
}

/** Result shape returned by `library.getByPath`. */
export interface LibraryGetByPathResponse {
    found: boolean;
    path: string;
    absolutePath?: string;
    title?: string;
    artist?: string;
    album?: string;
    duration?: number;
    trackNumber?: string;
    genre?: string;
    date?: string;
    success?: boolean;
    error?: string;
}

/** Result shape returned by `library.getFieldValues`. */
export interface LibraryFieldValuesResponse {
    success: boolean;
    values: Array<{ name: string; trackCount: number }>;
    total: number;
    field: string;
    error?: string;
}

/** Single library-root descriptor. */
export interface LibraryRootInfo {
    /** Stable root identifier (currently the canonical `absolutePath`). */
    id: string;
    /** Display name (directory name; falls back to full path on collisions). */
    displayName: string;
    /** Currently identical to `absolutePath`. */
    rawPath: string;
    /** Canonical absolute path on the local filesystem. */
    absolutePath: string;
    /** Number of tracks under this root. */
    trackCount: number;
}

/** Result shape returned by `library.getRoots`. */
export interface LibraryGetRootsResponse {
    success: boolean;
    /** True when the media library is enabled. */
    enabled: boolean;
    /** Real library-root descriptors. */
    roots: LibraryRootInfo[];
    /** Number of root directories. */
    total: number;
    /** Number of items successfully indexed. */
    indexedTracks: number;
    /** Number of items skipped during indexing. */
    skippedTracks: number;
    /** True when the response was served from cache. */
    fromCache: boolean;
    /** Error message; only present when `success` is `false`. */
    error?: string;
}

/** Directory-listing response for `library.browseDirectory`. */
export interface LibraryBrowseDirectoryResponse {
    success: boolean;
    directories: string[];
    files: TrackInfo[];
    /** Compatibility alias usually identical to `directories`. */
    items: string[];
    error?: string;
}

/** Typed directory-tree node descriptor. */
export interface LibraryDirectoryNodeInfo {
    /** Unique identifier in the form `rootId::pathId`. */
    id: string;
    /** Identifier of the owning library root. */
    rootId: string;
    /** Path within the root, slash-separated; the root itself uses `""`. */
    pathId: string;
    /** Parent directory's `pathId`; root nodes carry `""`. */
    parentPathId: string;
    /** Directory name (last path segment). */
    name: string;
    /** Human-readable display name. */
    displayName: string;
    /** Raw path; currently identical to `absolutePath`. */
    rawPath: string;
    /** Canonical absolute path on the local filesystem. */
    absolutePath: string;
    /** Path relative to the root (mirror of `pathId`). */
    relativePath: string;
    /** Depth derived from splitting `pathId` on `/`. */
    depth: number;
    /** Aggregate track count, including subdirectories. */
    trackCount: number;
    /** Number of immediate child directories. */
    childDirectoryCount: number;
    /** True when the node has at least one child directory. */
    hasChildren: boolean;
}

/** Result shape returned by `library.browseTree`. */
export interface LibraryBrowseTreeResponse {
    success: boolean;
    /** Owning library-root descriptor. */
    root: LibraryRootInfo;
    /** Requested `pathId`. */
    pathId: string;
    /** Absolute path of the current directory. */
    absolutePath: string;
    /** Immediate child directories. */
    directories: LibraryDirectoryNodeInfo[];
    /** Files inside the current directory; empty when `includeFiles=false`. */
    files: TrackInfo[];
    /** True when the response was served from cache. */
    fromCache: boolean;
    /** Error message; only present when `success` is `false`. */
    error?: string;
}

/** Result shape returned by `library.getRecentlyAdded`. */
export interface LibraryRecentlyAddedResponse {
    success: boolean;
    tracks: TrackInfo[];
    total: number;
    limit: number;
    sortBy: string;
    fallback: boolean;
    error?: string;
}

/** Result shape returned by `library.query` and friends. */
export interface LibraryQueryResponse {
    success: boolean;
    tracks: TrackInfo[];
    total: number;
    error?: string;
}

/** Library cache statistics; the shape grows over time, hence the index signature. */
export type LibraryCacheStatsResponse = BaseResponse & JsonObject;

/** Result of `library.addToPlaylist`. */
export interface LibraryAddToPlaylistResponse extends BaseResponse {
    /** Number of tracks actually appended (after path resolution). */
    added?: number;
}

/** Result of `library.invalidateCache`. */
export interface LibraryInvalidateCacheResponse extends BaseResponse {
    /** Epoch-ms timestamp when the cache was invalidated. */
    timestamp?: number;
}

/**
 * Result of `library.getRandomTracks` — a flat envelope with the
 * shuffled `tracks` slice and a `count` echo. Differs from
 * {@link LibraryPagedTracksResponse} (no `items` / `total` / `offset`).
 */
export interface LibraryRandomTracksResponse extends BaseResponse {
    tracks: TrackInfo[];
    count: number;
}

/** Result of `library.getArtistTracks`. */
export interface LibraryArtistTracksFlatResponse extends BaseResponse {
    tracks: TrackInfo[];
    /** Number of tracks returned (alias of `tracks.length`). */
    count: number;
    artist: string;
}

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
export interface LibraryEnumerateTracksPage extends LibraryPagedTracksResponse {
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

/** @legacy @deprecated High-level options for `library.enumerateDirectories`. */
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

/** @legacy @deprecated Single batch yielded by `library.enumerateDirectories`. */
export interface LibraryDirectoryBatch {
    path: string;
    directories: string[];
    files: TrackInfo[];
    visited: number;
    pending: number;
    success: boolean;
    error?: string;
}

/** @legacy Final summary returned by `library.enumerateDirectories`. */
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
export interface LibraryTreeBatch extends LibraryBrowseTreeResponse {
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

/** Technical audio-info block returned by `metadata.read`. */
export interface TrackTechnicalInfo {
    duration?: number;
    bitrate?: number;
    sampleRate?: number;
    channels?: number;
    codec?: string;
}

/**
 * Envelope returned by `metadata.read`.
 *
 * `tags` preserves the raw upstream ID3 / Vorbis key casing
 * (typically UPPERCASE - `TITLE` / `ARTIST` / …) and may hold a single
 * string or an array for multi-value fields. Callers that want the
 * foobar2000 "flat camelCase" shape should hit `metadata.readByPath`
 * instead.
 */
export interface MetadataReadResponse extends BaseResponse {
    path?: string;
    tags?: Record<string, string | string[]>;
    info?: TrackTechnicalInfo;
}

/**
 * Envelope returned by `metadata.readRaw`. Structurally identical to
 * {@link MetadataReadResponse} but bypasses the metadb cache and reads
 * the file directly, signalled by `source: 'file'`.
 */
export interface MetadataReadRawResponse extends MetadataReadResponse {
    /** Always `'file'` for `metadata.readRaw`; signals the bypass-cache path. */
    source?: 'file';
}

/** Per-track entry in the `metadata.readBatch` `results` array. */
export interface MetadataReadBatchItem {
    path: string;
    success: boolean;
    tags?: Record<string, string | string[]>;
    error?: string;
}

/** Envelope returned by `metadata.readBatch`. */
export interface MetadataReadBatchResponse extends BaseResponse {
    results: MetadataReadBatchItem[];
    total?: number;
    successCount?: number;
    errorCount?: number;
}

// ============================================================================
// Misc playlist / utility responses
// ============================================================================

/** Focus-track descriptor returned by `playlist.getFocusTrack`. */
export interface FocusedTrackResponse {
    index: number;
    track: TrackInfo | null;
}

/** Result shape returned by `utils.ping`. */
export interface PingResponse {
    pong: boolean;
    timestamp: number;
}

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
export type LyricsSaveTarget = 'file' | 'embedded' | 'config' | 'all';

/** Type filter for `lyrics.get`. */
export type LyricsType = 'synced' | 'unsynced' | 'any';

/** File-format filter for `lyrics.get`. */
export type LyricsFileFormat = 'lrc' | 'txt' | 'any';

/** `lyrics.get` return value. */
export interface LyricsGetResult {
    success: boolean;
    available: boolean;
    path?: string;
    /** Lyrics source. */
    source?: 'embedded' | 'file';
    /** Full path to the external lyrics file; only present when `source === 'file'`. */
    sourcePath?: string;
    /** Matched tag name; only present when `source === 'embedded'` and a `type` filter is used. */
    tagName?: string;
    /** Lyrics text. */
    lyrics?: string;
    /** True when the payload is a synced LRC document. */
    synced?: boolean;
    error?: string;
}

/** `lyrics.save` single-target return value. */
export interface LyricsSaveResult {
    success: boolean;
    /** Saved file path, when `target === 'file'`. */
    savedTo?: string;
    error?: string;
}

/** `lyrics.save` multi-target structured return value. */
export interface LyricsSaveAllResult {
    success: boolean;
    results: {
        file?: LyricsSaveResult;
        embedded?: {
            success: boolean;
            path?: string;
            tagsApplied?: Record<string, string>;
            error?: string;
        };
        config?: LyricsSaveResult;
    };
}

/** `lyrics.save` options. */
export interface LyricsSaveOptions {
    /** Save destination(s): `file` (default), `embedded`, `config`, `all`, or an array such as `['file','config']`. */
    target?: LyricsSaveTarget | LyricsSaveTarget[];
    /** Custom filename (relative to the audio file's directory). */
    filename?: string;
    /** Embedded tag name; only used when `target` is `embedded` / `all`. Defaults to `"LYRICS"`. */
    tagName?: string;
    /** File format; only used when `target` is `file` / `all`. Defaults to `"lrc"`. */
    format?: 'lrc' | 'txt';
}

/** `lyrics.get` options. */
export interface LyricsGetOptions {
    /** Source filter; defaults to `"any"`. */
    source?: LyricsSource;
    /** Type filter (`synced` / `unsynced` / `any`); defaults to `"any"`. */
    type?: LyricsType;
    /** File-format filter (`lrc` / `txt` / `any`); only used when `source === 'file'`. Defaults to `"any"`. */
    format?: LyricsFileFormat;
}

/** `lyrics.exists` return value. */
export interface LyricsExistsResult {
    exists: boolean;
    /** Provider list, e.g. `["embedded", "file:song.lrc"]`. */
    sources: string[];
}

// ============================================================================
// Titleformat
// ============================================================================

/** `titleformat.eval` single-result shape. */
export interface TitleformatEvalResult {
    success: boolean;
    path: string;
    pattern: string;
    result: string;
    error?: string;
}

/** `titleformat.evalBatch` aggregate shape. */
export interface TitleformatBatchResult {
    success: boolean;
    pattern: string;
    total: number;
    successCount: number;
    errorCount: number;
    results: Array<{
        path: string;
        success: boolean;
        result?: string;
        error?: string;
    }>;
}

/** `titleformat.evalFields` single-row shape. */
export interface TitleformatFieldsResult {
    success: boolean;
    path: string;
    [fieldName: string]: string | boolean | undefined;
}

/** `titleformat.evalFieldsBatch` aggregate shape. */
export interface TitleformatFieldsBatchResult {
    success: boolean;
    total: number;
    successCount: number;
    errorCount: number;
    results: Array<{
        path: string;
        success: boolean;
        error?: string;
        [fieldName: string]: string | boolean | undefined;
    }>;
}

/** `titleformat.getBuiltinFields` return value. */
export interface TitleformatBuiltinFields {
    success: boolean;
    fields: Record<string, string>;
}

// ============================================================================
// Shell
// ============================================================================

/** `shell.exec` / `shell.spawn` options. */
export interface ShellExecOptions {
    /** Command-line arguments. */
    args?: string[];
    /** Working directory. */
    cwd?: string;
    /** Hide the spawned window. */
    hidden?: boolean;
}

/** `shell.exec` / `shell.spawn` return value. */
export interface ShellExecResult {
    success: boolean;
    processId?: number;
    error?: string;
}

// ============================================================================
// Audio (waveform / spectrum)
// ============================================================================

/**
 * Result shape returned by `audio.getWaveform` (short window of the
 * currently playing stream, used for compact realtime visualisations).
 */
export interface AudioGetWaveformResponse extends BaseResponse {
    /** Sample buffer (length depends on `duration`). */
    waveform?: number[];
    /** Window length in seconds (echo of the request). */
    duration?: number;
    /** True when samples are signed (PCM convention). */
    signed?: boolean;
}

/**
 * Placeholder response from `audio.generateWaveform`. The method always
 * returns `success: false` plus whatever metadata the host could read
 * from the file.
 *
 * @deprecated Use `audio.generateFullWaveform` for real waveform jobs.
 */
export interface AudioGenerateWaveformResponse extends BaseResponse {
    duration?: number;
    sampleRate?: number;
    channels?: number;
    requestedResolution?: number;
}

/** Result shape returned by `audio.getOutputInfo`. */
export interface AudioOutputInfoResponse extends BaseResponse {
    /** Linear volume in dB (foobar2000 native scale). */
    volume?: number;
    /** Volume mapped to a 0-100% scale (linear conversion). */
    volumePercent?: number;
}

/** Result shape returned by `audio.getStreamInfo`. */
export interface AudioStreamInfoResponse extends BaseResponse {
    /** True when playback is currently active. */
    playing: boolean;
    /** Sample rate in Hz; only present when `playing === true`. */
    sampleRate?: number;
    /** Channel count; only present when `playing === true`. */
    channels?: number;
    /** Bitrate in kbps; only present when `playing === true`. */
    bitrate?: number;
    /** Codec identifier; only present when `playing === true`. */
    codec?: string;
    /** Track duration in seconds; only present when `playing === true`. */
    duration?: number;
}

/** `audio.generateFullWaveform` options. */
export interface FullWaveformOptions {
    /** Waveform resolution (number of data points); defaults to 256. */
    resolution?: number;
    /** Aggregation method (`peak` or `rms`); defaults to `rms`. */
    method?: 'peak' | 'rms';
    /** Use the on-disk cache; defaults to `true`. */
    useCache?: boolean;
    /** Force regeneration even on cache hit; defaults to `false`. */
    forceRegenerate?: boolean;
}

/** `audio.generateFullWaveform` synchronous result shape. */
export interface FullWaveformResult {
    success: boolean;
    /** Status: `ready` (waveform available) or `pending` (background job dispatched). */
    status?: 'ready' | 'pending';
    /** Waveform data; only present when `status === 'ready'`. */
    waveform?: number[];
    /** Background-task identifier; only present when `status === 'pending'`. */
    taskId?: string;
    /** Cache key; only present when `status === 'pending'`. */
    cacheKey?: string;
    /** True when the response was served from cache; alias of {@link fromCache}. */
    cached?: boolean;
    /** True when the response was served from cache. */
    fromCache?: boolean;
    /** Source file path. */
    path?: string;
    /** Sample resolution. */
    resolution?: number;
    /** Aggregation method used for the waveform points. */
    method?: 'peak' | 'rms';
    /** Error message when `success === false`. */
    error?: string;
    /** Track duration in seconds. */
    duration?: number;
    /** Sample rate in Hz. */
    sampleRate?: number;
    /** Channel count. */
    channels?: number;
    /** True when waveform samples are signed (PCM convention). */
    signed?: boolean;
    /** Amplitude scale factor applied to the waveform points. */
    scale?: number;
}

/** `audio.getSpectrumDebugState` return value. */
export interface SpectrumDebugState {
    success: boolean;
    active: boolean;
    timerRunning: boolean;
    effectiveFftSize: number;
    effectiveFps: number;
    effectiveBands: number;
    skipFrames: number;
    streamReady: boolean;
    subscriptionCount: number;
    /** Total number of distinct dispatch targets resolved from subscriptions. */
    dispatchTargetCount: number;
    /** Detail of every dispatch target the host plans to deliver to. */
    dispatchTargets?: Array<JsonObject>;
    subscriptions: Array<{
        token: string;
        windowId: string;
        fftSize: number;
        fps: number;
        bands: number;
    }>;
    callerOwnsSubscription: boolean;
    // Caller diagnostics (always present in debug builds).
    callerHwnd?: number;
    callerWindowId?: string;
    /** Foreground-window snapshot at the time of the call. */
    foregroundHwnd?: number;
    foregroundIsExternal?: boolean;
    foregroundPid?: number;
    foregroundTitle?: string;
    /** Total number of in-process WebView2 subscribers. */
    instanceCount?: number;
    /** HWND owning the dispatch timer, if any. */
    timerHwnd?: number;
}

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

/** Single port descriptor returned by `port.connect` / `port.getPorts`. */
export interface PortInfo {
    portId: string;
    windowId: string;
    name: string;
}

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
 * Per-track playcount entry returned by `playcount.get` / `playcount.getBatch`.
 *
 * On per-track failure (e.g. the file cannot be opened) the entry carries
 * `success: false` plus `error`, and the optional numeric fields are absent.
 */
export interface PlaycountInfo {
    /** Original path passed in (preserves `|subsong:N` suffix when present). */
    path: string;
    /** Per-track success flag — false when the track could not be resolved. */
    success: boolean;
    /** Per-track error message when `success === false`. */
    error?: string;
    /** Total playback count from foo_playcount; 0 when never played. */
    playCount?: number;
    /** Timestamp string of the first playback (omitted when unset / "?"). */
    firstPlayed?: string;
    /** Timestamp string of the most recent playback. */
    lastPlayed?: string;
    /** Timestamp string of when the track was added to the library. */
    added?: string;
    /** Rating in the 1-5 range (omitted when unrated, never zero). */
    rating?: number;
    /** True when the track is currently in the media library. */
    inLibrary?: boolean;
}

/** Result envelope for `playcount.get` / `playcount.getBatch`. */
export interface PlaycountGetResponse extends BaseResponse {
    /** Number of per-track entries (matches `results.length`). */
    count: number;
    results: PlaycountInfo[];
}

/** Result shape returned by `playcount.getStats`. */
export interface PlaycountStats extends BaseResponse {
    totalTracks: number;
    playedTracks: number;
    unplayedTracks: number;
    ratedTracks: number;
    totalPlayCount: number;
    maxPlayCount: number;
    averagePlayCount: number;
    averageRating: number;
}

// ============================================================================
// HTTP
// ============================================================================

/** Request options shared by every `fb.http.*` method. */
export interface HttpRequestOptions {
    headers?: Record<string, string>;
    timeout?: number;
    async?: boolean;
    redirect?: 'follow' | 'error' | 'manual';
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
}

// ============================================================================
// Dialog (file / folder pickers)
// ============================================================================

/** Response shape returned by `dialog.openFile`. */
export interface DialogOpenFileResponse {
    canceled: boolean;
    filePaths: string[];
    error?: string;
}

/** Response shape returned by `dialog.saveFile`. */
export interface DialogSaveFileResponse {
    canceled: boolean;
    filePath: string;
    error?: string;
}

/** Response shape returned by `dialog.openFolder`. */
export interface DialogOpenFolderResponse {
    canceled: boolean;
    folderPath: string;
    error?: string;
}

// ============================================================================
// Menu (main / context menu tree)
// ============================================================================

/** Availability counters attached to submenu nodes when `withAvailability=true`. */
export interface MenuAvailability {
    totalCommands: number;
    availableCommands: number;
    disabledCommands: number;
    allAvailable: boolean;
}

/** Menu separator — marks a visual gap in the menu tree. */
export interface MenuSeparator {
    type: 'separator';
}

/** Menu command — invokable leaf node. */
export interface MenuCommand {
    type: 'command';
    label: string;
    displayLabel: string;
    path: string;
    displayPath: string;
    flags: number;
    commandId: number;
    available: boolean;
    guid?: string;
    subGuid?: string;
}

/** Menu submenu — container with child nodes. */
export interface MenuSubmenu {
    type: 'submenu';
    label: string;
    displayLabel: string;
    path: string;
    displayPath: string;
    flags: number;
    children: MenuItem[];
    availability?: MenuAvailability;
}

/** Recursive menu-tree union node returned by `menu.getMainMenu` / `menu.getContextMenu`. */
export type MenuItem = MenuSeparator | MenuCommand | MenuSubmenu;

/** Response shape returned by `menu.getMainMenu`. */
export interface MenuGetMainMenuResponse extends BaseResponse {
    root?: string;
    requestedRoot?: string;
    rootMatched?: boolean;
    locale?: string;
    i18n?: boolean;
    withAvailability?: boolean;
    items?: MenuItem[];
    /** Source marker when an internal fallback path was used (`"v1-hmenu"`). */
    source?: string;
    /** Fallback marker when flat command list was returned instead of a tree. */
    fallback?: string;
}

/** Response shape returned by `menu.getContextMenu`. */
export interface MenuGetContextMenuResponse extends BaseResponse {
    mode?: string;
    locale?: string;
    i18n?: boolean;
    withAvailability?: boolean;
    items?: MenuItem[];
}

/** Response shape returned by `menu.showNativePopup` — defers rendering to a timer. */
export type MenuShowNativePopupResponse = BaseResponse;

/**
 * A single item in a self-drawn (WebView-rendered) popup menu.
 *
 * This is the *input* shape accepted by `menu.show` / `menu.popup`,
 * distinct from the read-only {@link MenuItem} tree returned by
 * `menu.getMainMenu` / `menu.getContextMenu`.
 */
export interface MenuPopupItem {
    /** Stable id echoed back through `menu:select` when the row is chosen. */
    id?: string;
    /** Visible row text; omitted for separators. */
    label?: string;
    /** `'separator'` renders a divider; any other value is a normal row. */
    type?: 'normal' | 'separator';
    /** Disabled rows are greyed out and cannot be chosen. Defaults to `true`. */
    enabled?: boolean;
    /** Renders a check mark to the left of the label. */
    checked?: boolean;
    /** Nested child items; presence renders a flyout arrow. */
    submenu?: MenuPopupItem[];
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
// UI interaction (notification / custom menu)
// ============================================================================

/** Response shape returned by `ui.showCustomMenu` (via `notification.showCustomMenu`). */
export interface UiShowCustomMenuResponse extends BaseResponse {
    /** ID of the clicked menu item, or `null` when the menu was dismissed. */
    selectedId?: string | null;
}

// ============================================================================
// Panel (webview-level config)
// ============================================================================

/** Shape of the panel config object returned by `panel.getConfig`. */
export interface PanelConfigShape {
    panelName: string;
    templateName: string;
    edgeStyle: string;
    urlOverride: string;
    transparentBackground: boolean;
    grabFocus: boolean;
    enableDragDrop: boolean;
    enableDevTools: boolean;
}

/** Response shape returned by `panel.getConfig`. */
export interface PanelGetConfigResponse extends BaseResponse {
    config?: PanelConfigShape;
}

// ============================================================================
// Keyboard (hotkeys)
// ============================================================================

/** Single hotkey descriptor returned by `keyboard.getRegisteredHotkeys`. */
export interface HotkeyInfo {
    /** Numeric handle from `keyboard.registerHotkey` (0 for shortcut-only entries). */
    id: number;
    key: string;
    action: string;
    global: boolean;
}

/** Response shape returned by `keyboard.getRegisteredHotkeys`. */
export interface KeyboardGetRegisteredHotkeysResponse extends BaseResponse {
    hotkeys: HotkeyInfo[];
}

// ============================================================================
// Selection — viewing track
// ============================================================================

/** Source that provided the viewing track. */
export type ViewingTrackSource = 'now_playing' | 'selection';

/** Resolution mode requested / used by `selection.getViewingTrack`. */
export type ViewingTrackMode = 'prefer_playing' | 'prefer_selection';

/** Response shape returned by `selection.getViewingTrack`. */
export interface SelectionGetViewingTrackResponse extends BaseResponse {
    found: boolean;
    mode: ViewingTrackMode;
    source?: ViewingTrackSource;
    handle?: string;
    playlistIndex?: number;
    itemIndex?: number;
    /** Present when `includeTrackInfo=true` was requested. */
    track?: TrackInfo;
}

// ============================================================================
// Drag-and-Drop (drop-zone listing)
// ============================================================================

/** Drop-zone descriptor returned by `dnd.getDropZones`. */
export interface DropZone {
    id: string;
    selector: string;
    /** Accept-types as registered by the frontend (media paths / file extensions). */
    accept: string[];
    /** Callback event name fired on drop (e.g. `'dnd:drop'`). */
    event: string;
}

/** Response shape returned by `dnd.getDropZones`. */
export interface DndGetDropZonesResponse extends BaseResponse {
    zones: DropZone[];
    count: number;
}

// ============================================================================
// Discovery (services / menu / components / formats)
// ============================================================================

/** Main-menu command descriptor returned by `discovery.getMainMenuCommands`. */
export interface DiscoveryMainMenuCommand {
    name: string;
    description: string;
    guid: string;
    parentGuid: string;
    index: number;
}

/** Main-menu group descriptor returned by `discovery.getMainMenuGroups`. */
export interface DiscoveryMainMenuGroup {
    guid: string;
    parentGuid: string;
    /** Display name (only set when the group is a `mainmenu_group_popup`). */
    name: string;
    sortPriority: number;
}

/** Input-file-type descriptor returned by `discovery.getInputFormats`. */
export interface DiscoveryInputFormatType {
    name: string;
    /** File-mask glob (e.g. `*.flac;*.wav`). */
    mask: string;
    index: number;
}

/** Installed-component descriptor returned by `discovery.getComponents`. */
export interface DiscoveryComponentInfo {
    filename: string;
    name: string;
    version: string;
    /** Long "about" message — may be empty. */
    about: string;
}

/** UI-element descriptor returned by `discovery.getUIElements`. */
export interface DiscoveryUIElementInfo {
    guid: string;
    /** Subclass GUID (`pfc::guid_null` rendered as zero-GUID when absent). */
    subclassGuid: string;
    name: string;
    description: string;
    isUserAddable: boolean;
}

/** DSP-entry descriptor returned by `discovery.getDspEntries`. */
export interface DiscoveryDspEntryInfo {
    guid: string;
    name: string;
}

/** Output-device descriptor returned by `discovery.getOutputDevices` (GUID-only). */
export interface DiscoveryOutputDeviceEntry {
    guid: string;
}

/** Context-menu command descriptor returned by `discovery.getContextMenuCommands`. */
export type DiscoveryContextMenuCommand = DiscoveryMainMenuCommand;

/** Recursive context-menu tree node returned by `discovery.getContextMenuTree`. */
export interface DiscoveryContextMenuTreeNode {
    name: string;
    type: 'command' | 'popup' | 'separator' | 'unknown';
    /** Set when `type === 'command'` — full path with parent labels. */
    fullName?: string;
    /** Set when `type === 'popup'`. */
    childCount?: number;
    children?: DiscoveryContextMenuTreeNode[];
}

/** Preference-page descriptor returned by `discovery.getPreferencePages`. */
export interface DiscoveryPreferencePageInfo {
    guid: string;
    parentGuid: string;
    name: string;
}

/** Aggregate service counts returned by `discovery.getAllServices`. */
export interface DiscoveryServiceCounts {
    mainMenuCommands: number;
    mainMenuGroups: number;
    inputFormats: number;
    uiElements: number;
    dspEntries: number;
    outputDevices: number;
    preferencePages: number;
    components: number;
}

/** Search hit returned by `discovery.searchCommands`. */
export interface DiscoverySearchResult {
    name: string;
    description: string;
    guid: string;
    /** Source taxonomy — currently only `'mainmenu'`. */
    type: string;
}

/** Response shape returned by `discovery.getMainMenuCommands`. */
export interface DiscoveryGetMainMenuCommandsResponse extends BaseResponse {
    commands: DiscoveryMainMenuCommand[];
    count: number;
}

/** Response shape returned by `discovery.getMainMenuGroups`. */
export interface DiscoveryGetMainMenuGroupsResponse extends BaseResponse {
    groups: DiscoveryMainMenuGroup[];
    count: number;
}

/** Response shape returned by `discovery.getInputFormats`. */
export interface DiscoveryGetInputFormatsResponse extends BaseResponse {
    fileTypes: DiscoveryInputFormatType[];
    count: number;
}

/** Response shape returned by `discovery.getComponents`. */
export interface DiscoveryGetComponentsResponse extends BaseResponse {
    components: DiscoveryComponentInfo[];
    count: number;
}

/** Response shape returned by `discovery.getUIElements`. */
export interface DiscoveryGetUIElementsResponse extends BaseResponse {
    elements: DiscoveryUIElementInfo[];
    count: number;
}

/** Response shape returned by `discovery.getDspEntries`. */
export interface DiscoveryGetDspEntriesResponse extends BaseResponse {
    entries: DiscoveryDspEntryInfo[];
    count: number;
}

/** Response shape returned by `discovery.getOutputDevices`. */
export interface DiscoveryGetOutputDevicesResponse extends BaseResponse {
    devices: DiscoveryOutputDeviceEntry[];
    count: number;
}

/** Response shape returned by `discovery.getContextMenuCommands`. */
export interface DiscoveryGetContextMenuCommandsResponse extends BaseResponse {
    commands: DiscoveryContextMenuCommand[];
    count: number;
}

/** Response shape returned by `discovery.getContextMenuTree`. */
export interface DiscoveryGetContextMenuTreeResponse extends BaseResponse {
    tree?: DiscoveryContextMenuTreeNode;
    itemCount?: number;
}

/** Response shape returned by `discovery.getPreferencePages`. */
export interface DiscoveryGetPreferencePagesResponse extends BaseResponse {
    pages: DiscoveryPreferencePageInfo[];
    count: number;
}

/** Response shape returned by `discovery.getAllServices`. */
export interface DiscoveryGetAllServicesResponse extends BaseResponse {
    services: DiscoveryServiceCounts;
    /** Sum of every entry in `services`. */
    totalServices: number;
}

/** Response shape returned by `discovery.searchCommands`. */
export interface DiscoverySearchCommandsResponse extends BaseResponse {
    query?: string;
    results?: DiscoverySearchResult[];
    count?: number;
}

// ============================================================================
// ReplayGain
// ============================================================================

/**
 * Per-track ReplayGain entry returned in the `results` array of
 * `replaygain.get`. All gain / peak fields are optional because
 * foobar2000 may have only partial RG data tagged on a given track.
 */
export interface ReplayGainTrackInfo {
    path: string;
    success: boolean;
    error?: string;
    /** Formatted track gain (e.g. `"-6.50 dB"`). */
    trackGain?: string;
    /** Raw track gain in dB. */
    trackGainRaw?: number;
    /** Formatted track peak (`"%.6f"`). */
    trackPeak?: string;
    /** Raw track peak (linear). */
    trackPeakRaw?: number;
    albumGain?: string;
    albumGainRaw?: number;
    albumPeak?: string;
    albumPeakRaw?: number;
    /** True when at least one of `trackGain` / `albumGain` is present. */
    hasReplayGain?: boolean;
}

/** Response shape returned by `replaygain.getSettings`. */
export interface ReplayGainGetSettingsResponse {
    sourceMode?: string;
    processingMode?: string;
    preampWithRg?: number;
    preampWithoutRg?: number;
    active?: boolean;
    /** `false` when the underlying SDK call threw. */
    success?: boolean;
    error?: string;
}

/** Response shape returned by `replaygain.getMode`. */
export interface ReplayGainGetModeResponse {
    sourceMode?: string;
    processingMode?: string;
    success?: boolean;
    error?: string;
}

/** Response shape returned by `replaygain.getPreamp`. */
export interface ReplayGainGetPreampResponse {
    withRg?: number;
    withoutRg?: number;
    success?: boolean;
    error?: string;
}

/** Response shape returned by `replaygain.get`. */
export interface ReplayGainGetResponse extends BaseResponse {
    count?: number;
    results?: ReplayGainTrackInfo[];
}

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

/** Response shape returned by `jitQueue.getState`. */
export interface JitQueueStateInfo {
    isActive: boolean;
    state: string;
    currentTrackId: string;
    nextTrackId: string;
    bufferSize: number;
    shadowPlaylist: number;
}

// ============================================================================
// File (directory listing / metadata)
// ============================================================================

/** Response shape returned by `file.list`. */
export interface FileListResponse extends BaseResponse {
    /** Regular-file names (or full paths when `recursive=true`). */
    files?: string[];
    /** Directory names (or full paths when `recursive=true`). */
    directories?: string[];
    /** Alias of {@link files} retained for test compatibility. */
    items?: string[];
}

/** Response shape returned by `file.getInfo`. */
export interface FileGetInfoResponse extends BaseResponse {
    exists: boolean;
    isDirectory?: boolean;
    isFile?: boolean;
    size?: number;
    /** JavaScript timestamp in milliseconds (last-write time). */
    modified?: number;
    name?: string;
    extension?: string;
    parent?: string;
}

// ============================================================================
// Shell (process execution)
// ============================================================================

/** Response shape returned by `shell.exec`. */
export interface ShellExecResponse extends BaseResponse {
    /** Windows process ID; absent when `success=false`. */
    processId?: number;
}

/** Response shape returned by `shell.spawn`. */
export interface ShellSpawnResponse extends BaseResponse {
    /** Windows process ID; absent when `success=false`. */
    processId?: number;
    /** True if the child exited within `waitForExitMs`; false on timeout. */
    exited?: boolean;
    /** Exit code; populated only when `exited=true`. */
    exitCode?: number;
}

// ============================================================================
// System (API discovery / plugin registry)
// ============================================================================

/** Individual entry returned by `system.listAvailableApis` / `searchApis` / `getApisByNamespace`. */
export interface SystemApiInfo {
    fullName: string;
    plugin: string;
    namespace: string;
    method: string;
    description: string;
    version: string;
    isExternal: boolean;
}

/** Response shape returned by `system.getApiStats`. */
export interface SystemApiStatsResponse {
    totalApis: number;
    internalApis: number;
    externalApis: number;
    pluginCount: number;
    /** Map of `namespace` → API count. */
    byNamespace: Record<string, number>;
}

/** Individual entry returned by `system.getRegisteredPlugins`. */
export interface SystemPluginInfo {
    name: string;
    namespace: string;
    version: string;
    author: string;
    description: string;
    apiCount: number;
    apis: string[];
}

// ============================================================================
// Generated per-API response types
// ============================================================================

export type {
    ApiResponseMap, ArtworkGetAvailableArtworkResponse, ArtworkGetAvailableTypesResponse, ArtworkGetBatchResponse, ArtworkGetByPathResponse, ArtworkGetByPlaylistItemResponse,
    ArtworkGetCurrentResponse, ArtworkGetFb2kUrlByPathBatchResponse, ArtworkGetFb2kUrlByPathResponse, ArtworkGetFb2kUrlResponse, ArtworkGetFolderImagesResponse, ArtworkGetForTrackResponse,
    ArtworkGetLyricsResponse, ArtworkGetMetadataResponse, AudioAnalyzeBPMResponse, AudioGenerateFullWaveformResponse, AudioGetOutputInfoResponse, AudioGetSpectrumDebugStateResponse,
    AudioGetSpectrumResponse, AudioGetStreamInfoResponse, AudioIsVisualizationAvailableResponse, AudioSetChannelModeResponse, AudioSubscribeSpectrumResponse, AudioSubscribeStreamResponse,
    AudioUnsubscribeSpectrumResponse, AudioUnsubscribeStreamResponse, ClipboardReadResponse, ClipboardWriteFilesResponse, ClipboardWriteHTMLResponse, ClipboardWriteResponse,
    ConfigGetActiveDspPresetResponse, ConfigGetAdvancedConfigResponse, ConfigGetAdvancedConfigValueResponse, ConfigGetComponentsResponse, ConfigGetCursorFollowPlaybackResponse, ConfigGetDspPresetsResponse,
    ConfigGetLibraryFilePatternsResponse, ConfigGetLibraryStatusResponse, ConfigGetOutputConfigResponse, ConfigGetOutputDevicesResponse, ConfigGetPlaybackFollowCursorResponse, ConfigGetPreferencesPagesResponse,
    ConfigGetPreferencesStandardGuidsResponse, ConfigGetReplaygainModeResponse, ConfigGetResponse, ConfigGetVersionInfoResponse, ConfigRemoveResponse, ConfigResetAdvancedConfigResponse,
    ConfigSetActiveDspPresetResponse, ConfigSetAdvancedConfigValueResponse, ConfigSetCursorFollowPlaybackResponse, ConfigSetOutputBufferResponse, ConfigSetOutputDeviceResponse, ConfigSetPlaybackFollowCursorResponse,
    ConfigSetReplaygainModeResponse, ConfigSetResponse, ConfigShowLibraryPreferencesResponse, ConsoleErrorResponse, ConsoleLogResponse, ConsoleWarnResponse,
    CursorIsHiddenResponse, CursorSetHiddenResponse,
    DialogConfirmResponse, DiscoveryExecuteContextMenuByPathResponse, DiscoveryExecuteContextMenuCommandResponse, DiscoveryExecuteMainMenuCommandResponse, DndRegisterDropZoneResponse, DndStartDragResponse,
    DndUnregisterDropZoneResponse, DspAddDspResponse, DspApplyPresetResponse, DspGetAvailableResponse, DspGetChainResponse, DspGetPresetsResponse,
    DspMoveDspResponse, DspRemoveDspResponse, DspSetChainResponse, EventEmitResponse, EventEmitToResponse, FileCopyResponse,
    FileDeleteResponse, FileExistsResponse, FileMkdirResponse, FileMoveResponse, FileReadResponse, FileRenameResponse,
    FileWriteResponse, HttpAbortResponse, HttpDeleteResponse, HttpDownloadResponse, HttpGetResponse, HttpHeadResponse,
    HttpPatchResponse, HttpPostResponse, HttpPutResponse, JitQueueClearResponse, JitQueueEnqueueNextResponse, JitQueueGetStateResponse,
    JitQueueNotifyEmptyResponse, JitQueuePlayNowResponse, JitQueuePreloadBatchResponse, JitQueueSkipResponse, JitQueueStopResponse, KeyboardRegisterHotkeyResponse,
    KeyboardRegisterShortcutResponse, KeyboardUnregisterHotkeyResponse, LibraryGetAlbumTracksResponse, LibraryGetAlbumsResponse, LibraryGetAllResponse, LibraryGetArtistAlbumsResponse,
    LibraryGetArtistTracksResponse, LibraryGetArtistsResponse, LibraryGetCacheStatsResponse, LibraryGetCountResponse, LibraryGetFieldValuesResponse, LibraryGetGenresResponse,
    LibraryGetRandomTracksResponse, LibraryGetRecentlyAddedResponse, LibraryGetStatsResponse, LibraryGetStatusResponse, LibraryIsEnabledResponse, LibraryRefreshResponse,
    LibraryRescanResponse, LogClearResponse, LogReadResponse, LogWriteResponse, LyricsExistsResponse, LyricsSaveResponse,
    MenuRunContextCommandByIdResponse, MenuRunContextCommandResponse, MenuRunMainMenuCommandResponse, MetadataEmbedArtworkResponse, MetadataReadByPathResponse, MetadataRemoveEmbeddedArtResponse,
    MetadataRemoveFieldResponse, MetadataRemoveTagResponse, MetadataWriteBatchResponse, MetadataWriteResponse, MiscExitResponse, MiscGetComponentPathResponse,
    MiscGetFoobarPathResponse, MiscGetProfilePathResponse, MiscRestartResponse, MiscShowConsoleResponse, MiscShowLibrarySearchResponse, MiscShowPopupMessageResponse,
    MiscShowPreferencesResponse, OutputGetDevicesResponse, PanelSetConfigResponse, PlaybackGetCurrentTrackIndexResponse, PlaybackGetCurrentTrackResponse, PlaybackGetPlaybackOrderResponse,
    PlaybackGetPlayingPlaylistResponse, PlaybackGetPositionResponse, PlaybackGetStateResponse, PlaybackGetStopAfterCurrentResponse, PlaybackGetVolumeResponse, PlaybackMuteResponse,
    PlaybackNextResponse, PlaybackPauseResponse, PlaybackPlayOrPauseResponse, PlaybackPlayPauseResponse, PlaybackPlayResponse, PlaybackPreviousResponse,
    PlaybackRandomResponse, PlaybackSetPlaybackOrderResponse, PlaybackSetStopAfterCurrentResponse, PlaybackSetVolumeResponse, PlaybackStopResponse, PlaybackToggleStopAfterCurrentResponse,
    PlaybackVolumeDownResponse, PlaybackVolumeUpResponse, PlaycountGetBatchResponse, PlaycountGetStatsResponse, PlaycountSetResponse, PlaylistConvertToAutoplaylistResponse,
    PlaylistCreateAutoplaylistResponse, PlaylistDeselectAllResponse, PlaylistFocusTrackResponse, PlaylistGetActiveResponse, PlaylistGetAllResponse, PlaylistGetAutoplaylistInfoResponse,
    PlaylistGetAutoplaylistQueryResponse, PlaylistGetAvailableColumnsResponse, PlaylistGetCountResponse, PlaylistGetFocusTrackResponse, PlaylistGetFocusedTrackResponse, PlaylistGetLockInfoResponse,
    PlaylistGetPlayingResponse, PlaylistGetSelectedTracksResponse, PlaylistGetSelectionResponse, PlaylistGetTrackCountResponse, PlaylistGetTracksResponse, PlaylistInsertTracksResponse,
    PlaylistIsAutoplaylistResponse, PlaylistIsLockedResponse, PlaylistMoveTracksResponse, PlaylistPlayTrackResponse, PlaylistRedoResponse, PlaylistRemoveResponse,
    PlaylistRemoveSelectedTracksResponse, PlaylistRemoveTracksResponse, PlaylistRenameResponse, PlaylistReverseResponse, PlaylistSelectAllResponse, PlaylistSetActiveResponse,
    PlaylistSetFocusedTrackResponse, PlaylistSetSelectionResponse, PlaylistShuffleResponse, PlaylistSortResponse, PlaylistUndoResponse, PortConnectResponse,
    PortDisconnectResponse, PortGetPortsResponse, PortPostMessageResponse, PortPostMessageToResponse, QueueAddPathsResponse, QueueAddResponse,
    QueueClearResponse, QueueFlushResponse, QueueGetCountResponse, QueueMoveToTopResponse, QueueRemoveResponse, RatingGetResponse,
    RatingSetResponse, ReplaygainClearResponse, ReplaygainGetModeResponse, ReplaygainGetPreampResponse, ReplaygainGetResponse, ReplaygainGetSettingsResponse,
    ReplaygainScanResponse, ReplaygainSetModeResponse, ReplaygainSetPreampResponse, SelectionGetResponse, SelectionGetTypeResponse, SelectionGetViewerModeResponse,
    SelectionSetPlaylistTrackingResponse, SelectionSetResponse, ShellOpenExternalResponse, ShellOpenWithResponse, ShellShowInExplorerResponse, StateDeleteResponse,
    StateGetResponse, StateKeysResponse, StateSetResponse, SystemGetApiStatsResponse, SystemGetApisByNamespaceResponse, SystemGetDPIResponse,
    SystemGetLocaleResponse, SystemGetRegisteredPluginsResponse, SystemGetThemeResponse, SystemIsPluginRegisteredResponse, SystemListAvailableApisResponse, SystemSearchApisResponse,
    TestEchoResponse, TestPingResponse, TitleformatEvalBatchResponse, TitleformatEvalFieldsBatchResponse, TitleformatEvalFieldsResponse, TitleformatEvalResponse,
    TitleformatGetBuiltinFieldsResponse, UiHideNotificationResponse, UiShowContextMenuResponse, UiShowNotificationResponse, UiShowToastResponse, WindowBlurResponse,
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

export type {
    ReplaygainSourceMode,
    ReplaygainSourceModeName,
} from './overrides/config.js';
export { REPLAYGAIN_SOURCE_MODE } from './overrides/config.js';

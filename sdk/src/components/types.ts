/**
 * Web component CustomEvent payload shapes.
 *
 * Each interface is the `detail` of a corresponding `fb-*` CustomEvent
 * dispatched by a `fb-*` web component. Subscribe via:
 *
 * ```ts
 * el.addEventListener('fb-play', (e: CustomEvent<FbPlayDetail>) => {...});
 * ```
 *
 * Empty interfaces (e.g. {@link FbPlayDetail}) are intentional: the
 * event communicates state by *occurring*, not by carrying data. They
 * exist so the typed `addEventListener` overload can resolve the
 * correct `CustomEvent<T>` instantiation.
 */

// A. Playback control events ─────────────────────────────────────────

/** `<fb-play-button>` entered the *playing* state (or user clicked while paused). */
export interface FbPlayDetail {}

/** `<fb-play-button>` entered the *paused* state. */
export interface FbPauseDetail {}

/** `<fb-stop-button>` was activated (after host stop succeeded). */
export interface FbStopDetail {}

/** `<fb-prev-button>` was activated (after host previous succeeded). */
export interface FbPrevDetail {}

/** `<fb-next-button>` was activated (after host next succeeded). */
export interface FbNextDetail {}

/** `<fb-shuffle-button>` toggled. `order` follows the C++ playback-order index. */
export interface FbShuffleToggleDetail {
    /** `true` when the new order falls within the shuffle range (3-6). */
    active: boolean;
    /** Numeric playback-order index (0-6). */
    order: number;
}

/** `<fb-repeat-button>` cycled to a new repeat mode. */
export interface FbRepeatChangeDetail {
    /** Human-readable mode label. `'off'` corresponds to order 0. */
    mode: 'off' | 'playlist' | 'track';
    /** Numeric playback-order index (0, 1, 2). */
    order: number;
}

/** `<fb-stop-after-current>` toggled. */
export interface FbStopAfterCurrentToggleDetail {
    active: boolean;
}

// B. Progress + volume events ────────────────────────────────────────

/** `<fb-seek-bar>` committed a seek (mouse-up or keyboard activation). */
export interface FbSeekDetail {
    /** Target position in seconds. */
    position: number;
    /** Optional 0-1 progress ratio. */
    progress?: number;
}

/** `<fb-seek-bar>` is mid-drag. Fires continuously during the drag gesture. */
export interface FbSeekingDetail {
    /** Tentative position in seconds, updated while the user drags. */
    position: number;
}

/** `<fb-volume-control>` committed a volume change. */
export interface FbVolumeChangeDetail {
    /** New volume on the 0-100 linear scale. */
    volume: number;
}

/** `<fb-volume-control>` mute button toggled. */
export interface FbMuteToggleDetail {}

/** `<fb-playback-order>` selected a new order. */
export interface FbOrderChangeDetail {
    /** Numeric order index (0-6). */
    order: number;
    /** Human-readable order name. See `ORDER_NAMES` in `./constants.ts`. */
    name: string;
}

// C. Track-info events ───────────────────────────────────────────────

/** `<fb-cover-art>` finished loading the cover image. */
export interface FbCoverLoadDetail {}

/** `<fb-cover-art>` failed to load the cover image. */
export interface FbCoverErrorDetail {}

// G. Rating events ───────────────────────────────────────────────────

/** `<fb-rating>` user changed the star rating. */
export interface FbRatingChangeDetail {
    /** New star count (0 = cleared). */
    value: number;
    /** Track absolute path the rating applied to. */
    path: string;
}

// D. Playlist + queue events ────────────────────────────────────────

/** `<fb-playlist-tabs>` user clicked / arrow-keyed onto a playlist. */
export interface FbPlaylistSelectDetail {
    /** Newly active playlist index. */
    index: number;
}

/** `<fb-playlist-tabs>` right-click on a tab. */
export interface FbPlaylistContextDetail {
    index: number;
    x: number;
    y: number;
}

/** `<fb-playlist-tabs>` `+` button activated. */
export interface FbPlaylistAddDetail {}

/** `<fb-playlist-tabs>` user dragged a tab to a new position. */
export interface FbPlaylistReorderDetail {
    /** Original index. */
    fromIndex: number;
    /** Drop-target slot index (already adjusted for drag direction). */
    toIndex: number;
    /** Permutation array sent to `fb.playlist.reorderPlaylists`. */
    newOrder: number[];
}

/** `<fb-playlist-selector>` `<select>` value changed. */
export interface FbPlaylistPickDetail {
    index: number;
    name: string;
}

/** `<fb-playlist-view>` row(s) selected (single click, ctrl, shift). */
export interface FbTrackSelectDetail {
    /** The clicked row index (focus anchor). */
    index: number;
    /** Full selection set after the click (sorted ascending). */
    indices: number[];
}

/** `<fb-playlist-view>` row was double-clicked / Enter-activated. */
export interface FbTrackPlayDetail {
    index: number;
}

/** `<fb-playlist-view>` right-click on a row. */
export interface FbTrackContextDetail {
    /** Selection set at the moment of the right-click. */
    indices: number[];
    x: number;
    y: number;
}

/** `<fb-queue-view>` right-click on a queue row. */
export interface FbQueueContextDetail {
    index: number;
    x: number;
    y: number;
}

/** `<fb-queue-view>` row double-clicked (queued track removed). */
export interface FbQueueRemoveDetail {
    index: number;
}

// J. Header (column resize / reorder / sort) ────────────────────────

/** `<fb-resizable-header>` right-click on the header row. */
export interface FbHeaderContextDetail {
    x: number;
    y: number;
}

/** `<fb-resizable-header>` user dragged a column resize handle. */
export interface FbColumnResizeDetail {
    /** Column id whose width changed. */
    column: string;
    /** New pixel width. */
    width: number;
    /** Resulting `grid-template-columns` value. */
    gridTemplate: string;
}

/** `<fb-resizable-header>` user dragged a column to a new slot. */
export interface FbColumnReorderDetail {
    fromIndex: number;
    toIndex: number;
    /** Column id list in the new visual order. */
    columns: string[];
}

/** `<fb-resizable-header>` user clicked a sortable column header. */
export interface FbColumnSortDetail {
    column: string;
    direction: 'asc' | 'desc';
}

// E. Window management events ────────────────────────────────────────

/** `<fb-titlebar>` drag region was double-clicked. Theme typically toggles maximize. */
export interface FbTitlebarDblclickDetail {}

/** `<fb-window-controls>` minimize button clicked. */
export interface FbWindowMinimizeDetail {}

/** `<fb-window-controls>` maximize / restore button clicked. */
export interface FbWindowMaximizeDetail {}

/** `<fb-window-controls>` close button clicked. */
export interface FbWindowCloseDetail {}

/** `<fb-popup-panel>` host opened the popup successfully. */
export interface FbPopupOpenDetail {
    /** Popup window id assigned by `fb.ui.createPopup`. */
    windowId: string;
}

/** `<fb-popup-panel>` host received a `window:popupClosed` event for our popup. */
export interface FbPopupCloseDetail {
    windowId: string;
}

/**
 * `<fb-popup-panel>` received an inter-window message via
 * `window:message`. The detail object is forwarded verbatim from the
 * bridge's `WindowMessagePayload`, so it carries both window ids and
 * the user-supplied message body.
 *
 * NOTE: an earlier hand-written declaration documented this as
 * `{ windowId; message }`, but the runtime always emits the full
 * `{ sourceWindowId; targetWindowId; message }` shape. The type now
 * matches the runtime payload.
 */
export interface FbPopupMessageDetail {
    /** Window id of the message sender. */
    sourceWindowId: string;
    /** Window id of the recipient (this popup). */
    targetWindowId: string;
    /** Loose payload — depends on what the sender posted. */
    message: unknown;
}

// G. Audio settings events ────────────────────────────────────────────

/** `<fb-output-selector>` user picked a different output device. */
export interface FbOutputChangeDetail {
    /** Index in the dropdown (0-based). */
    index: number;
    /** Display name of the device. */
    name: string;
    /** `outputId` and `deviceId` forwarded to `fb.config.setOutputDevice`. */
    outputId: string;
    deviceId: string;
}

/** `<fb-dsp-preset-selector>` user picked a different DSP preset. */
export interface FbDspChangeDetail {
    index: number;
    name: string;
}

/** `<fb-replaygain-selector>` user picked a different ReplayGain mode. */
export interface FbReplaygainChangeDetail {
    /** One of `'none' | 'track' | 'album' | 'auto'`. */
    mode: string;
}

// F. Lyrics + visualisation events ───────────────────────────────────

/** `<fb-lyrics-panel>` lyrics fetched and rendered for the active track. */
export interface FbLyricsLoadedDetail {
    /** Number of lines parsed from the LRC / plain-text source. */
    lineCount: number;
    /** `true` when the source contained `[mm:ss]` timestamps. */
    synced: boolean;
}

/** `<fb-lyrics-panel>` highlighted line changed. */
export interface FbLyricsLineChangeDetail {
    /** Zero-based index into the parsed line array. */
    index: number;
    /** Plain-text content of the new current line. */
    text: string;
    /** Timestamp (seconds) of the new current line. */
    time: number;
}

/** `<fb-lyrics-panel>` user clicked a line — request seek to that time. */
export interface FbLyricsSeekDetail {
    /** Timestamp in seconds the host should seek to. */
    time: number;
}

/**
 * `<fb-spectrum-visualizer>` raw-mode data tick. Only emitted when the
 * `mode="raw"` attribute is set; in `bars` / `wave` mode the component
 * keeps the data internal and draws it onto the Canvas.
 */
export interface FbSpectrumDataDetail {
    /** Per-band magnitude (length matches the active `bands` setting). */
    bands: Float32Array;
}

// H. Search / properties / console events ────────────────────────────

/** `<fb-search-bar>` user committed a query (debounced or Enter). */
export interface FbSearchDetail {
    /** Trimmed query string. */
    query: string;
}

/** `<fb-search-bar>` library search resolved. */
export interface FbSearchResultDetail {
    /** Hits returned by `library.search`; loosely typed because the
     * host envelope evolves independently of this declaration. */
    tracks: unknown[];
    /** `tracks.length`. */
    count: number;
    /** Echo of the query that produced the result set. */
    query: string;
}

/** `<fb-console>` log refresh fetched a new line set. */
export interface FbConsoleUpdateDetail {
    lineCount: number;
}

// I. Library tree events ─────────────────────────────────────────────

/**
 * Selection descriptor reused by `<fb-library-tree>` /
 * `<fb-library-filesystem-tree>` events. Matches the `data-*`
 * attributes that themes read off the DOM nodes.
 */
export interface FbLibrarySelectionEntry {
    key: string;
    type: string;
    artist?: string;
}

/** `<fb-library-tree>` row clicked — selection state updated. */
export interface FbLibrarySelectDetail {
    key: string;
    /** `'group' | 'album' | 'track'` — mirrors the row's `data-type`. */
    type: string;
    /** Active view at the time of the click (`'artist' | 'album' | 'genre'`). */
    view: string;
    /** Parent artist when `type === 'album' | 'track'`. */
    artist?: string;
    /** Full selection set (anchor + range or ctrl-toggled rows). */
    selected: FbLibrarySelectionEntry[];
}

/** `<fb-library-tree>` row double-clicked — request play / queue. */
export interface FbLibraryPlayDetail {
    key: string;
    type: string;
    view: string;
}

/** `<fb-library-tree>` right-click on a row. */
export interface FbLibraryContextDetail {
    key: string;
    type: string;
    view: string;
    x: number;
    y: number;
    selected: FbLibrarySelectionEntry[];
}

/** `<fb-library-tree>` Add-to-playlist completed for `count` paths. */
export interface FbLibraryAddedDetail {
    count: number;
    type: string;
    key: string;
}

/**
 * Selection / play / context payloads reused by
 * `<fb-library-filesystem-tree>`. Carry filesystem coordinates rather
 * than artist/album metadata.
 */
export interface FbLibraryFsSelectDetail {
    /** `'root' | 'directory' | 'file'`. */
    type: string;
    rootId: string;
    pathId: string;
    absolutePath: string;
    /** Present only for `type === 'file'`. */
    file?: unknown;
}

/** `<fb-library-filesystem-tree>` row double-clicked. */
export interface FbLibraryFsPlayDetail extends FbLibraryFsSelectDetail {}

/** `<fb-library-filesystem-tree>` right-click on a row. */
export interface FbLibraryFsContextDetail extends FbLibraryFsSelectDetail {
    x: number;
    y: number;
}

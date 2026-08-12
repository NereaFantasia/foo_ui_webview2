// ─────────────────────────────────────────────────────────────
// GENERATED FILE — DO NOT EDIT
// File: sdk/src/components/generated/global.d.ts
// Emitter: scripts/gen_sdk_components_global.mjs
// Regenerate: npm run gen:components-global (in sdk/) or
//             `node scripts/gen_sdk_components_global.mjs`
// ─────────────────────────────────────────────────────────────

/* eslint-disable */

import type {
    FbConsole,
    FbCoverArt,
    FbDspPresetSelector,
    FbLibraryFilesystemTree,
    FbLibraryTree,
    FbLyricsPanel,
    FbNextButton,
    FbOutputSelector,
    FbPlayButton,
    FbPlaybackOrder,
    FbPlaylistSelector,
    FbPlaylistTabs,
    FbPlaylistView,
    FbPopupPanel,
    FbPrevButton,
    FbPropertiesPanel,
    FbQueueView,
    FbRating,
    FbRepeatButton,
    FbReplaygainSelector,
    FbResizableHeader,
    FbSearchBar,
    FbSeekBar,
    FbShuffleButton,
    FbSpectrumVisualizer,
    FbStopAfterCurrent,
    FbStopButton,
    FbTechInfo,
    FbTimeCurrent,
    FbTimeRemaining,
    FbTimeTotal,
    FbTitlebar,
    FbTrackText,
    FbVolumeControl,
    FbWaveform,
    FbWindowControls,
} from '../index.js';

import type {
    FbColumnReorderDetail,
    FbColumnResizeDetail,
    FbColumnSortDetail,
    FbConsoleUpdateDetail,
    FbCoverErrorDetail,
    FbCoverLoadDetail,
    FbDspChangeDetail,
    FbHeaderContextDetail,
    FbLibraryAddedDetail,
    FbLibraryContextDetail,
    FbLibraryFsContextDetail,
    FbLibraryFsPlayDetail,
    FbLibraryFsSelectDetail,
    FbLibraryPlayDetail,
    FbLibrarySelectDetail,
    FbLyricsLineChangeDetail,
    FbLyricsLoadedDetail,
    FbLyricsSeekDetail,
    FbMuteToggleDetail,
    FbNextDetail,
    FbOrderChangeDetail,
    FbOutputChangeDetail,
    FbPauseDetail,
    FbPlayDetail,
    FbPlaylistAddDetail,
    FbPlaylistContextDetail,
    FbPlaylistPickDetail,
    FbPlaylistReorderDetail,
    FbPlaylistSelectDetail,
    FbPopupCloseDetail,
    FbPopupMessageDetail,
    FbPopupOpenDetail,
    FbPrevDetail,
    FbQueueContextDetail,
    FbQueueRemoveDetail,
    FbRatingChangeDetail,
    FbRepeatChangeDetail,
    FbReplaygainChangeDetail,
    FbSearchDetail,
    FbSearchResultDetail,
    FbSeekDetail,
    FbSeekingDetail,
    FbShuffleToggleDetail,
    FbSpectrumDataDetail,
    FbStopAfterCurrentToggleDetail,
    FbStopDetail,
    FbTitlebarDblclickDetail,
    FbTrackContextDetail,
    FbTrackPlayDetail,
    FbTrackSelectDetail,
    FbVolumeChangeDetail,
    FbWindowCloseDetail,
    FbWindowMaximizeDetail,
    FbWindowMinimizeDetail,
} from '../types.js';

declare global {
    interface HTMLElementTagNameMap {
        'fb-console': FbConsole;
        'fb-cover-art': FbCoverArt;
        'fb-dsp-preset-selector': FbDspPresetSelector;
        'fb-library-filesystem-tree': FbLibraryFilesystemTree;
        'fb-library-tree': FbLibraryTree;
        'fb-lyrics-panel': FbLyricsPanel;
        'fb-next-button': FbNextButton;
        'fb-output-selector': FbOutputSelector;
        'fb-play-button': FbPlayButton;
        'fb-playback-order': FbPlaybackOrder;
        'fb-playlist-selector': FbPlaylistSelector;
        'fb-playlist-tabs': FbPlaylistTabs;
        'fb-playlist-view': FbPlaylistView;
        'fb-popup-panel': FbPopupPanel;
        'fb-prev-button': FbPrevButton;
        'fb-properties-panel': FbPropertiesPanel;
        'fb-queue-view': FbQueueView;
        'fb-rating': FbRating;
        'fb-repeat-button': FbRepeatButton;
        'fb-replaygain-selector': FbReplaygainSelector;
        'fb-resizable-header': FbResizableHeader;
        'fb-search-bar': FbSearchBar;
        'fb-seek-bar': FbSeekBar;
        'fb-shuffle-button': FbShuffleButton;
        'fb-spectrum-visualizer': FbSpectrumVisualizer;
        'fb-stop-after-current': FbStopAfterCurrent;
        'fb-stop-button': FbStopButton;
        'fb-tech-info': FbTechInfo;
        'fb-time-current': FbTimeCurrent;
        'fb-time-remaining': FbTimeRemaining;
        'fb-time-total': FbTimeTotal;
        'fb-titlebar': FbTitlebar;
        'fb-track-text': FbTrackText;
        'fb-volume-control': FbVolumeControl;
        'fb-waveform': FbWaveform;
        'fb-window-controls': FbWindowControls;
    }

    interface HTMLElementEventMap {
        'fb-column-reorder': CustomEvent<FbColumnReorderDetail>;
        'fb-column-resize': CustomEvent<FbColumnResizeDetail>;
        'fb-column-sort': CustomEvent<FbColumnSortDetail>;
        'fb-console-update': CustomEvent<FbConsoleUpdateDetail>;
        'fb-cover-error': CustomEvent<FbCoverErrorDetail>;
        'fb-cover-load': CustomEvent<FbCoverLoadDetail>;
        'fb-dsp-change': CustomEvent<FbDspChangeDetail>;
        'fb-header-context': CustomEvent<FbHeaderContextDetail>;
        'fb-library-added': CustomEvent<FbLibraryAddedDetail>;
        'fb-library-context': CustomEvent<FbLibraryContextDetail>;
        'fb-library-fs-context': CustomEvent<FbLibraryFsContextDetail>;
        'fb-library-fs-play': CustomEvent<FbLibraryFsPlayDetail>;
        'fb-library-fs-select': CustomEvent<FbLibraryFsSelectDetail>;
        'fb-library-play': CustomEvent<FbLibraryPlayDetail>;
        'fb-library-select': CustomEvent<FbLibrarySelectDetail>;
        'fb-lyrics-line-change': CustomEvent<FbLyricsLineChangeDetail>;
        'fb-lyrics-loaded': CustomEvent<FbLyricsLoadedDetail>;
        'fb-lyrics-seek': CustomEvent<FbLyricsSeekDetail>;
        'fb-mute-toggle': CustomEvent<FbMuteToggleDetail>;
        'fb-next': CustomEvent<FbNextDetail>;
        'fb-order-change': CustomEvent<FbOrderChangeDetail>;
        'fb-output-change': CustomEvent<FbOutputChangeDetail>;
        'fb-pause': CustomEvent<FbPauseDetail>;
        'fb-play': CustomEvent<FbPlayDetail>;
        'fb-playlist-add': CustomEvent<FbPlaylistAddDetail>;
        'fb-playlist-context': CustomEvent<FbPlaylistContextDetail>;
        'fb-playlist-pick': CustomEvent<FbPlaylistPickDetail>;
        'fb-playlist-reorder': CustomEvent<FbPlaylistReorderDetail>;
        'fb-playlist-select': CustomEvent<FbPlaylistSelectDetail>;
        'fb-popup-close': CustomEvent<FbPopupCloseDetail>;
        'fb-popup-message': CustomEvent<FbPopupMessageDetail>;
        'fb-popup-open': CustomEvent<FbPopupOpenDetail>;
        'fb-prev': CustomEvent<FbPrevDetail>;
        'fb-queue-context': CustomEvent<FbQueueContextDetail>;
        'fb-queue-remove': CustomEvent<FbQueueRemoveDetail>;
        'fb-rating-change': CustomEvent<FbRatingChangeDetail>;
        'fb-repeat-change': CustomEvent<FbRepeatChangeDetail>;
        'fb-replaygain-change': CustomEvent<FbReplaygainChangeDetail>;
        'fb-search': CustomEvent<FbSearchDetail>;
        'fb-search-result': CustomEvent<FbSearchResultDetail>;
        'fb-seek': CustomEvent<FbSeekDetail>;
        'fb-seeking': CustomEvent<FbSeekingDetail>;
        'fb-shuffle-toggle': CustomEvent<FbShuffleToggleDetail>;
        'fb-spectrum-data': CustomEvent<FbSpectrumDataDetail>;
        'fb-stop': CustomEvent<FbStopDetail>;
        'fb-stop-after-current-toggle': CustomEvent<FbStopAfterCurrentToggleDetail>;
        'fb-titlebar-dblclick': CustomEvent<FbTitlebarDblclickDetail>;
        'fb-track-context': CustomEvent<FbTrackContextDetail>;
        'fb-track-play': CustomEvent<FbTrackPlayDetail>;
        'fb-track-select': CustomEvent<FbTrackSelectDetail>;
        'fb-volume-change': CustomEvent<FbVolumeChangeDetail>;
        'fb-window-close': CustomEvent<FbWindowCloseDetail>;
        'fb-window-maximize': CustomEvent<FbWindowMaximizeDetail>;
        'fb-window-minimize': CustomEvent<FbWindowMinimizeDetail>;
    }
}

export {};

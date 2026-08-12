/**
 * `foo-webview-sdk/components` — ESM entry.
 *
 * Re-exports every web-component class and the typed event-detail
 * interfaces. Importers must call {@link registerComponents} explicitly
 * (no automatic side effects) so consumers retain control over
 * registration timing — useful for micro-frontend hosts and unit-test
 * harnesses.
 *
 * The IIFE bundle (`dist/components.global.js`) is the auto-register
 * variant; see `./iife.ts`.
 */

// Activate the generated `declare global { ... }` augmentation for
// `HTMLElementTagNameMap` and `HTMLElementEventMap`. The import is
// type-only with an empty binding, so it carries no runtime cost yet
// still pulls `./generated/global.d.ts` into the consumer's type
// system. Regenerate via `npm run gen:components-global`.
import type {} from './generated/global.js';

export { FbBaseElement } from './FbBaseElement.js';
export { FbConsole } from './FbConsole.js';
export { FbCoverArt } from './FbCoverArt.js';
export { FbDspPresetSelector } from './FbDspPresetSelector.js';
export { FbLibraryFilesystemTree } from './FbLibraryFilesystemTree.js';
export { FbLibraryTree } from './FbLibraryTree.js';
export { FbLyricsPanel } from './FbLyricsPanel.js';
export { FbNextButton } from './FbNextButton.js';
export { FbOutputSelector } from './FbOutputSelector.js';
export { FbPlaybackOrder } from './FbPlaybackOrder.js';
export { FbPlayButton } from './FbPlayButton.js';
export { FbPlaylistSelector } from './FbPlaylistSelector.js';
export { FbPlaylistTabs } from './FbPlaylistTabs.js';
export { FbPlaylistView } from './FbPlaylistView.js';
export { FbPopupPanel } from './FbPopupPanel.js';
export { FbPrevButton } from './FbPrevButton.js';
export { FbPropertiesPanel } from './FbPropertiesPanel.js';
export { FbQueueView } from './FbQueueView.js';
export { FbRating } from './FbRating.js';
export { FbReplaygainSelector } from './FbReplaygainSelector.js';
export { FbRepeatButton } from './FbRepeatButton.js';
export { FbResizableHeader } from './FbResizableHeader.js';
export { FbSearchBar } from './FbSearchBar.js';
export { FbSeekBar } from './FbSeekBar.js';
export { FbShuffleButton } from './FbShuffleButton.js';
export { FbSpectrumVisualizer } from './FbSpectrumVisualizer.js';
export { FbStopAfterCurrent } from './FbStopAfterCurrent.js';
export { FbStopButton } from './FbStopButton.js';
export { FbTechInfo } from './FbTechInfo.js';
export { FbTimeCurrent } from './FbTimeCurrent.js';
export { FbTimeRemaining } from './FbTimeRemaining.js';
export { FbTimeTotal } from './FbTimeTotal.js';
export { FbTitlebar } from './FbTitlebar.js';
export { FbTrackText } from './FbTrackText.js';
export { FbVolumeControl } from './FbVolumeControl.js';
export { FbWaveform } from './FbWaveform.js';
export { FbWindowControls } from './FbWindowControls.js';

export type { ColumnDescriptor } from './FbResizableHeader.js';

export { defaultComponents, registerComponents } from './register.js';
export {
    ORDER_NAMES,
    RG_MODE_NAMES,
    SHUFFLE_ORDERS,
    formatTime,
} from './constants.js';
export type { ReplayGainMode } from './constants.js';

export type {
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
    FbLibrarySelectionEntry,
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
    FbReplaygainChangeDetail,
    FbRepeatChangeDetail,
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
} from './types.js';

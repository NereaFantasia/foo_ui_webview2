/**
 * Custom-element registration helpers.
 *
 * {@link defaultComponents} is the single source of truth for the
 * tag-name ↔ class binding.
 *
 * Registration is idempotent: tags already present in
 * `customElements` are left alone, so the helper is safe to call
 * multiple times.
 */

import { FbConsole } from './FbConsole.js';
import { FbCoverArt } from './FbCoverArt.js';
import { FbDspPresetSelector } from './FbDspPresetSelector.js';
import { FbLibraryFilesystemTree } from './FbLibraryFilesystemTree.js';
import { FbLibraryTree } from './FbLibraryTree.js';
import { FbLyricsPanel } from './FbLyricsPanel.js';
import { FbNextButton } from './FbNextButton.js';
import { FbOutputSelector } from './FbOutputSelector.js';
import { FbPlaybackOrder } from './FbPlaybackOrder.js';
import { FbPlayButton } from './FbPlayButton.js';
import { FbPlaylistSelector } from './FbPlaylistSelector.js';
import { FbPlaylistTabs } from './FbPlaylistTabs.js';
import { FbPlaylistView } from './FbPlaylistView.js';
import { FbPopupPanel } from './FbPopupPanel.js';
import { FbPrevButton } from './FbPrevButton.js';
import { FbPropertiesPanel } from './FbPropertiesPanel.js';
import { FbQueueView } from './FbQueueView.js';
import { FbRating } from './FbRating.js';
import { FbReplaygainSelector } from './FbReplaygainSelector.js';
import { FbRepeatButton } from './FbRepeatButton.js';
import { FbResizableHeader } from './FbResizableHeader.js';
import { FbSearchBar } from './FbSearchBar.js';
import { FbSeekBar } from './FbSeekBar.js';
import { FbShuffleButton } from './FbShuffleButton.js';
import { FbSpectrumVisualizer } from './FbSpectrumVisualizer.js';
import { FbStopAfterCurrent } from './FbStopAfterCurrent.js';
import { FbStopButton } from './FbStopButton.js';
import { FbTechInfo } from './FbTechInfo.js';
import { FbTimeCurrent } from './FbTimeCurrent.js';
import { FbTimeRemaining } from './FbTimeRemaining.js';
import { FbTimeTotal } from './FbTimeTotal.js';
import { FbTitlebar } from './FbTitlebar.js';
import { FbTrackText } from './FbTrackText.js';
import { FbVolumeControl } from './FbVolumeControl.js';
import { FbWaveform } from './FbWaveform.js';
import { FbWindowControls } from './FbWindowControls.js';

/**
 * Tag-name ↔ class map for every component shipped by this entry.
 *
 * Phases delivered so far:
 * - 4a: 4 playback-control buttons (play / stop / prev / next).
 * - 4b: progress + volume sliders (seek-bar / volume-control).
 * - 4c: track-info display (track-text / cover-art /
 *   time-current / time-total / time-remaining / tech-info).
 * - 4d: playback enhancements + rating (shuffle-button /
 *   repeat-button / stop-after-current / playback-order / rating).
 * - 4e: playlist + queue (playlist-tabs / resizable-header /
 *   playlist-view / queue-view / playlist-selector).
 * - 4f: window management (titlebar / window-controls / popup-panel).
 * - 4g: audio settings (output-selector / dsp-preset-selector /
 *   replaygain-selector).
 * - 4h: lyrics + visualisation + tag/log + library trees
 *   (lyrics-panel / spectrum-visualizer / waveform /
 *   properties-panel / search-bar / console / library-tree /
 *   library-filesystem-tree).
 *
 * Subsequent batches append additional entries here without altering
 * the map's shape (string keys, `CustomElementConstructor` values).
 */
export const defaultComponents: Record<string, CustomElementConstructor> = {
    // A. Playback control
    'fb-play-button': FbPlayButton,
    'fb-stop-button': FbStopButton,
    'fb-prev-button': FbPrevButton,
    'fb-next-button': FbNextButton,
    'fb-shuffle-button': FbShuffleButton,
    'fb-repeat-button': FbRepeatButton,
    'fb-stop-after-current': FbStopAfterCurrent,
    // B. Progress + volume
    'fb-seek-bar': FbSeekBar,
    'fb-volume-control': FbVolumeControl,
    'fb-playback-order': FbPlaybackOrder,
    // C. Track info
    'fb-track-text': FbTrackText,
    'fb-cover-art': FbCoverArt,
    'fb-time-current': FbTimeCurrent,
    'fb-time-total': FbTimeTotal,
    'fb-time-remaining': FbTimeRemaining,
    'fb-tech-info': FbTechInfo,
    // D. Playlist + queue
    'fb-playlist-tabs': FbPlaylistTabs,
    'fb-resizable-header': FbResizableHeader,
    'fb-playlist-view': FbPlaylistView,
    'fb-queue-view': FbQueueView,
    'fb-playlist-selector': FbPlaylistSelector,
    // E. Window management
    'fb-titlebar': FbTitlebar,
    'fb-window-controls': FbWindowControls,
    'fb-popup-panel': FbPopupPanel,
    // G. Audio settings + rating
    'fb-output-selector': FbOutputSelector,
    'fb-dsp-preset-selector': FbDspPresetSelector,
    'fb-replaygain-selector': FbReplaygainSelector,
    'fb-rating': FbRating,
    // F. Lyrics + visualisation
    'fb-lyrics-panel': FbLyricsPanel,
    'fb-spectrum-visualizer': FbSpectrumVisualizer,
    'fb-waveform': FbWaveform,
    // H. Metadata + search + console
    'fb-properties-panel': FbPropertiesPanel,
    'fb-search-bar': FbSearchBar,
    'fb-console': FbConsole,
    // I. Library trees
    'fb-library-tree': FbLibraryTree,
    'fb-library-filesystem-tree': FbLibraryFilesystemTree,
};

/**
 * Idempotently register the supplied tag → class map. Tags that are
 * already defined (for example by an earlier `<script>` registering
 * the same tag) are skipped so the existing constructor wins.
 *
 * @param components map of tag names to constructors. Defaults to
 *                   {@link defaultComponents}; pass a subset for
 *                   advanced lazy-load scenarios.
 * @returns the list of tag names actually registered (post-filter).
 */
export function registerComponents(
    components: Record<string, CustomElementConstructor> = defaultComponents,
): string[] {
    if (typeof customElements === 'undefined') return [];
    const registered: string[] = [];
    for (const [tag, ctor] of Object.entries(components)) {
        if (!customElements.get(tag)) {
            customElements.define(tag, ctor);
            registered.push(tag);
        }
    }
    return registered;
}

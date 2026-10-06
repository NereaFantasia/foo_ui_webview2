# Components Overview

foo_ui_webview2 ships **36 unstyled Web Components**. They are functional building blocks with no visual opinion, designed to be customized through CSS Parts, slots, and custom events.

Load them with the SDK, through `registerComponents()` with a bundler or `components.global.js` without one: see [Load the SDK in a theme](/how-to/load-sdk). [Style the components](/how-to/style-components) shows how to give them a look, and [How the components work](/concepts/components) explains how they find the SDK, report user actions and leave styling to the theme.

Each entry below links to the tag's reference: attributes, CSS parts, slots and events.

## Component Reference

### A. Playback Controls (7)

| Tag | Description |
| --- | --- |
| [`<fb-play-button>`](./play-controls#fb-play-button) | Play/pause toggle |
| [`<fb-stop-button>`](./play-controls#fb-stop-button) | Stop playback |
| [`<fb-prev-button>`](./play-controls#fb-prev-button) | Previous track |
| [`<fb-next-button>`](./play-controls#fb-next-button) | Next track |
| [`<fb-shuffle-button>`](./play-controls#fb-shuffle-button) | Shuffle toggle |
| [`<fb-repeat-button>`](./play-controls#fb-repeat-button) | Repeat cycle (`off` → `playlist` → `track`) |
| [`<fb-stop-after-current>`](./play-controls#fb-stop-after-current) | Stop after the current track |

### B. Progress and Volume (3)

| Tag | Description |
| --- | --- |
| [`<fb-seek-bar>`](./progress#fb-seek-bar) | Playback seek bar |
| [`<fb-volume-control>`](./progress#fb-volume-control) | Volume slider with mute control |
| [`<fb-playback-order>`](./progress#fb-playback-order) | Seven-state playback-order picker in select or button mode |

### C. Track Information (6)

| Tag | Description |
| --- | --- |
| [`<fb-track-text>`](./track-info#fb-track-text) | Track text from a field or Title Formatting expression |
| [`<fb-cover-art>`](./track-info#fb-cover-art) | Cover art with an optional SDK URL-helper lookup and standard artwork fallback |
| [`<fb-time-current>`](./track-info#fb-time-current) | Current playback time |
| [`<fb-time-total>`](./track-info#fb-time-total) | Total duration |
| [`<fb-time-remaining>`](./track-info#fb-time-remaining) | Remaining time |
| [`<fb-tech-info>`](./track-info#fb-tech-info) | Codec, bitrate, sample rate, and channel information |

### D. Playlists (5)

| Tag | Description |
| --- | --- |
| [`<fb-playlist-tabs>`](./playlist#fb-playlist-tabs) | Playlist tab strip |
| [`<fb-resizable-header>`](./playlist#fb-resizable-header) | Resizable, reorderable, sortable column header |
| [`<fb-playlist-view>`](./playlist#fb-playlist-view) | Virtualized playlist view |
| [`<fb-queue-view>`](./playlist#fb-queue-view) | Playback queue view |
| [`<fb-playlist-selector>`](./playlist#fb-playlist-selector) | Playlist dropdown |

### E. Window Management (3)

| Tag | Description |
| --- | --- |
| [`<fb-titlebar>`](./window#fb-titlebar) | Custom draggable title bar |
| [`<fb-window-controls>`](./window#fb-window-controls) | Minimize, maximize/restore, and close controls |
| [`<fb-popup-panel>`](./window#fb-popup-panel) | Popup-window trigger and lifecycle wrapper |

### F. Lyrics and Visualization (3)

| Tag | Description |
| --- | --- |
| [`<fb-lyrics-panel>`](./media#fb-lyrics-panel) | Synchronized or plain-text lyrics panel |
| [`<fb-spectrum-visualizer>`](./media#fb-spectrum-visualizer) | Real-time spectrum visualizer |
| [`<fb-waveform>`](./media#fb-waveform) | Full-track waveform overview |

### G. Rating and Audio Settings (4)

| Tag | Description |
| --- | --- |
| [`<fb-rating>`](./audio#fb-rating) | Star rating control |
| [`<fb-output-selector>`](./audio#fb-output-selector) | Audio output selector |
| [`<fb-dsp-preset-selector>`](./audio#fb-dsp-preset-selector) | DSP preset selector |
| [`<fb-replaygain-selector>`](./audio#fb-replaygain-selector) | ReplayGain mode selector |

### H. Metadata and Search (3)

| Tag | Description |
| --- | --- |
| [`<fb-properties-panel>`](./metadata#fb-properties-panel) | Read-only track properties panel |
| [`<fb-search-bar>`](./metadata#fb-search-bar) | Media-library search input |
| [`<fb-console>`](./metadata#fb-console) | foobar2000 console viewer |

### I. Media Library (2)

| Tag | Description |
| --- | --- |
| [`<fb-library-tree>`](./library#fb-library-tree) | Artist, album, or genre grouping tree |
| [`<fb-library-filesystem-tree>`](./library#fb-library-filesystem-tree) | Lazily loaded filesystem-style media-library root tree |

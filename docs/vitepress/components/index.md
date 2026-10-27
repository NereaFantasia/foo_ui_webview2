# Components Overview

foo_ui_webview2 ships **36 unstyled Web Components** (Custom Elements). They are functional building blocks that encapsulate playback control logic and Bridge data flow, designed to be customized through CSS Parts, slots, read-only host attributes, and custom events.

Load them with the SDK: call `registerComponents()` when using a bundler, or import `components.global.js` directly in plain script environments (see [Load the SDK in a theme](/how-to/load-sdk)). [Style the components](/how-to/style-components) guides you through theming, and [How the components work](/concepts/components) details the runtime lifecycle and underlying communication mechanics.

## Design Conventions

The components follow unified architectural conventions that keep behavior decoupled from presentation:

- **Zero-style functional blocks**: Components contain only layout structure and flow rules, with no visual styling opinions (no `color`, `background`, `font`, `padding`, `margin`, `border-radius`, `box-shadow`, or `transition`). All visual aesthetics are defined by the host theme through CSS.
- **CSS Part and Slot specifications**: Internal elements are exposed through kebab-case `part` attributes for styling via the `::part()` pseudo-element. Replaceable visual content (such as play/pause icons) is provided through `slot` elements.
- **Read-only host attributes**: Internal runtime state is reflected outwards through read-only host attributes (e.g. `[playing]`, `[disabled]`, `[active]`, `[muted]`), enabling state-driven attribute selectors in theme CSS.
- **Context menu separation**: Components never display native context menus directly on right click. Instead, they dispatch composed bubbling `CustomEvent`s containing screen coordinates and item context (such as `fb-track-context`, `fb-playlist-context`, `fb-queue-context`, and `fb-library-context`), allowing themes to surface custom menus through `ui.showCustomMenu`.

## Component Reference

Each entry below summarizes a component, linking to its reference page with attributes, CSS parts, slots, and events:

### A. Playback Controls (7)

| Tag | Description |
| --- | --- |
| [`<fb-play-button>`](./play-controls#fb-play-button) | Play/pause toggle button |
| [`<fb-stop-button>`](./play-controls#fb-stop-button) | Stop playback button |
| [`<fb-prev-button>`](./play-controls#fb-prev-button) | Previous track button |
| [`<fb-next-button>`](./play-controls#fb-next-button) | Next track button |
| [`<fb-shuffle-button>`](./play-controls#fb-shuffle-button) | Shuffle toggle button |
| [`<fb-repeat-button>`](./play-controls#fb-repeat-button) | Repeat cycle button (`off` → `playlist` → `track`) |
| [`<fb-stop-after-current>`](./play-controls#fb-stop-after-current) | Stop after current track toggle |

### B. Progress and Volume (3)

| Tag | Description |
| --- | --- |
| [`<fb-seek-bar>`](./progress#fb-seek-bar) | Playback seek bar |
| [`<fb-volume-control>`](./progress#fb-volume-control) | Volume slider with mute button |
| [`<fb-playback-order>`](./progress#fb-playback-order) | Seven-state playback-order selector in select or button mode |

### C. Track Information (6)

| Tag | Description |
| --- | --- |
| [`<fb-track-text>`](./track-info#fb-track-text) | Track text from a metadata field or Title Formatting expression |
| [`<fb-cover-art>`](./track-info#fb-cover-art) | Cover art image with SDK URL helper lookup and standard artwork fallback |
| [`<fb-time-current>`](./track-info#fb-time-current) | Current playback time display |
| [`<fb-time-total>`](./track-info#fb-time-total) | Track total duration display |
| [`<fb-time-remaining>`](./track-info#fb-time-remaining) | Track remaining time display |
| [`<fb-tech-info>`](./track-info#fb-tech-info) | Technical audio parameters (codec, bitrate, sample rate, and channels) |

### D. Playlists (5)

| Tag | Description |
| --- | --- |
| [`<fb-playlist-tabs>`](./playlist#fb-playlist-tabs) | Playlist tab strip |
| [`<fb-resizable-header>`](./playlist#fb-resizable-header) | Resizable, reorderable, and sortable column header |
| [`<fb-playlist-view>`](./playlist#fb-playlist-view) | Virtualized playlist view |
| [`<fb-queue-view>`](./playlist#fb-queue-view) | Playback queue view |
| [`<fb-playlist-selector>`](./playlist#fb-playlist-selector) | Playlist dropdown selector |

### E. Window Management (3)

| Tag | Description |
| --- | --- |
| [`<fb-titlebar>`](./window#fb-titlebar) | Draggable custom window title bar |
| [`<fb-window-controls>`](./window#fb-window-controls) | Frameless window control buttons (minimize, maximize/restore, close) |
| [`<fb-popup-panel>`](./window#fb-popup-panel) | Popup panel trigger and lifecycle wrapper |

### F. Lyrics and Visualization (3)

| Tag | Description |
| --- | --- |
| [`<fb-lyrics-panel>`](./media#fb-lyrics-panel) | Synchronized or plain-text lyrics panel |
| [`<fb-spectrum-visualizer>`](./media#fb-spectrum-visualizer) | Real-time Canvas spectrum visualizer |
| [`<fb-waveform>`](./media#fb-waveform) | Full-track waveform overview |

### G. Rating and Audio Settings (4)

| Tag | Description |
| --- | --- |
| [`<fb-rating>`](./audio#fb-rating) | Star rating control |
| [`<fb-output-selector>`](./audio#fb-output-selector) | Audio output device selector |
| [`<fb-dsp-preset-selector>`](./audio#fb-dsp-preset-selector) | DSP preset selector |
| [`<fb-replaygain-selector>`](./audio#fb-replaygain-selector) | ReplayGain processing mode selector |

### H. Metadata and Search (3)

| Tag | Description |
| --- | --- |
| [`<fb-properties-panel>`](./metadata#fb-properties-panel) | Read-only track properties panel |
| [`<fb-search-bar>`](./metadata#fb-search-bar) | Media library search input |
| [`<fb-console>`](./metadata#fb-console) | Real-time foobar2000 console log viewer |

### I. Media Library (2)

| Tag | Description |
| --- | --- |
| [`<fb-library-tree>`](./library#fb-library-tree) | Media library grouping tree by artist, album, or genre |
| [`<fb-library-filesystem-tree>`](./library#fb-library-filesystem-tree) | Filesystem-style media library tree with on-demand lazy loading |

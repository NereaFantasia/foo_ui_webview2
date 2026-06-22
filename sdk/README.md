English | [中文](./README.zh-CN.md)

# foo_ui_webview2 SDK

Simplified SDK for the foobar2000 WebView2 plugin — collapses verbose API calls into concise, namespaced calls.

Repository: https://github.com/NereaFantasia/foo_ui_webview2

## Installation

```bash
npm install foo-webview-sdk
```

### ESM / bundlers / TypeScript

```js
import fb from 'foo-webview-sdk'
// Or import namespaces individually (tree-shakeable)
import { player, playlist, library } from 'foo-webview-sdk'
// Shared types come from the package root as well
import type { TrackInfo, FBEventName } from 'foo-webview-sdk'

await fb.player.play()
```

> The runtime `fb` and the namespaces are also available from the `foo-webview-sdk/bridge` sub-path; that sub-path exposes only the runtime surface and does not separately export the shared types (import shared types from the package root).

Web Components and the SMP compatibility layer live under their own sub-paths:

```js
import { registerComponents } from 'foo-webview-sdk/components'
import { bootstrapSmpCompat } from 'foo-webview-sdk/smp-compat'
```

### `<script>` tag (IIFE global bundles)

```html
<script src="node_modules/foo-webview-sdk/dist/bridge.global.js"></script>
<script src="node_modules/foo-webview-sdk/dist/components.global.js"></script>
<!-- After loading, the global window.fb is available and components auto-register -->
```

## Quick start

### The old way (verbose)

```javascript
// 150+ characters
window.fb2k.invoke('playback.play', {})
window.fb2k.invoke('playlist.addPaths', { playlist: 0, paths: [...] })
```

### The new way (concise)

```javascript
// Just 20 characters
fb.player.play()
fb.playlist.add(0, paths)
```

## API namespaces

### `fb.player` — playback control

```javascript
// Playback control
await fb.player.play()
await fb.player.pause()
await fb.player.stop()
await fb.player.next()
await fb.player.prev()
await fb.player.toggle()  // toggle play/pause

// Seeking
await fb.player.seek(30)  // seek to 30 seconds

// Volume
await fb.player.setVolume(80)
const { volume, isMuted } = await fb.player.getVolume()
await fb.player.mute()

// State
const state = await fb.player.getState()
const track = await fb.player.getCurrentTrack()
const { position } = await fb.player.getPosition()

// Playback order
const { order, name } = await fb.player.getOrder()
await fb.player.setOrder(0)  // 0=default, 1=repeat playlist, 2=repeat track, 3=shuffle...

// Stop after current
await fb.player.setStopAfterCurrent(true)
```

### `fb.playlist` — playlist management

```javascript
// Get playlists
const playlists = await fb.playlist.getAll()
const active = await fb.playlist.getActive()
const playing = await fb.playlist.getPlaying()

// Manipulate playlists
await fb.playlist.setActive(0)
await fb.playlist.create('My Playlist')
await fb.playlist.rename(0, 'New Name')
await fb.playlist.duplicate(0)
await fb.playlist.remove(0)
await fb.playlist.clear(0)

// Track operations
const tracks = await fb.playlist.getTracks(0, 0, 100)
const { count } = await fb.playlist.getCount(0)
await fb.playlist.add(0, ['C:/Music/song.mp3'])
await fb.playlist.removeTracks(0, [0, 1, 2])
await fb.playlist.playTrack(0, 5)

// Selection
await fb.playlist.selectAll(0)
await fb.playlist.deselectAll(0)
await fb.playlist.setSelection(0, [0, 1, 2], true)

// Move tracks (delta is the offset; positive = down, negative = up)
await fb.playlist.moveTracks(0, [0, 1], 2)  // move down by 2

// Undo / redo
await fb.playlist.undo(0)
await fb.playlist.redo(0)

// Atomic operation: clear + add + play
await fb.playlist.replaceAllAndPlay({
    playlist: 0,
    paths: ['C:/Music/song1.mp3', 'C:/Music/song2.mp3'],
    playIndex: 0
})
```

### `fb.library` — media library

```javascript
// Search (pagination fields: tracks/items/total/offset/limit/hasMore)
const result = await fb.library.search('artist HAS Radiohead', 50)
console.log(result.total, result.tracks.length)

// Enumeration tip: get the total first, then page through it
const { count } = await fb.library.getCount()
for (let start = 0; start < count; start += 500) {
    const page = await fb.library.getAll(start, 500) // returns { tracks, items, total, offset, limit, fromCache }
    // process page.tracks
}

// High-level helper: async-generator paginated enumeration
for await (const page of fb.library.enumerateTracks({ pageSize: 500 })) {
    console.log(page.fetched, '/', page.total)
}

// ★ Recommended: get the real library roots
const { roots, total, indexedTracks, skippedTracks } = await fb.library.getRoots()
for (const root of roots) {
    console.log(root.displayName, root.absolutePath, root.trackCount)
}

// ★ Recommended: typed directory-tree browsing
const tree = await fb.library.browseTree({ rootId: roots[0].id })
for (const dir of tree.directories) {
    console.log(dir.name, dir.trackCount, dir.hasChildren)
}
// With a file list
const treeWithFiles = await fb.library.browseTree({
    rootId: roots[0].id, pathId: 'Anime/OST', includeFiles: true
})

// Legacy: directory-view enumeration (@deprecated as a root entry — prefer getRoots + browseTree + enumerateTree)
const root = await fb.library.browseDirectory('', false) // { directories, files, items }
for await (const node of fb.library.enumerateDirectories({ rootPath: '', includeFiles: false })) {
    console.log(node.path, node.directories.length)
}

// ★ Recommended: root-aware async tree walker
for await (const batch of fb.library.enumerateTree({ rootId: roots[0].id, strategy: 'bfs', includeFiles: true })) {
    console.log(batch.pathId, batch.directories.length, batch.files.length, batch.visited)
}

// Categories and aggregation
const albums = await fb.library.getAlbums(100)
const artists = await fb.library.getArtists(200)
const genres = await fb.library.getGenres() // { genres: [{name, trackCount}] }
const years = await fb.library.getFieldValues('date', 50)
const recent = await fb.library.getRecentlyAdded(20, 'modified')

// Stats
const stats = await fb.library.getStats()
const status = await fb.library.getStatus()

// Operations
await fb.library.refresh()
await fb.library.rescan()
const track = await fb.library.getByPath('C:/Music/song.mp3')
await fb.library.addToPlaylist(['C:/Music/song.mp3'])
// Note: library.add / library.remove were removed (not implemented in the C++ backend)
```

### `fb.ui` — window control

```javascript
// Window operations
await fb.ui.minimize()
await fb.ui.maximize()
await fb.ui.restore()
await fb.ui.close()
await fb.ui.toggleMaximize()
await fb.ui.toggleFullscreen()

// Window info
const { isMaximized } = await fb.ui.isMaximized()
const { isFullscreen } = await fb.ui.isFullscreen()
const state = await fb.ui.getState()

// Window settings
await fb.ui.setPosition(100, 100)
await fb.ui.setSize(1280, 720)
await fb.ui.setTitle('My Player')
await fb.ui.setAlwaysOnTop(true)

// Misc
await fb.ui.startDrag()
await fb.ui.showSystemMenu(100, 100)
const { scale } = await fb.ui.getDpiScale()
await fb.ui.flashTaskbar()
```

### `fb.config` — configuration

```javascript
// Output devices
const devices = await fb.config.getOutputDevices()
const config = await fb.config.getOutputConfig()
await fb.config.setOutputDevice('output-id', 'device-id')
await fb.config.setOutputBuffer(500)

// Advanced config
const advanced = await fb.config.getAdvancedConfig()
const { value } = await fb.config.getAdvancedConfigValue('{guid}')
await fb.config.setAdvancedConfigValue('{guid}', 'new-value')
await fb.config.resetAdvancedConfig('{guid}')

// Preferences
const pages = await fb.config.getPreferencesPages()
const guids = await fb.config.getPreferencesStandardGuids()
await fb.config.showLibraryPreferences()

// Component info
const components = await fb.config.getComponents()
const version = await fb.config.getVersionInfo()

// DSP
const presets = await fb.config.getDspPresets()
const active = await fb.config.getActiveDspPreset()
await fb.config.setActiveDspPreset(0)  // set by index
```

### `fb.artwork` — cover art

```javascript
// Get artwork
const art = await fb.artwork.getCurrent('front')
if (art) {
    console.log(`Cover: ${art.width}x${art.height}, ${art.mimeType}`)
    // art.data is Base64-encoded image data
}

// Get artwork by path
const art = await fb.artwork.getByPath('C:/Music/song.mp3', 'front')
```

### `fb.audio` — audio analysis

```javascript
// Subscribe to real-time spectrum data
const unsubscribe = fb.audio.subscribeSpectrum((data) => {
    renderVisualizer(data.spectrum);
}, { fftSize: 2048, fps: 60 });

// Use a custom event name to isolate multiple visualizer panels
const unsubscribePanel = fb.audio.subscribeSpectrum((data) => {
    renderPanelSpectrum(data.spectrum);
}, { event: 'audio:panelSpectrum', bands: 48, fps: 30 });

// Unsubscribe
unsubscribe();
unsubscribePanel();

// Generate a full-file waveform (supports caching and background decoding)
const result = await fb.audio.generateFullWaveform('E:\\Music\\song.flac', {
    resolution: 256,
    method: 'peak'  // or 'rms'
});

if (result.status === 'ready') {
    console.log('Waveform data:', result.waveform);
    console.log('Duration:', result.duration);
}

// Supports subsong and CUE splitting
const result2 = await fb.audio.generateFullWaveform('E:\\Music\\disc.flac|subsong:2', {
    resolution: 512
});

const result3 = await fb.audio.generateFullWaveform('E:\\Music\\album.cue', {
    cueIndex: 3,
    resolution: 256
});

// Analyze BPM
const { bpm, source } = await fb.audio.analyzeBPM('E:\\Music\\song.flac');
console.log(`BPM: ${bpm}, source: ${source}`);

// Force re-analysis (ignore any existing BPM tag)
const forced = await fb.audio.analyzeBPM('E:\\Music\\song.flac', {
    forceAnalysis: true
});
console.log(`BPM: ${forced.bpm}, source: ${forced.source}`);

// An invalid channel mode automatically falls back to "default"
const channelModeResult = await fb.audio.setChannelMode('invalid');
console.log(channelModeResult.mode); // "default"
```

> The SDK does not expose `subscriptionId`. If you need explicit control over the subscription ID, call `fb2k.invoke('audio.subscribeSpectrum', ...)` / `fb2k.invoke('audio.unsubscribeSpectrum', ...)` directly.

### `fb.utils` — utilities

```javascript
// Test the connection
const { pong, timestamp } = await fb.utils.ping()

// Format a title
const { result } = await fb.utils.formatTitle('%artist% - %title%', 'C:/Music/song.mp3')

// Get file info
const info = await fb.utils.getFileInfo('C:/Music/song.mp3')
```

### `fb.system` — system API

```javascript
// API discovery
const apis = await fb.system.listApis(true, true)
const namespaceApis = await fb.system.getApisByNamespace('playback')
const searchResults = await fb.system.searchApis('volume')

// API stats
const stats = await fb.system.getApiStats()

// Plugin management
const plugins = await fb.system.getRegisteredPlugins()
const { registered } = await fb.system.isPluginRegistered('my-plugin')
```

### `fb.state` — reactive state

`fb.state` is an auto-syncing reactive state object that keeps playback state up to date:

```javascript
// Read directly (auto-synced)
console.log(fb.state.volume)       // current volume
console.log(fb.state.isPlaying)    // whether playback is active
console.log(fb.state.currentTrack) // current track
console.log(fb.state.position)     // current playback position (seconds)

// In Vue / React
// Vue:
//   watch(() => fb.state.isPlaying, (playing) => { ... })
// React:
//   useEffect(() => { ... }, [fb.state.isPlaying])
```

> **Note**: `fb.state` is a read-only playback-state snapshot — different from `fb.sharedState`!

### `fb.sharedState` — cross-window shared state

`fb.sharedState` shares key/value data across multiple windows (e.g. lyrics cache, user preferences):

```javascript
// Write shared state
await fb.sharedState.set('lyrics:123', { text: 'lyrics content...' })

// Temporary cache with TTL (auto-expires after 5 minutes)
await fb.sharedState.set('cache:artwork', artworkData, false, 300000)

// Read shared state
const { value, exists } = await fb.sharedState.get('lyrics:123')

// Delete shared state
await fb.sharedState.delete('lyrics:123')

// Get all keys (wildcards supported)
const { keys } = await fb.sharedState.keys('lyrics:*')

// Listen for changes
fb.sharedState.onChange(({ key, value, previousValue, sourceWindowId }) => {
    console.log(`State ${key} was changed by window ${sourceWindowId}`)
})

// Listen for deletions (including expiry)
fb.sharedState.onDelete(({ key, reason }) => {
    console.log(`State ${key} was deleted, reason: ${reason}`)  // reason: 'deleted' | 'expired'
})
```

> **`fb.state` vs `fb.sharedState`**:
> - `fb.state`: read-only playback-state snapshot (volume, playback state, current track, ...), auto-synced
> - `fb.sharedState`: read/write cross-window key/value store for custom data sharing

### `fb.port` — named channels

`fb.port` provides a named-channel mechanism for bidirectional messaging between windows:

```javascript
// Create a channel
const { portId } = await fb.port.connect('lyrics-sync')

// Broadcast to all ports on the same channel name
await fb.port.postMessage(portId, { type: 'sync', data: '...' })

// Send to a specific port
await fb.port.postMessageTo(portId, targetPortId, { type: 'request' })

// Get all ports on a channel name
const { ports } = await fb.port.getPorts('lyrics-sync')

// Listen for messages
fb.port.onMessage(({ portId, message, sourceWindowId }) => {
    console.log(`Message from ${sourceWindowId}:`, message)
})

// Listen for connect / disconnect
fb.port.onConnect(({ portId, name, windowId }) => {
    console.log(`Port ${portId} connected to channel ${name}`)
})

// Disconnect
await fb.port.disconnect(portId)
```

### `fb.event` — custom events

`fb.event` provides a semantic custom-event publishing mechanism:

```javascript
// Broadcast a custom event to all windows (example: a theme-defined event name)
await fb.event.emit('myapp:lyricsUpdated', { trackId: '123' })

// Broadcast but exclude self
await fb.event.emit('myapp:lyricsUpdated', { trackId: '123' }, true)

// Send to a specific window
await fb.event.emitTo('myapp:lyricsUpdated', { trackId: '123' }, 'popup_1')

// Listen for custom events
fb.on('myapp:lyricsUpdated', ({ payload, sourceWindowId }) => {
    console.log(`Lyrics update from ${sourceWindowId}:`, payload.trackId)
})
```

> **`fb.event` vs `window.sendMessage/broadcast`**:
> - `window.sendMessage/broadcast`: raw message channel with the fixed event name `window:message`
> - `fb.event`: semantic event channel where the event name is provided by the caller

## Event subscription

```javascript
// Subscribe to playback state changes
const unsubscribe = fb.on('playback.state', (data) => {
    console.log('state:', data.state)
    console.log('track:', data.track)
    console.log('position:', data.position)
})

// Subscribe to playlist changes
fb.on('playlist.changed', (data) => {
    console.log('playlist:', data.playlistIndex)
})

// Unsubscribe
unsubscribe()

// One-time subscription
fb.once('playback.state', (data) => {
    console.log('one-time event:', data)
})
```

## TypeScript support

Type definitions ship with the package, so your IDE provides full IntelliSense:

```typescript
import fb from 'foo-webview-sdk'

async function playNext() {
    await fb.player.next()
    const track = await fb.player.getCurrentTrack()
    if (track) {
        console.log(`Now playing: ${track.artist} - ${track.title}`)
    }
}
```

> The package root `foo-webview-sdk` exposes both the runtime (`fb`, the namespaces, `bridge`, `state`) and the shared types (`TrackInfo`, `FBEventName`, ...); the `foo-webview-sdk/bridge` sub-path contains only the runtime surface and does not separately export the shared types.

## Availability check

```javascript
if (fb.isAvailable()) {
    // WebView2 environment is available
    await fb.player.play()
} else {
    console.warn('WebView2 is not available')
}
```

## Development mock mode

Outside a WebView2 environment the SDK automatically uses mock mode, and every API call returns mock data:

```javascript
// Behavior outside a WebView2 environment
const result = await fb.player.play()
// returns: { mock: true, method: 'playback.play' }
```

---

## Web Components library

Beyond the JavaScript API, the SDK ships ready-to-use Web Components.

### Quick start

Load the IIFE global bundles via `<script>` (`components.global.js` auto-registers every component):

```html
<script src="node_modules/foo-webview-sdk/dist/bridge.global.js"></script>
<script src="node_modules/foo-webview-sdk/dist/components.global.js"></script>

<!-- Use the components directly -->
<fb-play-button></fb-play-button>
<fb-seek-bar show-time></fb-seek-bar>
<fb-cover-art size="200"></fb-cover-art>
```

Bundler users should use the ESM entry and register explicitly (the ESM entry has no automatic side effects):

```js
import { registerComponents } from 'foo-webview-sdk/components'
registerComponents()
```

### Control components

#### `<fb-play-button>` — play/pause button
Toggles play/pause automatically and updates its icon.

```html
<fb-play-button size="32"></fb-play-button>
```

| Attribute | Type | Default | Description |
|------|------|--------|------|
| `size` | string | "24" | Icon size (px) |
| `disabled` | boolean | false | Disabled state |

#### `<fb-prev-button>` — previous track
```html
<fb-prev-button size="24"></fb-prev-button>
```

#### `<fb-next-button>` — next track
```html
<fb-next-button size="24"></fb-next-button>
```

#### `<fb-stop-button>` — stop
```html
<fb-stop-button size="24"></fb-stop-button>
```

#### `<fb-seek-bar>` — progress bar
Supports click-to-seek and drag positioning.

```html
<fb-seek-bar show-time color="#1db954"></fb-seek-bar>
```

| Attribute | Type | Default | Description |
|------|------|--------|------|
| `show-time` | boolean | false | Show time |
| `color` | string | "#1db954" | Progress color |

#### `<fb-volume-bar>` — volume bar
Supports click, drag, and wheel adjustment.

```html
<fb-volume-bar show-icon vertical color="#1db954"></fb-volume-bar>
```

| Attribute | Type | Default | Description |
|------|------|--------|------|
| `show-icon` | boolean | false | Show volume icon |
| `vertical` | boolean | false | Vertical layout |
| `color` | string | "#1db954" | Slider color |

### Display components

#### `<fb-cover-art>` — cover image
Auto-loads the current track's cover; supports lazy loading and a fallback image.

```html
<fb-cover-art size="200" radius="8" fallback-src="default.jpg"></fb-cover-art>
```

| Attribute | Type | Default | Description |
|------|------|--------|------|
| `size` | string | "200" | Size (px) |
| `radius` | string | "8" | Corner radius (px) |
| `fallback-src` | string | "" | Fallback image on load failure |

#### `<fb-title>` — track title
```html
<fb-title placeholder="Not playing" marquee></fb-title>
```

| Attribute | Type | Default | Description |
|------|------|--------|------|
| `placeholder` | string | "Not playing" | Shown when there is no title |
| `marquee` | boolean | false | Enable marquee effect |

#### `<fb-artist>` — artist
```html
<fb-artist placeholder="Unknown artist"></fb-artist>
```

#### `<fb-album>` — album
```html
<fb-album placeholder="Unknown album"></fb-album>
```

#### `<fb-time-display>` — time display
```html
<fb-time-display format="current / total"></fb-time-display>
<fb-time-display format="-remaining"></fb-time-display>
```

| Attribute | Type | Default | Description |
|------|------|--------|------|
| `format` | string | "current / total" | Format template |

Format placeholders:
- `current` — current playback time
- `total` — total duration
- `remaining` — remaining time

### Styling

All components use Shadow DOM; customize them via CSS variables and the `::part()` selector:

```css
/* Using the ::part() selector */
fb-play-button::part(button) {
    background: #1db954;
    border-radius: 50%;
}

fb-seek-bar::part(progress) {
    background: linear-gradient(90deg, #ff6b6b, #feca57);
}

fb-cover-art::part(container) {
    border: 2px solid rgba(255, 255, 255, 0.1);
}
```

### Full example

See the [examples directory](https://github.com/NereaFantasia/foo_ui_webview2/tree/main/examples) in the repository:

```html
<div class="player">
    <fb-cover-art size="280" radius="16"></fb-cover-art>
    
    <div class="track-info">
        <fb-title></fb-title>
        <fb-artist></fb-artist>
    </div>
    
    <fb-seek-bar show-time color="#1db954"></fb-seek-bar>
    
    <div class="controls">
        <fb-prev-button></fb-prev-button>
        <fb-play-button size="32"></fb-play-button>
        <fb-next-button></fb-next-button>
    </div>
    
    <fb-volume-bar show-icon></fb-volume-bar>
</div>
```

---

## License

This SDK is part of the foo_ui_webview2 plugin and is released under the [MIT License](./LICENSE).
Copyright © 2024-2026 NereaFantasia.

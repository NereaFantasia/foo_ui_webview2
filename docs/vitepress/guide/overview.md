# Overview

> **Minimum foobar2000 version**: 2.0+

**foo_ui_webview2** is a foobar2000 component that replaces the default user interface with WebView2, so you can build custom UIs with modern web technologies (HTML/CSS/JavaScript).

::: tip Migrating from Spider Monkey Panel (SMP)?
v1.2.0 ships an SMP compatibility layer, including:
- **35 SMP event mappings** — SMP names such as `on_playback_new_track` work directly
- **8 wrapper classes** — `FbMetadbHandle`, `FbTitleFormat`, `ContextMenuManager`, and more
- **Full `fb` / `plman` objects** — sync properties, playback control, and playlist management

How it works: a **cache + event** model wraps async APIs as sync properties to match SMP's sync semantics. This is an **eventual consistency** model (not strong consistency) and fits UI rendering. For precise transactional control, call the low-level `fb2k.invoke()` APIs directly. WARNING: SMP GDI/GDI+ drawing (`on_paint(gr)`, etc.) cannot migrate — rewrite the UI layer with HTML/CSS/Canvas. → [Full SMP compatibility docs](/reference/smp-compat)
:::

<a id="key-features"></a>
<a id="主要特性"></a>

## API distribution

| Module | Count | Description |
| --- | --- | --- |
| playback | 27 | Playback control, state, volume, path playback |
| playlist | 45 | Playlist management and autoplaylists |
| library | 21 | Media library browse, search, path lookup |
| artwork | 12 | Cover art, `fb2k://` URLs, batch fetch, server-side scaling |
| config | 29 | Configuration, devices, persistent storage, output settings |
| window | 72 | Window control, DWM effects, multi-window system |
| system | 9 | Theme/DPI/language, API discovery |
| file | 10 | File read/write, copy, move, directory ops |
| dialog | 4 | File/folder dialogs |
| clipboard | 4 | Clipboard operations |
| shell | 4 | System integration |
| http | 4 | HTTP requests |
| ui | 5 | Custom menus and notifications |
| keyboard | 4 | Global hotkeys |
| lyrics | 3 | Lyrics management |
| metadata | 9 | Metadata read/write and batch read |
| audio | 12 | Audio analysis, spectrum, waveform |
| console | 6 | Logging |
| dnd | 3 | External file drop |
| queue | 8 | Playback queue |
| jitQueue | 7 | JIT immediate queue |
| discovery | 15 | Service discovery and context menus |
| dsp | 8 | DSP chain management |
| output | 3 | Audio output devices |
| replaygain | 8 | ReplayGain settings |
| playcount | 6 | Play statistics and ratings |
| titleformat | 5 | Titleformat evaluation |
| selection | 6 | Selection sync and viewer mode |
| menu | 5 | Main/context menu commands |
| misc | 9 | Path queries, preferences, console helpers |
| panel | 2 | Panel mode detection |
| test | 2 | Test utilities |

## Design principles

> foo_ui_webview2 follows "the component provides capabilities; the app decides policy."

### Component responsibilities (provided by foo_ui_webview2)

| Category | Description | Examples |
| --- | --- | --- |
| foobar2000 core capabilities | Playback, playlists, library, artwork | `playback.*`, `playlist.*` |
| Window management | DWM effects, caption, scaling | `window.*` |
| System integration | Files, dialogs, shell | `file.*`, `dialog.*` |
| Audio analysis | Spectrum and waveform | `audio.*` |
| Config storage | Persistent key-value storage | `config.*` |
| Streaming helpers | Generic URL playback, JIT queue | `jitQueue.*` |

### Application responsibilities (implemented by you)

| Category | Description | Why not in the component |
| --- | --- | --- |
| Online service integration | Spotify, NetEase, QQ Music, etc. | Auth and business flows differ |
| Multi-source lyrics aggregation | Online search and translation merge | Business logic |
| Artwork cache policy | Prefetch and LRU | Policy choice |
| UI theme and layout | Colors, fonts, animation | App-specific |

### Example: capability vs policy

```javascript
// Component capability: fetch artwork
await fb2k.invoke('artwork.getFb2kUrl', { path: trackPath, maxSize: 200 });

// App policy: prefetch / cache
function prefetchCovers(albums) {
    albums.forEach(album => {
        const img = new Image();
        img.src = `fb2k://artwork/${encodeURIComponent(album.path)}?maxSize=200`;
    });
}
```

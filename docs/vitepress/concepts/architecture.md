# How foo_ui_webview2 works

> **Minimum foobar2000 version**: 2.0+

foo_ui_webview2 is a foobar2000 component that hosts web pages via Microsoft WebView2 and exposes playback control and status interfaces. A theme is a standard web application built with HTML, CSS, JavaScript, and any modern web framework; the component renders it as the main foobar2000 window or as an embedded panel within another user interface.

<a id="key-features"></a>
<a id="主要特性"></a>

## A page and the host

The component injects a global `window.fb2k` object into every loaded page. Calling `fb2k.invoke('playback.play')` sends a message through WebView2 to the native C++ backend, which executes the action against foobar2000 and returns a structured result envelope. In reverse, player state events (such as track progression) reach the page as named notifications received via `fb2k.on('playback:trackChanged', handler)`. Endpoint invocations use dot notation (`playback.play`), while event notifications use colon notation (`playback:trackChanged`). The complete wire format is documented in [Bridge protocol](/reference/bridge).

All interactions between the web page and the player traverse this Bridge channel. Outside foobar2000 (such as in an ordinary browser tab), `window.fb2k` is absent, and SDK calls resolve with a `NOT_SUPPORTED` failure envelope. Target page selection and directory resolution follow the priority rules detailed in [How a page is found](./page-loading.md).

## The bridge and the SDK

The bridge is enough for any theme, but its calls are untyped strings. The npm package `foo-webview-sdk` adds three layers on top of it:

- `fb.*` methods (`fb.player.play()`, `fb.playlist.getAll()`), typed in TypeScript, which call the same host methods;
- `fb-*` Web Components, such as a play button or a playlist view, which talk to the host themselves and carry no styling of their own;
- an SMP compatibility layer for porting Spider Monkey Panel scripts.

The component exposes more than 400 methods and 100 events, grouped into namespaces such as `playback`, `playlist`, `library` and `window`. The [API overview](/api/overview) lists every namespace with its method count and the page that documents it; [SDK namespaces](/sdk/namespaces) lists the `fb.*` wrappers over them.

::: tip Migrating from Spider Monkey Panel (SMP)?
The SDK includes an SMP compatibility layer:
- **35 SMP event mappings**: SMP names such as `on_playback_new_track` work directly
- **8 wrapper classes**: `FbMetadbHandle`, `FbTitleFormat`, `ContextMenuManager`, and more
- **Full `fb` / `plman` objects**: sync properties, playback control, and playlist management

A **cache + event** model wraps the asynchronous host calls as synchronous properties to match SMP's semantics. The cache is eventually consistent, which suits UI rendering; for precise transactional control, call `fb2k.invoke()` directly. SMP's GDI/GDI+ drawing (`on_paint(gr)` and the like) cannot be ported: rewrite that part with HTML, CSS or Canvas. See [SMP compatibility](/reference/smp-compat).
:::

## Design principles

> foo_ui_webview2 follows "the component provides capabilities; the app decides policy."

### What the component provides

| Category | Description | Examples |
| --- | --- | --- |
| foobar2000 core capabilities | Playback, playlists, library, artwork | `playback.*`, `playlist.*` |
| Window management | DWM effects, caption, scaling | `window.*` |
| System integration | Files, dialogs, shell | `file.*`, `dialog.*` |
| Audio analysis | Spectrum and waveform | `audio.*` |
| Config storage | Persistent key-value storage | `config.*` |
| Streaming helpers | Generic URL playback, JIT queue | `jitQueue.*` |

### What the theme implements

| Category | Description | Why not in the component |
| --- | --- | --- |
| Online service integration | Spotify, NetEase, QQ Music, etc. | Auth and business flows differ |
| Multi-source lyrics aggregation | Online search and translation merge | Business logic |
| Artwork cache policy | Prefetch and LRU | Policy choice |
| UI theme and layout | Colors, fonts, animation | App-specific |

### Example: capability vs policy

```javascript
// Component capability: a URL the page can use as an image source
const current = await fb2k.invoke('playback.getCurrentTrack');
if (current.success && current.track) {
    const res = await fb2k.invoke('artwork.getFb2kUrlByPath', { path: current.track.path, maxSize: 200 });
    const img = document.querySelector('img.cover');
    if (res.success && img instanceof HTMLImageElement) img.src = res.dataUrl;
}

// Theme policy: which covers to fetch ahead of time, and how long to keep them
function prefetchCovers(urls) {
    urls.forEach((url) => {
        new Image().src = url;
    });
}
```

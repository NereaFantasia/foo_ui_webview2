# API Overview

The methods a page calls with `fb2k.invoke('namespace.method', params)`, by namespace. Events are listed in [Events API](./events.md). The SDK wraps these methods as `fb.*`; see [SDK namespaces](../sdk/namespaces.md).

<!-- api-schema:begin index:namespaces -->
446 methods in 41 namespaces.

| Namespace | Methods | Documented on |
| --- | ---: | --- |
| `artwork` | 13 | [Artwork API](./artwork.md) |
| `audio` | 17 | [Audio API](./audio.md) |
| `clipboard` | 4 | [Clipboard API](./clipboard.md) |
| `config` | 29 | [Config API](./config.md) |
| `console` | 3 | [Console API](./console.md) |
| `cursor` | 2 | [Cursor API](./cursor.md) |
| `dialog` | 4 | [Dialog API](./dialog.md) |
| `discovery` | 15 | [Discovery API](./discovery.md) |
| `dnd` | 4 | [Drag and Drop API](./dnd.md) |
| `dsp` | 8 | [DSP API](./dsp.md) |
| `event` | 2 | [Event API](./event.md) |
| `file` | 14 | [File API](./file.md) |
| `http` | 8 | [Http API](./http.md) |
| `jitQueue` | 8 | [JIT Queue API](./jit-queue.md) |
| `keyboard` | 4 | [Keyboard API](./keyboard.md) |
| `library` | 25 | [Library API](./library.md) |
| `log` | 3 | [Log API](./log.md) |
| `lyrics` | 3 | [Lyrics API](./lyrics.md) |
| `media` | 2 | [Media API](./media.md) |
| `menu` | 8 | [Menu API](./menu.md) |
| `metadata` | 12 | [Metadata API](./metadata.md) |
| `misc` | 9 | [Misc API](./misc.md) |
| `output` | 3 | [Output API](./output.md) |
| `panel` | 2 | [Panel API](./panel.md) |
| `playback` | 27 | [Playback API](./playback.md) |
| `playcount` | 4 | [Playcount API](./playcount.md) |
| `playlist` | 50 | [Playlist API](./playlist.md) |
| `port` | 5 | [Port API](./port.md) |
| `queue` | 11 | [Queue API](./queue.md) |
| `rating` | 2 | [Rating API](./rating.md) |
| `replaygain` | 8 | [ReplayGain API](./replaygain.md) |
| `selection` | 6 | [Selection API](./selection.md) |
| `shell` | 5 | [Shell API](./shell.md) |
| `state` | 4 | [State API](./state.md) |
| `system` | 9 | [System API](./system.md) |
| `taskbar` | 5 | [Taskbar API](./taskbar.md) |
| `titleformat` | 5 | [Titleformat API](./titleformat.md) |
| `tray` | 15 | [Tray API](./tray.md) |
| `ui` | 5 | [UI API](./ui.md) |
| `webview` | 1 | [WebView API](./webview.md) |
| `window` | 82 | [Window API](./window.md) |
<!-- api-schema:end -->

Two further methods, `test.echo` and `test.ping`, exist for testing the bridge itself and are described under [Test API](./test.md#test).

## Conventions

- Method names use dot format (`playback.play`); event names use colon format (`playback:trackChanged`). An event name cannot be invoked.
- Names of the form `namespace.__method` belong to pages the component ships itself, such as the tray menu, and are not part of the API.

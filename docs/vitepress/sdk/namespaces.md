# SDK namespaces

The aggregate `fb` object exposes 41 runtime namespaces, plus the reactive `fb.state` mirror, the top-level `fb.on`, `fb.off`, `fb.once`, `fb.invoke`, `fb.isAvailable()`, and `fb.ready()` helpers, and the failure helpers `fb.unwrap` and `fb.ApiCallError`. Each namespace has its own page; the reactive mirror is described in [fb.state](./state.md) and event subscription in [SDK event system](./events.md).

## Core media and UI

| Namespace | Purpose | Example |
| --- | --- | --- |
| [`fb.player`](./player.md) | Playback and volume control | `fb.player.play()` |
| [`fb.playlist`](./playlist.md) | Playlist management | `fb.playlist.getAll()` |
| [`fb.library`](./library.md) | Media-library queries and traversal | `fb.library.search('query')` |
| [`fb.ui`](./ui.md) | Window and popup management | `fb.ui.minimize()` |
| [`fb.config`](./config.md) | Portable configuration and host settings | `fb.config.get('key')` |
| [`fb.artwork`](./artwork.md) | Album-art retrieval | `fb.artwork.getCurrent()` |
| [`fb.audio`](./audio.md) | Audio analysis and waveform operations | `fb.audio.subscribeSpectrum()` |
| [`fb.media`](./media.md) | Local media URLs and container metadata | `fb.media.getContainerInfo(path)` |
| [`fb.output`](./output.md) | Output-device queries | `fb.output.getDevices()` |
| [`fb.dsp`](./dsp.md) | DSP chain and preset operations | `fb.dsp.getChain()` |
| [`fb.replaygain`](./replaygain.md) | ReplayGain configuration | `fb.replaygain.getMode()` |
| [`fb.queue`](./queue.md) | Playback queue | `fb.queue.get()` |
| [`fb.jitQueue`](./jit-queue.md) | Just-in-time queue | `fb.jitQueue.enqueueNext()` |

## Metadata and data access

| Namespace | Purpose | Example |
| --- | --- | --- |
| [`fb.metadata`](./metadata.md) | Metadata reads and writes | `fb.metadata.read(path)` |
| [`fb.titleformat`](./titleformat.md) | Title Formatting evaluation | `fb.titleformat.eval()` |
| [`fb.playcount`](./playcount.md) | Playback statistics | `fb.playcount.get(path)` |
| [`fb.rating`](./rating.md) | Track ratings | `fb.rating.set(path, 5)` |
| [`fb.selection`](./selection.md) | Global selection synchronization | `fb.selection.get()` |
| [`fb.file`](./file.md) | File I/O | `fb.file.read(path)` |
| [`fb.http`](./http.md) | HTTP requests | `fb.http.get(url)` |
| [`fb.clipboard`](./clipboard.md) | Clipboard access | `fb.clipboard.read()` |
| [`fb.dialog`](./dialog.md) | Native dialogs | `fb.dialog.openFile()` |

## Cross-window and desktop integration

| Namespace | Purpose | Example |
| --- | --- | --- |
| [`fb.port`](./port.md) | Named cross-window ports | `fb.port.connect('main')` |
| [`fb.event`](./event.md) | Cross-window custom events | `fb.event.emit('refresh')` |
| [`fb.sharedState`](./shared-state.md) | Shared cross-window state | `fb.sharedState.get('theme')` |
| [`fb.cursor`](./cursor.md) | Per-window cursor visibility | `fb.cursor.setHidden(true)` |
| [`fb.taskbar`](./taskbar.md) | Taskbar buttons and progress | `fb.taskbar.setProgress(50)` |
| [`fb.tray`](./tray.md) | System tray icon and menus | `fb.tray.showBalloon(opts)` |
| [`fb.keyboard`](./keyboard.md) | Global hotkeys | `fb.keyboard.registerHotkey()` |
| [`fb.discovery`](./discovery.md) | Service and API discovery | `fb.discovery.getAllServices()` |
| [`fb.shell`](./shell.md) | Operating-system integration | `fb.shell.openExternal(url)` |

## Utilities and host services

| Namespace | Purpose | Example |
| --- | --- | --- |
| [`fb.system`](./system.md) | API, plugin, locale, DPI, and theme discovery | `fb.system.listApis()` |
| [`fb.utils`](./utils.md) | General utility methods | `fb.utils.formatTitle()` |
| [`fb.menu`](./menu.md) | Main-menu and context-menu commands | `fb.menu.runMainMenuCommand()` |
| [`fb.console`](./console.md) | Host console output | `fb.console.log(msg)` |
| [`fb.log`](./log.md) | Log-file operations | `fb.log.write(msg)` |
| [`fb.lyrics`](./lyrics.md) | Lyrics access | `fb.lyrics.get()` |
| [`fb.notification`](./notification.md) | Toasts and notifications | `fb.notification.showToast()` |
| [`fb.panel`](./panel.md) | Panel configuration | `fb.panel.getConfig()` |
| [`fb.webview`](./webview.md) | Where the page was loaded from | `fb.webview.getSource()` |
| [`fb.misc`](./misc.md) | Miscellaneous host operations | `fb.misc.restart()` |
| [`fb.dnd`](./dnd.md) | External file drop | `fb.dnd.getPathsAsync()` |

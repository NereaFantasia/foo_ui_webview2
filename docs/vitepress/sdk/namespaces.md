# SDK namespaces

The aggregate `fb` object exposes 40 runtime namespaces, plus the reactive `fb.state` mirror and the top-level `fb.on`, `fb.off`, `fb.once`, `fb.invoke`, `fb.isAvailable()`, and `fb.ready()` helpers.

The AST coverage gate currently reports `449` methods in the complete public facade and `0` undocumented example-call gaps. It derives that facade directly from the exported namespace objects; the narrower direct-`invoke` extractor remains a diagnostic subset rather than the SDK total.

## Core media and UI

| Namespace | Purpose | Example |
| --- | --- | --- |
| `fb.player` | Playback and volume control | `fb.player.play()` |
| `fb.playlist` | Playlist management | `fb.playlist.getAll()` |
| `fb.library` | Media-library queries and traversal | `fb.library.search('query')` |
| `fb.ui` | Window and popup management | `fb.ui.minimize()` |
| `fb.config` | Portable configuration and host settings | `fb.config.get('key')` |
| `fb.artwork` | Album-art retrieval | `fb.artwork.getCurrent()` |
| `fb.audio` | Audio analysis and waveform operations | `fb.audio.subscribeSpectrum()` |
| `fb.output` | Output-device queries | `fb.output.getDevices()` |
| `fb.dsp` | DSP chain and preset operations | `fb.dsp.getChain()` |
| `fb.replaygain` | ReplayGain configuration | `fb.replaygain.getMode()` |
| `fb.queue` | Playback queue | `fb.queue.get()` |
| `fb.jitQueue` | Just-in-time queue | `fb.jitQueue.enqueueNext()` |

## Metadata and data access

| Namespace | Purpose | Example |
| --- | --- | --- |
| `fb.metadata` | Metadata reads and writes | `fb.metadata.read(path)` |
| `fb.titleformat` | Title Formatting evaluation | `fb.titleformat.eval()` |
| `fb.playcount` | Playback statistics | `fb.playcount.get(path)` |
| `fb.rating` | Track ratings | `fb.rating.set(path, 5)` |
| `fb.selection` | Global selection synchronization | `fb.selection.get()` |
| `fb.file` | File I/O | `fb.file.read(path)` |
| `fb.http` | HTTP requests | `fb.http.get(url)` |
| `fb.clipboard` | Clipboard access | `fb.clipboard.read()` |
| `fb.dialog` | Native dialogs | `fb.dialog.openFile()` |

## Cross-window and desktop integration

| Namespace | Purpose | Example |
| --- | --- | --- |
| `fb.port` | Named cross-window ports | `fb.port.connect('main')` |
| `fb.event` | Cross-window custom events | `fb.event.emit('refresh')` |
| `fb.sharedState` | Shared cross-window state | `fb.sharedState.get('theme')` |
| `fb.cursor` | Per-window cursor visibility | `fb.cursor.setHidden(true)` |
| `fb.taskbar` | Taskbar buttons and progress | `fb.taskbar.setProgress(50)` |
| `fb.tray` | System tray icon and menus | `fb.tray.showBalloon(opts)` |
| `fb.keyboard` | Global hotkeys | `fb.keyboard.registerHotkey()` |
| `fb.discovery` | Service and API discovery | `fb.discovery.getAllServices()` |
| `fb.shell` | Operating-system integration | `fb.shell.openExternal(url)` |

## Utilities and host services

| Namespace | Purpose | Example |
| --- | --- | --- |
| `fb.system` | API, plugin, locale, DPI, and theme discovery | `fb.system.listApis()` |
| `fb.utils` | General utility methods | `fb.utils.formatTitle()` |
| `fb.menu` | Main-menu and context-menu commands | `fb.menu.runMainMenuCommand()` |
| `fb.console` | Host console output | `fb.console.log(msg)` |
| `fb.log` | Log-file operations | `fb.log.write(msg)` |
| `fb.lyrics` | Lyrics access | `fb.lyrics.get()` |
| `fb.notification` | Toasts and notifications | `fb.notification.showToast()` |
| `fb.panel` | Panel configuration | `fb.panel.getConfig()` |
| `fb.misc` | Miscellaneous host operations | `fb.misc.restart()` |
| `fb.dnd` | External file drop | `fb.dnd.getPathsAsync()` |

## Related documentation

**Core:** [player](./player) | [playlist](./playlist) | [library](./library) | [ui](./ui) | [config](./config) | [artwork](./artwork) | [audio](./audio) | [utils](./utils) | [system](./system) | [shell](./shell) | [state](./state) | [events](./events)

**Extended:** [file I/O](./file-io) | [extended audio](./audio-ext) | [navigation](./navigation) | [data](./data) | [miscellaneous](./misc)

**Desktop integration:** [cursor API](../api/cursor) | [taskbar and tray API](../api/taskbar-tray)

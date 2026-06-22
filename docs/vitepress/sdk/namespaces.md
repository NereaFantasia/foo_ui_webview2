# SDK 命名空间 

SDK 通过全局 `fb` 对象提供以下命名空间（共 40 个 namespace）。

R12 VitePress SDK 覆盖门禁当前按 SDK wrapper 视角统计为 `386` 个 method / `0` 个文档示例调用缺口；下表“数量”列沿用 API reference / host capability 口径，可能包含状态、事件或非 wrapper 入口，因此不等同于 R12 统计口径。

## 核心命名空间 

| 命名空间 | 描述 | 数量 | 示例 |
| --- | --- | --- | --- |
| fb.player | 播放控制 | 27 | fb.player.play() |
| fb.playlist | 播放列表管理 | 47 | fb.playlist.getAll() |
| fb.library | 媒体库操作 | 25 | fb.library.search('query') |
| fb.ui | 窗口控制 | 82 | fb.ui.minimize() |
| fb.config | 配置管理 | 29 | fb.config.get('key') |
| fb.artwork | 封面获取 | 14 | fb.artwork.getCurrent() |
| fb.audio | 音频分析 | 14 | fb.audio.subscribeSpectrum() |
| fb.utils | 工具函数 | 3 | fb.utils.formatTitle() |
| fb.system | 系统信息 | 9 | fb.system.listApis() |
| fb.shell | 系统集成 | 5 | fb.shell.openExternal(url) |
| fb.state | 响应式状态 | - | fb.state.isPlaying |

| 命名空间 | 描述 | 数量 | 示例 |
| --- | --- | --- | --- |
| fb.file | 文件读写 | 10 | fb.file.read(path) |
| fb.http | HTTP 请求 | 9 | fb.http.get(url) |
| fb.dialog | 对话框 | 4 | fb.dialog.openFile() |
| fb.clipboard | 剪贴板 | 4 | fb.clipboard.read() |

| 命名空间 | 描述 | 数量 | 示例 |
| --- | --- | --- | --- |
| fb.dsp | DSP 预设 | 8 | fb.dsp.getChain() |
| fb.output | 输出设备 | 3 | fb.output.getDevices() |
| fb.replaygain | ReplayGain | 8 | fb.replaygain.getMode() |

| 命名空间 | 描述 | 数量 | 示例 |
| --- | --- | --- | --- |
| fb.queue | 播放队列 | 8 | fb.queue.get() |
| fb.jitQueue | JIT 即时队列 | 8 | fb.jitQueue.enqueueNext() |
| fb.discovery | 服务发现 | 15 | fb.discovery.getAllServices() |
| fb.keyboard | 全局快捷键 | 4 | fb.keyboard.registerHotkey() |

| 命名空间 | 描述 | 数量 | 示例 |
| --- | --- | --- | --- |
| fb.metadata | 元数据读写 | 10 | fb.metadata.read(path) |
| fb.titleformat | Titleformat | 5 | fb.titleformat.eval() |
| fb.playcount | 播放统计 | 4 | fb.playcount.get(path) |
| fb.selection | 选择同步 | 6 | fb.selection.get() |
| fb.rating | 评分 | 2 | fb.rating.set(path, 5) |

| 命名空间 | 描述 | 数量 | 示例 |
| --- | --- | --- | --- |
| fb.port | PortHub 跨窗口端口 | 5 | fb.port.connect('main') |
| fb.event | 跨窗口事件 | - | fb.event.emit('refresh') |
| fb.sharedState | 跨窗口共享状态 | - | fb.sharedState.get('theme') |

| 命名空间 | 描述 | 数量 | 示例 |
| --- | --- | --- | --- |
| fb.cursor | 光标控制 | 2 | fb.cursor.set('hand') |
| fb.taskbar | 任务栏按钮 | 5 | fb.taskbar.setProgress(50) |
| fb.tray | 系统托盘 | 13 | fb.tray.showBalloon(opts) |

| 命名空间 | 描述 | 数量 | 示例 |
| --- | --- | --- | --- |
| fb.menu | 菜单命令 | 6 | fb.menu.runMainMenuCommand() |
| fb.console | 控制台 | 3 | fb.console.log(msg) |
| fb.log | 日志文件 | 3 | fb.log.write(msg) |
| fb.lyrics | 歌词 | 3 | fb.lyrics.get() |
| fb.notification | 通知/菜单 | 4 | fb.notification.showToast() |
| fb.panel | 面板配置 | 2 | fb.panel.getConfig() |
| fb.misc | 杂项工具 | 9 | fb.misc.restart() |
| fb.dnd | 拖放 | 4 | fb.dnd.registerDropZone() |

各命名空间详细文档：

**核心**: [player](./player) | [playlist](./playlist) | [library](./library) | [ui](./ui) | [config](./config) | [artwork](./artwork) | [audio](./audio) | [utils](./utils) | [system](./system) | [shell](./shell) | [state](./state) | [events](./events)

**扩展**: [file-io](./file-io) | [audio-ext](./audio-ext) | [navigation](./navigation) | [data](./data) | [misc](./misc)

**桌面集成**: [cursor API](../api/cursor) | [taskbar/tray API](../api/taskbar-tray)

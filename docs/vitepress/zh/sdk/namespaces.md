# SDK 命名空间

SDK 通过聚合 `fb` 对象提供 41 个运行时命名空间，此外还有响应式 `fb.state` 镜像，顶层的 `fb.on`、`fb.off`、`fb.once`、`fb.invoke`、`fb.isAvailable()`、`fb.ready()` 辅助方法，以及处理失败的 `fb.unwrap` 与 `fb.ApiCallError`。每个命名空间各有一页；响应式镜像见 [fb.state](./state.md)，事件订阅见 [SDK 事件系统](./events.md)。

## 核心命名空间

| 命名空间 | 用途 | 示例 |
| --- | --- | --- |
| [`fb.player`](./player.md) | 播放与音量控制 | `fb.player.play()` |
| [`fb.playlist`](./playlist.md) | 播放列表管理 | `fb.playlist.getAll()` |
| [`fb.library`](./library.md) | 媒体库查询与遍历 | `fb.library.search('query')` |
| [`fb.ui`](./ui.md) | 窗口与弹出窗口管理 | `fb.ui.minimize()` |
| [`fb.config`](./config.md) | 便携配置与宿主设置 | `fb.config.get('key')` |
| [`fb.artwork`](./artwork.md) | 获取专辑封面 | `fb.artwork.getCurrent()` |
| [`fb.audio`](./audio.md) | 音频分析与波形 | `fb.audio.subscribeSpectrum()` |
| [`fb.media`](./media.md) | 本地媒体地址与容器信息 | `fb.media.getContainerInfo(path)` |
| [`fb.output`](./output.md) | 查询输出设备 | `fb.output.getDevices()` |
| [`fb.dsp`](./dsp.md) | DSP 链与预设 | `fb.dsp.getChain()` |
| [`fb.replaygain`](./replaygain.md) | ReplayGain 设置 | `fb.replaygain.getMode()` |
| [`fb.queue`](./queue.md) | 播放队列 | `fb.queue.get()` |
| [`fb.jitQueue`](./jit-queue.md) | JIT 即时队列 | `fb.jitQueue.enqueueNext()` |

## 元数据与数据访问

| 命名空间 | 用途 | 示例 |
| --- | --- | --- |
| [`fb.metadata`](./metadata.md) | 读写元数据 | `fb.metadata.read(path)` |
| [`fb.titleformat`](./titleformat.md) | Titleformat 求值 | `fb.titleformat.eval()` |
| [`fb.playcount`](./playcount.md) | 播放统计 | `fb.playcount.get(path)` |
| [`fb.rating`](./rating.md) | 曲目评分 | `fb.rating.set(path, 5)` |
| [`fb.selection`](./selection.md) | 全局选择同步 | `fb.selection.get()` |
| [`fb.file`](./file.md) | 文件读写 | `fb.file.read(path)` |
| [`fb.http`](./http.md) | HTTP 请求 | `fb.http.get(url)` |
| [`fb.clipboard`](./clipboard.md) | 剪贴板 | `fb.clipboard.read()` |
| [`fb.dialog`](./dialog.md) | 原生对话框 | `fb.dialog.openFile()` |

## 跨窗口与桌面集成

| 命名空间 | 用途 | 示例 |
| --- | --- | --- |
| [`fb.port`](./port.md) | 命名的跨窗口端口 | `fb.port.connect('main')` |
| [`fb.event`](./event.md) | 跨窗口自定义事件 | `fb.event.emit('refresh')` |
| [`fb.sharedState`](./shared-state.md) | 跨窗口共享状态 | `fb.sharedState.get('theme')` |
| [`fb.cursor`](./cursor.md) | 各窗口的光标可见性 | `fb.cursor.setHidden(true)` |
| [`fb.taskbar`](./taskbar.md) | 任务栏按钮与进度 | `fb.taskbar.setProgress(50)` |
| [`fb.tray`](./tray.md) | 系统托盘图标与菜单 | `fb.tray.showBalloon(opts)` |
| [`fb.keyboard`](./keyboard.md) | 全局热键 | `fb.keyboard.registerHotkey()` |
| [`fb.discovery`](./discovery.md) | 服务与 API 发现 | `fb.discovery.getAllServices()` |
| [`fb.shell`](./shell.md) | 操作系统集成 | `fb.shell.openExternal(url)` |

## 工具与宿主服务

| 命名空间 | 用途 | 示例 |
| --- | --- | --- |
| [`fb.system`](./system.md) | API、插件、语言、DPI 与主题信息 | `fb.system.listApis()` |
| [`fb.utils`](./utils.md) | 通用工具方法 | `fb.utils.formatTitle()` |
| [`fb.menu`](./menu.md) | 主菜单与右键菜单命令 | `fb.menu.runMainMenuCommand()` |
| [`fb.console`](./console.md) | 输出到宿主控制台 | `fb.console.log(msg)` |
| [`fb.log`](./log.md) | 日志文件 | `fb.log.write(msg)` |
| [`fb.lyrics`](./lyrics.md) | 歌词 | `fb.lyrics.get()` |
| [`fb.notification`](./notification.md) | Toast 与通知 | `fb.notification.showToast()` |
| [`fb.panel`](./panel.md) | 面板配置 | `fb.panel.getConfig()` |
| [`fb.webview`](./webview.md) | 页面从哪里加载 | `fb.webview.getSource()` |
| [`fb.misc`](./misc.md) | 其他宿主操作 | `fb.misc.restart()` |
| [`fb.dnd`](./dnd.md) | 外部文件拖入 | `fb.dnd.getPathsAsync()` |

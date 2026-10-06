# API 概述

页面经 `fb2k.invoke('namespace.method', params)` 调用的全部方法，按命名空间列出。事件见[事件 API](./events.md)。SDK 把这些方法封装为 `fb.*`，见 [SDK 命名空间](../sdk/namespaces.md)。

<!-- api-schema:begin index:namespaces -->
共 446 个方法，分属 41 个命名空间。

| 命名空间 | 方法数 | 所在页面 |
| --- | ---: | --- |
| `artwork` | 13 | [Artwork API](./artwork.md) |
| `audio` | 17 | [Audio 音频 API](./audio.md) |
| `clipboard` | 4 | [Clipboard 剪贴板 API](./clipboard.md) |
| `config` | 29 | [Config API](./config.md) |
| `console` | 3 | [Console 控制台 API](./console.md) |
| `cursor` | 2 | [Cursor API](./cursor.md) |
| `dialog` | 4 | [Dialog 对话框 API](./dialog.md) |
| `discovery` | 15 | [Discovery 服务发现](./discovery.md) |
| `dnd` | 4 | [DnD 拖放 API](./dnd.md) |
| `dsp` | 8 | [DSP 音效处理 API](./dsp.md) |
| `event` | 2 | [Event 自定义事件 API](./event.md) |
| `file` | 14 | [File 文件 API](./file.md) |
| `http` | 8 | [HTTP API](./http.md) |
| `jitQueue` | 8 | [JIT Queue 即时队列 API](./jit-queue.md) |
| `keyboard` | 4 | [Keyboard 键盘 API](./keyboard.md) |
| `library` | 25 | [Library API](./library.md) |
| `log` | 3 | [Log 日志文件 API](./log.md) |
| `lyrics` | 3 | [Lyrics 歌词 API](./lyrics.md) |
| `media` | 2 | [Media 媒体 API](./media.md) |
| `menu` | 8 | [Menu 菜单 API](./menu.md) |
| `metadata` | 12 | [Metadata 元数据 API](./metadata.md) |
| `misc` | 9 | [Misc 杂项 API](./misc.md) |
| `output` | 3 | [Output 输出设备 API](./output.md) |
| `panel` | 2 | [Panel 面板 API](./panel.md) |
| `playback` | 27 | [Playback API](./playback.md) |
| `playcount` | 4 | [Playcount API](./playcount.md) |
| `playlist` | 50 | [Playlist API](./playlist.md) |
| `port` | 5 | [Port 跨窗口端口 API](./port.md) |
| `queue` | 11 | [Queue 播放队列 API](./queue.md) |
| `rating` | 2 | [Rating 评分 API](./rating.md) |
| `replaygain` | 8 | [ReplayGain 回放增益 API](./replaygain.md) |
| `selection` | 6 | [Selection 选择 API](./selection.md) |
| `shell` | 5 | [Shell 系统外壳 API](./shell.md) |
| `state` | 4 | [State 共享状态 API](./state.md) |
| `system` | 9 | [System 系统 API](./system.md) |
| `taskbar` | 5 | [Taskbar 任务栏 API](./taskbar.md) |
| `titleformat` | 5 | [Titleformat API](./titleformat.md) |
| `tray` | 15 | [Tray 托盘 API](./tray.md) |
| `ui` | 5 | [UI 界面 API](./ui.md) |
| `webview` | 1 | [WebView API](./webview.md) |
| `window` | 82 | [Window 窗口](./window.md) |
<!-- api-schema:end -->

另有 `test.echo` 与 `test.ping` 两个方法，用来测试 Bridge 本身，见[Test 测试 API](./test.md#test-api)。

## 调用约定

- 方法名用点号（`playback.play`），事件名用冒号（`playback:trackChanged`）；事件名不能拿来调用。
- 形如 `namespace.__method` 的名字属于组件自带的页面（例如托盘菜单），不是公开 API。

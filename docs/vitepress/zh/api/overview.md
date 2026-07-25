# API 概述

本节记录组件当前公开注册的 Bridge API。

当前运行时库存：**428** 个公开方法，覆盖 **40** 个命名空间；已排除 **6** 个内部 `*.__*` 端点。

## 公开命名空间

- `artwork`（13 个方法）
- `audio`（14 个方法）
- `clipboard`（4 个方法）
- `config`（29 个方法）
- `console`（3 个方法）
- `cursor`（2 个方法）
- `dialog`（4 个方法）
- `discovery`（15 个方法）
- `dnd`（4 个方法）
- `dsp`（8 个方法）
- `event`（2 个方法）
- `file`（10 个方法）
- `http`（8 个方法）
- `jitQueue`（8 个方法）
- `keyboard`（4 个方法）
- `library`（25 个方法）
- `log`（3 个方法）
- `lyrics`（3 个方法）
- `menu`（8 个方法）
- `metadata`（10 个方法）
- `misc`（9 个方法）
- `output`（3 个方法）
- `panel`（2 个方法）
- `playback`（27 个方法）
- `playcount`（4 个方法）
- `playlist`（47 个方法）
- `port`（5 个方法）
- `queue`（8 个方法）
- `rating`（2 个方法）
- `replaygain`（8 个方法）
- `selection`（6 个方法）
- `shell`（5 个方法）
- `state`（4 个方法）
- `system`（9 个方法）
- `taskbar`（5 个方法）
- `test`（2 个方法）
- `titleformat`（5 个方法）
- `tray`（14 个方法）
- `ui`（5 个方法）
- `window`（81 个方法）

## 调用约定

- 调用方法：`fb2k.invoke('namespace.method', params)`
- 监听事件：`fb2k.on('namespace:eventName', handler)`
- 高层 SDK 可以提供 `fb.player.play()` 一类的辅助方法；需要精确的 Bridge 请求或返回结构时，请使用对应的命名空间页面。
- 例如，`playback:trackChanged` 是事件订阅名，不是可调用的方法。
- 符合 `*.__*` 的内部端点不属于公开 API 表面。
- 每个 family 页面通过 page map 负责一个或多个命名空间。

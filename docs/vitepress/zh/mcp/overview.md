# MCP Server 概述

通过 [Model Context Protocol (MCP)](https://modelcontextprotocol.io/) 让 AI 智能体直接操控 foobar2000。服务器默认注册 **11 个工具**：10 个 Bridge 工具和 `fb2k_page_inspect`。将 `FB2K_ENABLE_EVAL` 设为 `1` 或 `true` 后还会注册 `fb2k_page_evaluate`；设置 `FB2K_READ_ONLY` 后只剩 5 个只读工具。

## 什么是 MCP

MCP（Model Context Protocol）是一种开放协议，允许 AI 客户端（如 VS Code、Claude Desktop、Cursor）通过标准化接口调用外部工具。foo-ui-webview2-mcp 把 foobar2000 Bridge API 中受控的一部分暴露为 MCP 工具，让 AI 能控制播放、管理播放列表、检索媒体库、读写曲目标签与封面，以及检查页面。

## 架构

```text
AI 智能体 (VS Code / Claude Desktop / Cursor)
    │  MCP (stdio, JSON-RPC)
    ▼
foo-ui-webview2-mcp (Node.js)
    │  CDP (localhost:9222)
    ▼
WebView2 (foobar2000 内运行)
    │  window.fb2k.invoke('namespace.method', params)
    ▼
C++ BridgeCore → foobar2000 SDK
```

### 数据流

1. AI 智能体带着 action 调用工具，如 `fb2k_playback_control { "action": "playback.play" }`。
2. 服务器按 `playback.play` 的声明检查其余参数，并填入声明的默认值。
3. 通过 CDP 在 WebView2 中执行 `window.fb2k.invoke('playback.play', {})`。
4. C++ 后端处理请求并返回 JSON 结果。
5. MCP Server 以紧凑 JSON 把结果返回给客户端；`artwork.getCurrent` 与 `artwork.getForTrack` 的封面以图片随结果返回。

### 关键设计

| 特性 | 说明 |
| --- | --- |
| 预连接 | stdio 服务启动后会尝试连接 CDP；若失败，则在首次工具调用时重试。 |
| 截图预热 | 每次成功连接 CDP 后，都会尽力执行一次 1×1 PNG 截图，再进行真实截图；不承诺固定耗时。 |
| 自动重连 | 初次尝试后，断开的客户端会按 1 秒、2 秒和 4 秒的延迟重试。 |
| 控制台记录 | 从第一次连接起，服务器收集页面的控制台消息与未捕获异常，页面此前留存的消息也会一并送来，保留最近 200 条。 |
| 频率限制 | 一次最多连续 40 次工具调用，之后每秒 20 次。 |
| 结构化日志 | JSON Lines 格式输出到 stderr，不干扰 MCP 协议通信。 |

## 工具

| 工具 | 性质 | 范围 |
| --- | --- | --- |
| `fb2k_playback_read` | 只读 | 播放状态、当前曲目、位置、音量、播放顺序与播放队列 |
| `fb2k_playback_control` | 改状态 | 播放控制、定位、音量、静音、播放顺序，以及播放文件或播放列表的某一行 |
| `fb2k_playlist_read` | 只读 | 播放列表、其曲目、选中、焦点、锁与自动播放列表状态 |
| `fb2k_playlist_manage` | 破坏性 | 新建、删除、改名、复制与重排播放列表；自动播放列表 |
| `fb2k_playlist_edit` | 破坏性 | 添加、删除、移动、排序与替换播放列表里的曲目；撤销与重做 |
| `fb2k_playlist_select` | 改状态 | 活动播放列表、选中与焦点 |
| `fb2k_library_read` | 只读 | 媒体库搜索、专辑、艺术家与统计 |
| `fb2k_queue_edit` | 破坏性 | 播放队列的一切改动 |
| `fb2k_track_read` | 只读 | 曲目文件的标签、技术信息、评分与封面 |
| `fb2k_track_write` | 破坏性 | 写入标签、评分与封面 |
| `fb2k_page_inspect` | 只读 | 页面的截图、DOM 快照与控制台消息 |
| `fb2k_page_evaluate` | 破坏性，条件注册 | 在页面里执行 JavaScript，仅在设置 `FB2K_ENABLE_EVAL` 时注册 |

每个工具的全部 action 及其参数见[工具](./tools.md)页。

## 与 SDK / 底层 API 的关系

| 维度 | MCP 工具 | SDK (fb.*) | 底层 API (fb2k.invoke) |
| --- | --- | --- | --- |
| 调用者 | AI 智能体 | Web 前端 JS | Web 前端 JS |
| 传输 | stdio + CDP | WebView2 内 postMessage | WebView2 内 postMessage |
| 适用场景 | AI 自动化、测试 | UI 开发 | 精确控制 |
| 参数格式 | `action` 加该方法的参数 | JS 函数参数 | JSON 对象 |

::: tip 参数取自声明
Bridge 工具与 C++ 参数解析器、SDK 类型由同一份宿主方法声明生成，所以每个 action 接受的参数正好就是其方法接受的参数。调用之前，服务器按所选方法的声明检查全部参数，包括取值范围、枚举、数组元素类型与嵌套必填字段；方法会拒绝的参数以工具错误返回，并列出该方法接受的参数。少数 action 比方法更严，例如分页上限为 500 行，免得一次调用塞满客户端的上下文。
:::

## 使用示例

### 查询当前播放

```text
用户: 现在在放什么歌？
AI → fb2k_playback_read { action: "playback.getCurrentTrack" }
AI: 正在播放「天ノ弱」by 164 feat. GUMI，时长 4:23
```

### 搜索并播放

```text
用户: 帮我找 Mili 的歌然后播放第一首
AI → fb2k_library_read { action: "library.search", query: "artist IS Mili", limit: 20 }
AI → fb2k_playback_control { action: "playback.playPath", path: "D:\\Music\\Mili\\Redo.flac" }
AI: 已开始播放 Mili 的「Redo」
```

### 截图验证

```text
用户: 截个图看看当前界面
AI → fb2k_page_inspect { action: "screenshot", fullPage: true }
AI: [显示截图] 当前主题加载正常，播放栏在底部...
```

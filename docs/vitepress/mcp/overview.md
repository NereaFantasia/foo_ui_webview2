# MCP Server 概述 

通过 [Model Context Protocol (MCP)](https://modelcontextprotocol.io/) 让 AI 智能体直接操控 foobar2000。共 **102 个工具**（98 个 Bridge 工具 + 4 个 UI 测试工具）。

## 什么是 MCP 

MCP（Model Context Protocol）是一种开放协议，允许 AI 助手（如 Claude、GitHub Copilot）通过标准化接口调用外部工具。foo-ui-webview2-mcp 将 foobar2000 的 Bridge API 暴露为 MCP 工具，使 AI 能直接控制播放、管理播放列表、检索媒体库等。

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

1. AI 智能体发出工具调用请求（如 `fb2k_playback_play`）
2. MCP Server 将工具名解析为 Bridge 方法（`playback.play`）
3. 通过 CDP 在 WebView2 中执行 `window.fb2k.invoke('playback.play', {})`
4. C++ 后端处理请求并返回 JSON 结果
5. MCP Server 将结果返回给 AI 智能体

### 关键设计 

| 特性 | 说明 |
| --- | --- |
| 预连接 | 服务启动时立即连接 CDP，减少首次调用延迟 |
| 截图预热 | 连接时执行 1×1 像素截图预热渲染管线（~50ms），避免首次真实截图 ~8s 延迟 |
| 自动重连 | 连接断开后指数退避重连（1s → 2s → 4s，最多 3 次） |
| 安全校验 | 方法名正则验证，防止 JS 注入 |
| 结构化日志 | JSON Lines 格式输出到 stderr，不干扰 MCP 协议通信 |

## 工具命名空间 

| 命名空间 | 工具数 | 说明 |
| --- | --- | --- |
| Playback | 12 | 播放控制、状态、音量 |
| Playback Extended | 13 | 静音、播放顺序、路径播放 |
| Playlist | 7 | 播放列表基础操作 |
| Playlist Extended | 40 | 选区、排序、自动播放列表 |
| Library | 4 | 媒体库搜索与统计 |
| Artwork | 2 | 封面获取 |
| Queue | 8 | 播放队列管理 |
| Metadata | 12 | 元数据读写与评分 |
| UI Testing | 4 | 截图、DOM 快照、控制台 |

## 与 SDK / 底层 API 的关系 

| 维度 | MCP 工具 | SDK (fb.*) | 底层 API (fb2k.invoke) |
| --- | --- | --- | --- |
| 调用者 | AI 智能体 | Web 前端 JS | Web 前端 JS |
| 传输 | stdio + CDP | WebView2 内 postMessage | WebView2 内 postMessage |
| 适用场景 | AI 自动化、测试 | UI 开发 | 精确控制 |
| 参数格式 | MCP JSON Schema | JS 函数参数 | JSON 对象 |

::: tip TIP
MCP 工具底层调用的是同一套 C++ Bridge API。工具参数与 `fb2k.invoke()` 的参数完全一致。
:::

## 使用示例 

### 查询当前播放 

```text
用户: 现在在放什么歌？
AI → fb2k_playback_get_current_track
AI: 正在播放「天ノ弱」by 164 feat. GUMI，时长 4:23
```

### 搜索并播放 

```text
用户: 帮我找 Mili 的歌然后播放第一首
AI → fb2k_library_search { query: "artist IS Mili" }
AI → fb2k_playlist_play_track { index: 0 }
AI: 已开始播放 Mili 的「Redo」
```

### 截图验证 

```text
用户: 截个图看看当前界面
AI → fb2k_screenshot { fullPage: true }
AI: [显示截图] 当前主题加载正常，播放栏在底部...
```

# MCP Server 概述 

通过 [Model Context Protocol (MCP)](https://modelcontextprotocol.io/) 让 AI 智能体直接操控 foobar2000。服务器默认注册 **104 个工具**：101 个 Bridge 工具和 3 个始终启用的 UI 测试工具。将 `FB2K_ENABLE_EVAL` 设为 `1` 或 `true` 后，还会注册条件工具 `fb2k_evaluate`，合计 **105 个工具**。

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
| 预连接 | stdio 服务启动后会尝试连接 CDP；若失败，则在首次工具调用时重试。 |
| 截图预热 | 每次成功连接 CDP 后，都会尽力执行一次 1×1 PNG 截图，再进行真实截图；不承诺固定耗时。 |
| 自动重连 | 初次尝试后，断开的客户端会按 1 秒、2 秒和 4 秒的延迟重试。 |
| 方法校验 | Bridge 方法 ID 必须匹配 `namespace.method` 模式，才能插入待求值表达式。 |
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
| UI Testing | 默认 3 个 + 条件启用 1 个 | 截图、DOM 快照、控制台和可选表达式求值 |

## 与 SDK / 底层 API 的关系 

| 维度 | MCP 工具 | SDK (fb.*) | 底层 API (fb2k.invoke) |
| --- | --- | --- | --- |
| 调用者 | AI 智能体 | Web 前端 JS | Web 前端 JS |
| 传输 | stdio + CDP | WebView2 内 postMessage | WebView2 内 postMessage |
| 适用场景 | AI 自动化、测试 | UI 开发 | 精确控制 |
| 参数格式 | MCP JSON Schema | JS 函数参数 | JSON 对象 |

::: tip 受控参数映射
每个 Bridge 工具都有明确的工具名到方法名映射。通用注册路径会先校验必填字段、字符串枚举、已声明的默认值、基础 `string` / `number` / `integer` / `boolean` 类型以及数组外层结构，再把字段传给对应的 `fb2k.invoke()` 方法。页面中来自 `ToolDefinition` 元数据的数值 `minimum` / `maximum` 与数组元素类型描述的是预期契约，但当前通用 Zod 转换不会应用这两类约束；映射后的 Bridge handler 还可能继续校验。这是 Bridge 契约的受控子集与映射，并不保证每个底层参数或类型转换都原样暴露。
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

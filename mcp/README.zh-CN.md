[English](./README.md) | 中文

# foo-ui-webview2-mcp

> [foo_ui_webview2](https://github.com/NereaFantasia/foo_ui_webview2) 的 MCP 服务器 —— 让 AI 智能体直接操控 foobar2000 WebView2 环境。

[![MCP](https://img.shields.io/badge/MCP-Compatible-blue)](https://modelcontextprotocol.io/)
[![Node.js](https://img.shields.io/badge/Node.js-18%2B-green)](https://nodejs.org/)

---

## 概述

`foo-ui-webview2-mcp` 通过 [Chrome DevTools Protocol (CDP)](https://chromedevtools.github.io/devtools-protocol/) 连接运行在 foobar2000 宿主内的 WebView2 实例，将 bridge API 暴露为标准 [Model Context Protocol (MCP)](https://modelcontextprotocol.io/) 工具。

```
AI 智能体 (VS Code / Claude Desktop / Cursor)
    |  MCP (stdio)
    v
foo-ui-webview2-mcp (Node.js)
    |  CDP (localhost:9222)
    v
WebView2 (运行于 fb2k 内)
    |  window.fb2k.invoke()
    v
C++ BridgeCore -> foobar2000 SDK
```

---

## 先决条件

1. **Node.js 18+**
2. **foobar2000** 已安装并运行 `foo_ui_webview2` 组件
3. **CDP 远程调试已启用** —— 在 foobar2000 高级设置中开启 DevTools / CDP（默认端口 `9222`）

---

## 安装

### 方式一：npx 直接运行（推荐）

无需安装，配置 MCP 客户端即可使用：

```json
{
  "foo-ui-webview2": {
    "command": "npx",
    "args": ["-y", "foo-ui-webview2-mcp"],
    "env": {
      "FB2K_CDP_PORT": "9222",
      "FB2K_CDP_HOST": "localhost"
    },
    "type": "stdio"
  }
}
```

### 方式二：全局安装

```bash
npm install -g foo-ui-webview2-mcp
foo-ui-webview2-mcp
```

### 方式三：本地开发

```bash
cd mcp/
npm install
npm run build
npm start
```

---

## 配置

### 环境变量

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `FB2K_CDP_PORT` | `9222` | WebView2 CDP 调试端口 |
| `FB2K_CDP_HOST` | `localhost` | WebView2 CDP 主机地址 |

### VS Code (GitHub Copilot)

在 `.vscode/mcp.json` 中添加：

```json
{
  "servers": {
    "foo-ui-webview2": {
      "command": "npx",
      "args": ["-y", "foo-ui-webview2-mcp"],
      "env": { "FB2K_CDP_PORT": "9222" },
      "type": "stdio"
    }
  }
}
```

### Claude Desktop

在 `claude_desktop_config.json` 中添加：

```json
{
  "mcpServers": {
    "foo-ui-webview2": {
      "command": "npx",
      "args": ["-y", "foo-ui-webview2-mcp"],
      "env": { "FB2K_CDP_PORT": "9222" }
    }
  }
}
```

配置文件位置：

- **Windows**：`%APPDATA%\Claude\claude_desktop_config.json`
- **macOS**：`~/Library/Application Support/Claude/claude_desktop_config.json`

### Cursor

在 Cursor 设置的 **MCP Servers** 部分添加同样的配置。

---

## 工具清单

**98 个 Bridge 工具 + 4 个 UI 测试工具**，分为 8 个 Bridge 命名空间。

### Playback 播放控制（12）

| 工具 | 参数 | 说明 |
|------|------|------|
| `fb2k_playback_play` | — | 开始播放 |
| `fb2k_playback_pause` | — | 暂停播放 |
| `fb2k_playback_stop` | — | 停止播放 |
| `fb2k_playback_next` | — | 下一首 |
| `fb2k_playback_previous` | — | 上一首 |
| `fb2k_playback_play_pause` | — | 切换播放 / 暂停 |
| `fb2k_playback_get_state` | — | 获取播放状态（`stopped` / `paused` / `playing`） |
| `fb2k_playback_get_current_track` | — | 获取当前曲目信息（标题、艺术家、专辑、时长等） |
| `fb2k_playback_get_position` | — | 获取播放位置和总时长（秒） |
| `fb2k_playback_set_position` | `seconds` | 跳转到指定位置 |
| `fb2k_playback_get_volume` | — | 获取音量（0-100）和静音状态 |
| `fb2k_playback_set_volume` | `volume`（0-100） | 设置音量 |

### Playlist 播放列表（7）

| 工具 | 参数 | 说明 |
|------|------|------|
| `fb2k_playlist_get_all` | — | 获取所有播放列表 |
| `fb2k_playlist_get_active` | — | 获取当前活动播放列表 |
| `fb2k_playlist_set_active` | `playlist`（索引） | 切换活动播放列表 |
| `fb2k_playlist_get_tracks` | `playlist?`, `start?`, `count?` | 获取曲目列表（支持分页） |
| `fb2k_playlist_play_track` | `index`, `playlist?`, `deferred?` | 播放指定曲目 |
| `fb2k_playlist_create` | `name` | 创建新播放列表 |
| `fb2k_playlist_remove` | `playlist`（索引） | 删除播放列表 |

### Library 媒体库（4）

| 工具 | 参数 | 说明 |
|------|------|------|
| `fb2k_library_search` | `query`, `offset?`, `limit?` | 搜索媒体库（支持 fb2k 查询语法） |
| `fb2k_library_get_albums` | — | 获取所有专辑 |
| `fb2k_library_get_artists` | — | 获取所有艺术家 |
| `fb2k_library_get_stats` | — | 获取媒体库统计信息 |

### Playback 播放扩展（13）

| 工具 | 参数 | 说明 |
|------|------|------|
| `fb2k_playback_mute` | `muted?` | 设置静音（默认 `true`） |
| `fb2k_playback_toggle_mute` | — | 切换静音 |
| `fb2k_playback_volume_up` | — | 音量增加一档 |
| `fb2k_playback_volume_down` | — | 音量减少一档 |
| `fb2k_playback_get_playback_order` | — | 获取播放顺序模式 |
| `fb2k_playback_set_playback_order` | `order` | 设置播放顺序模式 |
| `fb2k_playback_get_stop_after_current` | — | 获取“当前曲目后停止”状态 |
| `fb2k_playback_set_stop_after_current` | `enabled` | 设置“当前曲目后停止” |
| `fb2k_playback_toggle_stop_after_current` | — | 切换“当前曲目后停止” |
| `fb2k_playback_get_current_track_index` | `includeTrackInfo?` | 获取当前播放曲目索引 |
| `fb2k_playback_get_playing_playlist` | — | 获取正在播放的播放列表 |
| `fb2k_playback_play_path` | `path` | 播放指定文件路径 |
| `fb2k_playback_random` | — | 随机播放 |

### Playlist 播放列表扩展（40）

<details><summary>展开查看全部 40 个工具</summary>

| 工具 | 参数 | 说明 |
|------|------|------|
| `fb2k_playlist_get_count` | — | 获取播放列表总数 |
| `fb2k_playlist_get_playing` | — | 获取正在播放的播放列表信息 |
| `fb2k_playlist_get_track_count` | `playlist?` | 获取播放列表曲目数 |
| `fb2k_playlist_get_available_columns` | — | 获取可用列定义 |
| `fb2k_playlist_rename` | `playlist`, `name` | 重命名播放列表 |
| `fb2k_playlist_clear` | `playlist?` | 清空播放列表 |
| `fb2k_playlist_duplicate` | `playlist?`, `name?` | 复制播放列表 |
| `fb2k_playlist_reorder_playlists` | `newOrder` | 重排播放列表顺序 |
| `fb2k_playlist_insert_tracks` | `handles`, `playlist?`, `position?` | 插入曲目 |
| `fb2k_playlist_remove_tracks` | `items`, `playlist?` | 按索引移除曲目 |
| `fb2k_playlist_remove_selected_tracks` | `playlist?` | 移除选中曲目 |
| `fb2k_playlist_move_tracks` | `delta`, `playlist?`, `items?` | 移动曲目位置 |
| `fb2k_playlist_reorder` | `newOrder`, `playlist?` | 自定义曲目排序 |
| `fb2k_playlist_reverse` | `playlist?` | 反转曲目顺序 |
| `fb2k_playlist_sort` | `playlist?`, `pattern?`, `descending?`, `selectedOnly?` | 按 title-format 排序 |
| `fb2k_playlist_shuffle` | `playlist?` | 随机打乱顺序 |
| `fb2k_playlist_add_paths` | `paths`, `playlist?` | 按文件路径添加曲目 |
| `fb2k_playlist_add_handles` | `handles`, `playlist?` | 按句柄添加曲目 |
| `fb2k_playlist_add_paths_sequential` | `paths`, `playlist?` | 顺序添加文件路径 |
| `fb2k_playlist_add_paths_async` | `paths`, `playlist?` | 异步添加文件路径 |
| `fb2k_playlist_replace_all_and_play` | `paths`, `playlist?`, `playIndex?`, `stopFirst?`, `autoPlay?` | 原子替换并播放 |
| `fb2k_playlist_get_selected_tracks` | `playlist?` | 获取选中曲目信息 |
| `fb2k_playlist_get_selection` | `playlist?` | 获取选中曲目索引 |
| `fb2k_playlist_set_selection` | `indices`, `playlist?`, `clearOthers?` | 设置选中项 |
| `fb2k_playlist_select_all` | `playlist?` | 全选 |
| `fb2k_playlist_deselect_all` | `playlist?` | 取消全选 |
| `fb2k_playlist_get_focused_track` | `playlist?` | 获取焦点曲目索引 |
| `fb2k_playlist_set_focused_track` | `index`, `playlist?` | 设置焦点曲目 |
| `fb2k_playlist_focus_track` | `playlist?`, `index?` | 已弃用 |
| `fb2k_playlist_get_focus_track` | `playlist?` | 已弃用 |
| `fb2k_playlist_undo` | `playlist?` | 撤销 |
| `fb2k_playlist_redo` | `playlist?` | 重做 |
| `fb2k_playlist_get_lock_info` | `playlist?` | 获取锁定信息 |
| `fb2k_playlist_is_locked` | `playlist?` | 检查是否锁定 |
| `fb2k_playlist_is_autoplaylist` | `playlist?` | 检查是否自动播放列表 |
| `fb2k_playlist_create_autoplaylist` | `query`, `name?`, `sort?`, `keepSorted?` | 创建自动播放列表 |
| `fb2k_playlist_convert_to_autoplaylist` | `query`, `playlist?`, `sort?`, `keepSorted?` | 转换为自动播放列表 |
| `fb2k_playlist_remove_autoplaylist` | `playlist?` | 移除自动播放列表状态 |
| `fb2k_playlist_get_autoplaylist_info` | `playlist?` | 获取自动播放列表详情 |
| `fb2k_playlist_get_autoplaylist_query` | `playlist?` | 获取自动播放列表查询 |

</details>

### Queue 播放队列（8）

| 工具 | 参数 | 说明 |
|------|------|------|
| `fb2k_queue_get` | — | 获取队列内容 |
| `fb2k_queue_add` | `playlist?`, `tracks?`, `track?` | 添加曲目到队列 |
| `fb2k_queue_add_paths` | `paths`, `useQueuePlaylist?`, `playlist?` | 按文件路径添加到队列 |
| `fb2k_queue_remove` | `index?`, `indices?` | 移除队列中指定位置的曲目 |
| `fb2k_queue_clear` | — | 清空队列 |
| `fb2k_queue_get_count` | — | 获取队列曲目数 |
| `fb2k_queue_move_to_top` | `index` | 将曲目移至队列顶部 |
| `fb2k_queue_flush` | — | 刷新队列 |

### Metadata 元数据 + 评分（12）

| 工具 | 参数 | 说明 |
|------|------|------|
| `fb2k_metadata_read` | `path` | 读取文件元数据（结构化：tags + info 分离） |
| `fb2k_metadata_read_by_path` | `path` | 按路径读取元数据（扁平格式） |
| `fb2k_metadata_read_raw` | `path`, `cueIndex?` | 直接从文件读取原始元数据 |
| `fb2k_metadata_read_batch` | `paths` | 批量读取元数据 |
| `fb2k_metadata_write` | `path`, `tags` | 写入元数据标签 |
| `fb2k_metadata_write_batch` | `items` | 批量写入元数据 |
| `fb2k_metadata_embed_artwork` | `path`, `imageData`, `type?` | 嵌入封面图片 |
| `fb2k_metadata_remove_embedded_art` | `path`, `type?`, `removeAll?` | 移除嵌入封面 |
| `fb2k_metadata_remove_tag` | `path`, `tags` | 移除指定标签 |
| `fb2k_metadata_remove_field` | `path`, `tags` | 移除指定字段 |
| `fb2k_rating_set` | `rating`, `path?`, `cueIndex?` | 设置评分 |
| `fb2k_rating_get` | `path`, `cueIndex?` | 获取评分 |

### Artwork 封面（2）

| 工具 | 参数 | 说明 |
|------|------|------|
| `fb2k_artwork_get_current` | `type?` | 获取当前曲目封面（base64） |
| `fb2k_artwork_get_for_track` | `path`, `type?` | 获取指定曲目封面 |

封面类型：`front`、`back`、`disc`、`icon`、`artist`。

### UI Testing UI 测试（4）

| 工具 | 参数 | 说明 |
|------|------|------|
| `fb2k_screenshot` | `fullPage?` | 截取页面截图（返回 PNG） |
| `fb2k_dom_snapshot` | — | 获取 DOM 可访问性快照 |
| `fb2k_evaluate` | `expression` | 执行 JavaScript 表达式（需 `FB2K_ENABLE_EVAL=1`） |
| `fb2k_console_messages` | — | 获取控制台日志 |

---

## 使用示例

### 示例 1：查询当前播放状态

```
用户: 现在在放什么歌？
AI -> fb2k_playback_get_current_track
AI: 正在播放 Mili 的「Redo」，来自专辑「Millennium Mother」，时长 3:53。
```

### 示例 2：搜索并播放

```
用户: 帮我找 Mili 的歌然后播放第一首
AI -> fb2k_library_search { query: "artist IS Mili" }
AI -> fb2k_playlist_play_track { index: 0 }
AI: 已开始播放 Mili 的「Redo」。
```

### 示例 3：截图验证主题

```
用户: 截个图看看当前界面
AI -> fb2k_screenshot { fullPage: true }
AI: [显示截图] 当前主题加载正常，播放栏在底部……
```

---

## 开发

```bash
cd mcp/

# 安装依赖
npm install

# 编译 TypeScript
npm run build

# 监听模式开发
npm run dev

# 运行测试（离线，无需 foobar2000）
npm test

# 端到端冒烟测试（需要 foobar2000 运行且 CDP 已启用）
node tests/e2e-smoke.mjs
```

### 项目结构

```
mcp/
├── src/
│   ├── index.ts              # MCP Server 入口
│   ├── cdp-client.ts         # CDP 连接管理（自动发现、重连、超时）
│   ├── bridge-executor.ts    # fb2k.invoke() 封装
│   ├── types.ts              # 共享类型
│   └── tools/
│       ├── playback.ts       # 12 个播放控制工具
│       ├── playback-ext.ts   # 13 个播放扩展工具
│       ├── playlist.ts       # 7 个播放列表工具
│       ├── playlist-ext.ts   # 40 个播放列表扩展工具
│       ├── library.ts        # 4 个媒体库工具
│       ├── artwork.ts        # 2 个封面工具
│       ├── queue.ts          # 8 个队列工具
│       └── metadata.ts       # 12 个元数据 + 评分工具
├── tests/
│   ├── api-alignment.test.ts      # C++ 源码交叉验证门禁
│   ├── api-existence-gate.test.ts # API 白名单防幻觉门禁
│   ├── bridge-executor.test.ts    # BridgeExecutor 单元测试
│   ├── cdp-client.test.ts         # CdpClient 单元测试
│   ├── error-paths.test.ts        # 错误路径覆盖
│   ├── tools-integration.test.ts  # 工具集成测试
│   ├── e2e-smoke.mjs              # CDP 端到端冒烟测试
│   └── e2e-interact.mjs          # CDP 交互测试
├── dist/                     # 编译输出
├── package.json
└── tsconfig.json
```

### 测试

| 测试类型 | 命令 | 需要 foobar2000 |
|----------|------|-----------------|
| 全部离线测试 | `npm test` | 否 |
| E2E 冒烟 | `node tests/e2e-smoke.mjs` | 是 |
| E2E 交互 | `node tests/e2e-interact.mjs` | 是 |

---

## 连接说明

### CDP 端口配置

MCP Server 通过 CDP 连接 foobar2000 内的 WebView2。确保：

1. foobar2000 组件的 CDP 远程调试已启用（高级设置中开启）。
2. CDP 端口（默认 `9222`）未被其他程序占用。
3. 如使用非默认端口，通过 `FB2K_CDP_PORT` 环境变量指定。

### 连接失败处理

- **foobar2000 未运行** —— 工具调用返回 `"WebView2 not available at port 9222"`。
- **CDP 未启用** —— 同上。
- **连接中断** —— 自动重连（最多 3 次，间隔递增 1s -> 2s -> 4s）。
- **调用超时** —— 30 秒超时后返回错误。

---

## 与 Chrome DevTools MCP 配合

本项目推荐搭配 [chrome-devtools-mcp](https://github.com/ChromeDevTools/chrome-devtools-mcp) 使用，两者职责互补：

| MCP Server | 职责 | 适用场景 |
|------------|------|----------|
| **foo-ui-webview2-mcp** | 语义化 Bridge API | 播放控制、播放列表管理、媒体库检索 |
| **chrome-devtools-mcp** | 通用 DOM 交互 | 点击按钮、填写输入框、截图对比 |

两者均连接 `localhost:9222` 的同一个 WebView2 实例。

---

## 路线图

- [x] 基础设施（CDP 连接、MCP 框架、核心工具集）
- [x] 扩展工具集（播放扩展 / 播放列表扩展 / 队列 / 元数据 + 评分 —— 共 98 个 Bridge 工具）
- [ ] 更多扩展（window / config / audio / columns）
- [ ] 事件订阅（MCP Resource 机制）
- [ ] 更完整的覆盖与 CI

详见项目仓库 [NereaFantasia/foo_ui_webview2](https://github.com/NereaFantasia/foo_ui_webview2)。

---

## 许可

以 [MIT 许可证](./LICENSE) 发布，与 `foo_ui_webview2` 的 SDK / MCP 子包许可保持一致。

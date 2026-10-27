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
3. **CDP 远程调试已启用** —— 在 foobar2000 的 `Preferences → Display → WebView2 UI → 开发者` 页开启（重启后生效）。端口在同一页设置（默认 `9222`）；改了端口，MCP Server 的 `FB2K_CDP_PORT` 要设成同一个值

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
| `FB2K_CDP_TARGET_URL` | — | 同一端口上有多个 WebView2 页面时，要连接的页面 URL 的一部分 |
| `FB2K_READ_ONLY` | 关 | 设为 `1` 或 `true` 时只注册只读工具 |
| `FB2K_ENABLE_EVAL` | 关 | 设为 `1` 或 `true` 时注册 `fb2k_page_evaluate`，它能在页面里执行任意 JavaScript |
| `FB2K_MAX_RESPONSE_CHARS` | `100000` | 文本结果的最大字符数；超出部分被截断并附说明 |
| `FB2K_MAX_IMAGE_BYTES` | `3932160` | 以图片返回的封面最多多少字节（3.75 MiB）；更大的封面不附上，结果里会说明 |

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

每个 Bridge 工具对应一个命名空间，或其中同一类改动。调用时用 `action` 指定要调用的宿主方法，该方法的参数与它并列传入：

```json
{ "action": "playlist.getTracks", "playlistGuid": "{…}", "count": 50 }
```

工具的注解对其中每个 action 都成立，所以只读工具不会改动任何东西。调用到达 foobar2000 之前，服务器按所选方法自己的声明检查参数；方法会拒绝的情况以工具错误返回，说明问题并列出该方法接受的参数。返回值就是该方法经 `fb2k.invoke()` 返回的内容，以紧凑 JSON 给出。`artwork.getCurrent` 与 `artwork.getForTrack` 例外：封面以图片返回，其余字段仍是 JSON。

<!-- mcp-tools:begin -->
**10 个 Bridge 工具**，覆盖 93 个宿主方法，按命名空间与是否改动东西分组；页面工具见下文。每个工具接收 `action`（要调用的宿主方法），该方法的参数与它并列传入。带 `?` 的参数可以省略；参数的类型、取值范围与默认值见各工具的输入 schema。

### `fb2k_playback_read`

只读 · 9 个 action

读取 foobar2000 的播放状态：播放、暂停或停止，当前曲目，位置，音量，播放顺序，播完当前曲目后是否停止，正在播放的曲目所在的位置，以及播放队列。不改动任何东西。

| Action | 参数 | 说明 |
|--------|--------|-------------|
| `playback.getState` | — | 报告播放状态以及当前曲目允许的操作。 |
| `playback.getCurrentTrack` | — | 报告当前载入（播放中或暂停）的曲目。 |
| `playback.getPosition` | — | 报告播放位置，以及所在曲目的时长与身份。 |
| `playback.getVolume` | — | 以宿主用的两种刻度报告输出音量，以及是否静音。 |
| `playback.getPlaybackOrder` | — | 报告当前播放顺序，同时给出序号与名称。 |
| `playback.getStopAfterCurrent` | — | 报告是否在当前曲目结束后停止。 |
| `playback.getCurrentTrackIndex` | `includeTrackInfo?` | 定位当前播放曲目在所在播放列表中的索引位置。 |
| `playback.getPlayingPlaylist` | — | 报告最近一次起播所在的播放列表。 |
| `queue.get` | — | 按播放顺序读取完整待播队列；不分页一次性返回全量条目 |

### `fb2k_playback_control`

改状态 · 18 个 action

控制播放：开始、暂停、停止、切歌、定位，设置或增减音量，静音，选择播放顺序，播完当前曲目后停止，以及播放指定的文件或播放列表里的某一行。

| Action | 参数 | 说明 |
|--------|--------|-------------|
| `playback.play` | — | 开始播放；暂停中则继续。 |
| `playback.pause` | — | 暂停播放；停止状态下什么也不做。 |
| `playback.stop` | — | 停止播放。 |
| `playback.next` | — | 按当前播放顺序跳到下一首。 |
| `playback.previous` | — | 按当前播放顺序跳到上一首。 |
| `playback.playPause` | — | 在播放与暂停之间切换；停止状态下开始播放。 |
| `playback.random` | — | 从活动播放列表里随机起播一首。 |
| `playback.playPath` | `path` | 追加单个文件至活动播放列表并立即播放；无活动列表时自动创建。 |
| `playlist.playTrack` | `playlist?`, `playlistGuid?`, `index`, `deferred?`, `muted?`, `expectedVersion?` | 播放播放列表中的一行，与双击它相同。 |
| `playback.setPosition` | `position` | 在当前曲目内定位，播放中或暂停中均可。 |
| `playback.setVolume` | `volume` | 按百分比设置输出音量；超出 `0` 到 `100` 的值被夹到范围内。 |
| `playback.volumeUp` | — | 按宿主的步长调高音量：一分贝，落在整数分贝上。 |
| `playback.volumeDown` | — | 按宿主的步长调低音量：一分贝，落在整数分贝上。 |
| `playback.mute` | `muted?` | 静音或取消静音；要求的状态已经生效时什么也不做。 |
| `playback.toggleMute` | — | 翻转静音状态并报告新状态。 |
| `playback.setPlaybackOrder` | `order?`, `name?` | 按序号或名称选择播放顺序；两者必须恰好给一个。 |
| `playback.setStopAfterCurrent` | `enabled` | 设置或取消在当前曲目结束后停止。 |
| `playback.toggleStopAfterCurrent` | — | 翻转「当前曲目后停止」并报告新值。 |

### `fb2k_playlist_read`

只读 · 13 个 action

读取播放列表：播放列表清单（每项带索引、guid、名称、曲目数，以及是否活动、正在播放、带锁或自动播放列表），某个播放列表的一页曲目或按行号挑出的行、曲目符合查询的行、选中项与焦点，锁与自动播放列表的详情，以及播放列表视图可用的列。不改动任何东西。

| Action | 参数 | 说明 |
|--------|--------|-------------|
| `playlist.getAll` | — | 按顺序列出全部播放列表。 |
| `playlist.getActive` | — | 描述活动播放列表，即用户正在看的那个。 |
| `playlist.getPlaying` | — | 描述正在播放的播放列表，即播放从中取下一首的那个。 |
| `playlist.getTracks` | `playlist?`, `playlistGuid?`, `start?`, `count?`, `formats?`, `fields?` | 播放列表的一页行。 |
| `playlist.getTracksAt` | `rows`, `playlist?`, `playlistGuid?`, `formats?`, `fields?` | 按行号挑出的播放列表行，按传入顺序，例如 `playlist.getMatchingRows` 回答的那些不一定相邻的行。 |
| `playlist.getMatchingRows` | `query`, `playlist?`, `playlistGuid?` | 播放列表里曲目符合 foobar2000 查询的行，按列表顺序；不管列表多长都只要一次调用，各行本身用 `playlist.getTracksAt` 读取。 |
| `playlist.getSelectedTracks` | `playlist?`, `playlistGuid?` | 播放列表里选中的行，按列表顺序，不带播放统计。 |
| `playlist.getSelection` | `playlist?`, `playlistGuid?` | 列出播放列表中被选中的行。 |
| `playlist.getFocusedTrack` | `playlist?`, `playlistGuid?` | 报告播放列表的焦点行。 |
| `playlist.getLockInfo` | `playlist?`, `playlistGuid?` | 报告一个播放列表是否带锁，例如自动播放列表的锁。 |
| `playlist.getAutoplaylistInfo` | `playlist?`, `playlistGuid?` | 描述播放列表的自动列表状态及原生编辑器可用性，不打开 UI。 |
| `playlist.getAutoplaylistConfig` | `playlist?`, `playlistGuid?` | 读取由本组件托管的自动播放列表配置。 |
| `playlist.getAvailableColumns` | — | 列出 foobar2000 与已装组件为默认界面播放列表视图定义的列。 |

### `fb2k_playlist_manage`

破坏性 · 10 个 action

管理播放列表本身：新建、删除、改名、复制与重排，以及把播放列表转成由媒体库查询填充的自动播放列表，或转回普通播放列表。

| Action | 参数 | 说明 |
|--------|--------|-------------|
| `playlist.create` | `name`, `position?` | 新建一个空播放列表。 |
| `playlist.duplicate` | `playlist?`, `playlistGuid?`, `name?` | 把一个播放列表连同曲目复制成新列表，放在原列表紧后面。 |
| `playlist.rename` | `playlist?`, `playlistGuid?`, `name` | 重命名一个播放列表。 |
| `playlist.remove` | `playlist` | 删除一个播放列表。 |
| `playlist.reorderPlaylists` | `newOrder?`, `newOrderGuids?` | 重排播放列表的顺序，为每个新位置给出放到那里的播放列表的当前索引（`newOrder`）或它的 GUID（`newOrderGuids`）；两者恰好给一个，否则以 `INVALID_PARAMS` 失败。 |
| `playlist.createAutoplaylist` | `name?`, `query`, `sort?`, `keepSorted?` | 新建一个由 foobar2000 按媒体库查询填充并保持更新的播放列表。 |
| `playlist.createManagedAutoplaylist` | `name?`, `query`, `sort?`, `keepSorted?` | 创建由本组件托管查询与排序配置的自动播放列表（依赖本组件持续安装）。 |
| `playlist.setAutoplaylistConfig` | `playlist?`, `playlistGuid?`, `expectedRevision`, `query?`, `sort?`, `keepSorted?` | 基于预期版本号修改托管自动播放列表的配置。 |
| `playlist.convertToAutoplaylist` | `playlist?`, `playlistGuid?`, `query`, `sort?`, `keepSorted?` | 把已有的播放列表变成自动播放列表，曲目换成查询结果。 |
| `playlist.removeAutoplaylist` | `playlist?`, `playlistGuid?` | 解除 autoplaylist manager client（包括第三方 client），保留当前曲目。 |

### `fb2k_playlist_edit`

破坏性 · 15 个 action

改动播放列表里的曲目：添加文件、文件夹或曲目，插入、删除、移动、重排、排序、打乱或倒序行，清空，整体替换后播放，以及撤销与重做。

| Action | 参数 | 说明 |
|--------|--------|-------------|
| `playlist.addPaths` | `playlist?`, `playlistGuid?`, `paths` | 把文件、文件夹或 URL 追加到播放列表，事先保存撤销点。 |
| `playlist.addPathsSequential` | `playlist?`, `playlistGuid?`, `paths` | 按传入顺序把路径追加到播放列表，事先保存撤销点；展开成多首曲目的路径（文件夹、cue）占住它自己的位置。 |
| `playlist.addHandles` | `playlist?`, `playlistGuid?`, `handles` | 把曲目追加到播放列表，事先保存撤销点。 |
| `playlist.insertTracks` | `playlist?`, `playlistGuid?`, `position?`, `handles`, `expectedVersion?` | 在播放列表的某个位置插入曲目，事先保存撤销点。 |
| `playlist.replaceAllAndPlay` | `playlist?`, `playlistGuid?`, `paths`, `playIndex?`, `stopFirst?`, `autoPlay?` | 替换播放列表的全部内容并播放：停止播放，清空列表（保存撤销点），按 `playlist.addPaths` 的方式加入路径，把列表设为活动列表，再播放或聚焦 `playIndex`。 |
| `playlist.removeTracks` | `playlist?`, `playlistGuid?`, `items`, `expectedVersion?` | 从播放列表移除行，事先保存撤销点。 |
| `playlist.removeSelectedTracks` | `playlist?`, `playlistGuid?` | 从播放列表移除选中的行，事先保存撤销点。 |
| `playlist.clear` | `playlist?`, `playlistGuid?` | 移除播放列表里的全部曲目，事先保存撤销点。 |
| `playlist.moveTracks` | `playlist?`, `playlistGuid?`, `items?`, `delta`, `expectedVersion?` | 把播放列表中选中的行移动 `delta` 个位置，事先保存撤销点。 |
| `playlist.reorder` | `playlist?`, `playlistGuid?`, `newOrder`, `expectedVersion?` | 重排播放列表的曲目，事先保存撤销点：`newOrder[i]` 是移到第 `i` 行的那首曲目的当前行号。 |
| `playlist.sort` | `playlist?`, `playlistGuid?`, `pattern?`, `descending?`, `selectedOnly?` | 按 Title Formatting 模式给播放列表排序，事先保存撤销点。 |
| `playlist.shuffle` | `playlist?`, `playlistGuid?` | 把播放列表的曲目打乱成随机顺序，事先保存撤销点。 |
| `playlist.reverse` | `playlist?`, `playlistGuid?` | 把播放列表的曲目顺序倒过来，事先保存撤销点。 |
| `playlist.undo` | `playlist?`, `playlistGuid?` | 把播放列表恢复到上一个撤销点。 |
| `playlist.redo` | `playlist?`, `playlistGuid?` | 重新应用上一次 `playlist.undo` 撤销的改动。 |

### `fb2k_playlist_select`

改状态，可重复调用 · 5 个 action

改变哪个播放列表是活动的，以及其中哪些行被选中或获得焦点。曲目本身不动。

| Action | 参数 | 说明 |
|--------|--------|-------------|
| `playlist.setActive` | `playlist?`, `playlistGuid?` | 把一个播放列表设为活动列表。 |
| `playlist.setSelection` | `playlist?`, `playlistGuid?`, `indices`, `clearOthers?`, `expectedVersion?` | 选中播放列表中的行。 |
| `playlist.selectAll` | `playlist?`, `playlistGuid?` | 选中播放列表中的全部行。 |
| `playlist.deselectAll` | `playlist?`, `playlistGuid?` | 清除播放列表中的选中。 |
| `playlist.setFocusedTrack` | `playlist?`, `playlistGuid?`, `index`, `expectedVersion?` | 把播放列表的焦点移到一行，或去掉焦点。 |

### `fb2k_library_read`

只读 · 4 个 action

读取媒体库：用 foobar2000 查询语法搜索（如 `artist IS Beatles`），列出专辑或艺术家，以及取统计数据。结果分页返回；少要几个 `fields` 可以让结果更短。不改动任何东西。

| Action | 参数 | 说明 |
|--------|--------|-------------|
| `library.search` | `query`, `offset?`, `limit?`, `fields?` | 分页检索符合 foobar2000 查询表达式的曲目，固定按媒体库原生顺序排列（支持传入 `SORT BY` 子句但不生效）；查询解析失败时以 `INVALID_PARAMS` 失败（`details.param` 为 `query`）；`query` 为空或媒体库未启用时均返回空列表；数据在后台线程序列化写出 |
| `library.getAlbums` | `sort?`, `query?`, `offset?`, `limit?`, `includeTracks?`, `useCache?` | 获取全媒体库专辑列表，按专辑名加专辑艺术家归组并跳过缺失 `album` 标签的曲目；完整查询结果（`offset: 0`、未传 `includeTracks` 且全部专辑均在 `limit` 内）将缓存至媒体库变更，后续参数一致的调用直接复用缓存；归组缓存同时共享给 `library.getAlbumTracks`，避免重复全库遍历；媒体库未启用时返回空列表 |
| `library.getArtists` | `sort?`, `limit?`, `includeAlbums?` | 获取全媒体库所有署名艺术家及其参与统计；多值 `artist` 的每个取值单独成行；全库扫描结果将缓存至媒体库变更；媒体库未启用时以 `LIBRARY_DISABLED` 失败 |
| `library.getStats` | — | 获取全媒体库汇总统计计数；每次调用均实时遍历全库，媒体库未启用时各项计数均为 `0` |

### `fb2k_queue_edit`

破坏性 · 8 个 action

改动播放队列：加入播放列表的行或文件，插播下一首，移除条目，把某一条移到最前，整体替换或清空队列，以及立即播放某一条。读取队列用 fb2k_playback_read 的 `queue.get`。

| Action | 参数 | 说明 |
|--------|--------|-------------|
| `queue.add` | `playlist?`, `playlistGuid?`, `tracks?`, `track?`, `expectedPlaylistVersion?` | 按播放列表行号将一首或多首曲目追加至待播队列；超出列表范围的索引自动忽略，无任一有效索引时以 `INVALID_INDEX` 失败，负数索引以 `INVALID_PARAMS` 失败 |
| `queue.addPaths` | `paths`, `useQueuePlaylist?`, `playlist?`, `playlistGuid?` | 按文件路径或 URL 将曲目加入待播队列；宿主会先将曲目追加至播放列表（默认专用 `[WebView Queue]` 列表，不存在则自动创建）后再入队；目标列表已锁定时以 `LOCKED` 失败且不执行追加与入队；路径解析后重新定位目标列表，若期间被删除则以 `OPERATION_FAILED` 失败；未通过媒体根目录安全检查时整次调用以 `PERMISSION_DENIED` 失败；解析失败的无效路径自动丢弃，无任一有效曲目时以 `NOT_FOUND` 失败并在 `invalidCount` 中注明 |
| `queue.insertNext` | `paths?`, `items?`, `position?`, `expectedVersion?` | 将指定曲目插入待播队列前端以便下一首播放；已存在于队列中的曲目直接前移而不重复入队；`items` 列表排在 `paths` 列表之前，各自保持输入顺序；未提供 `paths` 与 `items` 时以 `INVALID_PARAMS` 失败；在解析路径前预检所有 `items` 引用，任一项非法立即失败且不写入变更；路径未通过媒体根目录安全检查以 `PERMISSION_DENIED` 失败；无效路径自动丢弃并计入 `invalidCount`，无任一有效曲目插入时以 `NOT_FOUND` 失败；路径解析期间播放列表发生变动或队列重建失败以 `OPERATION_FAILED` 失败；在路径解析完成后写入队列前校验 `expectedVersion` |
| `queue.setContents` | `items`, `expectedVersion?` | 使用指定的有序引用列表全量替换当前待播队列；任一引用项校验失败整次调用立即终止且不写入变更；传入空列表等同清空队列；播放列表索引以调用到达时的状态为准，使用 `playlistGuid` 可稳定引用目标列表；单项同时传入 `playlist` 与 `playlistGuid` 或 GUID 格式非法时以 `INVALID_PARAMS` 失败，GUID 未匹配到列表时以 `NOT_FOUND` 失败 |
| `queue.moveToTop` | `index`, `expectedVersion?` | 将指定队列位置的条目移动至队首以便下一首播放；仅调整队列排序，保持其他条目的相对顺序；传入 `index: 0`、越界索引或队列为空时均以 `INVALID_INDEX` 失败；队列重建失败时以 `OPERATION_FAILED` 失败（附带 `queueCount`） |
| `queue.playNow` | `index?`, `expectedVersion?` | 立即起播队列中指定索引的条目；若目标不在队首则先行移至队首；队列为空时以 `NOT_FOUND` 失败，索引越界以 `INVALID_INDEX` 失败；条目移动后队列重建失败时以 `OPERATION_FAILED` 失败且不起播 |
| `queue.remove` | `index?`, `indices?`, `expectedVersion?` | 按队列索引 `index` 或索引列表 `indices` 从待播队列中移除条目；队列为空时一律以 `NOT_FOUND` 失败；单项 `index` 越界以 `INVALID_INDEX` 失败；批量移除时跳过重复或越界索引，无任一有效索引时以 `INVALID_INDEX` 失败（错误详情附带 `removedCount: 0` 与 `queueCount`）；未提供索引或传入负数时以 `INVALID_PARAMS` 失败 |
| `queue.clear` | — | 清空待播队列中的所有条目 |

### `fb2k_track_read`

只读 · 6 个 action

读取曲目文件：取自媒体库缓存或直接读文件的标签与技术信息，一次读取多个文件的标签，曲目的评分，以及封面。不改动任何东西。

| Action | 参数 | 说明 |
|--------|--------|-------------|
| `metadata.read` | `path`, `cueIndex?` | 读取单个曲目的元数据标签与音频技术信息；优先读取宿主内存缓存，若缓存缺失或未收录标题标签则回退至直接读取音频文件；文件无法打开或读取失败时以 `OPERATION_FAILED` 失败；字段值中的非法 UTF-8 字符替换为 U+FFFD，字段名包含非法 UTF-8 的项自动省略且不修改原文件 |
| `metadata.readRaw` | `path`, `cueIndex?` | 绕过宿主内存缓存，直接从物理文件读取单个曲目的元数据标签与技术信息；返回结构同 `metadata.read` 且额外附带 `source: "file"` 字段；文件读取失败以 `OPERATION_FAILED` 失败 |
| `metadata.readBatch` | `paths` | 批量读取多个曲目的扁平元数据字段，按输入路径顺序逐行返回；单项路径读取失败在对应行报告自身错误而不导致整次调用失败；在宿主主线程同步执行（大量文件推荐改用 `metadata.probeBatchAsync`）；UTF-8 字符处理规则同 `metadata.read` |
| `rating.get` | `path`, `cueIndex?` | 获取指定曲目的星级评分（范围 0 至 5；优先读取 foo_playcount 维护的 1 至 5 星 %rating% 统计数据，未找到时回退读取音频文件的 RATING 标签并夹取至 0 至 5 区间；0 表示未评分；无法读取标签或文件不存在时不返回错误，统一视作无 RATING 标签处理） |
| `artwork.getForTrack` | `path`, `type?` | 通过专辑封面管理器读取曲目封面（包含内嵌封面及所在目录的外置封面文件）并获取尺寸；从 PNG 头信息解析 `width` 与 `height`（其他格式返回 `0`）；`file-relative://` 路径以 `INVALID_PATH` 拒绝 |
| `artwork.getCurrent` | `type?` | 读取当前正在播放曲目的封面图片；按顺序尝试三种检索策略并在 `source` 中指明胜出源；未在播放时返回 `available: false`（`reason: "no_track"`），均未查得图片时返回 `reason: "not_found"` |

### `fb2k_track_write`

破坏性 · 5 个 action

改动曲目文件：给一个或多个文件写标签（值为 `null` 即删除该标签），设置评分，以及嵌入或移除封面。

| Action | 参数 | 说明 |
|--------|--------|-------------|
| `metadata.write` | `path`, `tags`, `cueIndex?` | 将针对单个曲目的标签写入请求加入后台队列并立即返回；foobar2000 宿主在后台线程执行物理文件写入，并在操作完成后向所有窗口广播 `metadata:writeComplete` 事件；请求中未包含有效待写标签时直接返回成功且不派发后台写入 |
| `metadata.writeBatch` | `items` | 批量排入多个曲目的标签写入请求，并报告成功入队的数量；已派发的单项写入在完成后独立广播 `metadata:writeComplete` 事件（不存在的文件在此阶段亦计入入队成功）；若有条目参数非法则整次调用以 `OPERATION_FAILED` 失败（错误详情附带 `successCount`、`failCount` 与 `errors` 列表，其他合法项照常入队） |
| `rating.set` | `path?`, `rating`, `cueIndex?` | 设置指定曲目的星级评分（优先通过 foo_playcount 上下文菜单写入统计，菜单不可用时回退写入音频文件的 RATING 标签；省略 path 时优先为正在播放的曲目评分，无播放曲目时回退为当前活动播放列表的选中项，两者均无时返回 NO_ACTIVE_ITEM 失败） |
| `metadata.embedArtwork` | `path`, `imageData`, `type?`, `target?`, `filename?` | 将封面图片写入音频文件内嵌标签、写为同目录下的独立图片文件或两处同时写入；指定单个目标时返回该目标的写入结果；同时指定两处目标时在 `results` 下分别列出各自状态，仅当两处写入均失败时整次调用以 `OPERATION_FAILED` 失败 |
| `metadata.removeEmbeddedArt` | `path`, `type?`, `removeAll?` | 从音频文件中移除内嵌的封面图片；支持按 `type` 删除指定类型，或在省略 `type` 及设置 `removeAll: true` 时全量清空；音频格式不受专辑封面编辑器支持时以 `NOT_SUPPORTED` 失败 |
<!-- mcp-tools:end -->

### 页面工具

下面两个工具经 CDP 操作 WebView2 页面，不走 Bridge 方法。

| 工具 | 参数 | 说明 |
|------|------|------|
| `fb2k_page_inspect` | `action`、`fullPage?`、`limit?` | 只读。`screenshot` 返回 PNG 截图（`fullPage` 截整个页面）；`domSnapshot` 返回简化的 DOM 树；`consoleMessages` 返回页面最近的控制台消息与未捕获异常，保留最近 200 条（`limit` 默认 100） |
| `fb2k_page_evaluate` | `expression` | 在页面里执行 JavaScript 表达式。只在 `FB2K_ENABLE_EVAL=1` 且不在只读模式时注册 |

### 限制

- 服务器一次最多连续接受 40 次工具调用，之后每秒 20 次；超出的调用失败，错误信息说明要等多久。
- 文本结果超过 `FB2K_MAX_RESPONSE_CHARS` 个字符时被截断，截断处附一条说明，建议分页或少要几个 `fields`。
- 封面只有是 PNG、JPEG、GIF 或 WebP，且不超过 `FB2K_MAX_IMAGE_BYTES` 字节（默认 3.75 MiB，base64 编码后正好是 Claude API 单张图片接受的 5 MiB）时才以图片返回；否则结果只带其余字段，并附一条说明，写明图片为什么没有附上。

---

## 使用示例

### 示例 1：查询当前播放状态

```
用户: 现在在放什么歌？
AI -> fb2k_playback_read { action: "playback.getCurrentTrack" }
AI: 正在播放 Mili 的「Redo」，来自专辑「Millennium Mother」，时长 3:53。
```

### 示例 2：搜索并播放

```
用户: 帮我找 Mili 的歌然后播放第一首
AI -> fb2k_library_read { action: "library.search", query: "artist IS Mili", limit: 20 }
AI -> fb2k_playback_control { action: "playback.playPath", path: "D:\\Music\\Mili\\Redo.flac" }
AI: 已开始播放 Mili 的「Redo」。
```

### 示例 3：截图验证主题

```
用户: 截个图看看当前界面
AI -> fb2k_page_inspect { action: "screenshot", fullPage: true }
AI: [显示截图] 当前主题加载正常，播放栏在底部……
```

### 错误响应

Bridge handler 可返回 `{ success: false, error, code, details }` 形式的结构化
失败。MCP Server 会将其报告为工具错误，并在文本内容中保留 host 错误消息、
稳定错误码和 details。CDP 连接失败与调用超时同样会报告为工具错误。

### 参数校验

每次调用在 bridge handler 执行前检查两遍。先过工具自己的 schema：它接受 `action`
以及任一 action 会用到的参数，哪个 action 都不用的参数直接拒绝。再过所选方法的精确
schema：必填参数、数值范围、整数与数组元素类型、字符串枚举与嵌套必填字段都会生效，
声明的默认值也在这一步填入。同一工具里属于别的 action 的参数在这一步被拒绝，错误里
列出所选方法接受的参数。metadata tag 的键名仍是开放的。`__proto__`、`prototype` 和
`constructor` 等原型敏感键会在任意参数层级、bridge 调用之前受控拒绝，不会被含糊地
规范化或继续转发。

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
│   ├── bridge-tools.ts       # Bridge 工具注册与逐 action 校验
│   ├── page-tools.ts         # fb2k_page_inspect 与 fb2k_page_evaluate
│   ├── rate-limit.ts         # 所有工具共用的调用配额
│   ├── tool-results.ts       # 文本、JSON 与错误结果，含长度上限
│   ├── guarded-stdio-transport.ts # SDK 规范化前的原始 JSON 安全防线
│   ├── tool-schema.ts        # 声明式 JSON Schema 到 Zod 校验
│   ├── types.ts              # 共享类型
│   └── generated/
│       └── bridge-tools.ts   # Bridge 工具，生成物，勿手改
├── tool-table.json           # 各工具包含哪些宿主方法，以及工具说明
├── tests/
│   ├── bridge-executor.test.ts    # BridgeExecutor 单元测试
│   ├── cdp-client.test.ts         # CdpClient 单元测试
│   ├── error-paths.test.ts        # 错误路径覆盖
│   ├── guarded-stdio-transport.test.ts # 原始 stdio JSON 安全测试
│   ├── page-tools.test.ts         # 页面工具测试
│   ├── tool-schema.test.ts        # Schema builder 与生产注册测试
│   ├── tools-integration.test.ts  # 生成工具的核对
│   ├── e2e-smoke.mjs              # CDP 端到端冒烟测试
│   └── e2e-interact.mjs          # CDP 交互测试
├── dist/                     # 编译输出
├── package.json
└── tsconfig.json
```

`src/generated/bridge-tools.ts` 与本文的 Bridge 工具表由仓库里 `src/api/schema/` 下的宿主方法声明和
`tool-table.json` 生成：参数类型、取值范围、默认值与说明取自声明，各工具的注解由其方法的 `@effect`
标签算出。增改工具时改 `tool-table.json`，再在仓库根目录执行 `node scripts/api-schema/generate.mjs --write`。

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

1. foobar2000 组件的 CDP 远程调试已启用（`Preferences → Display → WebView2 UI → 开发者`，重启后生效）。
2. CDP 端口（默认 `9222`）未被其他程序占用。
3. 开发者页上的端口不是 `9222` 时，把 `FB2K_CDP_PORT` 环境变量设成同一个值。

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
- [x] 工具由宿主声明生成，按命名空间与改动性质分组，带注解
- [ ] 按任务挑选更多命名空间（titleformat、主菜单命令、输出与 DSP、播放统计、歌词）
- [ ] 经订阅句柄等待事件（如 `playlist:addComplete`）
- [ ] 结构化输出（`outputSchema`），等能按 action 表达返回值 schema 之后

详见项目仓库 [NereaFantasia/foo_ui_webview2](https://github.com/NereaFantasia/foo_ui_webview2)。

---

## 许可

以 [MIT 许可证](./LICENSE) 发布，与 `foo_ui_webview2` 的 SDK / MCP 子包许可保持一致。

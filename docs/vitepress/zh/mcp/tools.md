# 工具

服务器把 Bridge API 归并成少数几个工具。每个工具对应一个命名空间，或其中同一类改动，调用时用 `action` 指定要调用的宿主方法，该方法的参数与它并列传入：

```json
{ "action": "playlist.getTracks", "playlistGuid": "{…}", "count": 50 }
```

按方法做的事分组，工具的[注解](https://modelcontextprotocol.io/specification/2025-11-25/server/tools)才对其中每个 action 都成立：只读工具不会改动任何东西，客户端可以不经确认直接放行。调用到达 foobar2000 之前，服务器按所选方法自己的声明检查参数，方法会拒绝的情况直接报出，并列出该方法接受的参数。返回值就是该方法经 `fb2k.invoke()` 返回的内容。`artwork.getCurrent` 与 `artwork.getForTrack` 例外：封面以图片返回，其余字段仍是 JSON。

把 `FB2K_READ_ONLY` 设为 `1` 或 `true` 时只注册只读工具。

<!-- mcp-tools:begin -->
**10 个 Bridge 工具**，覆盖 90 个宿主方法，按命名空间与是否改动东西分组；页面工具见下文。每个工具接收 `action`（要调用的宿主方法），该方法的参数与它并列传入。带 `?` 的参数可以省略；参数的类型、取值范围与默认值见各工具的输入 schema。

### `fb2k_playback_read`

只读 · 9 个 action

读取 foobar2000 的播放状态：播放、暂停或停止，当前曲目，位置，音量，播放顺序，播完当前曲目后是否停止，正在播放的曲目所在的位置，以及播放队列。不改动任何东西。

| Action | 参数 | 说明 |
|--------|--------|-------------|
| [`playback.getState`](../api/playback.md#playback-getstate) | — | 报告播放状态以及当前曲目允许的操作。 |
| [`playback.getCurrentTrack`](../api/playback.md#playback-getcurrenttrack) | — | 报告当前载入（播放中或暂停）的曲目。 |
| [`playback.getPosition`](../api/playback.md#playback-getposition) | — | 报告播放位置，以及所在曲目的时长与身份。 |
| [`playback.getVolume`](../api/playback.md#playback-getvolume) | — | 以宿主用的两种刻度报告输出音量，以及是否静音。 |
| [`playback.getPlaybackOrder`](../api/playback.md#playback-getplaybackorder) | — | 报告当前播放顺序，同时给出序号与名称。 |
| [`playback.getStopAfterCurrent`](../api/playback.md#playback-getstopaftercurrent) | — | 报告是否在当前曲目结束后停止。 |
| [`playback.getCurrentTrackIndex`](../api/playback.md#playback-getcurrenttrackindex) | `includeTrackInfo?` | 定位正在播放的曲目在播放列表中的位置。 |
| [`playback.getPlayingPlaylist`](../api/playback.md#playback-getplayingplaylist) | — | 报告最近一次起播所在的播放列表。 |
| [`queue.get`](../api/queue.md#queue-get) | — | 按播放顺序读取整个播放队列。 |

### `fb2k_playback_control`

改状态 · 18 个 action

控制播放：开始、暂停、停止、切歌、定位，设置或增减音量，静音，选择播放顺序，播完当前曲目后停止，以及播放指定的文件或播放列表里的某一行。

| Action | 参数 | 说明 |
|--------|--------|-------------|
| [`playback.play`](../api/playback.md#playback-play) | — | 开始播放；暂停中则继续。 |
| [`playback.pause`](../api/playback.md#playback-pause) | — | 暂停播放；停止状态下什么也不做。 |
| [`playback.stop`](../api/playback.md#playback-stop) | — | 停止播放。 |
| [`playback.next`](../api/playback.md#playback-next) | — | 按当前播放顺序跳到下一首。 |
| [`playback.previous`](../api/playback.md#playback-previous) | — | 按当前播放顺序跳到上一首。 |
| [`playback.playPause`](../api/playback.md#playback-playpause) | — | 在播放与暂停之间切换；停止状态下开始播放。 |
| [`playback.random`](../api/playback.md#playback-random) | — | 从活动播放列表里随机起播一首。 |
| [`playback.playPath`](../api/playback.md#playback-playpath) | `path` | 把一个文件追加到活动播放列表并播放；没有活动列表时新建一个。 |
| [`playlist.playTrack`](../api/playlist.md#playlist-playtrack) | `playlist?`, `playlistGuid?`, `index`, `deferred?`, `muted?` | 播放播放列表中的一行，与双击它相同。 |
| [`playback.setPosition`](../api/playback.md#playback-setposition) | `position` | 在当前曲目内定位，播放中或暂停中均可。 |
| [`playback.setVolume`](../api/playback.md#playback-setvolume) | `volume` | 按百分比设置输出音量；超出 `0` 到 `100` 的值被夹到范围内。 |
| [`playback.volumeUp`](../api/playback.md#playback-volumeup) | — | 按宿主的步长调高音量：一分贝，落在整数分贝上。 |
| [`playback.volumeDown`](../api/playback.md#playback-volumedown) | — | 按宿主的步长调低音量：一分贝，落在整数分贝上。 |
| [`playback.mute`](../api/playback.md#playback-mute) | `muted?` | 静音或取消静音；要求的状态已经生效时什么也不做。 |
| [`playback.toggleMute`](../api/playback.md#playback-togglemute) | — | 翻转静音状态并报告新状态。 |
| [`playback.setPlaybackOrder`](../api/playback.md#playback-setplaybackorder) | `order?`, `name?` | 按序号或名称选择播放顺序；两者必须恰好给一个。 |
| [`playback.setStopAfterCurrent`](../api/playback.md#playback-setstopaftercurrent) | `enabled` | 设置或取消在当前曲目结束后停止。 |
| [`playback.toggleStopAfterCurrent`](../api/playback.md#playback-togglestopaftercurrent) | — | 翻转「当前曲目后停止」并报告新值。 |

### `fb2k_playlist_read`

只读 · 12 个 action

读取播放列表：播放列表清单（每项带索引、guid、名称、曲目数，以及是否活动、正在播放、带锁或自动播放列表），某个播放列表的一页曲目或按行号挑出的行、曲目符合查询的行、选中项与焦点，锁与自动播放列表的详情，以及播放列表视图可用的列。不改动任何东西。

| Action | 参数 | 说明 |
|--------|--------|-------------|
| [`playlist.getAll`](../api/playlist.md#playlist-getall) | — | 按顺序列出全部播放列表。 |
| [`playlist.getActive`](../api/playlist.md#playlist-getactive) | — | 描述活动播放列表，即用户正在看的那个。 |
| [`playlist.getPlaying`](../api/playlist.md#playlist-getplaying) | — | 描述正在播放的播放列表，即播放从中取下一首的那个。 |
| [`playlist.getTracks`](../api/playlist.md#playlist-gettracks) | `playlist?`, `playlistGuid?`, `start?`, `count?`, `formats?`, `fields?` | 播放列表的一页行。 |
| [`playlist.getTracksAt`](../api/playlist.md#playlist-gettracksat) | `rows`, `playlist?`, `playlistGuid?`, `formats?`, `fields?` | 按行号挑出的播放列表行，按传入顺序，例如 `playlist.getMatchingRows` 回答的那些不一定相邻的行。 |
| [`playlist.getMatchingRows`](../api/playlist.md#playlist-getmatchingrows) | `query`, `playlist?`, `playlistGuid?` | 播放列表里曲目符合 foobar2000 查询的行，按列表顺序；不管列表多长都只要一次调用，各行本身用 `playlist.getTracksAt` 读取。 |
| [`playlist.getSelectedTracks`](../api/playlist.md#playlist-getselectedtracks) | `playlist?`, `playlistGuid?` | 播放列表里选中的行，按列表顺序，不带播放统计。 |
| [`playlist.getSelection`](../api/playlist.md#playlist-getselection) | `playlist?`, `playlistGuid?` | 列出播放列表中被选中的行。 |
| [`playlist.getFocusedTrack`](../api/playlist.md#playlist-getfocusedtrack) | `playlist?`, `playlistGuid?` | 报告播放列表的焦点行。 |
| [`playlist.getLockInfo`](../api/playlist.md#playlist-getlockinfo) | `playlist?`, `playlistGuid?` | 报告一个播放列表是否带锁，例如自动播放列表的锁。 |
| [`playlist.getAutoplaylistInfo`](../api/playlist.md#playlist-getautoplaylistinfo) | `playlist?`, `playlistGuid?` | 描述一个播放列表的自动播放列表状态。 |
| [`playlist.getAvailableColumns`](../api/playlist.md#playlist-getavailablecolumns) | — | 列出 foobar2000 与已装组件为默认界面播放列表视图定义的列。 |

### `fb2k_playlist_manage`

破坏性 · 8 个 action

管理播放列表本身：新建、删除、改名、复制与重排，以及把播放列表转成由媒体库查询填充的自动播放列表，或转回普通播放列表。

| Action | 参数 | 说明 |
|--------|--------|-------------|
| [`playlist.create`](../api/playlist.md#playlist-create) | `name`, `position?` | 新建一个空播放列表。 |
| [`playlist.duplicate`](../api/playlist.md#playlist-duplicate) | `playlist?`, `playlistGuid?`, `name?` | 把一个播放列表连同曲目复制成新列表，放在原列表紧后面。 |
| [`playlist.rename`](../api/playlist.md#playlist-rename) | `playlist?`, `playlistGuid?`, `name` | 重命名一个播放列表。 |
| [`playlist.remove`](../api/playlist.md#playlist-remove) | `playlist` | 删除一个播放列表。 |
| [`playlist.reorderPlaylists`](../api/playlist.md#playlist-reorderplaylists) | `newOrder?`, `newOrderGuids?` | 重排播放列表的顺序，为每个新位置给出放到那里的播放列表的当前索引（`newOrder`）或它的 GUID（`newOrderGuids`）；两者恰好给一个，否则以 `INVALID_PARAMS` 失败。 |
| [`playlist.createAutoplaylist`](../api/playlist.md#playlist-createautoplaylist) | `name?`, `query`, `sort?`, `keepSorted?` | 新建一个由 foobar2000 按媒体库查询填充并保持更新的播放列表。 |
| [`playlist.convertToAutoplaylist`](../api/playlist.md#playlist-converttoautoplaylist) | `playlist?`, `playlistGuid?`, `query`, `sort?`, `keepSorted?` | 把已有的播放列表变成自动播放列表，曲目换成查询结果。 |
| [`playlist.removeAutoplaylist`](../api/playlist.md#playlist-removeautoplaylist) | `playlist?`, `playlistGuid?` | 把自动播放列表变回保留当前曲目的普通列表。 |

### `fb2k_playlist_edit`

破坏性 · 15 个 action

改动播放列表里的曲目：添加文件、文件夹或曲目，插入、删除、移动、重排、排序、打乱或倒序行，清空，整体替换后播放，以及撤销与重做。

| Action | 参数 | 说明 |
|--------|--------|-------------|
| [`playlist.addPaths`](../api/playlist.md#playlist-addpaths) | `playlist?`, `playlistGuid?`, `paths` | 把文件、文件夹或 URL 追加到播放列表，事先保存撤销点。 |
| [`playlist.addPathsSequential`](../api/playlist.md#playlist-addpathssequential) | `playlist?`, `playlistGuid?`, `paths` | 按传入顺序把路径追加到播放列表，事先保存撤销点；展开成多首曲目的路径（文件夹、cue）占住它自己的位置。 |
| [`playlist.addHandles`](../api/playlist.md#playlist-addhandles) | `playlist?`, `playlistGuid?`, `handles` | 把曲目追加到播放列表，事先保存撤销点。 |
| [`playlist.insertTracks`](../api/playlist.md#playlist-inserttracks) | `playlist?`, `playlistGuid?`, `position?`, `handles` | 在播放列表的某个位置插入曲目，事先保存撤销点。 |
| [`playlist.replaceAllAndPlay`](../api/playlist.md#playlist-replaceallandplay) | `playlist?`, `playlistGuid?`, `paths`, `playIndex?`, `stopFirst?`, `autoPlay?` | 替换播放列表的全部内容并播放：停止播放，清空列表（保存撤销点），按 `playlist.addPaths` 的方式加入路径，把列表设为活动列表，再播放或聚焦 `playIndex`。 |
| [`playlist.removeTracks`](../api/playlist.md#playlist-removetracks) | `playlist?`, `playlistGuid?`, `items` | 从播放列表移除行，事先保存撤销点。 |
| [`playlist.removeSelectedTracks`](../api/playlist.md#playlist-removeselectedtracks) | `playlist?`, `playlistGuid?` | 从播放列表移除选中的行，事先保存撤销点。 |
| [`playlist.clear`](../api/playlist.md#playlist-clear) | `playlist?`, `playlistGuid?` | 移除播放列表里的全部曲目，事先保存撤销点。 |
| [`playlist.moveTracks`](../api/playlist.md#playlist-movetracks) | `playlist?`, `playlistGuid?`, `items?`, `delta` | 把播放列表中选中的行移动 `delta` 个位置，事先保存撤销点。 |
| [`playlist.reorder`](../api/playlist.md#playlist-reorder) | `playlist?`, `playlistGuid?`, `newOrder` | 重排播放列表的曲目，事先保存撤销点：`newOrder[i]` 是移到第 `i` 行的那首曲目的当前行号。 |
| [`playlist.sort`](../api/playlist.md#playlist-sort) | `playlist?`, `playlistGuid?`, `pattern?`, `descending?`, `selectedOnly?` | 按 Title Formatting 模式给播放列表排序，事先保存撤销点。 |
| [`playlist.shuffle`](../api/playlist.md#playlist-shuffle) | `playlist?`, `playlistGuid?` | 把播放列表的曲目打乱成随机顺序，事先保存撤销点。 |
| [`playlist.reverse`](../api/playlist.md#playlist-reverse) | `playlist?`, `playlistGuid?` | 把播放列表的曲目顺序倒过来，事先保存撤销点。 |
| [`playlist.undo`](../api/playlist.md#playlist-undo) | `playlist?`, `playlistGuid?` | 把播放列表恢复到上一个撤销点。 |
| [`playlist.redo`](../api/playlist.md#playlist-redo) | `playlist?`, `playlistGuid?` | 重新应用上一次 `playlist.undo` 撤销的改动。 |

### `fb2k_playlist_select`

改状态，可重复调用 · 5 个 action

改变哪个播放列表是活动的，以及其中哪些行被选中或获得焦点。曲目本身不动。

| Action | 参数 | 说明 |
|--------|--------|-------------|
| [`playlist.setActive`](../api/playlist.md#playlist-setactive) | `playlist?`, `playlistGuid?` | 把一个播放列表设为活动列表。 |
| [`playlist.setSelection`](../api/playlist.md#playlist-setselection) | `playlist?`, `playlistGuid?`, `indices`, `clearOthers?` | 选中播放列表中的行。 |
| [`playlist.selectAll`](../api/playlist.md#playlist-selectall) | `playlist?`, `playlistGuid?` | 选中播放列表中的全部行。 |
| [`playlist.deselectAll`](../api/playlist.md#playlist-deselectall) | `playlist?`, `playlistGuid?` | 清除播放列表中的选中。 |
| [`playlist.setFocusedTrack`](../api/playlist.md#playlist-setfocusedtrack) | `playlist?`, `playlistGuid?`, `index` | 把播放列表的焦点移到一行，或去掉焦点。 |

### `fb2k_library_read`

只读 · 4 个 action

读取媒体库：用 foobar2000 查询语法搜索（如 `artist IS Beatles`），列出专辑或艺术家，以及取统计数据。结果分页返回；少要几个 `fields` 可以让结果更短。不改动任何东西。

| Action | 参数 | 说明 |
|--------|--------|-------------|
| [`library.search`](../api/library.md#library-search) | `query`, `offset?`, `limit?`, `fields?` | 符合 foobar2000 查询表达式的曲目中的一页，按媒体库顺序：`SORT BY` 子句会被接受但不生效。 |
| [`library.getAlbums`](../api/library.md#library-getalbums) | `sort?`, `query?`, `offset?`, `limit?`, `includeTracks?`, `useCache?` | 整个媒体库的专辑，按专辑名加专辑艺术家分组；没有 `album` 标签的曲目跳过。 |
| [`library.getArtists`](../api/library.md#library-getartists) | `sort?`, `limit?`, `includeAlbums?` | 每位署名艺术家及其参与计数：多值 `artist` 的每个值各成一行。 |
| [`library.getStats`](../api/library.md#library-getstats) | — | 整个媒体库的汇总计数。 |

### `fb2k_queue_edit`

破坏性 · 8 个 action

改动播放队列：加入播放列表的行或文件，插播下一首，移除条目，把某一条移到最前，整体替换或清空队列，以及立即播放某一条。读取队列用 fb2k_playback_read 的 `queue.get`。

| Action | 参数 | 说明 |
|--------|--------|-------------|
| [`queue.add`](../api/queue.md#queue-add) | `playlist?`, `playlistGuid?`, `tracks?`, `track?` | 按播放列表中的位置把一首或多首曲目加入队列。 |
| [`queue.addPaths`](../api/queue.md#queue-addpaths) | `paths`, `useQueuePlaylist?`, `playlist?`, `playlistGuid?` | 按路径把曲目加入队列。 |
| [`queue.insertNext`](../api/queue.md#queue-insertnext) | `paths?`, `items?`, `position?` | 插入曲目使其下一首播放，排在已入队的所有条目之前。 |
| [`queue.setContents`](../api/queue.md#queue-setcontents) | `items` | 用一份有序引用列表替换整个队列。 |
| [`queue.moveToTop`](../api/queue.md#queue-movetotop) | `index` | 把队列中的一条移到队首，使其下一首播放。 |
| [`queue.playNow`](../api/queue.md#queue-playnow) | `index?` | 立即播放队列中 `index` 处的条目；它不在队首时先移到队首。 |
| [`queue.remove`](../api/queue.md#queue-remove) | `index?`, `indices?` | 按 `index` 移除一条，或按 `indices` 移除多条。 |
| [`queue.clear`](../api/queue.md#queue-clear) | — | 清空队列。 |

### `fb2k_track_read`

只读 · 6 个 action

读取曲目文件：取自媒体库缓存或直接读文件的标签与技术信息，一次读取多个文件的标签，曲目的评分，以及封面。不改动任何东西。

| Action | 参数 | 说明 |
|--------|--------|-------------|
| [`metadata.read`](../api/metadata.md#metadata-read) | `path`, `cueIndex?` | 读取单个曲目的标签与技术信息。 |
| [`metadata.readRaw`](../api/metadata.md#metadata-readraw) | `path`, `cueIndex?` | 绕过宿主缓存，直接从文件读取单个曲目的标签与技术信息；结果同 `metadata.read`，另加 `source`。 |
| [`metadata.readBatch`](../api/metadata.md#metadata-readbatch) | `paths` | 读取多个曲目的扁平字段，每个路径一行。 |
| [`rating.get`](../api/rating.md#rating-get) | `path`, `cueIndex?` | 读取曲目 0 到 5 的评分：foo_playcount 的 `%rating%` 在 1 到 5 之间时优先，否则取文件的 `RATING` 标签并夹到 0..5。 |
| [`artwork.getForTrack`](../api/artwork.md#artwork-getfortrack) | `path`, `type?` | 经专辑封面管理器读图片（也会找到文件夹封面）并量尺寸。 |
| [`artwork.getCurrent`](../api/artwork.md#artwork-getcurrent) | `type?` | 读正在播放曲目的图片。 |

### `fb2k_track_write`

破坏性 · 5 个 action

改动曲目文件：给一个或多个文件写标签（值为 `null` 即删除该标签），设置评分，以及嵌入或移除封面。

| Action | 参数 | 说明 |
|--------|--------|-------------|
| [`metadata.write`](../api/metadata.md#metadata-write) | `path`, `tags`, `cueIndex?` | 把对单个曲目的标签写入排入队列并立即返回。 |
| [`metadata.writeBatch`](../api/metadata.md#metadata-writebatch) | `items` | 每条排入一次 `metadata.write`，报告成功排入的条数。 |
| [`rating.set`](../api/rating.md#rating-set) | `path?`, `rating`, `cueIndex?` | 通过 foo_playcount 的右键菜单设置评分，菜单不可用时改写文件的 `RATING` 标签。 |
| [`metadata.embedArtwork`](../api/metadata.md#metadata-embedartwork) | `path`, `imageData`, `type?`, `target?`, `filename?` | 把图片写进曲目的标签、写成音频旁的图片文件，或两者都写。 |
| [`metadata.removeEmbeddedArt`](../api/metadata.md#metadata-removeembeddedart) | `path`, `type?`, `removeAll?` | 删除文件里嵌入的图片：删一种类型，或在省略 `type`、设置 `removeAll` 时删除全部。 |
<!-- mcp-tools:end -->

## 页面工具

下面两个工具经 Chrome DevTools Protocol 操作 WebView2 页面，不走 Bridge 方法。

### `fb2k_page_inspect`

只读

| Action | 参数 | 结果 |
| --- | --- | --- |
| `screenshot` | `fullPage?` | 页面的 PNG 截图。`fullPage` 为 `true` 时先把视口调整为整个内容的尺寸；省略时截取当前视口。 |
| `domSnapshot` | — | 每个元素一行，形如 `tag#id.class "text"`，按层级缩进。只有唯一子节点是文本的元素才带文本，截断到 80 个字符。 |
| `consoleMessages` | `limit?` | 页面最近的控制台消息与未捕获异常，每行 `[level] text`，从旧到新。服务器从第一次连上页面起收集，页面此前留存的消息也会一并送来，保留最近 200 条；`limit`（默认 100）限制返回条数。 |

### `fb2k_page_evaluate`

破坏性 · 只在 `FB2K_ENABLE_EVAL` 为 `1` 或 `true` 且不在只读模式时注册

| 参数 | 说明 |
| --- | --- |
| `expression` | 在页面里求值的 JavaScript，结果以 JSON 返回。 |

::: danger 安全边界
`fb2k_page_evaluate` 在页面里执行任意 JavaScript，能调用页面可以调用的全部 Bridge 方法。只在可信的开发或调试会话里开启。
:::

## 限制

- **频率**：服务器一次最多连续接受 40 次工具调用，之后每秒 20 次；超出的调用失败，错误信息说明要等多久。
- **响应长度**：文本结果超过 `FB2K_MAX_RESPONSE_CHARS` 个字符（默认 100000）时被截断，截断处附一条说明，建议分页或少要几个 `fields`。
- **图片**：封面只有是 PNG、JPEG、GIF 或 WebP，且不超过 `FB2K_MAX_IMAGE_BYTES` 字节（默认 3932160，即 3.75 MiB，base64 编码后正好是 Claude API 单张图片接受的 5 MiB）时才以图片返回；否则结果只带其余字段，并附一条说明，写明图片为什么没有附上。

# SMP 兼容层 

v1.2.0 新增完整的 Spider Monkey Panel (SMP) 兼容层，可在 WebView2 UI 中运行 SMP 脚本。v1.2.1 补全 Phase 4 API（剪贴板/DSP/输出/队列扩展）、`utils` 对象（22 方法）和 `window` 属性方法。

## 快速开始 

```html
<!-- 1. bridge.js 必须先加载 -->
<script src="bridge.js"></script>
<!-- 2. smp-compat.js 自动加载所有包装器 -->
<script src="smp-compat.js"></script>

<script>
  await window.smp.ready;
  console.log('IsPlaying:', fb.IsPlaying);
  console.log('ActivePlaylist:', plman.ActivePlaylist);
</script>
```

## 与底层 API 的架构差异

::: warning 重要：请在使用前理解这些差异
兼容层与底层 `fb2k.invoke()` API 的工作模式存在本质区别。这不是 bug，而是有意为之的设计取舍。
:::

### 为什么需要缓存层？ 

SMP 的绝大多数 API 是**同步**的——调用立即返回结果。但 WebView2 与 C++ 后端的通信必须经过异步消息队列（postMessage），每次调用都是异步 `invoke()`。如果简单地把 SMP 的 `fb.IsPlaying` 改成 `await fb2k.invoke('playback.getState')`，就会破坏现有 SMP 脚本的同步语义。

因此兼容层引入了「缓存 + 事件」模式，用**最终一致性**换取**同步读取体验**。

### 两种模式对比 

**底层 API（`fb2k.invoke`）**：每次调用都是独立的异步请求→后端执行→返回真实结果。无状态、强一致。

```javascript
// 底层 API：每次都从后端拿真实值
const state = await fb2k.invoke('playback.getState'); // → C++ → 返回
```

**兼容层**：读走本地缓存（同步），写走双通道（先更新缓存，再异步通知后端）。

```javascript
// 兼容层：读取从缓存立即返回，不经过 C++
const playing = fb.IsPlaying;         // → _cache.isPlaying（同步）

// 兼容层：写入先更新缓存，再异步通知后端（fire-and-forget）
plman.ActivePlaylist = 2;
// → 立即：_cache.activePlaylist = 2
// → 异步：invoke('playlist.setActive', { playlist: 2 })（不等待结果）
```

### 优势 

- ✓ SMP 脚本的同步读取语义得以保留，`fb.IsPlaying`、`plman.PlaylistCount` 等立即返回
- ✓ 适合 UI 渲染场景（高频读取、低频写入），无需每帧 await
- ✓ setter 的 fire-and-forget 设计使得赋值后立即读回新值，体验与 SMP 一致
- ✓ 无死锁风险：JS 单线程 + 消息队列解耦，与底层 API 共享同一通信架构
- ✓ 无额外攻击面：所有调用经过 BridgeCore 统一入口，受同样的安全检查

### 风险与注意事项

**1. 缓存与后端的短暂不一致**

写入后，缓存立即更新，但后端可能尚未处理完成。如果后端拒绝操作（例如索引越界），缓存不会自动回滚。但后端处理完成后会推送事件触发缓存刷新，不一致窗口通常 < 1 帧。 

## 架构概述 

**核心**：`sdk/smp-compat.js` — IIFE 封装，提供缓存系统、事件映射、`plman` 对象、`fb` 扩展属性/方法

**包装器类**（`sdk/smp/` 目录，按需自动加载）：

| 文件 | 类 | 说明 |
| --- | --- | --- |
| utils.js | smpUtils | 共享工具模块（toHandleId、normalizeHandleList、buildMenuItems） |
| FbMetadbHandle.js | FbMetadbHandle | 曲目句柄（Path、RawPath、SubSong、Length、FileSize、HandleId） |
| FbMetadbHandleList.js | FbMetadbHandleList | 曲目列表（Count、Add、Remove、RemoveById、Find、Clone） |
| FbTitleFormat.js | FbTitleFormat | 标题格式化（Expression、Eval、EvalWithMetadb、EvalWithMetadbs） |
| FbProfiler.js | FbProfiler | 性能计时器（Time、Reset、Print） |
| FbFileInfo.js | FbFileInfo | 文件元数据（MetaCount、InfoCount、MetaFind、MetaName、MetaValue、MetaValueCount） |
| FbUiSelectionHolder.js | FbUiSelectionHolder | UI 选择持有者（SetSelection、SetPlaylistSelectionTracking、SetPlaylistTracking） |
| ContextMenuManager.js | ContextMenuManager | 上下文菜单管理器（InitContext、InitContextPlaylist、InitNowPlaying、BuildMenu、ExecuteByID） |
| MainMenuManager.js | MainMenuManager | 主菜单管理器（Init、BuildMenu、ExecuteByID） |

### 缓存 + 事件机制 

1. **初始化时**：批量调用后端 API 填充 `_cache`（11~15 个并行请求，取决于 `includePaths` 选项）
2. **运行时**：缓存层监听 22 个 bridge 事件实时更新，同时通过 `fb.onSMP()` 映射 35 个 SMP 事件名到对应的 bridge 事件
3. **读取时**：属性从缓存同步返回，不经过 C++

缓存字段：播放状态（isPlaying、isPaused、volumeDb、playbackTime）、播放列表（playlists、activePlaylist、playlistCount）、配置（cursorFollowPlayback、playbackFollowCursor、replaygainMode）、路径（componentPath、foobarPath、profilePath）等。

## fb 对象 — 属性 

| 属性 | 类型 | 读/写 | 说明 |
| --- | --- | --- | --- |
| fb.IsPlaying | boolean | 只读 | 是否正在播放 |
| fb.IsPaused | boolean | 只读 | 是否暂停 |
| fb.Volume | number | 读写 | 音量 dB（-100..0） |
| fb.PlaybackTime | number | 读写 | 当前播放位置（秒） |
| fb.PlaybackLength | number | 只读 | 当前曲目时长（秒） |
| fb.StopAfterCurrent | boolean | 读写 | 当前曲目结束后停止 |
| fb.AlwaysOnTop | boolean | 读写 | 窗口置顶 |
| fb.CursorFollowPlayback | boolean | 读写 | 光标跟随播放 |
| fb.PlaybackFollowCursor | boolean | 读写 | 播放跟随光标 |
| fb.ReplaygainMode | number | 读写 | ReplayGain 模式 |
| fb.ComponentPath | string | 只读 | 插件组件目录路径 |
| fb.FoobarPath | string | 只读 | foobar2000 安装目录 |
| fb.ProfilePath | string | 只读 | 用户配置文件目录 |

## fb 对象 — 方法 

### 播放控制 

`fb.Play()` / `fb.Pause()` / `fb.Stop()` / `fb.Next()` / `fb.Prev()` / `fb.Random()` / `fb.PlayOrPause()` / `fb.VolumeUp()` / `fb.VolumeDown()` / `fb.VolumeMute()` / `fb.Exit()`

均返回 Promise。

### 工厂方法 

| 方法 | 返回 | 说明 |
| --- | --- | --- |
| fb.TitleFormat(expr) | FbTitleFormat | 创建标题格式化对象 |
| fb.CreateHandleList() | FbMetadbHandleList | 创建空曲目列表 |
| fb.CreateProfiler(name) | FbProfiler | 创建性能计时器 |
| fb.AcquireUiSelectionHolder() | FbUiSelectionHolder | 获取 UI 选择持有者 |
| fb.CreateContextMenuManager() | ContextMenuManager | 创建上下文菜单管理器 |
| fb.CreateMainMenuManager() | MainMenuManager | 创建主菜单管理器 |

### 查询方法（返回 Promise） 

| 方法 | 返回 | 说明 |
| --- | --- | --- |
| fb.GetNowPlaying() | `FbMetadbHandle \| null` | 当前播放曲目 |
| fb.GetFocusItem() | `FbMetadbHandle \| null` | 焦点曲目 |
| fb.GetSelection() | FbMetadbHandleList | 当前选中项 |
| fb.GetSelectionType() | number | 选择类型 |
| fb.GetLibraryItems() | FbMetadbHandleList | 所有媒体库项目 |
| fb.GetQueryItems(handles, query) | FbMetadbHandleList | 媒体库查询 ⚠️ |
| fb.IsLibraryEnabled() | boolean | 媒体库是否启用 |
| fb.IsMetadbInMediaLibrary(handle) | boolean | 文件是否在媒体库中 |

### 命令方法（返回 Promise） 

| 方法 | 说明 |
| --- | --- |
| fb.RunMainMenuCommand(command) | 执行主菜单命令 |
| fb.RunContextCommand(command) | 执行上下文菜单命令 |
| fb.ShowConsole() | 显示控制台 |
| fb.ShowPreferences() | 显示首选项 |
| fb.ShowLibrarySearchUI(query) | 显示媒体库搜索 |
| fb.ShowPopupMessage(msg, title) | 显示弹出消息 |
| fb.Restart() | 重启 foobar2000 |

## plman 对象 

### 属性 

| 属性 | 类型 | 读/写 | 说明 |
| --- | --- | --- | --- |
| plman.ActivePlaylist | number | 读写 | 激活的播放列表索引 |
| plman.PlayingPlaylist | number | 只读 | 正在播放的播放列表索引（不播放时返回 -1） |
| plman.PlaylistCount | number | 只读 | 播放列表总数 |
| plman.PlaybackOrder | number | 读写 | 播放顺序 |

### 同步方法（来自缓存） 

| 方法 | 返回 | 说明 |
| --- | --- | --- |
| plman.GetPlaylistName(idx) | string | 播放列表名称 |
| plman.PlaylistItemCount(idx) | number | 曲目数 |
| plman.FindPlaylist(name) | number | 查找（-1 未找到） |
| plman.IsAutoPlaylist(idx) | boolean | 是否自动播放列表 |
| plman.IsPlaylistLocked(idx) | boolean | 是否锁定 |
| plman.UndoBackup(idx) | true | No-op |

### 异步方法（返回 Promise） 

| 方法 | 返回 | 说明 |
| --- | --- | --- |
| plman.CreatePlaylist(pos, name) | number | 创建播放列表 |
| plman.RemovePlaylist(idx) | boolean | 删除播放列表 |
| plman.RenamePlaylist(idx, name) | boolean | 重命名 |
| plman.ClearPlaylist(idx) | boolean | 清空 |
| plman.DuplicatePlaylist(from, name) | number | 复制 |
| plman.AddLocations(idx, paths, select) | number | 添加文件路径 |
| plman.GetPlaylistItems(idx) | FbMetadbHandleList | 全部曲目 |
| plman.GetPlaylistSelectedItems(idx) | FbMetadbHandleList | 选中曲目 |
| plman.InsertPlaylistItems(pl, base, handles, select) | number | 插入曲目 |
| plman.GetPlaylistFocusItemIndex(idx) | number | 焦点项索引 |
| plman.SetPlaylistFocusItem(idx, item) | boolean | 设置焦点项 |
| plman.SetPlaylistSelection(idx, items, state) | boolean | 设置选择 |
| plman.ClearPlaylistSelection(idx) | boolean | 清除选择 |
| plman.RemovePlaylistSelection(idx, crop) | boolean | 删除选中/裁剪 |
| plman.SortByFormat(idx, pattern, selectedOnly) | boolean | 按格式排序 |
| plman.MovePlaylistSelection(idx, delta) | boolean | 移动选中项 |
| plman.AddItemToPlaybackQueue(handle) | number | 添加到队列 ⚠️ |
| plman.GetPlaybackQueueContents() | Array | 获取队列内容 |
| plman.CreateAutoPlaylist(idx, name, query, sort, flags) | number | 创建智能播放列表 |
| plman.FlushPlaybackQueue() | boolean | 清空队列 |

## 事件系统 

### fb.onSMP(eventName, callback) 

使用 SMP 风格事件名注册回调，返回取消订阅函数。

```javascript
const unsub = fb.onSMP('on_playback_new_track', (track) => {
    console.log('New track:', track?.title);
});
unsub(); // 取消订阅
```

### 完整事件映射 

**播放：**

| SMP 事件 | WebView2 事件 | 回调参数 |
| --- | --- | --- |
| on_playback_starting | playback:starting | (command, is_paused) |
| on_playback_new_track | playback:trackChanged | (track_info) |
| on_playback_stop | playback:stopped | (reason: 0=user,1=eof,2=starting_another,3=shutting_down) |
| on_playback_pause | playback:paused | (is_paused) |
| on_playback_seek | playback:seeked | (time) |
| on_playback_time | playback:time | (time) |
| on_playback_order_changed | playback:orderChanged | (new_order_index) |
| on_playback_queue_changed | playback:queueChanged | (origin) |
| on_playback_edited | playback:edited | (data) |
| on_playback_dynamic_info | playback:dynamicInfo | (data) |
| on_playback_dynamic_info_track | playback:dynamicInfoTrack | (data) |
| on_item_played | playback:itemPlayed | (handle) |
| on_volume_change | playback:volumeChanged | (volume_db) |

**播放列表：**

| SMP 事件 | WebView2 事件 | 回调参数 |
| --- | --- | --- |
| on_playlist_switch | playlist:activated | () |
| on_playlist_items_added | playlist:itemsAdded | (playlist_idx) |
| on_playlist_items_removed | playlist:itemsRemoved | (playlist_idx, new_count) |
| on_playlist_items_reordered | playlist:itemsReordered | (playlist_idx) |
| on_playlist_items_selection_change | playlist:selectionChanged | () |
| on_item_focus_change | playlist:focusChanged | (playlist, from, to) |
| on_playlists_changed | 多事件合并 | () |

**选择/元数据/媒体库：**

| SMP 事件 | WebView2 事件 | 回调参数 |
| --- | --- | --- |
| on_selection_changed | selection:changed | () |
| on_metadb_changed | metadb:changed | (handle_list, fromHook) |
| on_library_items_added | library:itemsAdded | () |
| on_library_items_removed | library:itemsRemoved | () |
| on_library_items_changed | library:itemsModified | () |

**音频/DSP/UI/窗口：**

| SMP 事件 | WebView2 事件 | 回调参数 |
| --- | --- | --- |
| on_dsp_preset_changed | audio:dspPresetChanged | () |
| on_output_device_changed | audio:outputDeviceChanged | () |
| on_replaygain_mode_changed | audio:replaygainModeChanged | (new_mode) |
| on_colours_changed | ui:coloursChanged | (data) |
| on_font_changed | ui:fontChanged | (data) |
| on_always_on_top_changed | window:alwaysOnTopChanged | (state) |
| on_cursor_follow_playback_changed | playback:cursorFollowChanged | (state) |
| on_playback_follow_cursor_changed | playback:followCursorChanged | (state) |
| on_playlist_stop_after_current_changed | playback:stopAfterCurrentChanged | (state) |
| on_focus | panel:focus + panel:blur | (is_focused) |

## smp 对象 

### smp.ready 

```javascript
await window.smp.ready; // 等待兼容层初始化完成（缓存填充 + 包装器加载）
```

### smp.refreshCache() 

全量刷新缓存（播放状态 + 播放列表 + 配置），返回 Promise。用于在事件丢失后强制同步缓存。

```javascript
await smp.refreshCache();
console.log('cache refreshed');
```

### smp.dispose() 

清理所有事件监听器，防止内存泄漏。在组件卸载或页面切换时调用。

```javascript
smp.dispose(); // 移除所有缓存更新的事件监听
```

## 包装器类 

### FbMetadbHandle 

```javascript
const handle = new FbMetadbHandle('C:\\\\Music\\\\song.flac');
handle.Path;       // 文件路径
handle.RawPath;    // 同 Path（兼容）
handle.SubSong;    // 子曲目索引
handle.Length;     // 时长（秒）
handle.FileSize;   // 字节数
handle.HandleId;   // 完整 ID（含 subsong 后缀）
handle.Compare(other); // 比较
await handle.GetFileInfo(); // 返回 FbFileInfo
```

### FbMetadbHandleList 

```javascript
const list = fb.CreateHandleList();
list.Add(handle);  list.AddRange(otherList);
list.Count;  list[0];  // Proxy 索引访问
list.Find(handle);  list.Clone();
list.Remove(handle);  list.RemoveById(0); // 按索引删除
list.RemoveAll();
list.Convert();  // 转为普通数组
list.CalcTotalDuration();  list.CalcTotalSize();
```

### FbTitleFormat 

```javascript
const tf = fb.TitleFormat('%title% - %artist%');
await tf.Eval();                     // 当前播放曲目
await tf.EvalWithMetadb(handle);     // 指定句柄
await tf.EvalWithMetadbs(handleList); // 批量
```

### FbProfiler 

```javascript
const p = fb.CreateProfiler('MyTask');
// ... 执行任务 ...
console.log(p.Time + 'ms');
p.Print();  p.Reset();
```

### FbFileInfo 

```javascript
const info = await handle.GetFileInfo();
info.MetaCount;  info.InfoCount;
info.MetaFind('artist');  info.MetaName(0);
info.MetaValueCount(0);   // 某个 meta 字段的多值数量
info.MetaValue(0, 0);     // meta[index][value_index]
info.InfoFind('codec');   info.InfoName(0);   info.InfoValue(0);
```

### ContextMenuManager 

```javascript
const cm = fb.CreateContextMenuManager();
cm.InitNowPlaying();  // 或 cm.InitContext(handleList) / cm.InitContextPlaylist()
const items = await cm.BuildMenu();
// 用 ui.showCustomMenu 显示或自定义 HTML 菜单
await cm.ExecuteByID(selectedId);
```

### MainMenuManager 

```javascript
const mm = fb.CreateMainMenuManager();
mm.Init('file');  // file/edit/view/playback/library/help
const items = await mm.BuildMenu();
await mm.ExecuteByID(selectedId);
```

| 方法 | 返回 | 说明 |
| --- | --- | --- |
| fb.CheckClipboardContents() | boolean | 剪贴板是否包含文件 |
| fb.GetClipboardContents() | FbMetadbHandleList | 从剪贴板获取文件列表 |
| fb.CopyHandleListToClipboard(handles) | boolean | 复制文件路径到剪贴板 |

```javascript
if (await fb.CheckClipboardContents()) {
    const list = await fb.GetClipboardContents();
    console.log('Clipboard has', list.Count, 'files');
}

await fb.CopyHandleListToClipboard(myHandleList);
```

| 方法 | 返回 | 说明 |
| --- | --- | --- |
| fb.GetDSPPresets() | string (JSON) | 获取 DSP 预设列表 |
| fb.SetDSPPreset(index) | void | 设置活动 DSP 预设 |
| fb.GetOutputDevices() | string (JSON) | 获取输出设备列表 |
| fb.SetOutputDevice(guid, deviceId) | void | 设置输出设备 |

> `GetDSPPresets` / `GetOutputDevices` 返回 JSON 字符串（与 SMP 行为一致），需 `JSON.parse()` 解析。

```javascript
const devices = JSON.parse(await fb.GetOutputDevices());
const wasapi = devices.find(d => d.name.includes('WASAPI'));
if (wasapi) await fb.SetOutputDevice(wasapi.outputId, wasapi.deviceId);
```

| 方法/属性 | 类型 | 说明 |
| --- | --- | --- |
| fb.Version | string (只读) | foobar2000 版本字符串 |
| fb.RunContextCommandWithMetadb(cmd, handle) | boolean | 对指定句柄执行上下文命令 ⚠️ |
| fb.ClearPlaylist() | boolean | 清空活动播放列表 |
| fb.GetLibraryRelativePath(handle) | string | 获取相对于 foobar 目录的路径 |

::: warning RunContextCommandWithMetadb 已知限制
后端 `menu.runContextCommand` 不读取 `handles` 参数，命令始终作用于默认上下文（当前选择/播放曲目）。
:::

| 方法 | 返回 | 说明 |
| --- | --- | --- |
| plman.AddPlaylistItemToPlaybackQueue(pl, item) | boolean | 将播放列表项添加到队列 |
| plman.GetPlaybackQueueHandles() | FbMetadbHandleList | 获取队列中所有曲目 |
| plman.IsPlaylistItemSelected(pl, item) | boolean | 检查项目是否被选中 |
| plman.FindOrCreatePlaylist(name) | number | 查找或创建播放列表 |
| plman.MovePlaylist(from, to) | boolean | 移动播放列表位置 |

```javascript
// 将第一个播放列表的第3首添加到播放队列
await plman.AddPlaylistItemToPlaybackQueue(0, 2);

// 查找或创建
const idx = await plman.FindOrCreatePlaylist('My Favorites');
```

以下行为与原版 SMP 存在差异，属于已知设计取舍：

| API | 限制 | 说明 |
| --- | --- | --- |
| fb.GetQueryItems(handles, query) | handles 参数被忽略 | 始终搜索整个媒体库，等效于 fb.GetLibraryItems() + filter。当传入完整库作为 handles 时无影响 |
| fb.GetFocusItem(force) | force=true fallback 未实现 | SMP 中 force=true 会在无 focus item 时返回第 0 项，当前实现直接返回 null |
| plman.FindOrCreatePlaylist(name, unlocked) | unlocked 参数被忽略 | 创建后不会自动 unlock |
| plman.CreateAutoPlaylist(idx, ...) | idx 位置参数被忽略 | 始终追加到末尾 |
| window.NotifyOthers(name, info) | 接收端 on_notify_data 未映射 | 发送正常（使用 window.broadcast），但 SMP 的 on_notify_data 回调不会被自动触发；接收端需改用 fb2k.on('window:message', ...) |

// 移动播放列表位置 await plman.MovePlaylist(3, 0); // 将第4个移到第1个位置

## window 对象 

SMP 风格的窗口属性持久化存储，底层使用 `config.get/set/remove`。

| 方法 | 说明 | 底层 API |
|------|------|----------|
| `window.GetProperty(name, default?)` | 读取持久化属性 | `config.get` (键前缀 `smp.prop.`) |
| `window.SetProperty(name, value)` | 写入持久化属性（`null` 删除） | `config.set` / `config.remove` |
| `window.NotifyOthers(name, info)` | 向其他窗口广播消息 | `window.broadcast` |

```javascript
// 持久化属性
const theme = await window.GetProperty('theme', 'dark');
await window.SetProperty('theme', 'light');
await window.SetProperty('theme', null); // 删除

// 跨窗口通知
window.NotifyOthers('theme_changed', { theme: 'light' });
```

> `NotifyOthers` 是 fire-and-forget，payload 格式为 `{ type: 'smp.notifyOthers', name, info }`，通过 `window.broadcast` 发送。

`sdk/smp/utils-compat.js` 提供完整的 SMP `utils` 全局对象。

### 纯 JS 方法（无后端依赖） 

| 方法 | 返回 | 说明 |
| --- | --- | --- |
| utils.Version | string | 兼容层版本 |
| utils.FormatDuration(seconds) | string | 秒 → H:MM:SS 或 M:SS |
| utils.FormatFileSize(bytes) | string | 字节 → 人类可读（KB/MB/GB） |
| utils.SplitFilePath(path) | [dir, name, ext] | 拆分文件路径 |
| utils.PathWildcardMatch(path, pattern) | boolean | 通配符匹配（*、?） |
| utils.DateStringFromTimestamp(ts) | string | Unix 时间戳 → 日期字符串 |
| utils.CheckFont(name) | boolean | 检测字体是否可用 |

### 文件操作方法（委托后端） 

| 方法 | 返回 | 后端 API |
| --- | --- | --- |
| utils.FileExists(path) | boolean | file.exists |
| utils.IsFile(path) | boolean | file.getInfo |
| utils.IsDirectory(path) | boolean | file.getInfo |
| utils.GetFileSize(path) | number | file.getInfo |
| utils.ReadTextFile(path, codepage?) | string | file.read |
| utils.ReadUTF8(path) | string | file.read |
| utils.WriteTextFile(path, content) | boolean | file.write |
| utils.Glob(pattern) | string[] | file.list |
| utils.ListFiles(folder, recursive?) | string[] | file.list |
| utils.ListFolders(folder, recursive?) | string[] | file.list → res.directories |

### 其他方法 

| 方法 | 返回 | 后端 API |
| --- | --- | --- |
| utils.GetClipboardText() | string | clipboard.read |
| utils.SetClipboardText(text) | void | clipboard.write |
| utils.CheckComponent(name, isDll?) | boolean | config.getComponents |
| utils.ReadINI(path, section, key, default?) | string | file.read + JS 解析 |
| utils.WriteINI(path, section, key, value) | boolean | file.read + file.write |
| utils.InputBox(wid, prompt, caption, default?) | string | window.prompt() |
| utils.ColourPicker(wid, default) | number | 返回默认值（无原生选择器） |

```javascript
// 文件操作
if (await utils.FileExists('C:\\\\config.json')) {
    const content = await utils.ReadTextFile('C:\\\\config.json');
}

// 通配搜索
const flacs = await utils.Glob('E:\\\\Music\\\\*.flac');

// 格式化
console.log(utils.FormatDuration(3661)); // "1:01:01"
console.log(utils.FormatFileSize(1048576)); // "1.00 MB"

// 路径拆分
const [dir, name, ext] = utils.SplitFilePath('E:\\\\Music\\\\song.flac');
// ["E:\\\\Music\\\\", "song", ".flac"]
```

## 行为差异说明 

### 与 SMP 的 API 差异 

| API | 差异 |
| --- | --- |
| plman.UndoBackup() | No-op — 后端自动创建 undo 备份点 |
| fb.GetQueryItems(handles, query) | 忽略 handles — 直接对媒体库查询 |
| plman.AddItemToPlaybackQueue(handle) | 路径匹配入队，subsong 可能丢失 |
| fb.SetOutputDevice(guid, deviceId) | 内部映射 guid → outputId 传给后端 |
| fb.RunContextCommandWithMetadb(cmd, h) | handles 被忽略 — 后端始终用默认上下文 |
| window.NotifyOthers(name, info) | payload 整体放入 message 字段（非 data） |
| utils.ListFiles/Glob/ListFolders | 非递归模式也返回完整路径（与 SMP 一致） |
| utils.ColourPicker() | 返回默认值 — WebView2 无原生颜色选择器 |
| 所有方法 | SMP 同步 → 兼容层返回 Promise |
| GDI / 画布 API | 不支持 — 使用 HTML/CSS/Canvas 替代 |

### 与底层 API 的一致性差异 

| 维度 | 底层 fb2k.invoke() | 兼容层 fb / plman |
| --- | --- | --- |
| 一致性 | 强一致（每次从后端读） | 最终一致（缓存 + 事件自愈） |
| 死锁风险 | 无 | 无（同架构） |
| 安全攻击面 | 基线 | 无增加（同一入口） |
| 时序依赖 | 无 | 有（需 await smp.ready） |
| 写入失败反馈 | 有（返回错误） | 无（fire-and-forget） |

## 迁移示例 

**SMP 原始脚本：**

```javascript
function on_playback_new_track(metadb) {
    var tf = fb.TitleFormat('%title% - %artist%');
    var title = tf.EvalWithMetadb(metadb);
    console.log('Now playing: ' + title);
}
```

**WebView2 迁移：**

```javascript
await window.smp.ready;

fb.onSMP('on_playback_new_track', async (track) => {
    const tf = fb.TitleFormat('%title% - %artist%');
    const title = await tf.EvalWithMetadb(track);
    console.log('Now playing: ' + title);
});
```

## 相关资源 

- [SDK 概述](/sdk/overview)
- [事件系统](/reference/events)

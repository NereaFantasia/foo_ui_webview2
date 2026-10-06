[English](./README.md) | 中文

# src/callbacks/ — foobar2000 事件 → 前端事件桥

`callbacks/` 是「事件回流」的源头：它监听 foobar2000 SDK 的各类回调（播放、播放列表、媒体库、元数据、队列、统计、DSP、配置、音频设备），把它们翻译成 colon 格式的桥接事件，经 `api::emit::Broadcast` 广播给所有 WebView 实例，让前端实时感知 foobar2000 的状态变化。

---

## 概述

这是桥接三段式里的「③ 事件回流」。与 `api/`（前端主动 invoke）相反，本模块是 foobar2000 主动通知前端。统一模式为：

```
foobar2000 SDK 回调 (on_playback_new_track / on_items_added / ...)
      └─ 填写声明生成的载荷结构体
            └─ api::emit::Broadcast<api::<ns>::events::<Event>>(payload)
                  └─ 各实例 postMessage → fb2k.on("namespace:event", cb)
```

> 命名约定：事件名一律 **colon 格式**（`playback:trackChanged`），与 invoke 的 dot 格式（`playback.play`）严格区分。前端监听不到事件时，第一时间核对 dot/colon 是否混淆。

---

## 注册方式：手动 vs 自动

回调有两种注册途径：

- **手动初始化**：在 `core/WebViewPanel::InitializeCallbacks()` 中显式调用 `InitXxxCallbacks()`。包括 `InitPlaybackCallbacks` / `InitPlaylistCallbacks` / `InitLibraryCallbacks` / `InitMetadbCallbacks`。
- **SDK 自动注册**：通过 `service_factory_single_t` / `playback_statistics_collector_factory_t` 在组件加载时自注册，无需手动初始化。包括队列、统计、DSP、配置回调。
- **动态注册**：`AudioConfigCallback` 由 `initquit` 在启动时注册、`AudioConfigCallbackManager` 管理生命周期。

---

## 关键文件 / 类与事件清单

| 文件 | 注册方式 | 监听的 SDK 变化 | 广播的事件 |
|------|---------|----------------|-----------|
| `PlaybackCallback.*` | 手动 `InitPlaybackCallbacks` | 播放状态、换曲、音量、进度、动态信息 | `playback:trackChanged` `playback:starting` `playback:stopped` `playback:paused` `playback:seeked` `playback:volumeChanged` `playback:time` `playback:timeHighRes` `playback:stateChanged` `playback:dynamicInfo` `playback:dynamicInfoTrack` `playback:edited` |
| `PlaylistCallback.*` | 手动 `InitPlaylistCallbacks` | 播放列表内容/选择/焦点/增删改、播放顺序 | `playlist:itemsAdded` `playlist:itemsRemoved` `playlist:itemsReordered` `playlist:itemsReplaced` `playlist:selectionChanged` `playlist:focusChanged` `playlist:created` `playlist:removed` `playlist:reordered` `playlist:activated` `playlist:renamed` `playlist:lockChanged` `playlist:defaultFormatChanged` `playback:orderChanged` |
| `LibraryCallback.*` | 手动 `InitLibraryCallbacks` | 媒体库增删改、初始化（并触发缓存失效） | `library:itemsAdded` `library:itemsRemoved` `library:itemsModified` `library:initialized` |
| `MetadbCallback.*` | 手动 `InitMetadbCallbacks` | 元数据变化（评分、标签、播放统计） | `metadb:changed` |
| `QueueCallback.*` | 自动 `service_factory_single_t` | 播放队列变化 | `playback:queueChanged` |
| `PlaybackStatsCallback.*` | 自动 `playback_statistics_collector_factory_t` | 曲目被认定「已播放」（>=60s 或结束且已播 >1/3） | `playback:itemPlayed` |
| `DspCallback.*` | 自动 `service_factory_single_t` | DSP 预设变化 | `audio:dspPresetChanged` |
| `ConfigCallback.*` | 自动 `service_factory_single_t`（`config_object_notify`，监听 4 个标准配置对象） | 总在最前、停在当前曲、播放跟随光标、光标跟随播放 | `window:alwaysOnTopChanged` `playback:stopAfterCurrentChanged` `playback:followCursorChanged` `playback:cursorFollowChanged` |
| `AudioConfigCallback.*` | 动态 `initquit` | 输出设备、ReplayGain 模式变化 | `audio:outputDeviceChanged` `audio:replaygainModeChanged` |

> `QueueCallback`（SDK 播放队列 `playback:queueChanged`）与 `core/QueueManager`（JIT 流媒体影子队列 `jitQueue:*`）是两套不同机制，勿混淆。

---

## 工作原理 / 数据流

以换曲为例：

```
用户点下一首 / 自动续播
   └─ playback_control 切换曲目
        └─ on_playback_new_track(metadb_handle_ptr)        (PlaybackCallback)
              ├─ BuildTrackRow(track) 构造共享的 Track 行
              ├─ api::emit::Broadcast<…::TrackChanged>(row)
              └─ QueueManager::OnPlaybackNewTrack(track)     (推进 JIT 状态机)
```

`LibraryCallback` 在广播 `library:*` 事件的同时，会调用 `LibraryCache::Invalidate()` / `LibraryTreeIndex::Invalidate()`，确保下次库查询读到最新数据——它既是事件源，也是缓存一致性的守护者。

部分高频事件（如选择变化、进度）在源头做了节流/去重（参见 `selection/SelectionWatcher` 的 50ms 节流、`PlaybackCallback` 的进度发射控制），避免压垮前端。

---

## 依赖关系

- **依赖**：foobar2000 SDK 回调接口（`play_callback` / `playlist_callback` / `library_callback` / `config_object_notify` 等）、`api/EventEmit.h`（经 `core/WebViewContext` 广播）、`core/QueueManager`、`window/TaskbarIntegration`（播放状态联动任务栏按钮）。
- **被依赖**：`core/WebViewPanel::InitializeCallbacks()` 负责拉起手动注册的回调；前端通过 `fb2k.on()` / SDK 的 `fb.on()` 消费这些事件。

---

## 扩展指南

新增一个事件（如 `playback:fooChanged`）：

1. **先写声明**：在 `api/schema/<ns>.ts` 的 `Events` 接口里声明事件与载荷，事件名用 colon 格式；然后按 [api/schema/README.md](../api/schema/README.md) 重新生成。确认 SDK 确实有对应回调。
2. **选注册方式**：能用 `service_factory_single_t` 自注册就自注册；需要随面板生命周期初始化的，放进 `InitXxxCallbacks()` 并在 `WebViewPanel::InitializeCallbacks()` 调用。
3. **发送**：在回调实现里用 `api/EventEmit.h` 的助手发出，例如 `api::emit::Broadcast<api::playback::events::FooChanged>(payload)`；只发给单个页面时用 `Emit` / `EmitTo` / `SendTo`，与声明里的 `@delivery` 一致。
4. **同步**：SDK 的事件类型与文档站的事件区块由声明生成；声明里的说明就是对外文档。
5. **节流**：高频事件务必在源头去重/节流，避免前端被刷爆。

---

参见：仓库根 [README.md](../../README.md)「事件 (colon 格式)」、[../api/README.zh-CN.md](../api/README.zh-CN.md)、[../core/README.zh-CN.md](../core/README.zh-CN.md)。

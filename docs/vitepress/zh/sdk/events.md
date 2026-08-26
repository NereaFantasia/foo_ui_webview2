# SDK 事件系统

事件名使用冒号格式，例如 `playback:trackChanged`；方法调用使用点格式，例如 `playback.play`。

## 订阅事件

```javascript
// 订阅事件，返回取消订阅函数
const unsubscribe = fb.on('playback:trackChanged', (track) => {
    console.log('Now playing:', track.title);
});

// 取消订阅
unsubscribe();
```

## 一次性事件

```javascript
fb.once('playback:trackChanged', (track) => {
    console.log('First track:', track.title);
});
```

## 取消订阅

```javascript
// 方式一：使用返回的 unsubscribe 函数（推荐）
const unsub = fb.on('playback:time', handler);
unsub();

// 方式二：手动 off
fb.on('playback:time', handler);
fb.off('playback:time', handler);
```

## 播放事件

| 事件名 | 触发时机 | 数据 |
| --- | --- | --- |
| playback:trackChanged | 曲目切换 | {title, artist, album, duration, path, id, absolutePath} |
| playback:stateChanged | 播放状态变化 | {state, position, duration} (state: "playing" / "paused" / "stopped") |
| playback:paused | 播放/暂停切换 | {paused} |
| playback:stopped | 播放停止 | {reason} ("user" / "eof" / "starting_another" / "shutting_down") |
| playback:starting | 播放即将开始 | {command, paused} (command: "play" / "next" / "previous" / "random") |
| playback:seeked | 播放位置跳转 | {position} |
| playback:volumeChanged | 音量变化 | {volume, volumeDb, muted, isMuted} |
| playback:time | 播放时间更新（~1秒/次，整数秒） | {position} |
| playback:timeHighRes | 高精度时间更新（~100ms，小数秒，C++ 定时器轮询真实位置） | {position} |
| playback:orderChanged | 播放顺序变化 | {orderIndex, order} |
| playback:queueChanged | 播放队列变化 | {origin} ("user_added" / "user_removed" / "playback_advance") |
| playback:edited | 当前曲目元数据被编辑 | {title, artist, album, duration, path, ...} |
| playback:dynamicInfo | 动态信息变化（网络流） | {bitrate, streamTitle?} |
| playback:dynamicInfoTrack | 动态曲目信息变化 | {artist?, title?} |
| playback:itemPlayed | 曲目播放完成 | {title, artist, album, duration, path, ...} (完整 track info) |
| playback:stopAfterCurrentChanged | "当前曲目后停止"状态变化 | {enabled} |
| playback:followCursorChanged | "播放跟随光标"变化 | {enabled} |
| playback:cursorFollowChanged | "光标跟随播放"变化 | {enabled} |

## 播放列表事件

| 事件名 | 触发时机 | 数据 |
| --- | --- | --- |
| playlist:activated | 活动播放列表切换 | {oldIndex, newIndex} |
| playlist:created | 播放列表创建 | {index, name} |
| playlist:removed | 播放列表删除 | {oldCount, newCount} |
| playlist:renamed | 播放列表重命名 | {index, name} |
| playlist:reordered | 播放列表顺序变化 | {count} |
| playlist:lockChanged | 锁定状态变化 | {playlist, locked} |
| playlist:itemsAdded | 曲目添加 | {playlist, start, count} |
| playlist:itemsRemoved | 曲目移除 | {playlist, oldCount, newCount} |
| playlist:itemsReordered | 曲目重新排序 | {playlist, count} |
| playlist:itemsReplaced | 曲目替换 | {playlist, count} |
| playlist:selectionChanged | 选中项变化 | {playlist} |
| playlist:focusChanged | 焦点曲目变化 | {playlist, from, to} |
| playlist:defaultFormatChanged | 默认格式变化 | - |
| playlist:addComplete | 异步添加完成（广播） | {operationId, success, addedCount, totalCount} |

## 媒体库 / 元数据事件

| 事件名 | 触发时机 | 数据 |
| --- | --- | --- |
| library:itemsAdded | 媒体库新增项目 | {count, timestamp} |
| library:itemsRemoved | 媒体库移除项目 | {count, timestamp} |
| library:itemsModified | 媒体库项目修改 | {count, timestamp} |
| library:initialized | 媒体库初始化完成 | {timestamp} |
| library:getAllResult | 异步 `library.getAll` 结果就绪 | LibraryGetAllResultPayload |
| metadata:writeComplete | 异步元数据写入完成 | MetadataWriteCompletePayload |
| metadata:probeProgress | 异步批量探测的一批结果就绪 | MetadataProbeProgressPayload |
| metadata:probeComplete | 异步批量探测收尾（完成 / 取消 / 失败） | MetadataProbeCompletePayload |
| metadb:changed | 元数据变化 | {tracks, count, fromHook, timestamp} |
| selection:changed | 全局选择变化（50ms 节流） | {count, type, handles, truncated, track?, nowPlaying?} |

## 音频事件

| 事件名 | 触发时机 | 数据 |
| --- | --- | --- |
| audio:spectrum | 频谱数据更新（需订阅） | {spectrum, fftSize?, bands?} |
| audio:stream | 音频流数据更新（需调用 subscribeStream 订阅） | 目前类型为空对象 |
| audio:dspPresetChanged | DSP 预设变化 | - |
| audio:outputDeviceChanged | 输出设备变化 | - |
| audio:replaygainModeChanged | ReplayGain 模式变化 | {mode} |
| audio:fullWaveformReady | 完整波形生成完成 | {taskId, path, waveform, duration, ...} |
| audio:fullWaveformFailed | 完整波形生成失败 | {taskId, path, error, code} |

## 窗口 / 面板 / UI 事件

| 事件名 | 触发时机 | 数据 |
| --- | --- | --- |
| window:alwaysOnTopChanged | 置顶状态变化 | {enabled} |
| window:stateChanged | 窗口状态变化 | 规范字段 {isMaximized, isMinimized, isActive, isFullscreen}，另带兼容别名 maximized / minimized / active / fullscreen |
| window:popupOpened | 弹出窗口打开 | {windowId, title, url} |
| window:popupClosed | 弹出窗口关闭 | {windowId} |
| window:beforeClose | 窗口关闭前确认 | {windowId} |
| window:message | 跨窗口消息接收 | {sourceWindowId, message} |
| window:behaviorChanged | 弹窗行为策略变更 | {windowId, profile, behavior, resolvedBehavior} |
| window:minimizeSuppressed | 抑制最小化（Win+D 策略） | {windowId, reason} |
| window:backdropStateChanged | 主窗口 / popup 的 DWM 激活/失焦策略生效 | {windowId, active, mode, effect} |
| window:hoverStateChanged | 窗口悬停状态变化 | {windowId, reason?, hovering?} |
| panel:initialized | 面板初始化完成 | {mode, panelMode, windowId} |
| panel:focus | 面板获得焦点 | - |
| panel:blur | 面板失去焦点 | - |
| panel:visibilityChanged | DUI 面板可见性变化 | {visible} |
| panel:configChanged | 面板配置变化 | {panelName, templateName, edgeStyle, transparentBackground, grabFocus, enableDragDrop, enableDevTools, urlOverride} |
| ui:coloursChanged | 颜色主题变化 | - |
| ui:fontChanged | 字体变化 | - |
| ui:menuItemClicked | 菜单项点击 | {id, label} |
| ui:toast | 通知消息 | {message, duration, type, position} |
| system:themeChanged | 系统主题变化 | {darkMode} |
| cursor:hiddenChanged | 光标隐藏状态变化 | CursorHiddenChangedPayload |
| taskbar:buttonClicked | 任务栏按钮被点击 | {id} |
| webview:processFailed | WebView 进程失败或恢复 | {kind, kindRaw, recovered, recoveryAction} |

## Tray 事件

托盘图标为应用级（无来源窗口），事件广播到所有窗口。

| 事件名 | 触发时机 | 数据 |
| --- | --- | --- |
| tray:click | 单击托盘图标 | {button, x, y} |
| tray:doubleClick | 双击托盘图标 | {x, y} |
| tray:beforeContextMenu | 右键菜单弹出前（异步；处理器内修改仅影响下一次右键） | {x, y} |
| tray:menuItemClicked | 普通用户托盘项点击 / 富控件值变更 | {id, value?} |

`tray:menuItemClicked` 的 `value` 语义：普通用户项与 now-playing 卡片回报 `{id}` 并**关闭**菜单；富**值控件**回报 `{id, value}` 并**保持菜单打开**——`rating` 为 `0..5`，`slider` 为 `[min, max]` 内的整数，`segmented` 为被选中分段的从 0 起索引。忽略 `value` 的前端保持既有 `{id}` 行为。内置 `showPlaybackControls` / `showSystemItems` 注入项，以及声明了 `playbackAction` 的项，由插件原生执行且**不发**此事件。

## 应用 / 键盘 / 菜单事件

| 事件名 | 触发时机 | 数据 |
| --- | --- | --- |
| app:beforeQuit | foobar2000 即将退出 | - |
| keyboard:hotkey | 已注册快捷键触发 | {id, key, action} |
| menu:show | 自定义菜单打开 | {menuId} |
| menu:select | 选中自定义菜单项 | {menuId, itemId} |
| menu:valueChanged | 自定义菜单里 rating / slider / segmented 值变更，菜单保持打开 | {menuId, itemId, value} |
| menu:dismiss | 自定义菜单关闭 | {menuId, reason} |

## 跨窗口通信事件

| 事件名 | 触发时机 | 数据 |
| --- | --- | --- |
| port:connected | 命名通道连接 | {portId, name, windowId} |
| port:disconnected | 命名通道断开 | {portId, name, windowId} |
| port:message | 收到跨窗口消息 | {portId, sourcePortId, sourceWindowId, message} |
| state:changed | 共享状态变更 | {key, value, previousValue, sourceWindowId, expiresAt?} |
| state:deleted | 共享状态删除或过期 | {key, sourceWindowId, reason} |

## 文件操作事件

| 事件名 | 触发时机 | 数据 |
| --- | --- | --- |
| file:opProgress | 异步 `file.copyAsync` / `moveAsync` / `deleteAsync` 的一批结果就绪 | FileOpProgressPayload |
| file:opComplete | 异步文件操作收尾（完成 / 取消） | FileOpCompletePayload |

两者都投递给发起该操作的那个窗口，这是 `results` 里可以带真实路径的前提。该窗口一旦销毁就解析不到，事件会回退投递到主实例，主实例未挂 WebView 时则被静默丢弃；什么情况下还会收到收尾事件见 [`cancelOp()`](/zh/sdk/file-io#cancelop-operationid)。进度按 64 条或 100 ms 先到者分批，最后的残余批必定排在 `file:opComplete` 之前。

## 插件事件

| 事件名 | 触发时机 | 数据 |
| --- | --- | --- |
| plugin:registered | 外部插件注册 | {namespace, name, version} |
| plugin:unregistered | 外部插件注销 | {namespace} |
| api:registered | 外部 API 注册 | {namespace, method} |
| api:unregistered | 外部 API 注销 | {namespace, method} |
| http:response | 异步 HTTP 请求完成 | {requestId, success, status, body, headers} |
| http:downloadComplete | 下载完成 | HttpDownloadCompletePayload |
| jitQueue:needNext | 后端请求下一首 | {currentTrackId, reason} (reason: "trackChange" / "prefetch") |
| jitQueue:trackChanged | 播放曲目变化 | {trackId, title} |
| jitQueue:listExhausted | 缓冲区耗尽 | {lastTrackId} |
| jitQueue:preloadComplete | JIT 预加载完成 | JitQueuePreloadCompletePayload |
| jitQueue:error | 错误 | {trackId, error, url} |

所有事件同时派发为 `fb2k:*` DOM 事件（CustomEvent）。

完整事件参考及字段说明见 [事件系统参考](/zh/reference/events)。 SMP 事件名映射见 [SMP 兼容层](/zh/reference/smp-compat#smp-events)。

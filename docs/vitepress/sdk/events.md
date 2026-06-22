# SDK 事件系统 

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
| metadb:changed | 元数据变化 | {tracks, count, fromHook, timestamp} |
| selection:changed | 全局选择变化（50ms 节流） | {count, type, handles, truncated, track?, nowPlaying?} |

## 音频事件 

| 事件名 | 触发时机 | 数据 |
| --- | --- | --- |
| audio:spectrum | 频谱数据更新（需订阅） | {spectrum} |
| audio:stream | 音频流数据更新（需调用 subscribeStream 订阅） | 音频流数据 |
| audio:dspPresetChanged | DSP 预设变化 | - |
| audio:outputDeviceChanged | 输出设备变化 | - |
| audio:replaygainModeChanged | ReplayGain 模式变化 | {mode} |
| audio:fullWaveformReady | 完整波形生成完成 | {taskId, path, waveform, duration, ...} |
| audio:fullWaveformFailed | 完整波形生成失败 | {taskId, path, error, code} |

## 窗口 / 面板 / UI 事件 

| 事件名 | 触发时机 | 数据 |
| --- | --- | --- |
| window:alwaysOnTopChanged | 置顶状态变化 | {enabled} |
| window:stateChanged | 窗口状态变化 | 规范字段 {isMaximized, isMinimized}（运行时暂兼容 maximized / minimized） |
| window:popupOpened | 弹出窗口打开 | {windowId, title, url} |
| window:popupClosed | 弹出窗口关闭 | {windowId} |
| window:beforeClose | 窗口关闭前确认 | {windowId} |
| window:message | 跨窗口消息接收 | {sourceWindowId, message} |
| window:behaviorChanged | 弹窗行为策略变更 | {windowId, profile, behavior, resolvedBehavior} |
| window:minimizeSuppressed | 抑制最小化（Win+D 策略） | {windowId, reason} |
| window:backdropStateChanged | 主窗口 / popup 的 DWM 激活/失焦策略生效 | {windowId, active, mode, effect} |
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

## 其他事件 

| 事件名 | 触发时机 | 数据 |
| --- | --- | --- |
| app:beforeQuit | foobar2000 即将退出 | - |
| keyboard:hotkey | 键盘快捷键 | {action, key} |

## 跨窗口通信事件 

| 事件名 | 触发时机 | 数据 |
| --- | --- | --- |
| port:connected | 命名通道连接 | {portId, name, windowId} |
| port:disconnected | 命名通道断开 | {portId, name, windowId} |
| port:message | 收到跨窗口消息 | {portId, sourcePortId, sourceWindowId, message} |
| state:changed | 共享状态变更 | {key, value, sourceWindowId} |
| state:deleted | 共享状态删除 | {key, sourceWindowId} |

## 插件事件 

| 事件名 | 触发时机 | 数据 |
| --- | --- | --- |
| plugin:registered | 外部插件注册 | {namespace, name, version} |
| plugin:unregistered | 外部插件注销 | {namespace} |
| api:registered | 外部 API 注册 | {namespace, method} |
| api:unregistered | 外部 API 注销 | {namespace, method} |
| http:response | 异步 HTTP 请求完成 | {requestId, success, status, body, headers} |
| jitQueue:needNext | 后端请求下一首 | {currentTrackId, reason} (reason: "trackChange" / "prefetch") |
| jitQueue:trackChanged | 播放曲目变化 | {trackId, title} |
| jitQueue:listExhausted | 缓冲区耗尽 | {lastTrackId} |
| jitQueue:error | 错误 | {trackId, error, url} |

所有事件同时派发为 `fb2k:*` DOM 事件（CustomEvent）。

完整事件参考及字段说明见 [事件系统参考](/reference/events)。 SMP 事件名映射见 [SMP 兼容层](/reference/smp-compat#事件系统)。

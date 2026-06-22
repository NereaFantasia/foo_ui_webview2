# Events API - 事件监听系统 

foo_ui_webview2 提供了完整的事件系统，允许前端监听 C++ 后端推送的各种事件。

## 事件命名约定 

::: warning 重要
事件名使用 **冒号格式** (`namespace:eventName`)，与 API 调用的点格式 (`namespace.methodName`) 不同。
:::

```javascript
// ✅ 正确 - API 调用用点，事件监听用冒号
await fb2k.invoke('playback.play');
fb2k.on('playback:stateChanged', callback);

// ❌ 错误 - 事件名不能用点
fb2k.on('playback.stateChanged', callback);  // 永远不会触发
```

## 监听事件 

使用 `fb2k.on()` 或 `window.listen()` 监听事件：

```javascript
// 方式 1: 使用 fb2k.on()
fb2k.on('playback:trackChanged', (data) => {
    console.log('新曲目:', data.track);
});

// 方式 2: 使用 window.listen()
window.listen('playback:trackChanged', (data) => {
    console.log('新曲目:', data.track);
});
```

## 取消监听 

```javascript
// 保存监听器引用
const handler = (data) => {
    console.log('事件触发:', data);
};

// 注册监听
fb2k.on('playback:stateChanged', handler);

// 取消监听
fb2k.off('playback:stateChanged', handler);
```

## 核心事件列表 

### Playback 事件 

| 事件名 | 触发时机 | 数据结构 |
| --- | --- | --- |
| playback:trackChanged | 曲目切换 | 完整 track info 对象（直接发送，无 track 包装键） |
| playback:stateChanged | 播放状态变化 | `{ state: "playing" \| "paused" \| "stopped", position: number, duration: number }` |
| playback:paused | 暂停状态变化 | `{ paused: boolean }` |
| playback:stopped | 停止播放 | `{ reason: "user" \| "eof" \| "starting_another" \| "shutting_down" \| "unknown" }` |
| playback:seeked | 跳转位置 | `{ position: number }` |
| playback:volumeChanged | 音量变化 | `{ volume: number, volumeDb: number, muted: boolean, isMuted: boolean }` |
| playback:time | 播放进度（~1秒/次，整数秒） | `{ position: number }` |
| playback:timeHighRes | 高精度播放进度（~100ms，小数秒，C++ 定时器轮询真实位置） | `{ position: number }` |
| playback:starting | 播放即将开始 | `{ command: "play" \| "next" \| "previous" \| "random" \| "unknown", paused: boolean }` |
| playback:dynamicInfo | 流媒体动态信息变化 | `{ bitrate: number, streamTitle?: string }` |
| playback:dynamicInfoTrack | 流媒体曲目信息变化 | `{ artist?: string, title?: string }` |
| playback:edited | 当前曲目元数据被编辑 | 完整 track info 对象（同 playback:trackChanged） |
| playback:orderChanged | 播放顺序变化 | `{ orderIndex: number, order: number }` |
| playback:queueChanged | 播放队列变化 | `{ origin: "user_added" \| "user_removed" \| "playback_advance" \| "unknown" }` |
| playback:itemPlayed | 曲目播放完成（统计） | 完整 track info 对象 |
| playback:stopAfterCurrentChanged | “当前曲目后停止”变化 | `{ enabled: boolean }` |
| playback:followCursorChanged | “播放跟随光标”变化 | `{ enabled: boolean }` |
| playback:cursorFollowChanged | “光标跟随播放”变化 | `{ enabled: boolean }` |

### Playlist 事件 

| 事件名 | 触发时机 | 数据结构 |
| --- | --- | --- |
| playlist:itemsAdded | 曲目添加 | `{ playlist: number, count: number }` |
| playlist:itemsRemoved | 曲目删除 | `{ playlist: number, count: number }` |
| playlist:itemsReordered | 曲目重排 | `{ playlist: number }` |
| playlist:itemsReplaced | 曲目替换 | `{ playlist: number, count: number }` |
| playlist:activated | 激活列表变化 | `{ playlist: number }` |
| playlist:created | 创建播放列表 | `{ playlist: number, name: string }` |
| playlist:removed | 删除播放列表 | `{ playlist: number }` |
| playlist:renamed | 重命名播放列表 | `{ playlist: number, name: string }` |
| playlist:selectionChanged | 选择变化 | `{ playlist: number }` |
| playlist:focusChanged | 焦点曲目变化 | `{ playlist: number, from: number, to: number }` |
| playlist:reordered | 播放列表顺序变化 | `{ count: number }` |
| playlist:lockChanged | 播放列表锁定状态变化 | `{ playlist: number, locked: boolean }` |
| playlist:defaultFormatChanged | 默认格式变化 | `{}` |
| playlist:addComplete | 异步添加完成 | `{ operationId: string, success: boolean, addedCount: number, totalCount: number }` |

### UI 事件 

| 事件名 | 触发时机 | 数据结构 |
| --- | --- | --- |
| ui:toast | Toast 提示 | `{ message: string, duration: number, type: string, position: string }` |
| ui:menuItemClicked | 菜单项点击 | `{ id: string, label: string }` |

### Keyboard 事件 

| 事件名 | 触发时机 | 数据结构 |
| --- | --- | --- |
| keyboard:hotkey | 全局热键触发 | `{ id: string, key: string, action: string }` |

### HTTP 事件 

| 事件名 | 触发时机 | 数据结构 |
| --- | --- | --- |
| http:response | HTTP 请求完成 | `{ requestId: string, statusCode: number, body: string, headers: object }` 或 `{ requestId: string, success: false, error: string, code: ApiErrorCode }` |

### Audio 事件 

| 事件名 | 触发时机 | 数据结构 |
| --- | --- | --- |
| audio:spectrum | 频谱数据更新 | `{ spectrum: number[] }` |
| audio:outputDeviceChanged | 音频输出设备变化 | `{}` |
| audio:replaygainModeChanged | ReplayGain 模式变化 | `{ mode: number }` |
| audio:dspPresetChanged | DSP 预设变化 | `{}` |
| audio:fullWaveformReady | 全波形数据生成完成 | `{ taskId: string, path: string, peaks: number[], sampleRate: number }` |
| audio:fullWaveformFailed | 全波形数据生成失败 | `{ taskId: string, path: string, error: string, code: string }` |

### Metadata 事件 

| 事件名 | 触发时机 | 数据结构 |
| --- | --- | --- |
| metadb:changed | 评分/标签/统计变化（广播到所有窗口） | `{ tracks: Array, count: number, fromHook: boolean, timestamp: number }` |

### Plugin 事件 

| 事件名 | 触发时机 | 数据结构 |
| --- | --- | --- |
| plugin:registered | 外部插件注册 | `{ namespace: string, name: string, version: string }` |
| plugin:unregistered | 外部插件注销 | `{ namespace: string }` |
| api:registered | 外部 API 注册 | `{ namespace: string, method: string }` |
| api:unregistered | 外部 API 注销 | `{ namespace: string, method: string }` |

### Library 事件 

| 事件名 | 触发时机 | 数据结构 |
| --- | --- | --- |
| library:initialized | 媒体库初始化完成 | `{ timestamp: number }` |
| library:itemsAdded | 媒体库新增条目 | `{ count: number }` |
| library:itemsRemoved | 媒体库移除条目 | `{ count: number }` |
| library:itemsModified | 媒体库条目被修改 | `{ count: number }` |

### Window 事件 

| 事件名 | 触发时机 | 数据结构 |
| --- | --- | --- |
| window:stateChanged | 窗口状态变化 | `规范字段 { isMaximized: boolean, isMinimized: boolean }（运行时暂兼容 maximized / minimized）` |
| window:popupOpened | 弹出窗口打开 | `{ windowId: string, title: string, url: string }` |
| window:popupClosed | 弹出窗口关闭 | `{ windowId: string }` |
| window:beforeClose | 窗口即将关闭（仅当前窗口） | `{ windowId: string }` |
| window:behaviorChanged | 窗口行为策略变化 | `{ windowId: string, profile: string, behavior: object, resolvedBehavior: object }` |
| window:minimizeSuppressed | 最小化被策略抑制 | `{ windowId: string, reason: string }` |
| window:backdropStateChanged | 背景特效状态变化 | `{ windowId: string, active: boolean, mode: string, effect: string }` |
| window:message | 跨窗口消息 | `{ sourceWindowId: string, message: any }` |
| window:alwaysOnTopChanged | 置顶状态变化 | `{ enabled: boolean }` |

### Cursor 事件

| 事件名 | 触发时机 | 数据结构 |
| --- | --- | --- |
| cursor:hiddenChanged | `cursor.setHidden` 真实翻转隐藏标志位时（每窗口独立，不跨窗口广播） | `{ hidden: boolean }` |

### Panel 事件 

仅在面板模式（CUI/DUI）下触发，每个事件仅发射到当前面板实例。

| 事件名 | 触发时机 | 数据结构 |
| --- | --- | --- |
| panel:initialized | 面板初始化完成 | `{ mode: "dui" \| "cui", panelMode: true, windowId: string }` |
| panel:focus | 面板获得焦点 | `{}` |
| panel:blur | 面板失去焦点 | `{}` |
| panel:configChanged | 面板配置变化 | `{ panelName: string, templateName: string, edgeStyle: number, transparentBackground: boolean, ... }` |
| panel:visibilityChanged | 面板可见性变化（仅 DUI） | `{ visible: boolean }` |

### System / UI 事件 

| 事件名 | 触发时机 | 数据结构 |
| --- | --- | --- |
| system:themeChanged | 系统主题变化（仅 DUI 面板） | `{ darkMode: boolean }` |
| ui:coloursChanged | 颜色配置变化（仅 DUI 面板） | `{}` |
| ui:fontChanged | 字体配置变化（仅 DUI 面板） | `{}` |

### JIT Queue 事件 

即时队列管理事件，仅发射到当前窗口。

| 事件名 | 触发时机 | 数据结构 |
| --- | --- | --- |
| jitQueue:trackChanged | 当前曲目变化 | `{ trackId: string, title: string }` |
| jitQueue:needNext | 需要下一首曲目 | `{ currentTrackId: string, reason: "prefetch" \| "trackChange" }` |
| jitQueue:listExhausted | 队列已耗尽 | `{ lastTrackId: string }` |
| jitQueue:error | 队列错误 | `{ trackId: string, error: string, url?: string, path?: string }` |

### Port 事件 

| 事件名 | 触发时机 | 数据结构 |
| --- | --- | --- |
| port:connected | 端口连接 | `{ portId: string, name: string, windowId: string }` |
| port:disconnected | 端口断开 | `{ portId: string, name: string, windowId: string }` |
| port:message | 收到跨窗口消息 | `{ portId: string, sourcePortId: string, sourceWindowId: string, message: any }` |

### State 事件 

| 事件名 | 触发时机 | 数据结构 |
| --- | --- | --- |
| state:changed | 共享状态变化 | `{ key: string, value: any, previousValue: any, sourceWindowId: string, expiresAt?: number }` |
| state:deleted | 共享状态删除 | `{ key: string, sourceWindowId: string, reason: "deleted" \| "expired" }` |

### Selection 事件 

| 事件名 | 触发时机 | 数据结构 |
| --- | --- | --- |
| selection:changed | 选择集变化 | `{ count: number, type: string, handles: string[], truncated: boolean, track?: object, nowPlaying?: object }` |

### App 事件 

| 事件名 | 触发时机 | 数据结构 |
| --- | --- | --- |
| app:beforeQuit | 应用即将退出 | `{}` |

## 事件使用示例 

### 监听播放状态 

```javascript
// 监听曲目切换
fb2k.on('playback:trackChanged', (data) => {
    document.title = `${data.track.artist} - ${data.track.title}`;
});

// 监听播放/暂停
fb2k.on('playback:stateChanged', (data) => {
    if (data.state === 'playing') {
        playButton.textContent = '⏸';
    } else {
        playButton.textContent = '▶';
    }
});

// 监听播放进度（节流到 100ms）
fb2k.on('playback:time', (data) => {
    progressBar.value = data.position / data.duration * 100;
});
```

### 监听播放列表变化 

```javascript
// 监听曲目添加
fb2k.on('playlist:itemsAdded', async (data) => {
    console.log(`播放列表 ${data.playlist} 添加了 ${data.count} 首曲目`);
    // 刷新播放列表显示
    await refreshPlaylist(data.playlist);
});

// 监听异步添加完成
fb2k.on('playlist:addComplete', (data) => {
    if (data.success) {
        console.log(`操作 ${data.operationId} 完成: ${data.addedCount}/${data.totalCount}`);
    }
});
```

### 监听 UI 事件 

```javascript
// 监听 Toast 事件（由前端渲染）
fb2k.on('ui:toast', (data) => {
    showToast(data.message, data.type, data.duration);
});

// 监听菜单项点击
fb2k.on('ui:menuItemClicked', (data) => {
    console.log(`菜单项 ${data.id} (${data.label}) 被点击`);
});
```

### 监听全局热键 

```javascript
// 注册热键
const result = await fb2k.invoke('keyboard.registerHotkey', {
    key: 'MediaPlayPause',
    action: 'toggle-playback'
});
const hotkeyId = result.id;

// 监听热键触发
fb2k.on('keyboard:hotkey', async (data) => {
    if (data.id === hotkeyId) {
        await fb2k.invoke('playback.playOrPause');
    }
});
```

### 监听 HTTP 响应 

```javascript
// 发起异步 HTTP 请求
const result = await fb2k.invoke('http.get', {
    url: 'https://api.example.com/data',
    async: true  // 异步模式
});
const requestId = result.requestId;

// 监听响应
fb2k.on('http:response', (data) => {
    if (data.requestId === requestId) {
        if (data.error) {
            console.error('请求失败:', data.error);
        } else {
            console.log('响应:', data.body);
        }
    }
});
```

### 监听频谱数据 

```javascript
// 订阅频谱数据
const subscriptionId = 'custom-spectrum-demo';

await fb2k.invoke('audio.subscribeSpectrum', {
    subscriptionId,
    fftSize: 2048,
    bands: 96,
    fps: 60,
    event: 'audio:customSpectrum'
});

// 监听频谱更新（频率由订阅时的 fps 决定）
fb2k.on('audio:customSpectrum', (data) => {
    drawSpectrum(data.spectrum);  // data.spectrum 是归一化 number[]
});

// 取消订阅
await fb2k.invoke('audio.unsubscribeSpectrum', { subscriptionId });
```

`audio:spectrum` 与自定义频谱事件的 payload 保持一致，当前都是：

```json
{
  "spectrum": [0.1, 0.3, 0.5]
}
```

## 事件节流 

某些高频事件会自动节流以避免性能问题：

| 事件 | 节流间隔 |
| --- | --- |
| playback:time | 100ms |
| audio:spectrum | 约 1000 / fps ms（由 audio.subscribeSpectrum 的 fps 决定） |

## 最佳实践 

### 1. 及时取消监听 

```javascript
// 组件卸载时取消监听
onUnmounted(() => {
    fb2k.off('playback:trackChanged', handleTrackChange);
});
```

### 2. 使用命名函数便于调试 

```javascript
// ✅ 推荐 - 便于调试和取消监听
function handleTrackChange(data) {
    console.log('Track changed:', data);
}
fb2k.on('playback:trackChanged', handleTrackChange);

// ❌ 不推荐 - 难以取消监听
fb2k.on('playback:trackChanged', (data) => {
    console.log('Track changed:', data);
});
```

### 3. 避免在事件处理器中执行耗时操作 

```javascript
// ❌ 错误 - 阻塞事件循环
fb2k.on('playback:time', (data) => {
    // 耗时的 DOM 操作
    for (let i = 0; i < 1000; i++) {
    if (!pendingUpdate) {
        pendingUpdate = true;
        requestAnimationFrame(() => {
            updateUI(data);
            pendingUpdate = false;
        });
    }
});
```

### 4. 使用事件聚合减少监听器数量 

```javascript
// ❌ 不推荐 - 多个监听器
fb2k.on('playlist:itemsAdded', refreshPlaylist);
fb2k.on('playlist:itemsRemoved', refreshPlaylist);
fb2k.on('playlist:itemsReordered', refreshPlaylist);

// ✅ 推荐 - 单个监听器处理多个事件
const playlistEvents = [
    'playlist:itemsAdded',
    'playlist:itemsRemoved',
    'playlist:itemsReordered'
];
playlistEvents.forEach(event => {
    fb2k.on(event, refreshPlaylist);
});
```

## 调试事件 

### 查看所有事件 

```javascript
// 监听所有事件（调试用）
const originalOn = fb2k.on;
fb2k.on = function(event, handler) {
    console.log('[Event] Registered:', event);
    return originalOn.call(this, event, handler);
};
```

### 记录事件触发 

```javascript
// 记录所有事件触发
fb2k.on('*', (event, data) => {
    console.log('[Event]', event, data);
});
```

## 相关文档 

- [Playback API](./playback) - 播放控制相关事件
- [Playlist API](./playlist) - 播放列表相关事件
- [UI Interaction API](./ui-interaction) - UI 交互相关事件
- [Audio API](./audio) - 音频分析相关事件

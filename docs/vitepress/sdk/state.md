# fb.state 响应式状态 

`fb.state` 是一个自动同步的状态对象，通过订阅播放事件实时更新。适合 UI 绑定和状态轮询场景，无需手动调用 API 获取当前状态。

## 属性 

| 属性 | 类型 | 说明 |
| --- | --- | --- |
| volume | number | 当前音量（0-100） |
| isPlaying | boolean | 是否正在播放（含暂停状态） |
| currentTrack | object \| null | 当前曲目信息（曲目切换时更新，停止时为 null） |
| position | number | 当前播放位置（秒），约每 500ms 更新 |

## 自动更新机制 

`fb.state` 通过监听以下事件自动更新：

| 事件 | 更新字段 |
| --- | --- |
| playback:stateChanged | isPlaying、position |
| playback:trackChanged | currentTrack、isPlaying = true |
| playback:stopped | isPlaying = false、position = 0 |
| playback:volumeChanged | volume |
| playback:time | position |

## 使用示例 

### 基本读取 

```javascript
console.log(fb.state.isPlaying);     // true
console.log(fb.state.volume);        // 80
console.log(fb.state.position);      // 42.5
console.log(fb.state.currentTrack);  // {title, artist, album, ...}
```

### 定时器轮询 UI 更新 

```javascript
setInterval(() => {
    document.getElementById('playing').textContent = fb.state.isPlaying ? '▶' : '⏸';
    document.getElementById('position').textContent = Math.floor(fb.state.position) + 's';
}, 500);
```

### 结合事件使用 

```javascript
// state 自动更新，事件回调可以直接读取 state
fb.on('playback:trackChanged', () => {
    const track = fb.state.currentTrack;
    document.title = `${track.title} - ${track.artist}`;
});
```

::: tip TIP
`fb.state` 是简单的状态快照，不是 Proxy 响应式对象。如需精确状态变化通知，请使用 `fb.on()` 订阅事件。如需精确的实时值，请使用 `fb.player.getState()` / `fb.player.getPosition()` 等 API。
:::

::: warning 与 SMP 兼容层的区别
`fb.state` 仅跟踪 4 个基本字段。如需完整的缓存状态（播放列表、配置等），请使用 SMP 兼容层的 `window.smp.cache`，详见 [SMP 兼容层](/reference/smp-compat)。
:::

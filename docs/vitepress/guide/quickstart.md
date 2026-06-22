# 快速开始 

## 最小示例 

将以下内容保存为 `index.html`，放到 `profile/foo_ui_webview2/` 目录下即可运行：

```html
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>My foobar2000 UI</title>
</head>
<body>
    <h1 id="track">等待播放...</h1>
    <button id="play">▶ 播放</button>
    <button id="prev">⏮ 上一首</button>
    <button id="next">⏭ 下一首</button>

    <script>
    document.getElementById('play').onclick = () => fb2k.invoke('playback.playOrPause');
    document.getElementById('prev').onclick = () => fb2k.invoke('playback.previous');
    document.getElementById('next').onclick = () => fb2k.invoke('playback.next');

    fb2k.on('playback:trackChanged', (data) => {
        document.getElementById('track').textContent = data.artist + ' - ' + data.title;
    });
    </script>
</body>
</html>
```

> `window.fb2k` 由插件自动注入，无需引入任何文件。如需 SDK 封装（`fb.player.play()` 等简化调用），请另外引入 `sdk/bridge.js`。

## Bridge 对象 

插件会自动注入 `window.fb2k` 对象：

```javascript
window.fb2k = {
    invoke(method, params) → Promise<result>,  // 调用 API
    on(event, callback) → unsubscribe,         // 监听事件
};
```

## 基础调用 

```javascript
// 1. 调用 API（所有调用都是异步的）
const result = await fb2k.invoke('playback.play');
console.log(result); // { success: true }

// 2. 获取当前曲目信息
const track = await fb2k.invoke('playback.getCurrentTrack');
console.log(track.title, track.artist);

// 3. 监听事件
const unsubscribe = fb2k.on('playback:trackChanged', (data) => {
    console.log('正在播放:', data.title);
});
// 取消订阅：unsubscribe();

// 4. 带参数调用（音量范围 0-100）
await fb2k.invoke('playback.setVolume', { volume: 80 });
```

## 完整播放器示例 

初始化播放控制事件监听

```javascript
async function initPlayer() {
    const state = await fb2k.invoke('playback.getState');
    updatePlayButton(state.state === 'playing');

    const volumeInfo = await fb2k.invoke('playback.getVolume');
    updateVolumeSlider(volumeInfo.volume);

    if (state.state === 'playing' || state.state === 'paused') {
        const track = await fb2k.invoke('playback.getCurrentTrack');
        updateTrackInfo(track);
    }
}
```

```javascript
document.getElementById('btn-play').onclick = async () => {
    await fb2k.invoke('playback.playOrPause');
};
document.getElementById('btn-prev').onclick = async () => {
    await fb2k.invoke('playback.previous');
};
document.getElementById('btn-next').onclick = async () => {
    await fb2k.invoke('playback.next');
};
```

```javascript
fb2k.on('playback:trackChanged', async (track) => {
    updateTrackInfo(track);
    const artwork = await fb2k.invoke('artwork.getCurrent');
    if (artwork.available) {
        document.getElementById('cover').src = artwork.dataUrl;
    }
});

fb2k.on('playback:stateChanged', (state) => {
    updatePlayButton(state.state === 'playing');
});

fb2k.on('playback:time', (data) => {
    const percent = (data.position / currentDuration) * 100;
    document.getElementById('progress-fill').style.width = percent + '%';
});
```

## 辅助函数 

```javascript
function formatTime(seconds) {
    const min = Math.floor(seconds / 60);
    const sec = Math.floor(seconds % 60);
    return `${min}:${sec.toString().padStart(2, '0')}`;
}

function isStaticLocalFile(path) {
    if (!path) return false;
    return !path.startsWith('http://') &&
           !path.startsWith('https://') &&
           !path.startsWith('cdda://');
}
```

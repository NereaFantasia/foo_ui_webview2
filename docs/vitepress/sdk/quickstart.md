# SDK 快速入门 

## 最小示例 

```html
<!DOCTYPE html>
<html>
<head>
    <script src="bridge.js"></script>
    <script src="components.js"></script>
</head>
<body>
    <!-- 使用 Web Components -->
    <fb-play-button></fb-play-button>
    <fb-prev-button></fb-prev-button>
    <fb-next-button></fb-next-button>
    <fb-seek-bar></fb-seek-bar>
    <fb-volume-bar></fb-volume-bar>

    <script>
        fb.on('playback:trackChanged', (track) => {
            document.title = `${track.title} - ${track.artist}`;
        });
    </script>
</body>
</html>
```

## 完整播放器示例 

```javascript
async function init() {
    // 获取当前曲目
    const track = await fb.player.getCurrentTrack();
    if (track) updateUI(track);

    // 获取所有播放列表
    const playlists = await fb.playlist.getAll();
    renderPlaylists(playlists);

    // 订阅事件
    fb.on('playback:trackChanged', updateUI);
    fb.on('playback:stateChanged', updatePlaybackState);
}

// 播放控制
document.getElementById('playBtn').onclick = () => fb.player.toggle();
document.getElementById('prevBtn').onclick = () => fb.player.prev();
document.getElementById('nextBtn').onclick = () => fb.player.next();

// 音量控制 (0-100)
document.getElementById('volumeSlider').oninput = (e) => {
    fb.player.setVolume(Number(e.target.value));
};

init();
```

# 不用构建工具写主题

主题可以只是一个带内联脚本的 `index.html`，用组件给每个页面注入的原生 bridge `window.fb2k`，不用安装也不用构建。

## 写页面

把下面的内容存为模板文件夹里的 `index.html`（怎样新建模板并设为活动模板，见[安装主题](./install-theme.md)）：

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

在偏好设置页按 `应用`，或重新加载窗口，就能看到它。

## 在页面里调用宿主

`fb2k.invoke(method, params)` 调用宿主方法并 resolve 其结果；`fb2k.on(event, handler)` 订阅事件，返回取消订阅的函数。

```javascript
// 所有调用都是异步的
await fb2k.invoke('playback.play');

// 结果带 success 字段，读其他字段前先检查它
const current = await fb2k.invoke('playback.getCurrentTrack');
if (current.success === false) throw new Error(current.error);
if (current.track) console.log(current.track.title, current.track.artist);

// 参数放在对象里（音量范围 0-100）
await fb2k.invoke('playback.setVolume', { volume: 80 });

// 订阅，之后取消
const unsubscribe = fb2k.on('playback:trackChanged', (data) => {
    console.log('正在播放:', data.title);
});
unsubscribe();
```

方法名用点号（`playback.play`），事件名用冒号（`playback:trackChanged`）。[底层 API 参考](/zh/api/overview)列出全部方法和事件，[Bridge 协议](/zh/reference/bridge)说明消息格式。

## 不用打包工具也用 SDK

要在这样的页面里使用 `fb.*` 封装和 `fb-*` 组件，把 SDK 的全局构建复制进模板文件夹，在你的脚本之前加载：

```html
<script src="./sdk/bridge.global.js"></script>
<script src="./sdk/components.global.js"></script>
<fb-play-button></fb-play-button>
<script>
    fb.on('playback:trackChanged', (track) => {
        document.title = `${track.title} - ${track.artist}`;
    });
</script>
```

文件从哪里来，见[在主题里加载 SDK](./load-sdk.md#不用打包工具)。

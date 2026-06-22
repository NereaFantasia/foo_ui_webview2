# SDK 概述 & 安装 

**foo_ui_webview2 SDK** 是对原生 API 的高层封装，将冗长的 API 调用简化为简洁的命名空间调用。

## 简化对比 

| 原生调用方式 | SDK 调用方式 |
| --- | --- |
| window.fb2k.invoke('playback.play', {}) | fb.player.play() |
| window.fb2k.invoke('playlist.addPaths', {playlist: 0, paths: [...]}) | fb.playlist.add(0, paths) |
| window.fb2k.invoke('playback.setVolume', {volume: 80}) | fb.player.setVolume(80) |

## 核心优势 

## 安装 

### 方式一：直接引入（推荐） 

```html
<!-- 加载 SDK 核心 + 组件 -->
<script src="./sdk/bridge.js"></script>
<script src="./sdk/components.js"></script>

<!-- 直接使用 fb.* API 与 fb-* 组件 -->
<script>
  fb.player.play();
</script>
<fb-play-button></fb-play-button>
```

### 方式二：ES Module 

```javascript
import 'foo-webview-sdk/bridge.js';
import 'foo-webview-sdk/components.js';
```

### 验证安装 

```javascript
if (fb.isAvailable()) {
    console.log('SDK 已就绪');
    fb.player.play();
} else {
    console.log('运行在 Mock 模式');
}
```

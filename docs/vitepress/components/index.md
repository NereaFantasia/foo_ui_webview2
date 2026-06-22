# 组件总览 

foo_ui_webview2 提供 **36 个无样式 Web Components** — 零视觉主张的功能积木，通过 CSS Parts、Slots 和自定义事件实现完全可定制的 UI。

## 快速开始 

```html
<!doctype html>
<html>
  <head>
    <meta charset="utf-8" />
    <link rel="stylesheet" href="./theme.css" />
  </head>
  <body>
    <!-- 加载 SDK 与组件 -->
    <script src="./sdk/bridge.js"></script>
    <script src="./sdk/components.js"></script>

    <!-- 直接使用 Web Components -->
    <fb-play-button></fb-play-button>
    <fb-seek-bar></fb-seek-bar>
    <fb-volume-control></fb-volume-control>
    <fb-track-text format="%title% — %artist%"></fb-track-text>
  </body>
</html>
```

## 设计原则 

- **零样式** — 组件不包含 color、background、font-family 等视觉属性，全部通过 CSS Parts 由外部样式化
- **CSS Parts** — 每个可样式化元素都暴露 `part` 属性，使用 `::part()` 选择器自定义
- **Slots** — 图标、占位内容等通过 `<slot>` 允许替换
- **事件驱动** — 所有用户交互通过 `CustomEvent`（bubbles + composed）向外传递

### 样式化示例 

所有组件都通过 `::part()` 选择器自定义外观：

```css
/* 播放按钮 — 圆形绿色风格 */
fb-play-button::part(button) {
    background: #1db954;
    border: none;
    border-radius: 50%;
    color: white;
    width: 48px;
    height: 48px;
    cursor: pointer;
}
fb-play-button::part(button):hover {
    background: #1ed760;
}

/* 进度条 — 自定义颜色 */
fb-seek-bar {
    --track-color: #333;
    --progress-color: #1db954;
    --thumb-color: #fff;
}

/* 星级评分 — 金色星星 */
fb-rating::part(star) {
    color: #888;
    font-size: 20px;
    cursor: pointer;
}
fb-rating::part(star) {
    color: #ffc107;
}
```

## 组件速查表 

### A. 播放控制（7 个） 

| 标签名 | 说明 |
| --- | --- |
|  | 播放/暂停切换 |
|  | 停止 |
|  | 上一首 |
|  | 下一首 |
|  | 随机播放切换 |
|  | 重复播放（off→playlist→track） |
|  | 播完当前曲目后停止 |

### B. 进度与音量（3 个） 

| 标签名 | 说明 |
| --- | --- |
|  | 播放进度条 |
|  | 音量控制（含静音按钮） |
|  | 播放顺序选择（select/button 双模式） |

### C. 曲目信息（6 个） 

| 标签名 | 说明 |
| --- | --- |
|  | 通用曲目文本（title/artist/album/自定义 TF） |
|  | 封面（推荐 use-fb2k） |
|  | 当前播放时间 |
|  | 总时长 |
|  | 剩余时间 |
|  | 技术信息（codec/bitrate/samplerate） |

### D. 播放列表（5 个） 

| 标签名 | 说明 |
| --- | --- |
|  | 播放列表标签页 |
|  | 可调列宽表头（拖拽/重排序/排序指示） |
|  | 虚拟滚动播放列表视图 |
|  | 播放队列 |
|  | 播放列表下拉选择器 |

### E. 窗口管理（3 个） 

| 标签名 | 说明 |
| --- | --- |
|  | 自定义标题栏（拖拽区域） |
|  | 窗口控制按钮（最小化/最大化/关闭） |
|  | 弹出窗口管理器 |

### F. 歌词与可视化（3 个） 

| 标签名 | 说明 |
| --- | --- |
|  | 歌词面板（同步/非同步） |
|  | 频谱可视化器 |
|  | 波形概览 |

### G. 评分与音频设置（4 个） 

| 标签名 | 说明 |
| --- | --- |
|  | 星级评分 |
|  | 音频输出设备选择器 |
|  | DSP 预设选择器 |
|  | ReplayGain 模式选择器 |

### H. 元数据与搜索（3 个） 

| 标签名 | 说明 |
| --- | --- |
|  | 曲目属性面板 |
|  | 媒体库搜索 |
|  | foobar2000 控制台 |

### I. 媒体库（2 个） 

| 标签名 | 说明 |
| --- | --- |
|  | 媒体库分组树浏览（artist/album/genre，非文件系统根树） |
|  | 文件系统型媒体库根树（基于 getRoots + browseTree 懒加载） |

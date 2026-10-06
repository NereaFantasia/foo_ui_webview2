# 组件总览 

foo_ui_webview2 提供 **36 个无样式 Web Components** — 不预设任何视觉样式的功能积木，通过 CSS Parts、Slots 和自定义事件实现完全可定制的 UI。

通过 SDK 加载它们：使用打包工具时调用 `registerComponents()`，不用打包工具时加载 `components.global.js`，见[在主题里加载 SDK](/zh/how-to/load-sdk)。怎样给它们加外观，见[给组件加样式](/zh/how-to/style-components)；它们怎样找到 SDK、怎样报告用户操作、为什么把样式留给主题，见[组件的工作方式](/zh/concepts/components)。

下表每一项都链接到该标签的参考：属性、CSS part、slot 与事件。

## 组件速查表 

### A. 播放控制（7 个） 

| 标签名 | 说明 |
| --- | --- |
| [`<fb-play-button>`](./play-controls#fb-play-button) | 播放/暂停切换 |
| [`<fb-stop-button>`](./play-controls#fb-stop-button) | 停止播放 |
| [`<fb-prev-button>`](./play-controls#fb-prev-button) | 切换到上一首曲目 |
| [`<fb-next-button>`](./play-controls#fb-next-button) | 切换到下一首曲目 |
| [`<fb-shuffle-button>`](./play-controls#fb-shuffle-button) | 切换随机播放状态 |
| [`<fb-repeat-button>`](./play-controls#fb-repeat-button) | 循环切换重复播放模式（`off` → `playlist` → `track`） |
| [`<fb-stop-after-current>`](./play-controls#fb-stop-after-current) | 播放完当前曲目后停止 |

### B. 进度与音量（3 个） 

| 标签名 | 说明 |
| --- | --- |
| [`<fb-seek-bar>`](./progress#fb-seek-bar) | 播放进度条 |
| [`<fb-volume-control>`](./progress#fb-volume-control) | 音量控制（含静音按钮） |
| [`<fb-playback-order>`](./progress#fb-playback-order) | 七种状态的播放顺序选择器，支持 select 与 button 模式 |

### C. 曲目信息（6 个） 

| 标签名 | 说明 |
| --- | --- |
| [`<fb-track-text>`](./track-info#fb-track-text) | 通用曲目文本（title/artist/album/自定义 TF） |
| [`<fb-cover-art>`](./track-info#fb-cover-art) | 封面图；可选用 SDK URL 辅助查询，并在需要时回退到标准封面 |
| [`<fb-time-current>`](./track-info#fb-time-current) | 当前播放时间 |
| [`<fb-time-total>`](./track-info#fb-time-total) | 曲目总时长 |
| [`<fb-time-remaining>`](./track-info#fb-time-remaining) | 曲目剩余时间 |
| [`<fb-tech-info>`](./track-info#fb-tech-info) | 编解码器、比特率、采样率与声道信息 |

### D. 播放列表（5 个） 

| 标签名 | 说明 |
| --- | --- |
| [`<fb-playlist-tabs>`](./playlist#fb-playlist-tabs) | 播放列表标签栏 |
| [`<fb-resizable-header>`](./playlist#fb-resizable-header) | 可调整宽度、重排并排序的列表表头 |
| [`<fb-playlist-view>`](./playlist#fb-playlist-view) | 使用虚拟化渲染的播放列表视图 |
| [`<fb-queue-view>`](./playlist#fb-queue-view) | 播放队列视图 |
| [`<fb-playlist-selector>`](./playlist#fb-playlist-selector) | 播放列表下拉选择器 |

### E. 窗口管理（3 个） 

| 标签名 | 说明 |
| --- | --- |
| [`<fb-titlebar>`](./window#fb-titlebar) | 可拖动的自定义标题栏 |
| [`<fb-window-controls>`](./window#fb-window-controls) | 最小化、最大化/还原与关闭按钮 |
| [`<fb-popup-panel>`](./window#fb-popup-panel) | 弹出窗口触发器与生命周期包装组件 |

### F. 歌词与可视化（3 个） 

| 标签名 | 说明 |
| --- | --- |
| [`<fb-lyrics-panel>`](./media#fb-lyrics-panel) | 同步歌词或纯文本歌词面板 |
| [`<fb-spectrum-visualizer>`](./media#fb-spectrum-visualizer) | 实时频谱可视化器 |
| [`<fb-waveform>`](./media#fb-waveform) | 整首曲目的波形概览 |

### G. 评分与音频设置（4 个） 

| 标签名 | 说明 |
| --- | --- |
| [`<fb-rating>`](./audio#fb-rating) | 星级评分控件 |
| [`<fb-output-selector>`](./audio#fb-output-selector) | 音频输出设备选择器 |
| [`<fb-dsp-preset-selector>`](./audio#fb-dsp-preset-selector) | DSP 预设选择器 |
| [`<fb-replaygain-selector>`](./audio#fb-replaygain-selector) | ReplayGain 模式选择器 |

### H. 元数据与搜索（3 个） 

| 标签名 | 说明 |
| --- | --- |
| [`<fb-properties-panel>`](./metadata#fb-properties-panel) | 只读曲目属性面板 |
| [`<fb-search-bar>`](./metadata#fb-search-bar) | 媒体库搜索输入框 |
| [`<fb-console>`](./metadata#fb-console) | foobar2000 控制台查看器 |

### I. 媒体库（2 个） 

| 标签名 | 说明 |
| --- | --- |
| [`<fb-library-tree>`](./library#fb-library-tree) | 按艺术家、专辑或流派分组的媒体库树 |
| [`<fb-library-filesystem-tree>`](./library#fb-library-filesystem-tree) | 按需加载的文件系统型媒体库根树 |

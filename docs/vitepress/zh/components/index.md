# 组件总览

foo_ui_webview2 提供 **36 个无样式 Web Components**（Custom Elements）功能积木，专注于播放器控件逻辑与 Bridge 数据流，通过 CSS Parts、Slots、只读宿主属性与自定义事件实现完全解耦的自定义 UI。

通过 SDK 加载组件：使用打包工具时调用 `registerComponents()`，无打包工具时直接引入 `components.global.js`（详见[在主题里加载 SDK](/zh/how-to/load-sdk)）。组件外观定制指南见[给组件加样式](/zh/how-to/style-components)，运行时生命周期与底层通信机制见[组件的工作方式](/zh/concepts/components)。

## 设计约定

组件遵循统一的架构约定，确保功能与表现层彻底分离：

- **零样式功能积木**：组件仅包含结构排版与流式布局，不预设任何视觉样式主张（无 `color`、`background`、`font`、`padding`、`margin`、`border-radius`、`box-shadow`、`transition` 等）。所有视觉呈现均由宿主主题通过 CSS 定义。
- **CSS Part 与 Slot 规范**：内部元素通过 kebab-case 命名的 `part` 属性暴露，供外部使用 `::part()` 选择器定义外观；可替换的视觉图元（如播放/暂停图标）通过 `slot` 插槽开放。
- **只读宿主属性**：内部运行状态通过只读宿主属性向外反映（如 `[playing]`、`[disabled]`、`[active]`、`[muted]`），供主题编写状态驱动的选择器样式。
- **右键菜单分离**：组件在右键触发时不弹出原生菜单，仅向外派发包含屏幕坐标与条目上下文的 CustomEvent（`bubbles: true` 且 `composed: true`，如 `fb-track-context`、`fb-playlist-context`、`fb-queue-context`、`fb-library-context`），由主题统一调用 `ui.showCustomMenu` 弹出上下文菜单。

## 组件速查表

下表汇总全部 36 个组件，点击标签名可跳转至对应详细参考（含属性、CSS Part、Slot 与事件说明）：

### A. 播放控制（7 个）

| 标签名 | 说明 |
| --- | --- |
| [`<fb-play-button>`](./play-controls#fb-play-button) | 播放与暂停状态切换按钮 |
| [`<fb-stop-button>`](./play-controls#fb-stop-button) | 停止播放按钮 |
| [`<fb-prev-button>`](./play-controls#fb-prev-button) | 切换到上一首曲目按钮 |
| [`<fb-next-button>`](./play-controls#fb-next-button) | 切换到下一首曲目按钮 |
| [`<fb-shuffle-button>`](./play-controls#fb-shuffle-button) | 随机播放状态切换按钮 |
| [`<fb-repeat-button>`](./play-controls#fb-repeat-button) | 循环切换重复播放模式（`off` → `playlist` → `track`） |
| [`<fb-stop-after-current>`](./play-controls#fb-stop-after-current) | 播放完当前曲目后停止开关 |

### B. 进度与音量（3 个）

| 标签名 | 说明 |
| --- | --- |
| [`<fb-seek-bar>`](./progress#fb-seek-bar) | 播放进度拖动与状态条 |
| [`<fb-volume-control>`](./progress#fb-volume-control) | 音量调节滑块与静音控制按钮 |
| [`<fb-playback-order>`](./progress#fb-playback-order) | 播放顺序选择器（支持 7 种模式，提供 select 与 button 两种呈现形态） |

### C. 曲目信息（6 个）

| 标签名 | 说明 |
| --- | --- |
| [`<fb-track-text>`](./track-info#fb-track-text) | 通用曲目文本展示（支持 title、artist、album 等预设字段及自定义 TitleFormat 表达式） |
| [`<fb-cover-art>`](./track-info#fb-cover-art) | 封面图展示（支持 SDK URL 辅助查询并在缺失时回退到标准内置封面） |
| [`<fb-time-current>`](./track-info#fb-time-current) | 当前播放时间展示 |
| [`<fb-time-total>`](./track-info#fb-time-total) | 当前曲目总时长展示 |
| [`<fb-time-remaining>`](./track-info#fb-time-remaining) | 当前曲目剩余时间展示 |
| [`<fb-tech-info>`](./track-info#fb-tech-info) | 曲目技术参数展示（编解码器、比特率、采样率与声道信息） |

### D. 播放列表（5 个）

| 标签名 | 说明 |
| --- | --- |
| [`<fb-playlist-tabs>`](./playlist#fb-playlist-tabs) | 播放列表选项卡标签栏 |
| [`<fb-resizable-header>`](./playlist#fb-resizable-header) | 可调整列宽、拖拽重排与点击排序的列表表头 |
| [`<fb-playlist-view>`](./playlist#fb-playlist-view) | 虚拟化渲染的播放列表核心视图 |
| [`<fb-queue-view>`](./playlist#fb-queue-view) | 播放队列条目视图 |
| [`<fb-playlist-selector>`](./playlist#fb-playlist-selector) | 播放列表下拉选择器 |

### E. 窗口管理（3 个）

| 标签名 | 说明 |
| --- | --- |
| [`<fb-titlebar>`](./window#fb-titlebar) | 支持窗口拖拽移动的自定义标题栏 |
| [`<fb-window-controls>`](./window#fb-window-controls) | 无边框窗口控制按钮组（最小化、最大化/还原、关闭） |
| [`<fb-popup-panel>`](./window#fb-popup-panel) | 弹出面板触发器与生命周期包装容器 |

### F. 歌词与可视化（3 个）

| 标签名 | 说明 |
| --- | --- |
| [`<fb-lyrics-panel>`](./media#fb-lyrics-panel) | 同步动态滚动歌词与纯文本歌词面板 |
| [`<fb-spectrum-visualizer>`](./media#fb-spectrum-visualizer) | 基于 Canvas 的实时音频频谱可视化图表 |
| [`<fb-waveform>`](./media#fb-waveform) | 当前曲目整体音频波形图 |

### G. 评分与音频设置（4 个）

| 标签名 | 说明 |
| --- | --- |
| [`<fb-rating>`](./audio#fb-rating) | 星级曲目评分控件 |
| [`<fb-output-selector>`](./audio#fb-output-selector) | 音频输出设备下拉选择器 |
| [`<fb-dsp-preset-selector>`](./audio#fb-dsp-preset-selector) | DSP 效果预设下拉选择器 |
| [`<fb-replaygain-selector>`](./audio#fb-replaygain-selector) | 回放增益（ReplayGain）处理模式选择器 |

### H. 元数据与搜索（3 个）

| 标签名 | 说明 |
| --- | --- |
| [`<fb-properties-panel>`](./metadata#fb-properties-panel) | 只读曲目详细属性查看面板 |
| [`<fb-search-bar>`](./metadata#fb-search-bar) | 媒体库搜索输入框 |
| [`<fb-console>`](./metadata#fb-console) | foobar2000 控制台实时日志查看器 |

### I. 媒体库（2 个）

| 标签名 | 说明 |
| --- | --- |
| [`<fb-library-tree>`](./library#fb-library-tree) | 按艺术家、专辑或流派分组的媒体库层级树 |
| [`<fb-library-filesystem-tree>`](./library#fb-library-filesystem-tree) | 文件系统结构的媒体库目录树（支持按需异步延迟加载） |

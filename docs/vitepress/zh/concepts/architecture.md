# foo_ui_webview2 的工作原理

> **foobar2000 最低版本**: 2.0+

foo_ui_webview2 是一个 foobar2000 UI 组件：它通过 Microsoft WebView2 承载前端页面，并开放完整的播放器控制与状态通知。主题本质是标准 Web 工程，支持 HTML、CSS、JavaScript 及各类现代前端框架；组件负责将其渲染为主窗口或嵌入其他界面的面板。

<a id="主要特性"></a>
<a id="key-features"></a>

## 页面与宿主

组件向所有加载的页面注入全局对象 `window.fb2k`。调用 `fb2k.invoke('playback.play')` 时，消息经由 WebView2 发送到 C++ 原生后端，由宿主执行对应操作并返回结构化结果信封。反方向上，曲目切换等播放器事件以具名通知的形式到达前端，通过 `fb2k.on('playback:trackChanged', handler)` 接收。方法调用采用点号分隔（`playback.play`），事件通知采用冒号分隔（`playback:trackChanged`）。协议细节见 [Bridge 协议](/zh/reference/bridge)。

页面与播放器的所有交互均经过此 Bridge 通道。在 foobar2000 之外（例如普通浏览器标签页），页面不存在 `window.fb2k`，SDK 调用将以 `NOT_SUPPORTED` 错误信封解决（resolve）。加载哪个页面及路径如何解析，由[页面从哪里加载](./page-loading.md)中定义的优先级规则决定。

## bridge 与 SDK

只用 bridge 就能写任何主题，但它的调用是没有类型的字符串。npm 包 `foo-webview-sdk` 在它之上加了三层：

- `fb.*` 方法（`fb.player.play()`、`fb.playlist.getAll()`），带 TypeScript 类型，调用的是同样的宿主方法；
- `fb-*` Web Components，例如播放按钮、播放列表视图，它们自己与宿主通信，本身不带任何样式；
- SMP 兼容层，用于移植 Spider Monkey Panel 脚本。

组件提供 400 多个方法和 100 多个事件，按命名空间分组，例如 `playback`、`playlist`、`library`、`window`。[API 概述](/zh/api/overview)列出每个命名空间的方法数和所在页面，[SDK 命名空间](/zh/sdk/namespaces)列出对应的 `fb.*` 封装。

::: tip 从 Spider Monkey Panel (SMP) 迁移？
SDK 自带 SMP 兼容层，包含：
- **35 个 SMP 事件映射**：`on_playback_new_track` 等 SMP 事件名直接可用
- **8 个包装器类**：`FbMetadbHandle`、`FbTitleFormat`、`ContextMenuManager` 等
- **完整的 `fb` / `plman` 对象**：同步属性、播放控制、播放列表管理

兼容层用「缓存 + 事件」把异步的宿主调用包装成同步属性，以符合 SMP 的同步语义。缓存是最终一致的，适合 UI 渲染；需要精确控制的事务性操作，直接用 `fb2k.invoke()`。SMP 的 GDI/GDI+ 绘图（`on_paint(gr)` 等）无法移植，这部分要用 HTML、CSS 或 Canvas 重写。见 [SMP 兼容层](/zh/reference/smp-compat)。
:::

## 设计原则

> foo_ui_webview2 遵循「组件做能力，应用做策略」的设计原则。

### 组件提供什么

| 类别 | 描述 | 示例 |
| --- | --- | --- |
| foobar2000 核心能力 | 播放控制、播放列表、媒体库、封面 | `playback.*`、`playlist.*` |
| 窗口管理 | DWM 效果、标题栏、缩放 | `window.*` |
| 系统集成 | 文件操作、对话框、Shell | `file.*`、`dialog.*` |
| 音频分析 | 频谱、波形 | `audio.*` |
| 配置存储 | 持久化 key-value 存储 | `config.*` |
| 流媒体能力 | 通用 URL 播放、即时队列 | `jitQueue.*` |

### 主题实现什么

| 类别 | 描述 | 为什么不在组件里 |
| --- | --- | --- |
| 在线服务集成 | Spotify、网易云、QQ 音乐等 | 业务逻辑、授权流程各异 |
| 歌词多源聚合 | 在线搜索、翻译合并 | 业务逻辑 |
| 封面缓存策略 | 预加载、LRU 缓存 | 策略选择 |
| UI 主题与布局 | 颜色、字体、动画 | 应用特性 |

### 示例：能力与策略

```javascript
// 组件提供能力：一个可直接用作图片地址的 URL
const current = await fb2k.invoke('playback.getCurrentTrack');
if (current.success && current.track) {
    const res = await fb2k.invoke('artwork.getFb2kUrlByPath', { path: current.track.path, maxSize: 200 });
    const img = document.querySelector('img.cover');
    if (res.success && img instanceof HTMLImageElement) img.src = res.dataUrl;
}

// 主题决定策略：提前取哪些封面、保留多久
function prefetchCovers(urls) {
    urls.forEach((url) => {
        new Image().src = url;
    });
}
```

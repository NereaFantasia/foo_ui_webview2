# 组件的工作方式

`foo-webview-sdk/components` 导出的 `fb-*` 元素是标准 Web Components（Custom Elements），如 `<fb-play-button>` 与 `<fb-playlist-view>`。每个组件封装了一个独立的播放器控件或视图模型，并在内部自主维护与 foobar2000 宿主的 Bridge 通信与事件流。主题只需将其声明于 HTML 中并通过 CSS Part 定义视觉样式，无需手动编写端点调用与事件监听逻辑。

## 只带行为，不带外观

组件的 Shadow DOM 仅包含布局、流排版与尺寸等纯结构性样式，不预设颜色、背景、字体、边框、内/外边距、阴影或过渡动画（未设置样式的 `<fb-seek-bar>` 不显示可见色块）。视觉呈现完全由宿主主题控制，通过以下三种扩展机制实现：

- **CSS Part（部件样式化）**：组件内部关键 DOM 节点显式暴露 `part` 属性标识符，主题可在外部通过标准 `::part()` 伪元素穿透 Shadow DOM 进行精细化视觉定制（如 `fb-seek-bar::part(fill)` 自定义进度填充色）。
- **宿主属性（状态反射）**：组件实时运行时状态作为只读属性同步至宿主元素（如 `<fb-play-button>` 的 `[playing]`、`<fb-shuffle-button>` 的 `[active]` 或 `<fb-volume-control>` 的 `[muted]`），供主题直接通过属性选择器挂接状态驱动的样式规则。
- **Slot（插槽投影）**：组件关键视觉节点提供命名或默认插槽，允许主题将自定义标记与图标直接投射至 Shadow DOM 预设位置（如替换 `<fb-play-button>` 的播放与暂停 SVG 图标）。

将外观完全留给外部主题，同一套组件才能无缝融入任何视觉风格；样式定义与定制指南见[给组件加样式](/zh/how-to/style-components)。

## 组件报告，主题决定

用户操作以 `CustomEvent` 的形式向外派发，事件名固定为 `fb-` 加短横线连接（kebab-case），如 `fb-play` 或 `fb-seek`。所有事件均开启 `bubbles: true` 与 `composed: true`，支持穿透 Shadow DOM 边界并在 `document` 层统一捕获。

组件内部不调用原生菜单：交互时派发携带屏幕坐标与目标条目上下文的专用事件（如 `<fb-playlist-view>` 派发 `fb-track-context`、`<fb-playlist-tabs>` 派发 `fb-playlist-context`、`<fb-queue-view>` 派发 `fb-queue-context`、`<fb-library-tree>` 派发 `fb-library-context`），最终由主题调用 `fb.notification.showCustomMenu()` 等端点呈现业务菜单。

## 组件使用主题的 SDK {#组件用的是主题的-sdk}

Web Components 自身不创建独立的 Bridge 连接，而是复用主题环境中已初始化的 SDK 实例：

- **构建工具环境**：通过 `foo-webview-sdk/components` 导出的 `registerComponents()`，将组件与当前模块绑定的 SDK 实例关联。
- **原生脚本环境**：引入 `components.global.js` 时自动完成注册，并绑定至 `bridge.global.js` 挂载的 `window.fb`。

组件在首次挂载至 DOM 时动态解析 SDK 实例，因此脚本引入顺序不影响正常执行。若未检测到有效 SDK 运行时，组件将抛出包含加载指引的显式错误。

当宿主不可用或调用失败（例如在无 foobar2000 宿主的浏览器中点击播放）时，元素保持当前状态，不向页面抛出异常。

## 相关页面

- [组件参考](/zh/components/)
- [在主题里加载 SDK](/zh/how-to/load-sdk)

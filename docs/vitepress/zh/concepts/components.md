# 组件的工作方式

`foo-webview-sdk/components` 提供的 `fb-*` 元素是 Web Components：`<fb-play-button>`、`<fb-playlist-view>` 这样的自定义 HTML 标签，每个包装播放器的一个控件或视图。它们自己与 foobar2000 通信，主题只要把它们写进页面并加上样式，不必为每个元素接调用和事件。

## 只带行为，不带外观

组件的 shadow DOM 里只有结构性的 CSS：布局、尺寸、定位之类。它不设颜色、背景、字体、边框、外边距、阴影或过渡，所以没加样式的 `fb-seek-bar` 什么也看不见。外观完全由主题决定，通过三种挂钩：

- **CSS part**：内部元素带 `part` 名，从外部用 `::part()` 选中，例如 `fb-seek-bar::part(fill)`。
- **宿主属性**：状态反映为元素自身的属性，例如 `<fb-play-button>` 的 `[playing]`、`<fb-shuffle-button>` 的 `[active]`、`<fb-volume-control>` 的 `[muted]`，CSS 可以据此变化。
- **slot**：可替换的内容，例如 `<fb-play-button>` 的播放与暂停图标，可以换成主题自己的标记。

把外观留在组件之外，同一套元素才能适配任何主题；这些挂钩怎么用，见[给组件加样式](/zh/how-to/style-components)。

## 组件报告，主题决定

用户操作以 `CustomEvent` 的形式离开组件，名称是 `fb-` 加 kebab 写法，例如 `fb-play`、`fb-seek`。它们会冒泡并穿过 shadow DOM 边界，所以在 `document` 上就能监听到。右键不会弹菜单：视图派发带指针位置和所指条目的上下文事件（`<fb-playlist-view>` 的 `fb-track-context`、`<fb-playlist-tabs>` 的 `fb-playlist-context`、`<fb-queue-view>` 的 `fb-queue-context`、`<fb-library-tree>` 的 `fb-library-context`），由主题弹出自己想要的菜单，例如用 `fb.notification.showCustomMenu()`。

## 组件用的是主题的 SDK

组件从不自建 bridge，用的是主题已经加载的 SDK 实例。使用打包工具时，`foo-webview-sdk/components` 的 `registerComponents()` 把它们绑定到 `foo-webview-sdk` 导出的实例。用 `<script>` 标签时，`components.global.js` 注册它们，它们使用 `bridge.global.js` 安装的 `window.fb`。组件在第一次加入页面时才查找 SDK，所以两个 script 标签的先后无所谓。两者都没有时，组件抛出一条说明如何加载 SDK 的错误。

调用失败时，例如没有宿主时点了播放按钮，元素保持原样，不会把异常抛给页面。

## 相关页面

- [组件参考](/zh/components/)
- [在主题里加载 SDK](/zh/how-to/load-sdk)

# 在主题里加载 SDK

组件自动向加载的页面注入原生 Bridge 实例 `window.fb2k`，直接使用 `fb2k.invoke()` 和 `fb2k.on()` 时无需引入外部依赖。类型化的 `fb.*` 封装方法与 `fb-*` Web Components 则由 npm 包 `foo-webview-sdk` 提供，需在工程中显式引入或手动复制分发文件。

| 调用方式与目标特性 | 是否依赖 `foo-webview-sdk` |
| --- | --- |
| `window.fb2k.invoke` / `window.fb2k.on` | 否 |
| `fb.*` 类型化封装与 `fb-*` Web Components | 是 |

## 使用打包工具

安装 npm 依赖：

```bash
npm install foo-webview-sdk
```

在前端源码中导入并注册。`registerComponents()` 负责注册 `fb-*` 自定义元素，并将其绑定至当前 SDK 实例：

```js
import fb from 'foo-webview-sdk';
import { registerComponents } from 'foo-webview-sdk/components';

registerComponents();
await fb.player.play();
```

构建项目后，将输出目录部署为模板（见[安装主题](./install-theme.md)）。完整的 Vite 项目搭建见[构建第一个主题](/zh/tutorials/first-theme)。

<a id="不用打包工具"></a>

## 在无构建（纯 HTML/JS）环境中引入

从依赖包（`node_modules/foo-webview-sdk/dist/`）中复制预编译全局构建产物至模板目录，通过 `<script>` 标签引入（优先加载 Bridge）：

```html
<!-- 路径相对于 index.html；此处将分发脚本放置于 ./sdk/ 目录下 -->
<script src="./sdk/bridge.global.js"></script>
<script src="./sdk/components.global.js"></script>
```

`bridge.global.js` 负责挂载全局 `window.fb` 对象；`components.global.js` 加载时自动注册所有 `fb-*` 元素。示例中的 `./sdk/` 目录需手动创建，组件不会自动生成。完整页面结构示例见[不用构建工具写主题](./plain-html.md)。

## 后续阅读 {#接下来}

- [SDK 概述](/zh/sdk/overview)：概览 SDK 各功能模块与导出入口。
- [SDK 命名空间](/zh/sdk/namespaces)：查阅全部类型化 `fb.*` API。
- [组件参考](/zh/components/)：查阅全部 `fb-*` Web Components 规范。

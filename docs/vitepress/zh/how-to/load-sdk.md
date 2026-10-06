# 在主题里加载 SDK

组件会给每个页面注入原生 bridge `window.fb2k`，用 `fb2k.invoke()` 和 `fb2k.on()` 不需要安装任何东西。`fb.*` 封装和 `fb-*` Web Components 来自 npm 包 `foo-webview-sdk`，组件不会把它装进模板。

| 用到的东西 | 是否需要 `foo-webview-sdk` |
| --- | --- |
| `window.fb2k.invoke` / `window.fb2k.on` | 否 |
| `fb.*` 封装与 `fb-*` 组件 | 是 |

## 使用打包工具

安装这个包：

```bash
npm install foo-webview-sdk
```

在要用的地方导入。`registerComponents()` 定义 `fb-*` 元素，它们用的是同一个 SDK 实例：

```js
import fb from 'foo-webview-sdk';
import { registerComponents } from 'foo-webview-sdk/components';

registerComponents();
await fb.player.play();
```

构建项目，再把输出装成模板（见[安装主题](./install-theme.md)）。完整的 Vite 项目见[构建第一个主题](/zh/tutorials/first-theme)。

## 不用打包工具

把包里的全局构建（`node_modules/foo-webview-sdk/dist/`）复制进模板文件夹，用 `<script>` 标签加载，先加载 bridge。`bridge.global.js` 安装 `window.fb`；`components.global.js` 加载时注册全部 `fb-*` 元素。

```html
<!-- 路径相对于 index.html；这里把文件复制到了 ./sdk/ -->
<script src="./sdk/bridge.global.js"></script>
<script src="./sdk/components.global.js"></script>
```

示例里的 `./sdk/` 文件夹由你自己创建，组件不会建它。按这种方式写的完整页面见[不用构建工具写主题](./plain-html.md)。

## 接下来

- [SDK 概述](/zh/sdk/overview)列出这个包的各个入口。
- [SDK 命名空间](/zh/sdk/namespaces)列出全部 `fb.*` 方法。
- [组件](/zh/components/)列出全部 `fb-*` 元素。

# 页面从哪里加载

窗口或面板打开时，组件要决定加载哪个页面。了解这个顺序，就能明白为什么窗口显示的是旧主题、内置测试页，或是 “Frontend not found” 页面。

## 模板

模板是 profile 资源根目录 `<profile>\webview-ui\` 下带 `index.html` 的文件夹。一个主题可以有多个页面、脚本和资源文件，必需的只有 `index.html`。

```text
<profile>/
└── webview-ui/                 资源根目录
    ├── default/                没选过模板时使用的模板
    │   └── index.html
    └── my-theme/               其他任意模板
        ├── index.html
        └── assets/
```

全局活动模板在 `File → Preferences → Display → WebView2 UI` 偏好设置中指定；未显式配置时默认为 `default`。偏好设置中的「新建模板」仅生成基础 `index.html` 占位文件，不包含 SDK、构建脚本或前端依赖。

本地模板由自定义虚拟源 `https://foo-ui-webview2.local/` 托管并映射到模板根目录：根路径 `/index.html` 对应模板入口文件，`/assets/app.js` 等绝对根路径亦在模板目录内正确解析。

## 查找顺序

主窗口、后台窗口和每个面板都按下面的顺序取第一个适用的：

1. **开发服务器。** 打开了 `使用开发服务器（HMR 热重载）` 且填了 URL 时，所有窗口和面板都加载这个 URL。连不上时从第 2 步继续。
2. **面板的 URL 覆盖。** 面板配置里填了 URL 时加载它。
3. **面板自己的模板**，指定了且该文件夹里有 `index.html` 时。
4. **全局活动模板**，其文件夹里有 `index.html` 时。
5. **组件安装目录中的 `foo_ui_webview2_resources\dist`**：存在 `index.html` 时加载；若该目录为空或不存在入口文件则跳过。
6. **`<profile>\webview-ui\default\`**，里面有 `index.html` 时。
7. **内置页面**。主窗口与后台窗口显示一个测试页，上面的按钮可以试用各个 API；面板显示 “Frontend not found. Please install a template.”。

由 `window.createPopup` 创建的弹出窗口遵循独立加载规则：直接从当前模板（或开发服务器）加载指定页面文件（如 `<page>.html`），并在 URL 查询参数中附带 `windowId`。当开发服务器不可达时回退加载本地模板的对应文件；若模板目录中不存在入口文件，则显示 “Frontend not found” 错误提示。

## 什么时候重新加载

- 在偏好设置页换了模板后按 `应用`，主窗口和跟随全局模板的面板重新加载；指定了自己模板或 URL 的面板保持原页面。
- 改了开发服务器设置后按 `应用`，重新加载的也是这些窗口。
- 两种 `应用` 都不会重新加载弹出窗口：已打开的弹出窗口保持原页面，之后新开的按新设置加载。
- 开发服务器开着时，换模板不会让任何页面重新加载：页面仍从开发服务器来。
- 在面板的配置对话框里改模板或 URL，会让该面板重新加载。

## 相关页面

- [安装主题](/zh/how-to/install-theme)
- [用开发服务器调试主题](/zh/how-to/dev-server)
- [把页面放进面板](/zh/how-to/panels)

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

全局活动模板是在 `File → Preferences → Display → WebView2 UI` 页选中的那个；从没选过时为 `default`。在那里新建模板只会写入一个占位的 `index.html`，不带 SDK、脚本或其他文件。

本地页面经 `https://foo-ui-webview2.local/` 提供，这个地址映射到模板文件夹，所以 `/index.html` 就是模板的 `index.html`，`/assets/app.js` 这类以根开头的路径也在文件夹内解析。

## 查找顺序

主窗口、后台窗口和每个面板都按下面的顺序取第一个适用的：

1. **开发服务器。** 打开了 `使用开发服务器（HMR 热重载）` 且填了 URL 时，所有窗口和面板都加载这个 URL。连不上时从第 2 步继续。
2. **面板的 URL 覆盖。** 面板配置里填了 URL 时加载它。
3. **面板自己的模板**，指定了且该文件夹里有 `index.html` 时。
4. **全局活动模板**，其文件夹里有 `index.html` 时。
5. **组件文件夹里的 `foo_ui_webview2_resources\dist`**，里面有 `index.html` 时。旧版本曾把内置主题装在这里，现在的版本不再提供；组件包建出的空文件夹会被跳过。
6. **`<profile>\webview-ui\default\`**，里面有 `index.html` 时。
7. **内置页面**。主窗口与后台窗口显示一个测试页，上面的按钮可以试用各个 API；面板显示 “Frontend not found. Please install a template.”。

页面打开的弹出窗口另有规则：它们从同一个模板（或开发服务器）加载 `<页面>.html`，查询参数里带窗口 ID。连不上开发服务器时，弹出窗口改从模板加载同一个页面；没有哪个模板文件夹有 `index.html` 时显示 “Frontend not found” 页面。

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

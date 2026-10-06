# 安装组件

## 系统要求

| 项目 | 要求 |
| --- | --- |
| 操作系统 | Windows 10/11 (64-bit) |
| foobar2000 | v2.0 或更高版本 (64-bit) |
| WebView2 Runtime | 通常已预装于 Windows 10/11；如未安装，请从 Microsoft 官网下载 |

::: tip 提示
foobar2000 v2.0 是 64 位版本的分水岭。如果你使用的是 v1.x 版本，请先升级到 v2.0+。
:::

## 安装组件包

1. **下载安装包**：从发布页面下载 `foo_ui_webview2-<version>.fb2k-component`，`<version>` 是组件版本。
2. **通过 foobar2000 安装**：打开或双击该安装包，让 foobar2000 弹出组件安装确认对话框。
3. **确认安装**：点击 `Yes`，foobar2000 会安装包内的文件。
4. **重启 foobar2000。**
5. **选择界面**：`File → Preferences → Display → Default User Interface` → 选择 `Webview2 UI`。

::: info
`.fb2k-component` 是 foobar2000 官方组件安装包格式。请使用 foobar2000 的组件安装流程安装；不要把它当作普通 zip 手工解压到 profile。
:::

如果不想让它接管整个界面，而是作为面板放进 Default UI 或 Columns UI，跳过第 5 步，按[把页面放进面板](./panels.md)操作。

## 手动复制二进制（不推荐）

发布包同时包含按架构组织的二进制（`foo_ui_webview2.dll`、`WebView2Loader.dll` 以及 `x64/` 布局）。只把单个 DLL 复制到任意 `components` 目录通常不完整，且依赖具体布局。手工解压仅可作为高级恢复手段。

## 验证安装

安装成功后，你应该看到：

- Default User Interface 列表中出现 `Webview2 UI`
- 菜单项 `View → WebView2 UI → Show/Hide Window`
- 偏好设置页 `File → Preferences → Display → WebView2 UI`
- 高级分支 `File → Preferences → Advanced → Tools → WebView2 UI`

组件本身不带主题。模板里还没有 `index.html` 时，主窗口显示内置的测试页，上面的按钮可以试用各个 API，面板则显示 “Frontend not found. Please install a template.”；怎样装上主题见[安装主题](./install-theme.md)与[构建第一个主题](/zh/tutorials/first-theme)。

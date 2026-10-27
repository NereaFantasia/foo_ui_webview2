# 安装组件

## 系统要求

| 项目 | 要求 |
| --- | --- |
| 操作系统 | Windows 10/11 (64 位 / x64) |
| foobar2000 | v2.0 或更高版本（推荐 64 位，同时支持 32 位） |
| WebView2 Runtime | 通常已预装于 Windows 10/11；如未安装，请从 Microsoft 官网下载 |

::: tip 提示
foobar2000 自 v2.0 起引入 64 位架构支持。组件安装包同时包含 x64 与 32 位二进制，若你仍在使用 foobar2000 v1.x，须先升级至 v2.0+。
:::

## 安装组件包

1. **下载安装包**：从发布页面下载 `foo_ui_webview2-<version>.fb2k-component`（`<version>` 为对应版本号）
2. **通过 foobar2000 安装**：双击或在 foobar2000 中打开该安装包，唤起组件安装确认对话框
3. **确认安装**：点击 `Yes`，由 foobar2000 自动释放并注册包内文件
4. **重启应用**：重启 foobar2000 使组件生效
5. **选择主界面**：前往 `File → Preferences → Display → Default User Interface`，将当前界面切换为 `Webview2 UI`

::: info
`.fb2k-component` 是 foobar2000 官方组件安装包格式。请使用 foobar2000 的组件安装流程安装；不要把它当作普通 zip 手工解压到 profile。
:::

若无需接管整个主窗口，仅希望将其作为子面板置于 Default UI 或 Columns UI 中，可跳过第 5 步，直接查阅[把页面放进面板](./panels.md)。

## 手动复制二进制（不推荐）

发布包同时包含按架构组织的二进制文件（`foo_ui_webview2.dll`、`WebView2Loader.dll` 以及 `x64/` 目录布局）。仅将单个 DLL 复制到任意 `components` 目录通常会导致依赖缺失且依赖具体目录布局。手动解压仅作为高级恢复手段。

## 验证安装

安装完成后，可通过以下入口核验组件加载状态：

- Default User Interface 选项列表中出现 `Webview2 UI`
- 顶层菜单新增 `View → WebView2 UI → Show/Hide Window`
- 偏好设置新增页面 `File → Preferences → Display → WebView2 UI`
- 高级配置新增分支 `File → Preferences → Advanced → Tools → WebView2 UI`

组件本身不内置主题模板。在主题目录尚无 `index.html` 时，独立主窗口将加载内置测试页（可直接点击按钮调用各 Bridge API 进行功能自检），嵌入面板则显示 “Frontend not found. Please install a template.”。安装主题的具体方法参见[安装主题](./install-theme.md)与[构建第一个主题](/zh/tutorials/first-theme)。

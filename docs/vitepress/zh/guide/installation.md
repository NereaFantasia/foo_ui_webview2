# 安装配置

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

1. **下载安装包** — 从发布页面下载 `foo_ui_webview2-<version>.fb2k-component`（文件名包含组件版本，例如 `foo_ui_webview2-1.9.0.fb2k-component`）
2. **通过 foobar2000 安装** — 打开或双击该安装包，让 foobar2000 弹出组件安装确认对话框
3. **确认安装** — 点击 `Yes`，由 foobar2000 安装包内容
4. **重启 foobar2000**
5. **选择 UI** — `File → Preferences → Display → Default User Interface` → 选择 `Webview2 UI`

::: info INFO
`.fb2k-component` 是 foobar2000 官方组件安装包格式。请使用 foobar2000 的组件安装流程安装；不要把它当作普通 zip 手工解压到 profile。
:::

## 手动复制二进制（不推荐）

发布包同时包含按架构组织的二进制（`foo_ui_webview2.dll`、`WebView2Loader.dll` 以及 `x64/` 布局）。只把单个 DLL 复制到任意 `components` 目录通常不完整，且依赖具体布局。

请优先使用上面的官方 `.fb2k-component` 安装方式。手工解压仅可作为高级恢复参考，不是推荐安装方法。

## 验证安装

安装成功后，你应该看到：

- Default User Interface 列表中出现 `Webview2 UI`
- 菜单项 `View → WebView2 UI → Show/Hide Window`
- 偏好设置页 `File → Preferences → Display → WebView2 UI`
- 高级分支 `File → Preferences → Advanced → Tools → WebView2 UI`

## 资源根目录与模板

用户可编辑的前端资源位于 profile 资源根目录：

```text
profile/
└── webview-ui/                      # 资源根
    ├── default/                     # 常见默认模板
    │   └── index.html
    └── <active-template>/           # 全局活动模板
        └── index.html
```

组件实际解析顺序：

1. 面板级覆盖：`panelConfig.templateName` → `profile/webview-ui/<templateName>/`
2. 全局活动模板：`profile/webview-ui/<active-template>/`
3. 组件自带资源：`foo_ui_webview2_resources/dist`
4. 兼容回退：`profile/webview-ui/default/`

`default` 只是常见起始模板名，不是永久或唯一 contract。创建模板只会生成模板目录和起始 `index.html`，**不会**自动部署 SDK 目录。

## 获取 SDK

组件会注入原生 `window.fb2k`。更高层的 `fb.*` 封装来自独立 npm 包 `foo-webview-sdk`（或版本化 SDK ZIP 发布资产）。组件**不会**在 profile 下自动创建 `sdk/`。

### Bundler / ESM

```bash
npm install foo-webview-sdk
```

```js
import fb from 'foo-webview-sdk'
// 或使用公开 subpath，例如 'foo-webview-sdk/bridge' / 'foo-webview-sdk/components'
await fb.player.play()
```

### 无 bundler（IIFE 全局构建）

从 npm 包或 SDK ZIP 复制构建后的 IIFE 文件到主题目录，再按你实际复制后的相对路径加载：

```html
<!-- 示例：把构建产物复制到 ./sdk/dist/ 后的布局 -->
<script src="./sdk/dist/bridge.global.js"></script>
<script src="./sdk/dist/components.global.js"></script>
```

这里的 `./sdk/dist/...` 只是复制后的示例目录，不是组件安装器自动创建的路径。

| 使用面 | 是否需要 SDK 文件 |
| --- | --- |
| 原生 `window.fb2k.invoke` / `window.fb2k.on` | 否 |
| `fb.*` 封装与 Web Components 辅助 | 是（`foo-webview-sdk`） |

## 后台模式 (Background Mode)

后台模式允许 WebView2 UI 与 Default UI 或 Columns UI 同时使用。

**启用步骤：**

1. `Preferences → Advanced → Tools → WebView2 UI` → `Enable Background Mode`
2. `Preferences` → `Display` → `Default User Interface` → 选择其他 UI
3. 重启 foobar2000
4. `View → WebView2 UI → Show/Hide Window`

可见性取决于后台模式设置以及上次 Show/Hide 状态。当没有独立 WebView 窗口时（仅面板场景），同一菜单项可能打开偏好设置而不是切换窗口。

**使用场景：**

- 将 WebView2 UI 作为歌词、封面等辅助信息窗口
- 使用 Default UI 管理播放列表，同时用 WebView2 UI 显示可视化效果
- 在另一个主 UI 运行时，用伴随窗口执行自定义脚本

**页面可见性契约：** 窗口最小化、隐藏到托盘或锁屏时，WebView 页面进入 `visibilityState=hidden`——`requestAnimationFrame` 停止、定时器被大幅节流。主题应优先使用事件驱动更新（`fb2k.on(...)`）而非轮询循环，恢复时才不会错乱。若通过 CDP/MCP 自动化播放器，参见 [MCP 配置指南](/zh/mcp/setup)中的 keep-alive 开关以在上述状态保持页面活跃。

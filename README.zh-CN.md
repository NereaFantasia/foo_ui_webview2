[English](./README.md) | 中文

# foo_ui_webview2

[![Docs](https://img.shields.io/badge/docs-online-brightgreen)](https://nereafantasia.github.io/foo_ui_webview2/)
[![License](https://img.shields.io/badge/license-GPL--3.0%20%7C%20MIT-blue)](LICENSE)
[![npm: foo-webview-sdk](https://img.shields.io/npm/v/foo-webview-sdk?label=foo-webview-sdk)](https://www.npmjs.com/package/foo-webview-sdk)
[![npm: foo-ui-webview2-mcp](https://img.shields.io/npm/v/foo-ui-webview2-mcp?label=foo-ui-webview2-mcp)](https://www.npmjs.com/package/foo-ui-webview2-mcp)
[![CI](https://github.com/NereaFantasia/foo_ui_webview2/actions/workflows/ci.yml/badge.svg)](https://github.com/NereaFantasia/foo_ui_webview2/actions/workflows/ci.yml)
[![CodeQL](https://github.com/NereaFantasia/foo_ui_webview2/actions/workflows/codeql.yml/badge.svg)](https://github.com/NereaFantasia/foo_ui_webview2/actions/workflows/codeql.yml)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/NereaFantasia/foo_ui_webview2)

**文档站**: https://nereafantasia.github.io/foo_ui_webview2/ <!-- TODO: englishify the documentation site (currently Chinese-only) -->

基于 WebView2 的 foobar2000 现代 UI 组件 (C++ DLL)。将整个 foobar2000 窗口变为 WebView2 画布，使用现代 Web 技术构建界面，同时保留 Windows 11 原生视觉效果 (Mica/Acrylic)。

- 版本: 1.11.0
- 许可证: GPL-3.0-or-later (主组件) / MIT (`sdk/`)

<!-- TODO(screenshots): 首屏截图待补 —— 计划：内置 Winamp 经典风格预设主题 + 基于本组件开发的社区主题（独立窗口、DUI/CUI 面板、Mica/Acrylic 效果）。 -->

---

## 特性

- 完全 WebView2 UI - 整个客户区由 WebView2 渲染；用纯 HTML/CSS/JS 或任意框架（Vue、React 等）构建
- 三种运行形态 - 独立窗口（完整替换 UI）、DUI 面板、CUI 面板
- Windows 11 原生效果 - Mica / Acrylic / Tabbed 背景
- 客户区扩展标题栏 - 自定义标题栏内容、Snap Layout 支持
- 双向 C++ / JS 通信桥接 (BridgeCore) - 播放器状态与事件实时推送到你的界面
- 200+ API，覆盖 20+ 命名空间
- 多窗口系统 - 弹出窗口、跨窗口消息、异步关闭处理
- Web Components 组件库 (fb-* 元素) 与 npm 上的类型化 TypeScript SDK
- SMP 兼容层 - Spider Monkey Panel 的知识与大量既有代码可直接迁移
- MCP Server - AI 智能体通过 CDP 集成
- 插件扩展系统 (PluginRegistry) - 其他组件可向你的界面暴露 API

---

## 为什么选 foo_ui_webview2？

foobar2000 已有非常优秀的自定义方案；本组件占据的是另一处权衡点：

| | foo_ui_webview2 | JS 脚本面板宿主 (JSplitter / SMP) | WebView 面板组件 (foo_uie_webview / foo_webview2) |
| --- | --- | --- | --- |
| UI 技术 | Web 平台 —— HTML/CSS/JS、任意框架、Chromium DevTools | 基于 GDI/GDI+ 绘制回调的 JavaScript | Web 平台 |
| 运行形态 | 独立完整 UI、DUI/CUI 面板、弹出窗口 | DUI/CUI 内的面板 / 分割容器 | DUI/CUI 内的面板 |
| 播放器集成 | 内置桥接 —— 播放、播放列表、媒体库、元数据、封面、队列、配置等一个组件全覆盖 | 成熟稳定的脚本 API，拥有庞大的既有主题生态 | 以面板与渲染为中心的 host-object API |
| 原生窗口能力 | 全屏、Mica/Acrylic、自定义标题栏、多窗口（独立模式） | 由宿主 UI 管理 | 由宿主 UI 管理 |
| 技能 / 代码复用 | Web 技能；SMP 知识经兼容层迁移 | 既有 SMP / JScript 脚本与技能 | Web 技能 |
| AI 工具链 | 内置 MCP server | — | — |

这些方案是可组合的，不是二选一：本组件自身可以作为 CUI/DUI 面板嵌入你现有布局，其他 foobar2000 组件也可以通过 PluginRegistry 把 API 注册进桥接层。

---

## 30 秒上手

1. 安装 `.fb2k-component` 包，在 `File → Preferences → Display → Default User Interface` 中选择 `Webview2 UI`（[安装指南](https://nereafantasia.github.io/foo_ui_webview2/zh/guide/installation)）。
2. 把下面内容保存为 `<profile>\webview-ui\default\index.html`（目录不存在则新建）：

```html
<!DOCTYPE html>
<html>
<head><meta charset="UTF-8"><title>My foobar2000 UI</title></head>
<body>
    <h1 id="track">等待播放...</h1>
    <button id="play">播放 / 暂停</button>

    <script>
    document.getElementById('play').onclick = () => fb2k.invoke('playback.playOrPause');
    fb2k.on('playback:trackChanged', (data) => {
        document.getElementById('track').textContent = data.artist + ' - ' + data.title;
    });
    </script>
</body>
</html>
```

3. 重启 foobar2000。

不需要 SDK 文件、不需要 Node.js、不需要构建步骤 —— `fb2k` 桥接对象由组件原生注入。在你部署模板之前，组件会显示内置测试页（可点击试用各 API），全新安装不会出现黑屏。

## 模板与主题

组件本身不内置前端界面。请把你的 WebUI 放入 foobar2000 的 **profile** 目录下的 `webview-ui\<模板名>\`，并以 `index.html` 作为模板根目录的入口文件（例如 `<profile>\webview-ui\default\index.html`）。`<模板名>` 默认为 `default`，可在组件的「首选项」页中管理 / 切换。为向后兼容，旧路径 `<组件目录>\foo_ui_webview2_resources\dist\` 仍受支持。重启 foobar2000 即可加载。可基于 `sdk/`（`foo-webview-sdk`）构建自定义主题。

<!-- TODO(preset-theme): Winamp 经典风格预设主题起草中，发布后在此登记链接。 -->

## 按人群选路线

- **会 Spider Monkey Panel / JScript？** SMP 兼容层映射了 35 个 SMP 回调（`on_playback_new_track` 等）并提供 `fb` / `plman` 包装对象，既有知识与大量既有代码可直接迁移。→ [SMP 兼容层文档](https://nereafantasia.github.io/foo_ui_webview2/zh/reference/smp-compat)
- **Web 开发者？** `npm install foo-webview-sdk` 获得类型化 `fb.*` 封装与 Web Components，还可以把组件指向你的 Vite/webpack 开发服务器（`window.setDevServerConfig`），对着真实播放器热重载开发。→ [SDK 快速上手](https://nereafantasia.github.io/foo_ui_webview2/zh/sdk/quickstart)
- **不想手写代码？** 把 AI 智能体接到 MCP server（`npx foo-ui-webview2-mcp`）：它可以经 CDP 控制播放与播放列表、读取播放器状态、截图 —— 足以让智能体和你一起搭建并迭代主题。→ [MCP 文档](https://nereafantasia.github.io/foo_ui_webview2/zh/mcp/overview)

---

## npm 包

以下两个 JavaScript 包已发布到 npm，可直接安装使用（无需从源码构建）：

| 包 | 安装 / 使用 | 说明 |
|----|------------|------|
| [`foo-webview-sdk`](https://www.npmjs.com/package/foo-webview-sdk) | `npm install foo-webview-sdk` | 主题 / 前端 SDK（Bridge API + Web Components + SMP 兼容层），详见 [`sdk/`](sdk/) |
| [`foo-ui-webview2-mcp`](https://www.npmjs.com/package/foo-ui-webview2-mcp) | `npx foo-ui-webview2-mcp` | AI 智能体经 CDP 操控 foobar2000 的 MCP Server，详见 [`mcp/`](mcp/) |

---

## API 概览

200+ API 分布在以下命名空间:

playback, playlist, library, window, config, file, dialog, clipboard, shell, http, keyboard, ui, lyrics, metadata, audio, console, dnd, queue, discovery, replaygain, playcount, titleformat, selection, port

### 事件 (colon 格式)

```
playback:trackChanged, playback:paused, playback:stopped, playback:seeked,
playback:volumeChanged, playback:time, playlist:itemsAdded, playlist:itemsRemoved,
playlist:activated, playlist:created, playlist:removed, playlist:renamed,
playlist:selectionChanged, metadb:changed, plugin:registered, api:registered
```

### 调用示例

```javascript
// invoke 使用 dot 格式
const track = await fb2k.invoke('playback.getCurrentTrack');
const results = await fb2k.invoke('library.search', { query: 'artist:Radiohead' });

// 事件监听使用 colon 格式
fb2k.on('playback:trackChanged', (track) => { /* ... */ });
```

---

## 插件扩展

其他 foobar2000 插件可通过 PluginRegistry 注册 API 供前端调用:

```cpp
#include "api/PluginRegistry.h"

void RegisterMyApis() {
    auto& registry = PluginRegistry::GetInstance();
    registry.RegisterPlugin("my_plugin", "My Plugin", "1.0.0", "Author", "Description");
    registry.RegisterExternalApi("my_plugin", "doSomething",
        [](const json& params) -> json {
            return {{"result", "done"}};
        },
        "执行某操作"
    );
}
```

```javascript
const result = await fb2k.invoke('my_plugin.doSomething', { param1: 'value' });
```

---

## 安全限制

**威胁模型**：这是一个面向 foobar2000 的专用、垂直 UI 宿主——主题来自你自己或可信来源，安装一个主题的信任等同于安装一个 foobar2000 组件。因此设计目标是 *fail-safe*（防止有 bug 的主题误伤系统），**而非** *sandbox*（隔离不可信代码）；对这样一个专用组件做过度防护只会自缚手脚。真正的护栏是路径校验与协议限制：

| API | 限制 |
|-----|------|
| shell.exec / shell.spawn | 无命令/可执行白名单 - 命令按原样执行；`cwd` 与绝对路径经 PathSecurity 校验（信任主题作者，不是 sandbox）|
| shell.openWith | 黑名单 - 禁止打开 29 种可执行/脚本扩展名（`.exe`、`.bat`、`.ps1`、`.dll`、`.lnk` 等）|
| file.read/write | 路径限制 - 系统目录（`C:\Windows`、`C:\Program Files`、`C:\ProgramData`）禁止访问；写入限 foobar2000 配置目录与 `%TEMP%` |
| http.get/post | SSRF 防护 - 禁止 localhost/私有/链路本地地址（含 DNS 解析后防 rebinding 校验）|
| CDP 远程调试 | 默认关闭 - 需手动开启，仅绑定 localhost |

完整细节：[安全说明](https://nereafantasia.github.io/foo_ui_webview2/zh/reference/security) 与 [权限参考](https://nereafantasia.github.io/foo_ui_webview2/zh/reference/permissions)。

---

## 从源码构建

> 仅开发组件本身时需要本节。普通用户直接安装打包好的 `.fb2k-component` 即可 —— 见上方「30 秒上手」。

### 环境要求

- Visual Studio 2022 (17.8+), v145 工具集
- Windows SDK 10.0.22621+
- WebView2 Runtime
- Node.js 18+ (构建 SDK / MCP / 文档站)

### C++ 组件

```powershell
# 标准构建
.\build.ps1 -Config Release -Platform x64

# 打包 .fb2k-component (x86 + x64)
.\build-package.ps1

# 打包离线文档（Windows 用户无需额外运行时）
.\build-docs-package.ps1
```

文档 ZIP 通过 `open-docs.cmd` 打开，仅使用 Windows 10/11 自带的 Windows PowerShell 5.1、.NET Framework 和默认浏览器；无需安装 Node.js、Python、npm，无需管理员权限，也不需要联网。

### TypeScript SDK（从源码构建，开发用）

> 消费者无需构建，直接 `npm install foo-webview-sdk`（见上方「npm 包」）。

```powershell
cd sdk
npm run build
```

### MCP Server（从源码构建，开发用）

> 消费者无需构建，直接 `npx foo-ui-webview2-mcp`（见上方「npm 包」）。

```powershell
cd mcp
npm install
npm run build
```

### 技术栈

| 层 | 技术 |
|----|------|
| C++ 组件 | C++20, VS 2022 (v145), Windows SDK 10.0 |
| WebView2 | Microsoft.Web.WebView2 1.0.3719.77 (NuGet) |
| WIL | Microsoft.Windows.ImplementationLibrary 1.0.240803.1 |
| JSON | nlohmann/json |
| foobar2000 | SDK v2.0+ (lib/) |
| Columns UI | SDK (git submodule: lib/columns_ui-sdk) |
| TypeScript SDK | foo-webview-sdk (sdk/, MIT, tsup) |
| MCP Server | foo-ui-webview2-mcp (mcp/, Node.js 18+) |

### 项目结构

```
foo_ui_webview2/
├── src/                    # C++ 源代码
│   ├── core/               # WebView 核心、UserInterface 等
│   ├── window/             # MainWindow, PopupWindow, WindowManager
│   ├── webview/            # WebView 宿主与环境
│   ├── api/                # BridgeCore + API handlers
│   ├── callbacks/          # foobar2000 SDK 事件回调
│   ├── panels/             # DUI/CUI 面板集成
│   ├── selection/          # 选择追踪
│   ├── interfaces/         # 服务抽象
│   └── utils/              # 工具函数
├── sdk/                    # TypeScript SDK (foo-webview-sdk)
│   ├── src/                # 源码 (bridge/, components/, smp/, types/)
│   └── package.json
├── mcp/                    # MCP Server (AI 智能体 CDP 桥接)
│   ├── src/                # TypeScript 源码
│   └── tests/              # 单元 + E2E 测试
├── tests/                  # C++ 单元测试 (GoogleTest)
├── docs/vitepress/         # 用户文档站
├── lib/                    # 第三方库
│   ├── foobar2000_sdk/     # foobar2000 SDK (BSD)
│   ├── columns_ui-sdk/     # Columns UI SDK (submodule)
│   └── json/               # nlohmann/json (MIT)
├── build.ps1               # 标准构建脚本
├── build-package.ps1       # .fb2k-component 打包
├── foo_ui_webview2.sln     # VS 解决方案
├── foo_ui_webview2.vcxproj # VS 项目文件
└── packages.config         # NuGet 包配置
```

---

## 许可证

本项目主组件采用 **GNU General Public License v3.0 or later** (GPL-3.0-or-later)，完整条款见 [LICENSE](LICENSE)。

- 主组件: GPL-3.0-or-later
- TypeScript SDK (`sdk/`): MIT License（见 `sdk/LICENSE`）

第三方库各自的许可：foobar2000 SDK (BSD)、pfc / libPPUI (zlib)、nlohmann/json (MIT)、Columns UI SDK。

---

## 致谢

- [foobar2000 SDK](https://www.foobar2000.org/SDK)
- [WebView2](https://developer.microsoft.com/microsoft-edge/webview2/)
- [nlohmann/json](https://github.com/nlohmann/json)
- [Columns UI SDK](https://github.com/reupen/columns_ui-sdk)
- [WIL](https://github.com/microsoft/wil)

[English](./README.md) | 中文

# foo_ui_webview2

[![Docs](https://img.shields.io/badge/docs-online-brightgreen)](https://nereafantasia.github.io/foo_ui_webview2/)
[![License](https://img.shields.io/badge/license-GPL--3.0%20%7C%20MIT-blue)](LICENSE)
[![npm: foo-webview-sdk](https://img.shields.io/npm/v/foo-webview-sdk?label=foo-webview-sdk)](https://www.npmjs.com/package/foo-webview-sdk)
[![npm: foo-ui-webview2-mcp](https://img.shields.io/npm/v/foo-ui-webview2-mcp?label=foo-ui-webview2-mcp)](https://www.npmjs.com/package/foo-ui-webview2-mcp)
<!-- 仓库私有期间 CI / CodeQL 徽章会返回 404，公开后取消注释启用：
[![CI](https://github.com/NereaFantasia/foo_ui_webview2/actions/workflows/ci.yml/badge.svg)](https://github.com/NereaFantasia/foo_ui_webview2/actions/workflows/ci.yml)
[![CodeQL](https://github.com/NereaFantasia/foo_ui_webview2/actions/workflows/codeql.yml/badge.svg)](https://github.com/NereaFantasia/foo_ui_webview2/actions/workflows/codeql.yml)
-->
<!-- DeepWiki（仓库被索引后启用）: [![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/NereaFantasia/foo_ui_webview2) -->

**文档站**: https://nereafantasia.github.io/foo_ui_webview2/

基于 WebView2 的 foobar2000 现代 UI 组件 (C++ DLL)。将整个 foobar2000 窗口变为 WebView2 画布，使用现代 Web 技术构建界面，同时保留 Windows 11 原生视觉效果 (Mica/Acrylic)。

- 版本: 1.9.0
- 许可证: GPL-3.0-or-later (主组件) / MIT (`sdk/`)

---

## 特性

- 完全 WebView2 UI - 整个客户区由 WebView2 渲染
- Windows 11 原生效果 - Mica / Acrylic / Tabbed 背景
- 客户区扩展标题栏 - 自定义标题栏内容、Snap Layout 支持
- 双向 C++ / JS 通信桥接 (BridgeCore)
- 200+ API，覆盖 20+ 命名空间
- 插件扩展系统 (PluginRegistry)
- Web Components 组件库 (fb-* 元素)
- SMP 兼容层
- MCP Server - AI 智能体通过 CDP 集成
- DUI / CUI 面板支持

---

## 技术栈

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

---

## npm 包

以下两个 JavaScript 包已发布到 npm，可直接安装使用（无需从源码构建）：

| 包 | 安装 / 使用 | 说明 |
|----|------------|------|
| [`foo-webview-sdk`](https://www.npmjs.com/package/foo-webview-sdk) | `npm install foo-webview-sdk` | 主题 / 前端 SDK（Bridge API + Web Components + SMP 兼容层），详见 [`sdk/`](sdk/) |
| [`foo-ui-webview2-mcp`](https://www.npmjs.com/package/foo-ui-webview2-mcp) | `npx foo-ui-webview2-mcp` | AI 智能体经 CDP 操控 foobar2000 的 MCP Server，详见 [`mcp/`](mcp/) |

---

## 项目结构

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

## 构建

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
```

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

### WebUI / 主题

组件本身不内置前端界面。将你的 WebUI（至少包含 `index.html`）放入 foobar2000 组件目录下的
`foo_ui_webview2_resources/dist/`，重启 foobar2000 即可加载。可基于 `sdk/`（`foo-webview-sdk`）构建自定义主题。

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

| API | 限制 |
|-----|------|
| shell.exec | 已禁用 - 任意命令执行风险过高 |
| shell.openWith | 黑名单 - 禁止打开 30+ 种可执行扩展名 |
| file.read/write | 路径限制 - 系统保护目录禁止访问 |
| http.get/post | SSRF 防护 - 禁止访问内网地址 |
| CSP | 前端页面受 Content-Security-Policy 保护 |
| CDP 远程调试 | 默认关闭 - 需手动开启，仅绑定 localhost |

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

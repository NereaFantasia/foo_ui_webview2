[English](./README.md) | 中文

# src/ — C++ 后端源代码地图

`src/` 是 foo_ui_webview2 的 C++ 实现核心：把整个 foobar2000 窗口（以及 DUI/CUI 面板）变为 WebView2 画布，并在 foobar2000 SDK 与 Web 前端之间架起一条双向 JSON 桥接。

本文件是 `src/` 的总览地图。每个一级功能模块都有独立的 README，深入了解某个模块时请跳转到对应子目录。

---

## 概述

整个组件的运行链路可以概括为「前端调用 → 桥接分发 → SDK 执行 → 事件回流」：

```
JavaScript                       C++                          foobar2000 SDK
──────────────────────────────────────────────────────────────────────────
fb2k.invoke('playback.play')
      └─ postMessage ─────> BridgeCore::HandleMessage   (src/api)
                                 └─ handlers_["playback.play"]
                                       └─ PlaybackApi   (src/api)
                                             └─ playback_control::start()
                                                   └─ on_playback_new_track
PlaybackCallback (src/callbacks) ──> BridgeCore::EmitEvent / WebViewContext::BroadcastEvent
      └─ postMessage ─────> fb2k.on('playback:trackChanged')
```

- 窗口与宿主层（`window/` + `core/` + `webview/`）负责把 WebView2 嵌入 foobar2000 的窗口/面板里，并管理其生命周期。
- 桥接与 API 层（`api/`）负责接收前端消息、路由到具体 handler、回送响应与事件。
- 事件回调层（`callbacks/` + `selection/`）监听 foobar2000 SDK 的变化并广播给前端。
- 工具与抽象层（`utils/` + `interfaces/`）提供路径安全、编码转换、服务注入等横切能力。

---

## 模块全景

> P1：核心模块，已配独立 README。P2：支撑模块，职责见本页「支撑模块（P2）」一节。

| 模块 | 文件数 | 职责 | 关键类/文件 | 文档 |
|------|-------|------|------------|------|
| `api/` | 74 | C++ ↔ JS 桥接 + 30+ 命名空间 API + 插件扩展 | `BridgeCore`、`PluginRegistry`、`*Api` | [api/README.zh-CN.md](api/README.zh-CN.md) |
| `window/` | 31 | 窗口体系：主窗口/弹窗、Chrome（Mica/标题栏）、托盘/任务栏 | `MainWindow`、`WindowManager`、`ChromeController`、`WindowChrome*` | [window/README.zh-CN.md](window/README.zh-CN.md) |
| `callbacks/` | 18 | foobar2000 SDK 事件 → BridgeCore 事件广播 | `InitPlaybackCallbacks` 等 9 个回调 | [callbacks/README.zh-CN.md](callbacks/README.zh-CN.md) |
| `core/` | 17 | WebView 生命周期、UI 入口、库缓存、JIT 队列 | `UserInterface`、`WebViewPanel`、`WebViewContext`、`LibraryCache` | [core/README.zh-CN.md](core/README.zh-CN.md) |
| `utils/` | 13 | 路径安全 / Base64 / 图标 / i18n 等工具 | `PathSecurity`、`PathExpansion`、`Base64`、`I18n` | 见本页 P2 |
| `panels/` | 8 | DUI/CUI 面板集成与配置持久化 | `WebViewDuiElement`、`WebViewCuiPanel`、`PanelConfig` | [panels/README.zh-CN.md](panels/README.zh-CN.md) |
| `webview/` | 5 | WebView2 宿主与共享环境、注入脚本 | `WebViewHost`、`WebViewEnvironment` | 见本页 P2 |
| `interfaces/` | 4 | 服务抽象（依赖注入 / 可测试） | `IPlaybackService`、`IPlaylistService` | 见本页 P2 |
| `selection/` | 4 | 选择追踪 | `SelectionHolder`、`SelectionWatcher` | 见本页 P2 |

> 顶层还有 `main.cpp`（组件入口与 cfg_var 配置）、`pch.h/.cpp`（预编译头）、`version.h`（版本号）。

---

## 核心机制速览

### 1. 桥接三段式（`api/BridgeCore`）

- **注册**：每个 `XxxApi.cpp` 暴露 `RegisterXxxApi()`，内部以 `RegisterApi("namespace.method", handler)` 注册；带路径参数的方法用装饰器重载附加 `PathSecuritySpec`。
- **分发**：`HandleMessage()` 解析前端 `postMessage`，按 `method` 查表执行 handler。
- **响应/事件**：handler 返回 JSON → `SendResponse()`；变化推送走 `EmitEvent()`（单实例）/ `WebViewContext::BroadcastEvent()`（全窗口）。

### 2. 命名约定（严禁混淆）

| 场景 | 格式 | 示例 |
|------|------|------|
| invoke 方法 | dot | `playback.play` |
| C++ → JS 事件 | colon | `playback:trackChanged` |

### 3. 双模式桥接

- **单例模式**：独立主窗口用 `BridgeCore::GetInstance()`（向后兼容）。
- **per-instance 模式**：DUI/CUI 面板各自持有 `BridgeCore`，经 `WebViewContext` 按 HWND 注册与路由。

### 4. 路径安全 5 档

`SecurityLevel`（`None / Read / Write / MediaRead / MediaWrite`）对应 `utils/PathSecurity.h` 的不同校验函数，在桥接分发前由装饰器统一拦截。

### 5. 插件扩展

外部 foobar2000 组件通过导出的 `GetPluginRegistry()` → `RegisterPlugin` + `RegisterExternalApi` 注册自己的命名空间 API，受 `RESERVED_NAMESPACES` 隔离保护。

---

## 支撑模块（P2）

这些模块不单独配 README，职责如下：

- **`webview/`** — WebView2 的底层宿主。`WebViewHost` 封装 WebView2 创建、导航、`ExecuteScript`/`PostMessage`、虚拟主机映射、DWM 透明（Visual Hosting + DirectComposition）、光标与鼠标输入转发；`WebViewEnvironment` 在启动时预热并共享同一个 WebView2 环境以加速后续创建；`SdkBridgeScript.inl` 是注入到页面的桥接脚本。
- **`interfaces/`** — 服务抽象层。`IPlaybackService` / `IPlaylistService` 把 API handler 依赖的 SDK 子集抽象为接口，生产环境用 `Fb2kPlaybackService` / `Fb2kPlaylistService` 实现，单测时可替换为 mock（见 `api/` 的服务注入）。
- **`selection/`** — 选择追踪。`SelectionHolder` 封装 `ui_selection_holder`，面板获焦时获取、失焦时释放；`SelectionWatcher` 监听全局选择变化并节流广播 `selection:changed` 给各面板。
- **`utils/`** — 横切工具集。`PathSecurity`（路径安全校验，动态信任模式）、`PathExpansion`（路径变量展开）、`Base64`、`GuidUtils`、`I18n`（`TRU` 双语宏）、`IconLoader`、`ImageUtils`、`SubsongUtils`、`ArtworkCacheKey`、`WindowUtils` 等。

---

## 依赖关系

```
main.cpp / UserInterface
        │
        ▼
  window/  ──┐
  panels/  ──┤── 继承 ──> core/WebViewPanel ── 持有 ──> webview/WebViewHost
             │                  │
             │                  ├── 注册 ──> api/ (BridgeCore + *Api)
             │                  └── 初始化 ─> callbacks/ + selection/
             ▼
        core/WebViewContext (多实例路由)
```

- `window/`、`panels/` 都继承 `core/WebViewPanel` 复用 WebView2 + 桥接能力。
- `api/` 被所有窗口/面板共享注册；`callbacks/` 把 SDK 事件回流到 `api/` 的事件通道。
- `utils/`、`interfaces/` 是底层依赖，被 `api/`、`window/`、`core/` 广泛引用。

---

## 构建

源码统一通过仓库根的构建脚本编译，禁止硬编码 MSBuild 路径或直接调用 `msbuild`：

```powershell
.\build.ps1 -Config Release -Platform x64
```

更多约定见仓库根 [README.md](../README.md)、`AGENTS.md` 与 `CLAUDE.md`。

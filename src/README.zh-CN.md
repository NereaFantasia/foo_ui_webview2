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
PlaybackCallback (src/callbacks) ──> api::emit::Broadcast<…>（src/api/EventEmit.h）
      └─ postMessage ─────> fb2k.on('playback:trackChanged')
```

- 窗口与宿主层（`window/` + `core/` + `webview/`）负责把 WebView2 嵌入 foobar2000 的窗口/面板里，并管理其生命周期；`media/` 把本地音视频文件交给页面播放。
- 桥接与 API 层（`api/`）负责接收前端消息、路由到具体 handler、回送响应与事件。
- 事件回调层（`callbacks/` + `selection/`）监听 foobar2000 SDK 的变化并广播给前端。
- 工具与抽象层（`utils/` + `interfaces/`）提供编码转换、服务注入等横切能力。
- 领域层（`domain/`）放不依赖桥接、窗口与 UI 的逻辑：路径安全校验，以及媒体库的目录树索引与缓存。

---

## 模块全景

> 核心模块已配独立 README；支撑模块的职责见本页「支撑模块」一节。

| 模块 | 职责 | 关键类/文件 | 文档 |
|------|------|------------|------|
| `api/` | C++ ↔ JS 桥接 + 40 多个命名空间的 API（声明在 `api/schema/`）+ 插件扩展 | `BridgeCore`、`PluginRegistry`、`*Api` | [api/README.zh-CN.md](api/README.zh-CN.md) |
| `window/` | 窗口体系：主窗口/弹窗、Chrome（Mica/标题栏）、托盘/任务栏 | `MainWindow`、`WindowManager`、`ChromeController`、`WindowChrome*` | [window/README.zh-CN.md](window/README.zh-CN.md) |
| `callbacks/` | foobar2000 SDK 事件 → BridgeCore 事件广播 | `InitPlaybackCallbacks` 等 9 个回调 | [callbacks/README.zh-CN.md](callbacks/README.zh-CN.md) |
| `core/` | WebView 生命周期、多实例路由、JIT 队列 | `WebViewPanel`、`WebViewContext`、`QueueManager` | [core/README.zh-CN.md](core/README.zh-CN.md) |
| `prefs/` | 偏好设置页（Display → WebView2 UI 及其子页）、模板管理、`webview_prefs::*` 访问函数 | `PreferencesPage`、`PreferencesPageBase`、`PreferencesFields`、`PreferencesDraft` | 见本页下文 |
| `ui/` | foobar2000 UI 入口、后台模式、主菜单命令、`app:beforeQuit` | `UserInterface`（`WebViewUI`）、`BackgroundService`、`WebViewMainMenu`、`AppQuit` | 见本页下文 |
| `settings/` | cfg_var 与 Advanced 设置的存储，`security_config` / `window_config` 访问函数 | `SecurityConfig`、`AdvancedConfig`、`WindowStateConfig` | 见本页下文 |
| `domain/` | 不依赖桥接、窗口与 UI 的领域逻辑：路径安全校验、媒体库目录树索引与缓存 | `PathSecurity`、`LibraryTreeIndex`、`LibraryCache` | 见本页下文 |
| `utils/` | 路径展开 / 编码 / Base64 / 图标 / i18n 等工具 | `PathExpansion`、`Encoding`、`Base64`、`I18n` | 见本页下文 |
| `panels/` | DUI/CUI 面板集成与配置持久化 | `WebViewDuiElement`、`WebViewCuiPanel`、`PanelConfig` | [panels/README.zh-CN.md](panels/README.zh-CN.md) |
| `webview/` | WebView2 宿主与共享环境、注入脚本 | `WebViewHost`、`WebViewEnvironment` | 见本页下文 |
| `interfaces/` | 服务抽象（依赖注入 / 可测试） | `IPlaybackService`、`IPlaylistService` | 见本页下文 |
| `selection/` | 选择追踪 | `SelectionHolder`、`SelectionWatcher` | 见本页下文 |
| `media/` | 本地音视频文件经虚拟主机交给页面的 `<video>` / `<audio>`，含 MP4、Matroska 容器解析 | `MediaService`、`MediaFile`、`ContainerInfo` | 见本页下文 |

> 顶层还有 `main.cpp`（组件版本声明）、`ComponentDescription.inc`（Preferences → Components 里显示的说明文本）、`pch.h/.cpp`（预编译头）、`version.h`（版本号）。

---

## 核心机制速览

### 1. 桥接三段式（`api/BridgeCore`）

- **声明与注册**：方法与事件声明在 `api/schema/<ns>.ts`，C++ 参数结构体与解析器由它生成；每个 `XxxApi.cpp` 暴露 `RegisterXxxApi()`，内部以 `api::RegisterApi("namespace.method", handler)` 注册。路径参数的校验级别也写在声明里。
- **分发**：`HandleMessage()` 解析前端 `postMessage`，按 `method` 查表执行 handler。
- **响应/事件**：handler 返回结果 → `SendResponse()`；变化推送走 `api/EventEmit.h` 的 `api::emit::` 助手，按声明的投递方式发给一个页面或全部窗口。

### 2. 命名约定（严禁混淆）

| 场景 | 格式 | 示例 |
|------|------|------|
| invoke 方法 | dot | `playback.play` |
| C++ → JS 事件 | colon | `playback:trackChanged` |

### 3. 双模式桥接

- **单例模式**：独立主窗口用 `BridgeCore::GetInstance()`（向后兼容）。
- **per-instance 模式**：DUI/CUI 面板各自持有 `BridgeCore`，经 `WebViewContext` 按 HWND 注册与路由。

### 4. 路径安全 6 档

`SecurityLevel`（`None / Read / Write / MediaRead / MediaWrite / FileWrite`）对应 `domain/PathSecurity.h` 的不同校验函数，在桥接分发前按声明统一拦截。

### 5. 插件扩展

外部 foobar2000 组件通过导出的 `GetPluginRegistry()` → `RegisterPlugin` + `RegisterExternalApi` 注册自己的命名空间 API，受 `RESERVED_NAMESPACES` 隔离保护。

---

## 支撑模块

这些模块不单独配 README，职责如下：

- **`webview/`** — WebView2 的底层宿主。`WebViewHost` 封装 WebView2 创建、导航、`ExecuteScript`/`PostMessage`、虚拟主机映射、DWM 透明（Visual Hosting + DirectComposition）、光标与鼠标输入转发，它的成员按职责分在多个 `WebViewHost*.cpp`。`WebViewEnvironment` 在启动时预热并共享同一个 WebView2 环境以加速后续创建。注入到每个页面的脚本在 `BridgeBootstrapScript.inl`（`window.fb2k` 桥接）、`StartupProbeScript.inl`（启动就绪信号）与 `SdkBridgeScript.inl`（`window.fb` SDK）。`dnd/` 是文件拖放的目标与来源检查，`SharedPcmBuffer` 用 SharedBuffer 把 PCM 数据交给页面。
- **`media/`** — 页面播放本地音视频的后端。`MediaService` 为允许读取的文件发放 `https://foo-ui-webview2.local/fb2k-media/` 下的令牌地址，`WebViewHostMedia.cpp` 按 Range 请求回送文件内容；`ContainerInfo` 与 `Mp4Container`、`MatroskaContainer` 读取容器信息，`MediaIoQueue` 在后台线程做读取。对应 `media.*` 接口（实验性）。
- **`interfaces/`** — 服务抽象层。`IPlaybackService` / `IPlaylistService` 把 API handler 依赖的 SDK 子集抽象为接口，生产环境用 `api/adapters/` 下的 `Fb2kPlaybackService` / `Fb2kPlaylistService` 实现，单测时可替换为 mock（见 `api/` 的服务注入）。
- **`selection/`** — 选择追踪。`SelectionHolder` 封装 `ui_selection_holder`，面板获焦时获取、失焦时释放；`SelectionWatcher` 监听全局选择变化并节流广播 `selection:changed` 给各面板。
- **`prefs/`** — 偏好设置页。`PreferencesPage` 注册 Preferences → Display → WebView2 UI（总览页），并持有 `webview_prefs::*` 访问函数背后的 cfg_var：活动模板与 `profile/webview-ui/` 下的模板目录、窗口与托盘任务栏开关、预热、默认缩放、背景效果、CDP 端口。总览页的模板命令在 `PreferencesTemplateActions.cpp`，「API 与服务...」打开的清单窗口在 `ApiInventoryWindow.cpp`。`PreferencesPageBase` 是各页共用的骨架（容器、字体、布局、滚动与草稿提交），Window、Performance、Developer 三个子页建在它之上。`PreferencesLayout`、`PreferencesLayoutBuilder`、`PreferencesDraft`、`PreferencesFields`、`PreferencesCdpPort`、`PreferencesDevServerUrl`、`PreferencesTemplateName`、`PreferencesZoom` 八个纯头文件承载纯逻辑，单测直接 include 它们。配置 GUID 一律不能改。
- **`ui/`** — foobar2000 调进来的入口。`UserInterface` 实现 `WebViewUI : user_interface`（`init`/`shutdown`/`activate`/`hide`），创建并持有 `MainWindow`；`BackgroundService` 在使用其它 UI 时让一个 WebView2 窗口在后台运行，保留 API 访问；`WebViewMainMenu.cpp` 注册 View → WebView2 UI → 显示/隐藏窗口命令；`AppQuit` 是发出 `app:beforeQuit` 的唯一位置。
- **`settings/`** — 组件设置的存储。`WindowStateConfig` 持久化主窗口位置与后台窗口可见性（`window_config::*`）；`AdvancedConfig` 放 Advanced 设置分支与其中的复选项、从 Advanced 迁到偏好子页的设置及其一次性迁移，以及 `SecurityConfig.h` 声明的 `security_config::*` 访问函数；`AdvconfigI18n.h` 提供双语的 Advanced 复选项工厂。配置 GUID 一律不能改，改了用户已存的值就丢了。
- **`domain/`** — 领域逻辑，只许 include `settings/`、`utils/`、`interfaces/`、foobar2000 SDK 与标准库，不 include `api/`、`core/`、`window/`、`webview/`、`ui/`、`prefs/`、`panels/`。`PathSecurity` 按动态信任模式校验路径（系统盘白名单、受保护目录黑名单、媒体库与播放列表内文件的上下文信任），是 `api/BridgeCore` 里 `SecurityLevel` 检查的实现；类声明在 `PathSecurity.h`，实现在 `PathSecurity.cpp`。`library/` 放 `api/LibraryApi` 背后的媒体库服务：`LibraryTreeIndex`（单例）用 `library_manager::get_relative_path` 与路径尾比对推断真实媒体根，服务 `library.getRoots` / `library.browseTree`，全量构建在 `LibraryTreeBuilder.cpp`，两者共用的路径辅助函数在 `LibraryPathAlgebra`；`LibraryCache`（单例）记录媒体库何时变化：`Invalidate()` 让一个版本计数加一，`api/LibraryApi` 复用自己保留的结果前先比对它，缓存命中统计也在这里。媒体库变化时 `callbacks/LibraryCallback` 让两者失效。
- **`utils/`** — 横切工具集。`PathExpansion`（路径变量展开）、`Encoding`（UTF-8 与 UTF-16 互转）、`Base64`、`GuidUtils`、`I18n`（`TRU` 双语宏）、`IconLoader`、`ImageUtils`、`SubsongUtils`、`ArtworkCacheKey`、`WindowUtils` 等。路径安全校验在 `domain/PathSecurity`，它委托的 `PathCanonicalForm`、`PathProtocolScheme`、`PathTraversalSegments`、`NetworkShareResolver` 仍在本目录。

---

## 依赖关系

```
main.cpp / ui/UserInterface
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
- `domain/` 由 `api/`、`callbacks/`、`webview/` 调用。

---

## 构建

源码统一通过仓库根的构建脚本编译，禁止硬编码 MSBuild 路径或直接调用 `msbuild`：

```powershell
.\build.ps1 -Config Release -Platform x64
```

更多约定见仓库根 [README.md](../README.md)。

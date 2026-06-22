[English](./README.md) | 中文

# src/core/ — WebView 生命周期与运行时核心

`core/` 是组件的运行时地基：它定义了 foobar2000 的 UI 入口、所有窗口/面板共享的 WebView2 基类、多实例路由表，以及媒体库缓存、目录树索引、JIT 流媒体队列、后台模式、偏好页等运行时服务。`window/` 和 `panels/` 都建立在本模块之上。

---

## 概述

`core/` 回答三个问题：

1. **谁是 UI 入口？** `WebViewUI`（实现 foobar2000 `user_interface`）在 fb2k 启动时创建主窗口。
2. **WebView2 的通用能力放在哪？** `WebViewPanel` 把初始化、API 注册、回调初始化、消息处理、配置热重载等收敛为基类，供 `MainWindow` / `WebViewDuiElement` / `WebViewCuiPanel` 继承。
3. **多个 WebView 实例如何互相找到？** `WebViewContext` 以 HWND 为键登记所有实例，支撑跨实例事件广播与按窗口 ID 的定向路由。

其余文件是围绕这三件事的运行时服务（缓存、队列、后台、偏好、安全配置）。

---

## 关键文件 / 类

| 文件 | 职责 |
|------|------|
| `UserInterface.h/.cpp` | `WebViewUI : user_interface`——foobar2000 主 UI 入口（GUID、`init`/`shutdown`/`activate`/`hide`），创建并持有 `MainWindow`，单例 `GetInstance()` |
| `WebViewPanel.h/.cpp` | WebView 面板基类：`InitializeWebView`、`RegisterAllApis`、`InitializeCallbacks`、导航/重载、`ApplyConfig` 配置热重载、`PanelConfig` 持有、`SelectionHolder` 持有；模式枚举 `Standalone/DuiPanel/CuiPanel`；定义可重写虚函数（`OnWebViewReady` 等） |
| `WebViewContext.h/.cpp` | 多实例管理器（单例）：`RegisterInstance`/`UnregisterInstance`（按 HWND）、`GetBridge`/`GetWebViewHost`/`GetPanelByHwnd`、`BroadcastEvent`/`BroadcastEventExcept`、按 windowId 的 `SendEventTo` 与反查 |
| `LibraryCache.h/.cpp` | 媒体库内存缓存（单例）：albums/tracks/artists/genres/stats/cover 多级缓存，`shared_mutex` 读写锁，tracks 用 `shared_ptr<const json>` 避免深拷贝，封面缓存 100MB 上限，库变化时 `Invalidate()` |
| `LibraryTreeIndex.h/.cpp` | 媒体根与目录树索引器（单例）：用 `library_manager::get_relative_path` + 路径尾比对推断真实媒体根，惰性构建、线程安全；服务 `library.getRoots` / `library.browseTree` |
| `QueueManager.h/.cpp` | JIT 流媒体队列（单例）：前端逻辑队列 + 后端「影子播放列表」（仅 current+next），状态机 `Idle/Active/WaitingNext/Exhausted`，即时 URL 解析、自动缓冲、`jitQueue:needNext` 预取、影子列表锁保护 |
| `BackgroundService.h/.cpp` | 后台模式：当使用其它 UI（Default UI 等）时让 WebView2 在后台运行以保留 API 访问；窗口可全程不可见 |
| `PreferencesPage.h/.cpp` | 偏好页（Preferences → Display → WebView2 UI）：模板管理、窗口设置、DWM 背景效果、开发者选项；暴露 `webview_prefs::*` 配置访问函数 |
| `SecurityConfig.h` | 安全配置访问接口（`security_config::*`，实现在 `main.cpp`）：DevTools、CDP 远程调试、本地网络、明文 HTTP、自签 TLS、后台模式、HMR 开发服务器开关 |

---

## 工作原理 / 数据流

### WebView 生命周期

```
fb2k 启动
   └─ WebViewUI::init()                       (core/UserInterface)
        └─ new MainWindow → Create()          (window/)
             └─ WebViewPanel::InitializeWebView(hwnd, Standalone)
                   ├─ WebViewHost::Initialize()         (webview/, 共享预热环境)
                   ├─ RegisterAllApis()                 (api/)
                   ├─ InitializeCallbacks()             (callbacks/)
                   ├─ WebViewContext::RegisterInstance(hwnd, host, bridge, id, panel)
                   └─ OnWebViewReady() → 加载前端页面
```

面板模式（DUI/CUI）走同一条 `WebViewPanel` 路径，只是 `mode_` 不同、各自持有 per-instance `BridgeCore`，并把实例登记进 `WebViewContext`。

### 事件广播与定向

`callbacks/` 产生的事件大多通过 `WebViewContext::BroadcastEvent(event, data)` 推给所有实例；需要排除发送者时用 `BroadcastEventExcept`；需要点对点时用 `SendEventTo(windowId, ...)`。`WebViewContext` 还支持子窗口 HWND → 顶层窗口的回溯查找（`GetHostByHwnd`）。

### 媒体库缓存与索引

`LibraryApi` 优先读 `LibraryCache`（命中即返回 `shared_ptr` 句柄，零深拷贝）；目录树/根浏览走 `LibraryTreeIndex`（首次访问同步构建并缓存）。`callbacks/LibraryCallback` 监听库变化时调用 `Invalidate()` 让两者失效，下次访问重建。

### JIT 流媒体队列

前端维护完整逻辑队列，后端 `QueueManager` 只在隐藏的「影子播放列表」里保留当前+下一首。临近曲末（默认剩 30s）发 `jitQueue:needNext` 让前端解析下一首 URL 再 `enqueueNext`，实现流媒体无缝衔接；`PlaybackCallback` 把 fb2k 的换曲/停止/进度回调转交给 `QueueManager` 推进状态机。

---

## 依赖关系

- **依赖**：`webview/`（`WebViewHost`/`WebViewEnvironment`）、`api/`（注册与桥接）、`callbacks/`（事件源）、`selection/`（`SelectionHolder`）、`panels/PanelConfig`、`utils/`、foobar2000 SDK。
- **被依赖**：`window/`（`MainWindow`/`PopupWindow` 继承 `WebViewPanel`）、`panels/`（DUI/CUI 实例继承 `WebViewPanel`）、`api/`（`LibraryApi`/`QueueApi` 调用缓存与队列、各 handler 经 `WebViewContext` 广播事件）、`main.cpp`（注册 `WebViewUI`、`PreferencesPage`、`BackgroundService`）。

---

## 扩展指南

- **新增运行时服务**：优先做成单例（参考 `LibraryCache`/`QueueManager`），线程安全用 `mutex`/`shared_mutex`，并明确失效/清理时机（在对应 `callbacks/` 里触发 `Invalidate`）。
- **新增可重写生命周期钩子**：在 `WebViewPanel` 增虚函数（如 `OnXxx`），基类给空实现，`MainWindow`/面板按需重写，保持三种模式行为一致。
- **新增偏好项**：在 `PreferencesPage` 增 cfg_var 与 UI 控件，并通过 `webview_prefs::*` / `security_config::*` 暴露访问函数，避免在业务层直接读全局变量。

---

参见：仓库根 [README.md](../../README.md)、[../api/README.zh-CN.md](../api/README.zh-CN.md)、[../window/README.zh-CN.md](../window/README.zh-CN.md)。

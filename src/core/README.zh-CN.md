[English](./README.md) | 中文

# src/core/ — WebView 生命周期与运行时核心

`core/` 是组件的运行时地基：它定义了所有窗口/面板共享的 WebView2 基类、多实例路由表，以及 JIT 流媒体队列。`window/` 和 `panels/` 都建立在本模块之上。foobar2000 的 UI 入口与后台模式在 [`ui/`](../README.zh-CN.md#支撑模块)，偏好设置页在 [`prefs/`](../README.zh-CN.md#支撑模块)，媒体库缓存与目录树索引在 [`domain/library/`](../README.zh-CN.md#支撑模块)。

---

## 概述

`core/` 回答三个问题：

1. **谁是 UI 入口？** `ui/` 里的 `WebViewUI`（实现 foobar2000 `user_interface`）在 fb2k 启动时创建主窗口，这个窗口建在下面的基类与路由表之上。
2. **WebView2 的通用能力放在哪？** `WebViewPanel` 把初始化、API 注册、回调初始化、消息处理、配置热重载等收敛为基类，供 `MainWindow` / `WebViewDuiElement` / `WebViewCuiPanel` 继承。
3. **多个 WebView 实例如何互相找到？** `WebViewContext` 以 HWND 为键登记所有实例，支撑跨实例事件广播与按窗口 ID 的定向路由。

除这三件事外，`core/` 还放着一个运行时服务：JIT 流媒体队列。

---

## 关键文件 / 类

| 文件 | 职责 |
|------|------|
| `WebViewPanel.h/.cpp` | WebView 面板基类：`InitializeWebView`、`RegisterAllApis`、`InitializeCallbacks`、导航/重载、`ApplyConfig` 配置热重载、`PanelConfig` 持有、`SelectionHolder` 持有；模式枚举 `Standalone/DuiPanel/CuiPanel`；定义可重写虚函数（`OnWebViewReady` 等） |
| `PanelBootstrap.cpp` | `WebViewPanel::RegisterAllApis` / `InitializeCallbacks` 的定义；全部 `api/` 与 `callbacks/` 模块头只在这个 TU 里 include |
| `PanelCrashDiagnostics.h/.cpp` | `webview:processFailed` 载荷的 `kind` 与 `recoveryAction` 两个字段 |
| `WebViewContext.h/.cpp` | 多实例管理器（单例）：`RegisterInstance`/`UnregisterInstance`（按 HWND）、`GetBridge`/`GetWebViewHost`/`GetPanelByHwnd`、`BroadcastEvent`/`BroadcastEventExcept`、按 windowId 的 `SendEventTo` 与反查 |
| `QueueManager.h/.cpp` | JIT 流媒体队列（单例）：前端逻辑队列 + 后端「影子播放列表」（仅 current+next），状态机 `Idle/Active/WaitingNext/Exhausted`，即时 URL 解析、自动缓冲、`jitQueue:needNext` 预取、影子列表锁保护。`QueueManager.cpp` 放单例、公开命令、状态查询与播放回调 |
| `QueueShadowPlaylist.cpp` | `QueueManager` 管影子列表的成员：`ShadowPlaylistLock` 锁与锁回调、查找或创建影子列表、判断是否正从影子列表播放、清理已播曲目、移除过期的缓冲下一首、在影子列表上启动播放 |
| `QueueSources.cpp` | `QueueManager` 管曲目来源的成员：判断本地路径、`PreloadBatch`、拆解子曲目后缀、地址转 handle，以及把流媒体地址或本地文件异步加入影子列表 |
| `QueueManagerInternal.h` | `QueueManager.cpp` 与 `QueueSources.cpp` 共用的事件辅助 `Announce` |
| `FrontendDirectoryResolver.h/.cpp` | 按当前配置拼出前端目录的各级候选；主窗口、弹窗、DUI/CUI 面板与面板配置对话框都经这里解析 |
| `FrontendDirectoryPolicy.h/.cpp` | 从候选里选出第一个有 `index.html` 的目录，顺序为面板模板 → 活动模板 → 组件目录下的 `foo_ui_webview2_resources\dist` → `default` 模板；不依赖 foobar2000 SDK，单测直接链接 |

---

## 工作原理 / 数据流

### WebView 生命周期

```
fb2k 启动
   └─ WebViewUI::init()                       (ui/UserInterface)
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

`callbacks/` 产生的事件经 `api/EventEmit.h` 的助手发出，助手再调用 `WebViewContext`：大多数用 `BroadcastEvent(event, data)` 推给所有实例；需要排除发送者时用 `BroadcastEventExcept`；需要点对点时用 `SendEventTo(windowId, ...)`。`WebViewContext` 还支持子窗口 HWND → 顶层窗口的回溯查找（`GetHostByHwnd`）。

### JIT 流媒体队列

前端维护完整逻辑队列，后端 `QueueManager` 只在隐藏的「影子播放列表」里保留当前+下一首。临近曲末（默认剩 30s）发 `jitQueue:needNext` 让前端解析下一首 URL 再 `enqueueNext`，实现流媒体无缝衔接；`PlaybackCallback` 把 fb2k 的换曲/停止/进度回调转交给 `QueueManager` 推进状态机。

---

## 依赖关系

- **依赖**：`webview/`（`WebViewHost`/`WebViewEnvironment`）、`api/`（注册与桥接）、`callbacks/`（事件源）、`selection/`（`SelectionHolder`）、`panels/PanelConfig`、`prefs/`（`WebViewPanel` 读 `webview_prefs::*`）、`utils/`、foobar2000 SDK。
- **被依赖**：`window/`（`MainWindow`/`PopupWindow` 继承 `WebViewPanel`）、`panels/`（DUI/CUI 实例继承 `WebViewPanel`）、`api/`（`QueueApi` 调用队列、各 handler 经 `WebViewContext` 广播事件）、`ui/`（主菜单命令经 `WebViewContext` 数实例）、`prefs/`（经 `WebViewContext`/`WebViewPanel` 重载面板与应用默认缩放）。

---

## 扩展指南

- **新增运行时服务**：优先做成单例（参考 `QueueManager` 或 `domain/library/LibraryCache`），线程安全用 `mutex`/`shared_mutex`，并明确失效/清理时机（在对应 `callbacks/` 里触发 `Invalidate`）。不依赖 `api/`、窗口与 UI 的服务放进 `domain/`。
- **新增可重写生命周期钩子**：在 `WebViewPanel` 增虚函数（如 `OnXxx`），基类给空实现，`MainWindow`/面板按需重写，保持三种模式行为一致。
- **新增偏好项**：在 `prefs/PreferencesPage`（或对应子页）增 cfg_var 与 UI 控件，并通过 `webview_prefs::*` / `security_config::*` 暴露访问函数，避免在业务层直接读全局变量。

---

参见：仓库根 [README.md](../../README.md)、[../api/README.zh-CN.md](../api/README.zh-CN.md)、[../window/README.zh-CN.md](../window/README.zh-CN.md)。

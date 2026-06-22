[English](./README.md) | 中文

# src/window/ — 窗口体系与 Chrome 子系统

`window/` 负责把 WebView2 内容承载成「真正的 Windows 窗口」：独立主窗口、多开弹窗，以及它们的边框/背景/标题栏（Chrome）、启动呈现时序、托盘与任务栏集成。它解决的是原生窗口层的所有问题——客户区扩展标题栏、Mica/Acrylic 背景、无边框、圆角、激活与置顶守护、Snap Layout 等。

---

## 概述

主窗口和弹窗都继承自 `core/WebViewPanel`（拿到 WebView2 + 桥接能力），再各自叠加原生窗口逻辑。两者同时实现统一抽象接口 `WindowShellBase`，对外提供一致的「壳状态观测 + Chrome 命令」入口；Chrome 的 resolve/apply 被收敛到一套 `WindowChrome*` 结构中，避免散落的 DWM 直写。

```
WebViewPanel (core/)
     ├── MainWindow  : WebViewPanel + WindowShellBase   (独立主窗口，单例语义)
     └── PopupWindow : WebViewPanel + WindowShellBase   (多开弹窗，受 WindowManager 管理)
```

---

## 关键文件 / 类

### 窗口家族

| 文件 | 职责 |
|------|------|
| `MainWindow.h/.cpp` | 主窗口：客户区扩展标题栏（`WM_NCCALCSIZE`/`WM_NCHITTEST`）、拖拽区、最小/最大尺寸、圆角、启动 reveal 状态机、后台挂起省内存、overlay 误激活防御、期望置顶守护 |
| `MainWindowDwm.cpp` | 主窗口 DWM 相关实现（Mica/帧扩展等拆分单元） |
| `MainWindowMenu.cpp` | 主窗口菜单命令（打开文件、偏好、DevTools、主菜单查找等） |
| `MainWindowInternal.h` | 主窗口内部共享声明 |
| `PopupWindow.h/.cpp` | 弹窗：`CreateParams` 创建参数、profile（standard/miniPlayer/desktopLyrics）、`beforeClose` 异步关闭、鼠标穿透 + 交互热区、overlay 自定义拖拽与激活链根除 |
| `WindowManager.h/.cpp` | 单例：弹窗创建/销毁/查询、跨窗口定向/广播消息、激活承接 sink、面板模式引用计数、`MAX_POPUPS=8` 限制 |
| `WindowShellBase.h/.cpp` | 统一窗口壳抽象：能力描述 `WindowShellCapabilities`、观测快照 `WindowShellSnapshot`、Chrome 命令与全屏生命周期接口 |
| `WindowTargetResolver.h/.cpp` | 统一 target 解析：`ResolveForMutation`（找不到必须失败）/ `ResolveForObservation`（可回退 main），替代散落的 caller HWND 查找 |
| `StartupPresentationCoordinator.h/.cpp` | 启动呈现决策机：根据导航完成 / ready 信号 / chrome 就绪 / 兜底定时器，决定何时 commit reveal（避免白屏闪烁） |

### Chrome 子系统（分层）

Chrome 的「解析」与「应用」严格分层，只有一套 schema，禁止引入第二套 raw/compat/resolved：

| 文件 | 层级 | 职责 |
|------|------|------|
| `WindowChromeState.h` | 数据 | 全部 chrome 数据结构：`WindowKind`、`WindowNativeFrameStrategy`、backdrop 策略、base/compat/standard/derived 状态、`WindowChromeResolverSnapshot`、`ResolvedWindowChromeState` |
| `WindowChromeResolver.h/.cpp` | 解析 | 把快照 `Resolve()` 成 `ResolvedWindowChromeState`，`NormalizeBackdropEffect` 归一化背景效果词表 |
| `WindowChromeApplier.h/.cpp` | 应用 | 通过 `WindowChromeApplyHooks`（窗口提供的回调集）把 resolved 状态落到原生窗口；`RefreshNativeFrame` 刷新原生帧 |
| `ChromeController.h/.cpp` | 入口 | 包装 Resolver + Applier，提供 `Resolve` / `Apply` / `ResolveAndApply` / `RefreshNativeFrame` 统一入口 |
| `WindowChromeTrace.h` | 诊断 | `[WindowChromeTrace]` 诊断日志（默认关闭，置环境变量 `FOO_UI_WEBVIEW2_WINDOW_TRACE=1` 开启） |

### 托盘 / 任务栏 / 菜单覆盖面

| 文件 | 职责 |
|------|------|
| `TrayIcon.h/.cpp` | 系统托盘单例：图标/气泡、三区上下文菜单（top/playback/bottom）、Native（`TrackPopupMenu`）与 WebView（自绘）双后端、富菜单项（nowplaying/rating/slider/segmented）、最小化/关闭到托盘、Explorer 重启后重注册 |
| `TaskbarIntegration.h/.cpp` | 任务栏单例（`ITaskbarList3`）：缩略图工具栏按钮、进度指示、叠加图标、闪烁；随播放状态更新默认按钮 |
| `TaskbarTrayContracts.h` | 托盘/任务栏共享契约定义 |
| `MenuOverlayHost.h/.cpp` | 自绘菜单覆盖面宿主（单例）：内持独立 `PopupWindow`（不进 `WindowManager`、不计 `MAX_POPUPS`），池化复用 WebView，支持 owner-mode 事件路由、内容尺寸窗、退场动画 |

> `TestPageHtml.inl` 是内嵌测试页 HTML。

---

## 工作原理 / 数据流

### Chrome 解析—应用流水线

```
窗口状态变化 (激活/偏好更改/全屏切换/运行时 setBackdropPolicy)
      │
      ▼
窗口构建快照  BuildChromeBaseState / StandardState / DerivedState
      │                       (+ compatibility overrides)
      ▼
ChromeController::ResolveAndApply
      ├─ WindowChromeResolver::Resolve(snapshot) ─► ResolvedWindowChromeState
      └─ WindowChromeApplier::Apply(hooks, request)
              ├─ applyFrameless()              (无边框)
              ├─ applyNativeFrameStrategy()    (Standard/Captionless/Popup/Fullscreen)
              ├─ applyCornerPreference()       (圆角)
              ├─ 应用 DWM backdrop (Mica/MicaAlt/Acrylic/none)
              └─ broadcastBackdropStateChanged() (通知前端)
```

`WindowChromeApplyHooks` 是 Applier 与具体窗口之间的解耦点：窗口把「如何无边框 / 如何设帧策略 / 如何应用圆角 / 背景应用后做什么」以 `std::function` 形式交给 Applier 调用，因此 Resolver/Applier 不直接依赖 `MainWindow`/`PopupWindow`。

### 客户区扩展标题栏（Mica 兼容）

主窗口默认 frameless：`WM_NCCALCSIZE` 移除非客户区让 WebView 铺满；`WM_NCHITTEST` 结合前端上报的拖拽区/非拖拽区返回 `HTCAPTION` 等命中码，实现「前端自绘标题栏 + 系统 Snap Layout/双击最大化」。背景效果用 DWM `DWMSBT_*` 系统背景材质，配合帧扩展实现 Mica/Acrylic 透出。

### 启动 reveal 时序

为避免「先黑屏/白屏再出内容」，主窗口启动隐藏，由 `StartupPresentationCoordinator` 汇总导航完成、页面 ready handshake、chrome 就绪三类信号决定 commit reveal；并 arm 一个兜底定时器（默认 1500ms）防止信号缺失永不显示。

### 多窗口与跨窗口消息

`WindowManager` 统一分配窗口 ID、创建/销毁弹窗，并提供 `SendWindowMessage`（定向）/ `BroadcastMessage`（广播）。overlay 类弹窗（如桌面歌词）的激活回退问题由「激活承接 sink + overlay 交互信号」机制处理，避免误把 foobar2000 主窗口推到外部应用之上。

---

## 依赖关系

- **依赖**：`core/WebViewPanel`（基类）、`core/WebViewContext`（事件广播/实例查询）、`api/`（窗口/托盘/任务栏相关 API 经此暴露给前端）、`utils/`。
- **被依赖**：`core/UserInterface` 创建并持有 `MainWindow`；`window/`、`api/WindowApi`、`api/TrayApi`、`api/TaskbarApi`、`api/MenuApi` 通过本模块落地原生窗口行为；`callbacks/PlaybackCallback` 通过 `TaskbarIntegration` 更新任务栏按钮。

---

## 扩展指南

- **新增窗口 Chrome 能力**：只在 `WindowChromeState.h` 扩 schema，再在 `Resolver`（解析规则）与 `Applier`（应用动作 + 对应 hook）补齐；窗口侧实现新的 `WindowChromeApplyHooks` 回调。不要在窗口内散写 DWM 调用。
- **新增弹窗形态**：优先用 `PopupWindow::CreateParams` 的 profile/behavior/backdropPolicy 组合，而非新增布尔分支；overlay 类务必走既有的 noActivate / clickThrough / 自定义拖拽路径。
- **新增托盘富菜单项**：扩 `TrayMenuItem::type` 与对应渲染；注意 Native 后端会降级（仅文本），WebView 后端才渲染富控件。
- **调试窗口行为**：设环境变量 `FOO_UI_WEBVIEW2_WINDOW_TRACE=1` 打开 `WindowChromeTrace` / ActivationEvidence 等诊断日志。

---

参见：`docs/execution/self-drawn-menu/`（自绘菜单设计）、仓库根 [README.md](../../README.md)。

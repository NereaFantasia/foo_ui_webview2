[English](./README.md) | 中文

# src/api/ — C++ ↔ JS 桥接与命名空间 API

`api/` 是组件的「神经中枢」：所有前端 `fb2k.invoke()` 调用都在这里被接收、路由、执行并回送，所有推给前端的事件也从这里发出。它同时承载了 30+ 命名空间的内置 API、统一错误信封、路径安全装饰器，以及面向外部 foobar2000 组件的插件扩展系统。

---

## 概述

前端与 C++ 之间只有一条物理通道：WebView2 的 `postMessage`。`BridgeCore` 在这条通道上实现了一套精简的 JSON-RPC 风格协议：

- 前端发来 `{ id, method, params }`，`BridgeCore` 按 `method` 查表执行对应 handler。
- handler 同步返回 JSON 结果，`BridgeCore` 以 `id` 回送响应。
- foobar2000 状态变化时，`callbacks/` 调用 `EmitEvent` / `BroadcastEvent` 主动推事件给前端。

`api/` 在架构中处于「窗口/面板」与「foobar2000 SDK」之间，是唯一允许把前端请求翻译为 SDK 调用的层。

---

## 关键文件 / 类

| 文件 | 职责 |
|------|------|
| `BridgeCore.h/.cpp` | 桥接核心：API 注册、消息分发、响应/事件发送、路径安全校验、单例 + per-instance 双模式 |
| `ErrorEnvelope.h` | 统一失败信封：`ApiErrorCode` 机读错误码 + `ApiEnvelope::MakeError/MakeFailureEvent` + 失败采样钩子 |
| `CallerContext.h/.cpp` | 从 `_callerHwnd` 提取调用方 bridge，把事件路由回发起调用的那个 WebView 实例 |
| `PluginRegistry.h/.cpp` | 外部插件 API 注册中心：命名空间隔离、API 发现、`system.*` 发现 API、动态加载通知 |
| `ApiConstants.h` | API 层共享常量 |
| `PlaybackApi.*` | `playback.*` + `test.*`（播放控制、音量、播放顺序、按路径播放） |
| `PlaylistApi.*` | `playlist.*`（播放列表增删改查、选择、移动、撤销/重做） |
| `LibraryApi.*` | `library.*`（媒体库搜索、枚举、根/目录树浏览、聚合统计） |
| `WindowApi.*` | `window.*`（窗口创建/控制、Chrome、跨窗口消息） |
| `ConfigApi.*` | `config.*`（输出设备、DSP 预设、advconfig、偏好页、组件信息） |
| `ArtworkApi.*` | `artwork.*`（封面获取，Base64） |
| `AudioApi.*` | `audio.*`（实时频谱订阅、整曲波形、BPM 分析、声道模式） |
| `FileApi.*` | `file.*`（文件读写/列目录/复制移动，带路径安全） |
| `MetadataApi.*` | `metadata.*`（标签读写） |
| `LyricsApi.*` | `lyrics.*`（歌词） |
| `QueueApi.*` | `queue.*` + JIT 队列（流媒体即时队列，配合 `core/QueueManager`） |
| `SelectionApi.*` | `selection.*`（选择设置/查询，配合 `selection/`） |
| `PortApi.*` / `PortHub.*` | `port.*`（命名通道）、`event.*`（语义事件）、`state.*`（跨窗口共享状态） |
| `MenuApi.*` | `menu.*`（自绘菜单、上下文菜单） |
| `TrayApi.*` / `TaskbarApi.*` | `tray.*` / `taskbar.*`（托盘与任务栏，配合 `window/`） |
| 其余 `*Api.*` | `dialog` `clipboard` `shell` `http` `keyboard` `ui` `cursor` `console` `dnd` `replaygain` `playcount` `titleformat` `misc` `output` `dsp` `discovery` 等 |

> 注册的权威清单是 `core/WebViewPanel.cpp` 中的 `WebViewPanel::RegisterAllApis()`；每个 `XxxApi.h` 顶部注释会列出该文件注册的 API 全名，是核对 API 是否存在的第一手依据。

---

## 工作原理 / 数据流

### 桥接三段式

```
① 注册 (启动时一次)
   WebViewPanel::RegisterAllApis()
        └─ RegisterPlaybackApi()
              └─ bridge.RegisterApi("playback.play", PlaybackPlay);
                 bridge.RegisterApi("playback.playPath", PlaybackPlayPath,
                                    {{"path", SecurityLevel::MediaRead}});  // 带路径安全装饰器

② 分发 (每次调用)
   前端 fb2k.invoke("playback.play", {...})
        └─ postMessage → BridgeCore::HandleMessage(message, responseTarget, callerHwnd)
              ├─ 解析 { id, method, params }
              ├─ (装饰器) 按 PathSecuritySpec 校验路径参数
              ├─ handlers_["playback.play"](params)
              └─ SendResponse(id, result)   // 或 SendError(id, code, msg)

③ 事件回流 (异步)
   callbacks/PlaybackCallback → BridgeCore::EmitEvent("playback:trackChanged", data)
        └─ postMessage → fb2k.on("playback:trackChanged", cb)
```

### 统一错误信封

handler 内失败统一用 `ApiEnvelope::MakeError(message, code)` 构造同步错误（`{ success:false, error, code }`）；异步任务失败用 `MakeFailureEvent` 推事件。框架级错误（方法不存在、JSON 非法、handler 抛异常）由 `BridgeCore::SendError` 产出，错误码取自 `ApiErrorCode`（如 `METHOD_NOT_FOUND`、`INVALID_PARAMS`、`PERMISSION_DENIED`）。

---

## 路径安全：`SecurityLevel` 5 档

带路径参数的 API 用 `RegisterApi` 的装饰器重载声明校验规格 `PathSecuritySpec`（参数 key、层级、是否数组、嵌套 key、是否跳过无效项）。分发前由 `ValidatePathParam` 统一拦截，校验失败直接 `PERMISSION_DENIED`，handler 不会被调用。

| SecurityLevel | 对应 `utils/PathSecurity.h` 函数 | 语义 |
|---------------|-------------------------------|------|
| `None` | — | 无路径参数 / 不校验 |
| `Read` | `ValidatePath` | 文件系统只读（非系统盘放行，系统盘走黑白名单） |
| `Write` | `ValidateWritePath` | 严格写白名单（仅 profile / temp 目录） |
| `MediaRead` | `ValidateMediaAccess` | 读 + 媒体库/播放列表上下文信任 |
| `MediaWrite` | `ValidateMediaWriteAccess` | 写 + 媒体上下文信任，但系统盘任意路径不放行 |

声明示例（来自 `FileApi.cpp`，多参数各自指定层级）：

```cpp
bridge.RegisterApi("file.copy", FileCopy, {
    {"source",      SecurityLevel::Read},
    {"destination", SecurityLevel::MediaWrite}
});

// 数组参数：第 3 个字段 true 表示数组，第 5 个 true 表示逐条跳过无效项
bridge.RegisterApi("playback.playPaths", PlaybackPlayPaths,
    {{"paths", SecurityLevel::MediaRead, true, "", true}});
```

---

## 扩展指南：如何新增一个 API

以新增 `playback.fooBar` 为例（内置 API）：

1. **三点验证先行**（强制，见 `AGENTS.md` §2.5）：确认 SDK 有对应能力、参数 key 与 handler 内 `params.value("key", ...)` 对齐、事件名用 colon 格式。
2. **实现 handler**：在 `PlaybackApi.cpp` 写 `static json PlaybackFooBar(const json& params)`，从 `params` 取参，调用 SDK / 服务接口，返回 JSON；失败用 `ApiEnvelope::MakeError`。
3. **注册**：在该文件的 `RegisterPlaybackApi()` 内加 `bridge.RegisterApi("playback.fooBar", PlaybackFooBar);`；若带路径参数，用装饰器重载附加 `PathSecuritySpec`。
4. **更新头部注释**：在 `PlaybackApi.h` 顶部「Registered APIs」列表补上 `playback.fooBar`，保持文档与代码一致。
5. **（如发事件）** 在对应 `callbacks/` 里用 `EmitEvent("playback:fooChanged", data)` 推送，并保证 invoke 用 dot、事件用 colon。
6. **同步 SDK 与文档**：API 改动需同步 `sdk/`（bridge.js / 类型）与 `docs/`（见 `CLAUDE.md` 开发流程）。
7. **构建验证**：`\.\build.ps1 -Config Release -Platform x64`。

> 若 handler 需要把异步事件回送给「发起调用的那个窗口」，用 `CallerContext::FromParams(params)` 拿到调用方 bridge 再 `EmitEvent`，避免错发到其它实例。

---

## 服务注入与单元测试

播放/播放列表类 handler 不直接调用 SDK，而是通过 `interfaces/` 的服务接口，便于离线单测：

```cpp
// 生产环境默认使用 Fb2kPlaybackService；测试时注入 mock
void SetPlaybackService(IPlaybackService* service);  // 传 nullptr 复位为默认
IPlaybackService* GetPlaybackService();
```

测试中先 `SetPlaybackService(&mock)`，调用 handler 后断言 mock 行为，结束再 `SetPlaybackService(nullptr)` 复位。`PlaylistApi` 提供同样的 `SetPlaylistService/GetPlaylistService`。

---

## 单例 vs per-instance 双模式

`BridgeCore` 同时支持两种生命周期：

- **单例模式**：`BridgeCore::GetInstance()`，用于独立主窗口，向后兼容。`RegisterXxxApi()` 默认注册到单例。
- **per-instance 模式**：DUI/CUI 面板各自 `new BridgeCore`，经 `WebViewContext::RegisterInstance(hwnd, host, bridge, windowId, panel)` 按 HWND 登记。`HandleMessage` 带 `responseTarget` / `callerHwnd` 参数，确保响应与事件回到正确的 WebView。

`BridgeCore` 内部以 `mutex_` 保护 `handlers_` / `securitySpecs_`，注册与分发线程安全。

---

## PluginRegistry — 外部插件扩展

其它 foobar2000 组件可以把自己的 API 暴露给前端调用：

```cpp
#include "api/PluginRegistry.h"

void RegisterMyApis() {
    auto& registry = GetPluginRegistry();   // 跨 DLL 导出的访问器
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

- 命名空间隔离：`RESERVED_NAMESPACES` 阻止外部插件冒用内置命名空间。
- API 发现：`system.*` 系列（`listAvailableApis` / `getApisByNamespace` / `searchApis` / `getApiStats` / `getRegisteredPlugins` / `isPluginRegistered`）由 `PluginRegistry::Initialize()` 注册，前端可枚举全部 API。
- 动态通知：插件注册/注销会发出 `plugin:*` / `api:*` 事件。

---

## 依赖关系

- **依赖**：`interfaces/`（服务抽象）、`utils/PathSecurity`（路径校验）、`core/WebViewContext`（多实例路由与广播）、`webview/WebViewHost`（实际 `postMessage`）。
- **被依赖**：`core/WebViewPanel` 统一调用 `RegisterAllApis()`；`callbacks/` 通过 `EmitEvent`/`BroadcastEvent` 复用本层事件通道；`window/`、`panels/`、`selection/`、`window/TrayIcon`、`core/QueueManager` 等均向本层注册或经本层暴露能力。

---

参见：仓库根 [README.md](../../README.md)「API 概览 / 插件扩展 / 安全限制」、`docs/AI_API_REFERENCE.md`、`sdk/`。

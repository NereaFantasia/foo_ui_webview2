[English](./README.md) | 中文

# src/api/ — C++ ↔ JS 桥接与命名空间 API

`api/` 是组件的「神经中枢」：所有前端 `fb2k.invoke()` 调用都在这里被接收、路由、执行并回送，所有推给前端的事件也从这里发出。它同时承载了 40 多个命名空间的内置 API、统一错误信封、路径安全校验，以及面向外部 foobar2000 组件的插件扩展系统。

---

## 概述

前端与 C++ 之间只有一条物理通道：WebView2 的 `postMessage`。`BridgeCore` 在这条通道上实现了一套精简的 JSON-RPC 风格协议：

- 前端发来 `{ id, method, params }`，`BridgeCore` 按 `method` 查表执行对应 handler。
- handler 返回结果，`BridgeCore` 以 `id` 回送响应。
- foobar2000 状态变化时，`callbacks/` 经 `EventEmit.h` 的 `api::emit::` 助手主动推事件给前端。

`api/` 在架构中处于「窗口/面板」与「foobar2000 SDK」之间，是唯一允许把前端请求翻译为 SDK 调用的层。

---

## 关键文件 / 类

| 文件 | 职责 |
|------|------|
| `schema/*.ts` | 方法与事件的声明，唯一手写的契约；`generated/` 下的 C++ 头由它生成（见 [schema/README.md](schema/README.md)） |
| `TypedApi.h` | `api::RegisterApi` / `RegisterApiDeferred`：按生成的参数类型注册 handler，编译期核对方法名与返回类型 |
| `EventEmit.h` | `api::emit::` 助手：按声明发事件，每种投递方式一个助手 |
| `BridgeCore.h/.cpp` | 桥接核心：API 注册、消息分发、响应/事件发送、路径安全校验、单例 + per-instance 双模式 |
| `ErrorEnvelope.h` | 统一失败信封：`ApiErrorCode` 机读错误码 + `ApiEnvelope::MakeError/MakeFailureEvent` + 失败采样钩子 |
| `CallerContext.h/.cpp` | 从 `_callerHwnd` 提取调用方 bridge，把事件路由回发起调用的那个 WebView 实例 |
| `PluginRegistry.h/.cpp` | 外部插件 API 注册中心：命名空间隔离、API 发现、`system.*` 发现 API、动态加载通知 |
| `ApiConstants.h` | API 层共享常量 |
| `PlaybackApi.*` | `playback.*` + `test.*`（播放控制、音量、播放顺序、按路径播放） |
| `MediaApi.*` | `media.*`（把本地音视频文件交给页面的 `<video>` / `<audio>`，配合 `media/`） |
| `PlaylistApi.*` | `playlist.*`（播放列表增删改查、选择、移动、撤销/重做） |
| `LibraryApi.*` | `library.*`（媒体库搜索、枚举、根/目录树浏览、聚合统计） |
| `WindowApi.*` | `window.*`（窗口创建/控制、Chrome、跨窗口消息） |
| `ConfigApi.*` | `config.*`（输出设备、DSP 预设、advconfig、偏好页、组件信息） |
| `WebviewApi.*` | `webview.*`（页面从哪里加载） |
| `ArtworkApi.*` | `artwork.*`（封面获取，Base64） |
| `AudioApi.*` | `audio.*`（实时频谱订阅、整曲波形、BPM 分析、声道模式） |
| `FileApi.*` | `file.*`（文件读写/列目录/复制移动，带路径安全） |
| `MetadataApi.*` | `metadata.*` + `rating.*`（标签读写、评分） |
| `LyricsApi.*` | `lyrics.*`（歌词） |
| `QueueApi.*` | `queue.*` + `jitQueue.*`（流媒体即时队列，配合 `core/QueueManager`） |
| `SelectionApi.*` | `selection.*`（选择设置/查询，配合 `selection/`） |
| `PortApi.*` / `PortHub.*` | `port.*`（命名通道）、`event.*`（语义事件）、`state.*`（跨窗口共享状态） |
| `MenuApi.*` | `menu.*`（自绘菜单、上下文菜单） |
| `TrayApi.*` / `TaskbarApi.*` | `tray.*` / `taskbar.*`（托盘与任务栏，配合 `window/`） |
| 其余 `*Api.*` | `dialog` `clipboard` `shell` `http` `keyboard` `ui` `cursor` `console`（含 `log.*`） `dnd` `replaygain` `playcount` `titleformat` `misc` `output` `dsp` `discovery` 等；`panel.*` 与部分 `system.*` 在 `WindowApi` |

> 方法与事件以 `schema/<ns>.ts` 的声明为准；注册入口是 `core/PanelBootstrap.cpp` 中的 `WebViewPanel::RegisterAllApis()`。提交检查核对声明与注册一一对应。

---

## 工作原理 / 数据流

### 桥接三段式

```
① 注册 (启动时一次)
   WebViewPanel::RegisterAllApis()
        └─ RegisterPlaybackApi()
              └─ api::RegisterApi("playback.play", PlaybackPlay);
                 api::RegisterApi("playback.playPath", PlaybackPlayPath);
                 // 参数结构体、解析器与路径安全规格都来自 schema/playback.ts 生成的头

② 分发 (每次调用)
   前端 fb2k.invoke("playback.play", {...})
        └─ postMessage → BridgeCore::HandleMessage(message, responseTarget, callerHwnd)
              ├─ 解析 { id, method, params }
              ├─ 按声明的 @security 校验路径参数
              ├─ 生成的解析器把 params 转成参数结构体，未声明的键或类型不对返回 INVALID_PARAMS
              ├─ handler(parsed) 返回 Result<R>
              └─ SendResponse(id, result)   // 或 SendError(id, code, msg)

③ 事件回流 (异步)
   callbacks/PlaybackCallback → api::emit::Broadcast<playback::events::TrackChanged>(row)
        └─ postMessage → fb2k.on("playback:trackChanged", cb)
```

`RegisterApiDeferred` 注册的方法不在 handler 返回时回包，而是由 handler 拿到的 `DeferredResponder` 在稍后（例如工作线程完成后）回送。

### 统一错误信封

handler 失败时返回 `api::Fail(message, code[, extra])`（`ApiResult.h`），注册包装层再用 `ApiEnvelope::MakeError` 转成失败信封 `{ success:false, error, code }`；异步任务失败用 `MakeFailureEvent` 推事件。框架级错误（方法不存在、JSON 非法、handler 抛异常）由 `BridgeCore::SendError` 产出，错误码取自 `ApiErrorCode`（如 `METHOD_NOT_FOUND`、`INVALID_PARAMS`、`PERMISSION_DENIED`）。

---

## 路径安全：`SecurityLevel` 6 档

路径参数的校验级别写在声明里：`schema/<ns>.ts` 中参数上的 `@security` 标签，路径数组可以再加 `@skipInvalid`，对象数组用 `@pathKey` 指明持有路径的成员（写法见 [schema/README.md](schema/README.md)）。生成器把它转成 `PathSecuritySpec`，注册时一并交给 `BridgeCore`。分发前由 `ValidatePathParam` 统一拦截，校验失败时 handler 不会被调用；路径被拒返回 `PERMISSION_DENIED`，参数形状或类型不对返回 `INVALID_PARAMS`。

| SecurityLevel | 对应 `domain/PathSecurity.h` 函数 | 语义 |
|---------------|-------------------------------|------|
| `None` | — | 无路径参数 / 不校验 |
| `Read` | `ValidatePath` | 文件系统只读（非系统盘放行，系统盘走黑白名单） |
| `Write` | `ValidateWritePath` | 严格写白名单（仅 profile / temp 目录） |
| `MediaRead` | `ValidateMediaAccess` | 读 + 媒体库/播放列表上下文信任 |
| `MediaWrite` | `ValidateMediaWriteAccess` | 写 + 媒体上下文信任，但系统盘任意路径不放行 |
| `FileWrite` | `ValidateFileWriteAccess` | 写白名单 → 监视目录 → 非系统盘直通 → 库/列表信任（按此顺序）；承载 `file.*` 通用写端点 |

声明示例（来自 `schema/file.ts`，多参数各自指定层级）：

```ts
interface CopyParams {
  /**
   * Source file path.
   * @security Read
   */
  source: string;
  /**
   * Destination file path.
   * @security FileWrite
   */
  destination: string;
}
```

---

## 扩展指南：如何新增一个 API

方法与事件的参数、返回值、路径安全级别和事件载荷只写在 `schema/<ns>.ts`，C++ 参数结构体与解析器（`generated/<Ns>Schema.h`）、SDK 类型和文档区块都由它生成，生成物不手改。以新增 `playback.fooBar` 为例：

1. **先写声明**：在 `schema/playback.ts` 的 `Api` 接口里加 `fooBar(params: FooBarParams): FooBarResult;`，写好参数与返回值接口；带路径的参数加 `@security`。写法见 [schema/README.md](schema/README.md)。
2. **重新生成**：`node scripts/api-schema/generate.mjs --write`，再 `npm --prefix sdk run gen:types`。
3. **实现 handler**：在 `PlaybackApi.cpp` 写 `api::Result<playback::FooBarResult> PlaybackFooBar(const playback::FooBarParams& params)`，失败时返回 `api::Fail(message, ApiErrorCode::…)`。需要知道调用方页面时用带 `const CallerContext&` 的重载。
4. **注册**：在 `RegisterPlaybackApi()` 里加 `api::RegisterApi("playback.fooBar", PlaybackFooBar);`。方法名与生成的参数类型对不上，或 handler 返回了别的方法的结果类型，都在编译期报错。
5. **（如发事件）** 在 `schema/playback.ts` 的 `Events` 接口里声明事件与载荷，发射处用 `EventEmit.h` 的 `api::emit::` 助手，不写事件名字面量。
6. **构建验证**：`.\build.ps1 -Config Release -Platform x64`。构建会先按声明重写 C++ 头，最后跑单元测试。

> 若 handler 需要把异步事件回送给「发起调用的那个窗口」，用 `CallerContext` 重载拿到调用方，再用 `api::emit::EmitTo` / `ToCaller` 发送，避免错发到其它实例。

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

- **依赖**：`interfaces/`（服务抽象）、`domain/PathSecurity`（路径校验）、`core/WebViewContext`（多实例路由与广播）、`webview/WebViewHost`（实际 `postMessage`）。
- **被依赖**：`core/WebViewPanel` 统一调用 `RegisterAllApis()`；`callbacks/` 通过 `api::emit::` 助手复用本层事件通道；`window/`、`panels/`、`selection/`、`window/TrayIcon`、`core/QueueManager` 等均向本层注册或经本层暴露能力。

---

参见：仓库根 [README.md](../../README.md)「API 概览 / 插件扩展 / 安全限制」、`docs/vitepress/` 文档站、`sdk/`。

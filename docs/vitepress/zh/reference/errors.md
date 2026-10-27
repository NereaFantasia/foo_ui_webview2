# 错误处理参考

> **概述**: 统一错误信封 (ErrorEnvelope) 规范

## 错误结构概述

foo_ui_webview2 的所有 API 在失败时遵循统一的错误信封结构。

### 同步 API 错误

通过 `fb2k.invoke()` 调用的 API 在失败时返回：

```json
{
  "success": false,
  "error": "Human-readable error message",
  "code": "MACHINE_READABLE_CODE"
}
```

- `success` — 始终为 `false`
- `error` — 面向人类的错误描述（string）
- `code` — 机器可读错误码（string，UPPER_SNAKE_CASE）

部分 API 还可能包含 `details` 对象提供额外上下文：

```json
{
  "success": false,
  "error": "Playlist is locked",
  "code": "LOCKED",
  "details": { "playlist": 3, "isLocked": true }
}
```

### 异步失败事件

后台任务或异步操作失败时通过事件推送：

```json
{
  "error": "Failed to open decoder",
  "code": "DECODER_FAILED",
  "taskId": "waveform_42",
  "path": "E:\\Music\\song.flac"
}
```

### 框架级错误

当请求格式无效或方法不存在时，由 BridgeCore 框架直接返回：

```json
{
  "error": "Method not found: foo.bar",
  "code": "METHOD_NOT_FOUND"
}
```

### 超时

`fb2k.invoke()` 等待应答 30 秒，之后以 `code` 为 `TIMEOUT` 的 `Error` 拒绝。宿主可能仍在执行这次调用，也可能已经做完，迟到的应答会被丢弃，所以结果应当作未知处理。见[操作结果](./bridge.md#operation-results)。

## 标准错误码 (ApiErrorCode)

### 框架级

| Code | 说明 |
| --- | --- |
| `INVALID_REQUEST` | 请求缺少 method 字段 |
| `METHOD_NOT_FOUND` | 调用的方法名不存在 |
| `INTERNAL_ERROR` | handler 抛出未捕获异常 |

### 参数错误

| Code | 说明 |
| --- | --- |
| `REQUIRED_PARAM` | 必填参数缺失。历史端点 `audio.cancelFullWaveform` 返回此码；其余端点缺少必填参数时返回 `INVALID_PARAMS`（错误消息标明缺失参数名） |
| `INVALID_PARAMS` | 参数值非法 |
| `INVALID_INDEX` | 索引越界（播放列表、曲目等） |

### 状态/资源错误

| Code | 说明 |
| --- | --- |
| `NOT_FOUND` | 资源不存在 |
| `OPERATION_PENDING` | 媒体库仍在加载 |
| `STALE_TARGET` | 目标状态或内容版本已过期（乐观并发冲突） |
| `LOCKED` | 播放列表处于锁定状态 |
| `NOT_SUPPORTED` | 当前环境或数据不支持这个操作 |
| `PANEL_MODE_UNSUPPORTED` | 从 DUI/CUI 面板调用了需要独立主窗口的方法。要预先知道运行模式，用 `window.getMode` |
| `LIBRARY_DISABLED` | 媒体库未启用 |
| `NO_ACTIVE_ITEM` | 无活动播放列表、当前无播放曲目，或 `jitSession.skip` 的下一条尚未就绪 |
| `CONFLICT` | 当前状态与会话命令冲突，或请求标识被用于不同载荷、已超出保留期 |
| `NOT_OWNER` | 调用文档不持有当前 JIT 会话的控制令牌 |

### 操作失败

| Code | 说明 |
| --- | --- |
| `OPERATION_FAILED` | 通用操作失败 |
| `CANCELLED` | 调用方取消了异步任务（如 `audio.cancelFullWaveform`） |
| `TIMEOUT` | 页面等待超过 30 秒超时，调用可能已被执行。由 `fb2k.invoke()` 报告，非宿主返回 |

### 安全

| Code | 说明 |
| --- | --- |
| `PERMISSION_DENIED` | PathSecuritySpec 路径校验拒绝访问；参数形状或类型不对而非路径策略拒绝时返回 `INVALID_PARAMS` |
| `ORIGIN_DENIED` | 页面来源不受信任，不能使用这项能力。宿主不信任的页面发来的每个调用都以它失败（见[安全限制](./security.md#哪些页面能调用-api)）；拿不到文件路径的页面调用 `dnd.prepareDrag` 时返回它；`audio.decodePcm` 进行中页面转到了不受信任的来源时，`audio:pcmFailed` 带这个码 |

### 媒体/路径相关

| Code | 说明 |
| --- | --- |
| `MISSING_PATH` | path 参数未提供 |
| `INVALID_PATH` | 路径无效或文件不存在 |
| `INVALID_HANDLE` | metadb handle 创建失败 |
| `NO_INFO` | 文件技术信息获取失败 |
| `DECODER_FAILED` | 音频解码器打开失败 |
| `DECODE_FAILED` | 解码过程中发生异常 |
| `UNKNOWN_ERROR` | 未知错误 |
| `EXCEPTION` | 捕获到未归类异常时返回 |

### 菜单与端口

菜单命令调用与端口通信定义了以下专用错误码：

| Code | 说明 |
| --- | --- |
| `MENU_ITEM_DISABLED` | 菜单命令存在但被禁用（`menu.runMainMenuCommand`） |
| `MENU_MATCH_AMBIGUOUS` | 命令名或路径匹配到多条命令，失败里的 `candidates` 列出它们（`menu.runMainMenuCommand`） |
| `MENU_COMMAND_NOT_FOUND` | 没有菜单命令是这个名称或路径（`menu.runMainMenuCommand`、`menu.runContextCommand`） |
| `PORT_NOT_FOUND` | 没有打开的端口是这个 id（`port.disconnect`、`port.postMessage`、`port.postMessageTo`） |
| `TARGET_NOT_FOUND` | `port.postMessageTo` 的目标端口不存在 |

## TypeScript 类型

```typescript
import type { ErrorEnvelope, FailureEventPayload, ApiErrorCode, BaseResponse } from 'foo-webview-sdk';
```

- `ErrorEnvelope` — 同步 API 失败的最小结构
- `FailureEventPayload` — 异步失败事件 data 的最小结构
- `ApiErrorCode` — 所有标准错误码的联合类型
- `BaseResponse` — 已扩展，包含可选的 `error` 和 `code` 字段

## 错误处理示例

```javascript
// 同步 API 错误处理
const result = await fb2k.invoke('library.browseTree', { rootId: 'invalid' });
if (result.success === false) {
    console.error(`错误 [${result.code}]: ${result.error}`);
    // 错误 [NOT_FOUND]: Unknown rootId
}

// 异步事件错误处理
fb2k.on('audio:fullWaveformFailed', (event) => {
    console.error(`波形生成失败 [${event.code}]: ${event.error}`);
    console.error(`任务 ID: ${event.taskId}, 路径: ${event.path}`);
});

// HTTP 异步错误处理
fb2k.on('http:response', (data) => {
    if (data.error) {
        console.error(`HTTP 请求 ${data.requestId} 失败 [${data.code}]: ${data.error}`);
    }
});
```

## 兼容性说明 {#向后兼容}

- 同步 API 失败响应均保证包含 `success: false` 与面向人类排障的 `error` 文本描述。
- 机器可读的 `code` 字段覆盖所有标准错误分类，消费方应优先使用 `code` 进行逻辑分流与国际化提示。
- artwork 相关端点额外提供专用 `available` 字段指示封面可用性。
- 框架级拒绝与业务 handler 抛出的未捕获异常均被包装为标准错误信封。

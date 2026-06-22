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
  "path": "E:\\\\Music\\\\song.flac"
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

## 标准错误码 (ApiErrorCode) 

### 框架级 

| Code | 说明 |
| --- | --- |
| INVALID_REQUEST | 请求缺少 method 字段 |
| METHOD_NOT_FOUND | 调用的方法名不存在 |
| INTERNAL_ERROR | handler 抛出未捕获异常 |

### 参数错误 

| Code | 说明 |
| --- | --- |
| REQUIRED_PARAM | 必填参数缺失 |
| INVALID_PARAMS | 参数值非法 |
| INVALID_INDEX | 索引越界（播放列表、曲目等） |

### 状态/资源错误 

| Code | 说明 |
| --- | --- |
| NOT_FOUND | 资源不存在 |
| LOCKED | 播放列表处于锁定状态 |
| NOT_SUPPORTED | 当前模式不支持此操作（如面板模式） |
| LIBRARY_DISABLED | 媒体库未启用 |
| NO_ACTIVE_ITEM | 无活动播放列表或当前无播放曲目 |

### 操作失败 

| Code | 说明 |
| --- | --- |
| OPERATION_FAILED | 通用操作失败 |

### 安全 

| Code | 说明 |
| --- | --- |
| PERMISSION_DENIED | PathSecuritySpec 路径校验拒绝访问 |

### 媒体/路径相关 

| Code | 说明 |
| --- | --- |
| MISSING_PATH | path 参数未提供 |
| INVALID_PATH | 路径无效或文件不存在 |
| INVALID_HANDLE | metadb handle 创建失败 |
| NO_INFO | 文件技术信息获取失败 |
| DECODER_FAILED | 音频解码器打开失败 |
| DECODE_FAILED | 解码过程中发生异常 |
| UNKNOWN_ERROR | 未知错误 |
| EXCEPTION | 异常捕获兜底 |

## TypeScript 类型 

```typescript
import type { ErrorEnvelope, FailureEventPayload, ApiErrorCode } from 'sdk/index.d.ts';
```

- `ErrorEnvelope` — 同步 API 失败的最小结构
- `FailureEventPayload` — 异步失败事件 data 的最小结构
- `ApiErrorCode` — 所有标准错误码的联合类型
- `BaseResponse` — 已扩展，包含可选的 `error` 和 `code` 字段

## 错误处理示例 

```javascript
// 同步 API 错误处理
const result = await fb2k.invoke('library.browseTree', { rootId: 'invalid' });
if (!result.success) {
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

## 向后兼容 

- 已有 API 中 `{ success: false, error: string }` 结构保持不变
- `code` 字段为新增，消费方应以 optional 方式读取
- artwork 系列的 `available` 字段继续保留
- 框架级错误（SendError）新增 `code` 字段，原有 `error` 字段不变

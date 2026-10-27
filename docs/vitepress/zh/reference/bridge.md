# Bridge 协议

插件通过 WebView2 的 `window.chrome.webview.postMessage` 与 C++ 层通信，上层封装为 `window.fb2k` 对象。

## Bridge 对象

插件会自动注入 `window.fb2k` 对象，形状如下：

```ts
interface Fb2k {
    // 调用 API
    invoke(method: string, params?: object): Promise<any>;

    // 监听事件；返回一个函数，调用它即移除这个回调
    on(event: string, callback: (data: any) => void): () => void;

    // 移除传给 on() 的回调
    off(event: string, callback: (data: any) => void): void;
}
```

名字以 `_` 开头的成员是内部实现，勿直接使用。

## 请求格式

```json
{
    "id": 1,
    "method": "playback.play",
    "params": {}
}
```

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | number | 自增请求 ID，用于匹配对应响应 |
| `method` | string | API 方法全名（格式为 `namespace.action`） |
| `params` | object | 请求参数对象 |

## 响应格式

### 成功响应

```json
{
    "type": "response",
    "id": 1,
    "result": { "success": true }
}
```

handler 自己报告的失败同样放在 `result` 里，`fb2k.invoke()` 照常 resolve：

```json
{
    "type": "response",
    "id": 2,
    "result": {
        "success": false,
        "error": "Invalid playlist index",
        "code": "INVALID_INDEX"
    }
}
```

### 错误响应

宿主直接拒绝请求时（缺少 `method`、方法不存在、handler 抛出异常），响应里没有 `result`，而是 `error` 与 `code`：

```json
{
    "type": "response",
    "id": 3,
    "error": "Method not found: foo.bar",
    "code": "METHOD_NOT_FOUND"
}
```

此时 `fb2k.invoke()` 以 `Error` reject，`message` 是 `error`，`code` 属性是 `code`。错误码见[错误处理参考](./errors.md)。

## 事件格式

```json
{
    "type": "event",
    "event": "playback:trackChanged",
    "data": {
        "title": "Song Name",
        "artist": "Artist Name",
        "album": "Album Name",
        "duration": 180.5
    }
}
```

## 操作结果 {#operation-results}

耗时的调用（展开播放列表文件、复制文件、探测标签等）立即返回一张回执，结束情况另由事件报告。回执里有操作 id（`operationId`），结束事件带同一个 id。

- **回执不是结果。** 回执里的 `status: 'pending'` 只表示宿主受理了调用。
- **如何结束。** 结束事件与状态查询在适用时使用这几个值：`completed`、`partial`（做了一部分）、`cancelled`、`failed`（什么也没做成）、`unknown`（宿主无从得知）。各方法写明它会报哪些值。
- **先订阅。** 结束事件可能在代码处理回执之前就到达。先订阅再调用，把还没对上 id 的事件暂存起来：

```javascript
async function addPathsAndWait(paths) {
    const early = new Map();
    let waiter = null;
    const off = fb2k.on('playlist:addComplete', (event) => {
        if (waiter?.id === event.operationId) waiter.resolve(event);
        else early.set(event.operationId, event);
    });
    try {
        const receipt = await fb2k.invoke('playlist.addPathsAsync', { paths });
        if (!receipt.success) return receipt;
        return early.get(receipt.operationId)
            ?? await new Promise((resolve) => { waiter = { id: receipt.operationId, resolve }; });
    } finally {
        off();
    }
}
```

- **错过的事件。** 方法提供状态查询时（如 `jitSession.getOperation`），页面重载后、或事件可能早于订阅到达时，用它补查。
- **命令不回报结果。** 菜单命令的应答带 `completion: 'unknown'`：foobar2000 不报告命令是否做完，也不报告它打开的对话框是否被取消。
- **超时不等于失败。** `fb2k.invoke()` 等待应答 30 秒，之后以 `code: 'TIMEOUT'` 拒绝。宿主可能仍在执行这次调用，也可能已经做完；迟到的应答会被丢弃。弹出模态对话框的菜单命令（如重命名播放列表）会让调用一直等到对话框关闭。

## 重要注意事项

### 异步操作

所有 `fb2k.invoke()` 调用都返回 Promise，**必须使用 `await` 或 `.then()`**：

```javascript
// ✅ 正确
await fb2k.invoke('playback.play');
const current = await fb2k.invoke('playback.getCurrentTrack');

// ❌ 错误：忘记 await 会导致操作未完成
fb2k.invoke('playback.play');
const pending = fb2k.invoke('playback.getCurrentTrack'); // 返回 Promise 而非结果
```

### 事件名称格式

所有事件使用 **冒号分隔** 的命名格式：

```javascript
// ✅ 正确格式
fb2k.on('playback:trackChanged', callback);
fb2k.on('playback:stateChanged', callback);
fb2k.on('playlist:itemsAdded', callback);
```

```javascript @ts-nocheck
// ❌ 错误格式（不支持）
fb2k.on('playback.trackChanged', callback);
fb2k.on('playbackTrackChanged', callback);
```

### 音量格式

| 场景 | 范围 | 说明 |
| --- | --- | --- |
| API 输入/输出 | 0–100 | 百分比数值，不一定是整数；0 表示静音，100 表示最大音量 |
| 滑块控件 | 0–100 | 直接双向绑定 |
| dB 换算 | — | 百分比 = 100 × 10^(dB/20)；-100dB 对应 0%，0dB 对应 100% |

### 路径格式

API 返回的曲目对象包含两个路径字段：

| 字段 | 说明 |
| --- | --- |
| `path` | foobar2000 内部路径（可能是 `file-relative://` 等特殊协议格式） |
| `absolutePath` | 本地文件系统绝对物理路径（推荐优先使用） |

::: warning 始终使用 absolutePath
在调用需要文件路径的 API（如 `artwork.getForTrack`）时，始终使用 `absolutePath` 而非 `path`。
:::

#### 路径类型

| 路径前缀 | 类型 | 示例 |
| --- | --- | --- |
| `C:\` `D:\` | 本地物理文件 | `D:\Music\song.flac` |
| `file://` | URI 协议格式 | `file://D:/Music/song.flac` |
| `file-relative://` | 相对路径 | `file-relative://../../song.flac` |
| `archive://` | 压缩包归档条目 | `archive://D:\Album.zip|track01.flac` |
| `cdda://` | CD 音轨 | `cdda://E,1` |
| `http://` `https://` | 网络流媒体 | `https://stream.example.com/live` |

#### 文件类型识别

```javascript
function getFileType(absolutePath) {
    if (!absolutePath) return 'unknown';
    if (absolutePath.startsWith('http://') || 
        absolutePath.startsWith('https://')) return 'stream';
    if (absolutePath.startsWith('cdda://')) return 'cd';
    if (absolutePath.startsWith('archive://') ||
        absolutePath.startsWith('unpack://')) return 'archive';
    return 'local';
}
```

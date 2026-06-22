# Bridge 协议 

插件通过 WebView2 的 `window.chrome.webview.postMessage` 与 C++ 层通信，上层封装为 `window.fb2k` 对象。

## Bridge 对象 

插件会自动注入 `window.fb2k` 对象：

```javascript
window.fb2k = {
    // 调用 API
    invoke(method, params) → Promise<result>,
    
    // 监听事件
    on(event, callback) → unsubscribe function,
    
    // 内部属性（勿直接使用）
    _callbacks: Map,
    _callId: number,
    _evts: Object
};
```

## 请求格式 

```json
{
    "id": 1,
    "method": "playback.play",
    "params": {}
}
```

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| id | number | 自增请求 ID，用于匹配响应 |
| method | string | API 方法名，格式为 namespace.action |
| params | object | 请求参数 |

## 响应格式 

### 成功响应 

```json
{
    "type": "response",
    "id": 1,
    "result": { "success": true }
}
```

### 错误响应 

```json
{
    "type": "response",
    "id": 1,
    "error": "Invalid playlist index"
}
```

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

## 重要注意事项 

### 异步操作 

所有 `fb2k.invoke()` 调用都返回 Promise，**必须使用 `await` 或 `.then()`**：

```javascript
// ✅ 正确
await fb2k.invoke('playback.play');
const track = await fb2k.invoke('playback.getCurrentTrack');

// ❌ 错误：忘记 await 会导致操作未完成
fb2k.invoke('playback.play');
const track = fb2k.invoke('playback.getCurrentTrack'); // 返回 Promise 而非结果
```

### 事件名称格式 

所有事件使用 **冒号分隔** 的命名格式：

```javascript
// ✅ 正确格式
fb2k.on('playback:trackChanged', callback);
fb2k.on('playback:stateChanged', callback);
fb2k.on('playlist:itemsAdded', callback);

// ❌ 错误格式（不支持）
fb2k.on('playback.trackChanged', callback);
fb2k.on('playbackTrackChanged', callback);
```

### 音量格式 

| 场景 | 范围 | 说明 |
| --- | --- | --- |
| API 输入/输出 | 0-100 | 整数百分比，0=静音，100=最大 |
| 滑块控件 | 0-100 | 直接绑定 |
| dB 换算 | - | 0% ≈ -100dB，100% = 0dB |

### 路径格式 

API 返回的曲目对象包含两个路径字段：

| 字段 | 说明 |
| --- | --- |
| path | foobar2000 内部路径（可能是 file-relative:// 等特殊格式） |
| absolutePath | 本地文件系统绝对路径（推荐使用） |

::: warning 始终使用 absolutePath
在调用需要文件路径的 API（如 `artwork.getForTrack`）时，始终使用 `absolutePath` 而非 `path`。
:::

#### 路径类型 

| 路径前缀 | 类型 | 示例 |
| --- | --- | --- |
| C:\\ D:\\ | 本地文件 | D:\\Music\\song.flac |
| file:// | URI 格式 | file://D:/Music/song.flac |
| file-relative:// | 相对路径 | file-relative://../../song.flac |
| archive:// | 压缩包 | archive://D:\\Album.zip\|track01.flac |
| cdda:// | CD 音轨 | cdda://E,1 |
| http:// https:// | 网络流 | https://stream.example.com/live |

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

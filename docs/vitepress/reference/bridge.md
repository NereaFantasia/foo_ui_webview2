# Bridge Protocol

The component talks to the C++ host through WebView2 `window.chrome.webview.postMessage`, wrapped as the `window.fb2k` object.

## Bridge object

The component injects `window.fb2k` with this shape:

```ts
interface Fb2k {
    // Call an API
    invoke(method: string, params?: object): Promise<any>;

    // Subscribe to an event; returns a function that removes this handler
    on(event: string, callback: (data: any) => void): () => void;

    // Remove a handler that was passed to on()
    off(event: string, callback: (data: any) => void): void;
}
```

Members whose names start with `_` are internal; do not use them.

## Request format

```json
{
    "id": 1,
    "method": "playback.play",
    "params": {}
}
```

| Field | Type | Description |
| --- | --- | --- |
| id | number | Auto-increment request id used to match responses |
| method | string | API method name as `namespace.action` |
| params | object | Request parameters |

## Response format

### Success

```json
{
    "type": "response",
    "id": 1,
    "result": { "success": true }
}
```

A failure that the handler reports itself also arrives in `result`, and `fb2k.invoke()` resolves with it:

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

### Error

When the host refuses the request itself (no `method`, an unknown method, or a handler that threw), the response carries `error` and `code` instead of `result`:

```json
{
    "type": "response",
    "id": 3,
    "error": "Method not found: foo.bar",
    "code": "METHOD_NOT_FOUND"
}
```

`fb2k.invoke()` then rejects with an `Error` whose `message` is `error` and whose `code` property is `code`. See the [error reference](./errors.md) for the codes.

## Event format

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

## Important notes

### Async calls

Every `fb2k.invoke()` returns a Promise — **always `await` or use `.then()`**:

```javascript
// ✅ Correct
await fb2k.invoke('playback.play');
const current = await fb2k.invoke('playback.getCurrentTrack');

// ❌ Wrong: missing await leaves the call unfinished
fb2k.invoke('playback.play');
const pending = fb2k.invoke('playback.getCurrentTrack'); // Promise, not result
```

### Event name format

Events use **colon-separated** names:

```javascript
// ✅ Correct
fb2k.on('playback:trackChanged', callback);
fb2k.on('playback:stateChanged', callback);
fb2k.on('playlist:itemsAdded', callback);
```

```javascript @ts-nocheck
// ❌ Unsupported
fb2k.on('playback.trackChanged', callback);
fb2k.on('playbackTrackChanged', callback);
```

### Volume format

| Context | Range | Notes |
| --- | --- | --- |
| API input/output | 0-100 | Percent as a number, not necessarily whole; 0=mute, 100=max |
| Slider controls | 0-100 | Bind directly |
| dB conversion | - | percent = 100 × 10^(dB/20); -100dB reads as 0%, 0dB as 100% |

### Path format

Track objects returned by the API include two path fields:

| Field | Description |
| --- | --- |
| path | foobar2000 internal path (may be `file-relative://`, etc.) |
| absolutePath | Local filesystem absolute path (preferred) |

::: warning Always prefer absolutePath
When calling path-based APIs such as `artwork.getForTrack`, pass `absolutePath`, not `path`.
:::

#### Path kinds

| Prefix | Kind | Example |
| --- | --- | --- |
| C:\\ D:\\ | Local file | D:\\Music\\song.flac |
| file:// | URI form | file://D:/Music/song.flac |
| file-relative:// | Relative path | file-relative://../../song.flac |
| archive:// | Archive entry | archive://D:\\Album.zip\|track01.flac |
| cdda:// | CD track | cdda://E,1 |
| http:// https:// | Network stream | https://stream.example.com/live |

#### File type detection

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

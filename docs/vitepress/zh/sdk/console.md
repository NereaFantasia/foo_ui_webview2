# fb.console 控制台

`fb.console` 把消息写到 foobar2000 的控制台窗口，与浏览器的全局 `console` 无关。

每个方法写一行。只传 `message` 时，这一行就是消息原文。`message` 之后的值依次接在同一行，用空格隔开：字符串照原样，其他 JSON 值（数字、布尔、`null`、数组、对象）写成 JSON 文本，与浏览器控制台一样。这时 SDK 把全部内容作为宿主的 `args` 发出，因为宿主只在没有 `message` 时才读 `args`。

## error(message, ...args)

签名：`fb.console.error(message: string, ...args: NonNullable<ConsoleErrorParams['args']>): Promise<ConsoleErrorResponse>`

写一行错误级消息，前缀 `[WebView][ERROR]`。

```javascript
await fb.console.error('Artwork loading failed');
```

## log(message, ...args)

签名：`fb.console.log(message: string, ...args: NonNullable<ConsoleLogParams['args']>): Promise<ConsoleLogResponse>`

写一行普通消息，前缀 `[WebView]`。

```javascript
await fb.console.log('Theme initialized');

// 写出：[WebView] Loaded 42 tracks {"view":"albums"}
const r = await fb.console.log('Loaded', 42, 'tracks', { view: 'albums' });
if (r.success === false) {
    console.warn('console.log failed:', r.error);
}
```

## warn(message, ...args)

签名：`fb.console.warn(message: string, ...args: NonNullable<ConsoleWarnParams['args']>): Promise<ConsoleWarnResponse>`

写一行警告级消息，前缀 `[WebView][WARN]`。

```javascript
await fb.console.warn('Using fallback artwork');
await fb.console.warn('Cover missing for', 'C:\\Music\\a.flac');
```

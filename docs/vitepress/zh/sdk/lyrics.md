# fb.lyrics lyrics

本页是 `fb.lyrics` 的 SDK 视角文档入口。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## 其余方法

### exists()

封装 `lyrics.exists`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.lyrics.exists(/* 参数见 TypeScript 声明 */);
```

### get()

封装 `lyrics.get`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.lyrics.get(/* 参数见 TypeScript 声明 */);
```

<!-- END AUTO-GENERATED SDK STUBS -->

## 保存歌词

`fb.lyrics.save(path, lyricsText, options?)` 调用 `lyrics.save`。`options` 的类型为 `Omit<LyricsSaveParams, 'path' | 'lyrics'>`，可包含 `filename`、`tagName`、`format` 与 `target`。

SDK 返回类型为 `BaseResponse & { results?: Array<{ target: string; success: boolean; error?: string }>; savedTo?: string[] }`。

```javascript
const result = await fb.lyrics.save(
	'E:\\Music\\song.flac',
	'[00:00.00]歌词……',
	{
		target: ['file', 'embedded'],
		format: 'lrc',
		tagName: 'SYNCEDLYRICS',
	},
);
```

# fb.replaygain replaygain

本页是 `fb.replaygain` 的 SDK 视角文档入口。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## 其余方法

### clear()

封装 `replaygain.clear`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.replaygain.clear(/* 参数见 TypeScript 声明 */);
```

### get()

封装 `replaygain.get`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.replaygain.get(/* 参数见 TypeScript 声明 */);
```

### getMode()

封装 `replaygain.getMode`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.replaygain.getMode(/* 参数见 TypeScript 声明 */);
```

### getPreamp()

封装 `replaygain.getPreamp`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.replaygain.getPreamp(/* 参数见 TypeScript 声明 */);
```

### getSettings()

封装 `replaygain.getSettings`，返回值包含来源模式、处理模式、两组前级增益值和启用状态。要启动分析，可调用 `scan(paths, { mode? })`；默认模式为 `'track'`，`'album'` 会将所选曲目视为同一张专辑。

```javascript
const result = await fb.replaygain.getSettings();
await fb.replaygain.scan(['E:\\Music\\one.flac'], { mode: 'track' });
```

### setMode()

封装 `replaygain.setMode`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.replaygain.setMode(/* 参数见 TypeScript 声明 */);
```

### setPreamp()

封装 `replaygain.setPreamp`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.replaygain.setPreamp(/* 参数见 TypeScript 声明 */);
```

<!-- END AUTO-GENERATED SDK STUBS -->

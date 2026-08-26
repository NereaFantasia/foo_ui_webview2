# fb.dsp dsp

本页是 `fb.dsp` 的 SDK 视角文档入口。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## 其余方法

### addDsp()

封装 `dsp.addDsp`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
const result = await fb.dsp.addDsp('{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}');
```

### applyPreset()

封装 `dsp.applyPreset`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
const result = await fb.dsp.applyPreset(0);           // 按索引
const byName = await fb.dsp.applyPreset('My Preset'); // 按名称
```

### getAvailable()

封装 `dsp.getAvailable`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.dsp.getAvailable(/* 参数见 TypeScript 声明 */);
```

### getChain()

封装 `dsp.getChain`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.dsp.getChain(/* 参数见 TypeScript 声明 */);
```

### getPresets()

封装 `dsp.getPresets`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.dsp.getPresets(/* 参数见 TypeScript 声明 */);
```

### moveDsp()

封装 `dsp.moveDsp`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
const result = await fb.dsp.moveDsp(0, 2);
```

### removeDsp()

封装 `dsp.removeDsp`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
const result = await fb.dsp.removeDsp(2);
```

### setChain()

封装 `dsp.setChain`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
const result = await fb.dsp.setChain([{ guid: '{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}' }]);
```

<!-- END AUTO-GENERATED SDK STUBS -->

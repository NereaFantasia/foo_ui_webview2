# fb.playcount playcount

`fb.playcount` 读取 `foo_playcount` 提供的播放统计，支持单曲、批量读取和媒体库整体汇总。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## 其余方法

### getBatch()

封装 `playcount.getBatch`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.playcount.getBatch(/* 参数见 TypeScript 声明 */);
```

### getStats()

封装 `playcount.getStats`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.playcount.getStats(/* 参数见 TypeScript 声明 */);
```

### set()

封装 `playcount.set`。该方法已弃用：宿主不支持直接修改播放次数，固定返回 `{ success: false }`，且不会应用 `count` 参数。评分应使用 `fb.rating.set()`，播放次数则由实际播放更新。

```javascript
const result = await fb.playcount.set();
```

<!-- END AUTO-GENERATED SDK STUBS -->

## 读取单曲统计

`fb.playcount.get(path: string): Promise<PlaycountInfo | null>` 使用 `paths: [path]` 调用已注册的 `playcount.get` handler，并解包第一项结果。宿主返回空或失败信封时，该方法解析为 `null`。

```javascript
const info = await fb.playcount.get('E:\\Music\\song.flac');
if (info?.success) {
	console.log(info.playCount, info.lastPlayed);
}
```

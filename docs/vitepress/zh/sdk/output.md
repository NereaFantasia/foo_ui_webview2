# fb.output output

本页是 `fb.output` 的 SDK 视角文档入口。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## 其余方法

### getEntries()

封装 `output.getEntries`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.output.getEntries(/* 参数见 TypeScript 声明 */);
```

### getSettings()

封装 `output.getSettings`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.output.getSettings(/* 参数见 TypeScript 声明 */);
```

<!-- END AUTO-GENERATED SDK STUBS -->

## 输出设备列表

`fb.output.getDevices(): Promise<OutputDeviceInfo[]>` 调用 `output.getDevices`，并解包宿主返回的 `{ devices, count }` 信封。每个设备携带 `guid`、`name`、`entry`（所属输出后端显示名）与 `entryGuid`（所属后端 GUID）。宿主报告失败时，封装会抛出带宿主错误文本的 `Error`，而不是返回空数组。

设备 `guid` 在各后端的“默认设备”行为全零（`{00000000-0000-0000-0000-000000000000}`）且可能跨后端重复 —— 请用 `(entryGuid, guid)` 组合作键，不要单独用 `guid`。

```javascript
const devices = await fb.output.getDevices();
// [{ guid, name, entry, entryGuid }, ...]
const byKey = new Map(devices.map((d) => [`${d.entryGuid}|${d.guid}`, d]));
```

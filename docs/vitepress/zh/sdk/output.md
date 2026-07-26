# fb.output output

本页是 `fb.output` 的 SDK 视角文档入口。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## SDK 方法 stub

> 该区块用于补齐 SDK 视角方法覆盖，后续可人工扩展为完整示例与最佳实践。

### getEntries()

签名：`fb.output.getEntries(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `output.getEntries` 调用结果。

```javascript
const result = await fb.output.getEntries();
```

### getSettings()

签名：`fb.output.getSettings(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `output.getSettings` 调用结果。

```javascript
const result = await fb.output.getSettings();
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

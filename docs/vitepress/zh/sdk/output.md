# fb.output 音频输出

`fb.output` 提供音频输出设备、输出模块和输出设置的信息。

## getDevices()

签名：`fb.output.getDevices(): Promise<OutputGetDevicesResponse>`

在 `devices` 中按模块依次列出所有输出模块的设备，另带 `count`。设备的 `guid` 单独不唯一，设备字段和作键方式见[输出设备列表](#输出设备列表)。

```javascript
const res = await fb.output.getDevices();
if (res.success === false) throw new Error(res.error);
console.log(`共 ${res.count} 个输出设备`);
```

## getEntries()

签名：`fb.output.getEntries(): Promise<OutputGetEntriesResponse>`

在 `entries` 中返回可用的输出模块描述，另带 `count`。每个 `OutputEntryInfo` 含 `guid`、`name` 和一组能力标志，如 `needsBitdepthConfig`、`needsDitherConfig`、`supportsMultipleStreams`、`isHighLatency`、`isLowLatency`。可能有多个模块同名，也有模块报告空名字，所以要用 `guid` 区分模块。

```javascript
const res = await fb.output.getEntries();
if (res.success === false) throw new Error(res.error);
const { entries = [] } = res;
```

## getSettings()

签名：`fb.output.getSettings(): Promise<OutputGetSettingsResponse>`

返回宿主的输出设置概要：`availableOutputs` 是各输出模块的显示名，`note` 是一句固定的英文说明，指出输出设置在哪里修改。它不返回当前生效的设备配置。输出设置在 foobar2000 首选项里修改，切换设备用 [`fb.config.setOutputDevice`](./config.md)。

`availableOutputs` 中的名字可能重复或为空，两次调用之间的顺序也不稳定。需要名字和 GUID 的对应时，用 `getEntries()`。

```javascript
const settings = await fb.output.getSettings();
```

## 输出设备列表

`fb.output.getDevices(): Promise<OutputGetDevicesResponse>` 调用 `output.getDevices`，返回 `{ devices, count }`；失败时返回失败信封。每个设备携带 `guid`、`name`、`entry`（所属输出后端显示名）与 `entryGuid`（所属后端 GUID）。

设备 `guid` 在各后端的“默认设备”行为全零（`{00000000-0000-0000-0000-000000000000}`）且可能跨后端重复 —— 请用 `(entryGuid, guid)` 组合作键，不要单独用 `guid`。

```javascript
const res = await fb.output.getDevices();
if (res.success === false) throw new Error(res.error);
// res.devices: [{ guid, name, entry, entryGuid }, ...]
const byKey = new Map(res.devices.map((d) => [`${d.entryGuid}|${d.guid}`, d]));
```

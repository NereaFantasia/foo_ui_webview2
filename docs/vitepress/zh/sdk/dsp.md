# fb.dsp DSP 链

`fb.dsp` 读取和修改当前生效的 DSP 处理链，并应用已保存的 DSP 预设。

## addDsp(guid, position?)

签名：`fb.dsp.addDsp(guid: string, position?: number): Promise<DspAddDspResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| guid | string | 是 | 已安装 DSP 的 GUID，取自 `getAvailable()` |
| position | number | 否 | 插入位置。省略、为负数或不小于链长时追加到末尾 |

用默认设置把 `guid` 指定的 DSP 插入当前链，可以指定插入位置（`-1` 或省略即追加）。响应带 `addedDsp` 和它最终所在的 `position`。

```javascript
await fb.dsp.addDsp('{00000000-0000-0000-0000-000000000000}', 0);
```

## applyPreset(indexOrName)

签名：`fb.dsp.applyPreset(indexOrName: number | string): Promise<DspApplyPresetResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| indexOrName | number \| string | 是 | 来自 `getPresets()` 的预设 `index` 或 `name`。传数字时作为 `index` 发送，传字符串时作为 `name` 发送 |

按索引或显示名应用已保存的预设。预设整条替换当前链，不改写预设文件。响应带 `appliedPreset` 和 `appliedIndex`。

```javascript
await fb.dsp.applyPreset('Headphones');
await fb.dsp.applyPreset(0); // 按索引
```

## getAvailable()

签名：`fb.dsp.getAvailable(): Promise<DspGetAvailableResponse>`

在 `dsps` 中按服务枚举顺序返回可用的 DSP，另带 `count`。每项（`DspAvailableEntry`）有 `guid`、`name`、`hasConfig` 三个字段；DSP 有配置对话框时 `hasConfig` 为 `true`。`addDsp()` 和 `setChain()` 用的就是这里的 `guid`。

```javascript
const res = await fb.dsp.getAvailable();
if (res.success === false) throw new Error(res.error);
const configurable = res.dsps.filter((d) => d.hasConfig);
```

## getChain()

签名：`fb.dsp.getChain(): Promise<DspGetChainResponse>`

按处理顺序返回当前链中的 DSP（`DspChainEntry`：`index`、`guid`、`name`）。成功结果总带 `activePreset`（没有选中预设时为 `null`）和 `activePresetIndex`（没有时为 `-1`）。经 `addDsp()`、`removeDsp()`、`moveDsp()` 或 `setChain()` 改过之后，链不再对应任何预设，这两个字段都回到未选中的值。

```javascript
const res = await fb.dsp.getChain();
if (res.success === false) throw new Error(res.error);
const { dsps, activePreset } = res;
```

## getPresets()

签名：`fb.dsp.getPresets(): Promise<DspGetPresetsResponse>`

返回 `presets`（`DspPresetEntry`：`index`、`name`、`active`）、`count` 和 `selectedIndex`（没有选中预设时为 `-1`）。预设的 `name` 也是它在 foobar2000 配置目录 `dsp-presets` 下的文件名。

```javascript
const res = await fb.dsp.getPresets();
if (res.success === false) throw new Error(res.error);
const names = res.presets.map((p) => p.name);
```

## moveDsp(from, to)

签名：`fb.dsp.moveDsp(from: number, to: number): Promise<DspMoveDspResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| from | number | 是 | 该项现在的索引 |
| to | number | 是 | 重排后该项所在的索引 |

把当前链中的一项从一个索引移到另一个索引，保留它的设置；重排链请用它，不要用 `setChain()`。响应带 `from` 和 `to`（`to` 是最终所在的索引），并在 `movedDsp` 中给出移动的 DSP 名。`from === to` 时什么都不动：没有 `movedDsp`，改带 `message`。

```javascript
await fb.dsp.moveDsp(2, 0);
```

## removeDsp(index)

签名：`fb.dsp.removeDsp(index: number): Promise<DspRemoveDspResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| index | number | 是 | 该项在当前链中的索引，取自 `getChain()` |

移除当前链中指定索引的 DSP。响应带 `removedDsp` 和 `removedIndex`。

```javascript
const res = await fb.dsp.removeDsp(2);
if (res.success === false) throw new Error(res.error);
console.log(`已移除 ${res.removedDsp}`);
```

## setChain(dsps)

签名：`fb.dsp.setChain(dsps: ReadonlyArray<Pick<DspChainSpec, 'guid'>>): Promise<DspSetChainResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| dsps | `{ guid: string }[]` | 是 | 新链，按处理顺序；空数组即清空链 |

用给定的条目整条替换当前链，每项只带 `guid`。SDK 发送前会去掉其他字段，所以 `getChain()` 和 `getAvailable()` 返回的条目可以原样传入。响应给出新的 `count`。任一项解析不到已安装的 DSP，整次调用失败，链保持不变。

每一项都按其 DSP 的默认设置重建。想重排又不丢设置，用 `moveDsp()`。

```javascript
const available = await fb.dsp.getAvailable();
if (available.success === false) throw new Error(available.error);
const eq = available.dsps.find((d) => d.name === 'Equalizer');
if (eq) await fb.dsp.setChain([{ guid: eq.guid }]);
```

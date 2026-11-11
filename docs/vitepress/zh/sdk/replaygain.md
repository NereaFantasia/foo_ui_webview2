# fb.replaygain ReplayGain

`fb.replaygain` 读取 ReplayGain 元数据，控制播放时的 ReplayGain 处理设置，并在宿主端发起扫描或清除。

## clear(paths)

签名：`fb.replaygain.clear(paths: string[]): Promise<ReplaygainClearResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| paths | string[] | 是 | 要清除 ReplayGain 元数据的文件路径，至少一条。带 `\|subsong:N` 后缀时选子曲目，如 CUE 里的一首 |

从音频文件中移除 ReplayGain 元数据。响应包含 `clearedCount`，指示提交给标签写入器的文件数量。由于文件标签在后台异步重写，可通过 `get()` 再次查询以确认标签已完全清除。

```javascript
const result = await fb.replaygain.clear(['E:\\Music\\song.flac']);
```

## get(paths)

签名：`fb.replaygain.get(paths: string | string[]): Promise<ReplaygainGetResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| paths | string \| string[] | 是 | 单个文件路径或路径数组；SDK 总是以 `{ paths: string[] }` 发送 |

响应包含 `count` 与 `results` 数组，按请求顺序对应每个文件路径。每项结果包含原请求 `path` 与 `success` 标志；若单个文件无法读取，该行将返回带 `error` 的局部失败项，而不导致整个调用失败。

读取成功的曲目包含 `hasReplayGain` 标识及已存储的增益数据：格式化文本 `trackGain` 与 `albumGain`（如 `-7.25 dB`），格式化峰值 `trackPeak` 与 `albumPeak`（保留六位小数）；对应的数值分别通过 `trackGainRaw`、`albumGainRaw`（单位 dB）与 `trackPeakRaw`、`albumPeakRaw`（线性幅度）提供。路径包含 `|subsong:N` 后缀时定位指定子曲目；未包含后缀时默认读取首个子曲目。

```javascript
const result = await fb.replaygain.get('E:\\Music\\song.flac');

const res = await fb.replaygain.get(['E:\\Music\\one.flac', 'E:\\Music\\two.flac']);
if (res.success === false) throw new Error(res.error);
for (const row of res.results) {
  if (row.success) console.log(row.path, row.trackGain ?? '无音轨增益');
}
```

## getMode()

签名：`fb.replaygain.getMode(): Promise<ReplaygainGetModeResponse>`

返回 `sourceMode` 和 `processingMode`。`sourceMode` 指示播放时应用的增益来源：`none` 不应用，`track`、`album` 采用对应的存储值，`auto` 跟随播放顺序（随机模式采用音轨增益，否则采用专辑增益）。`processingMode` 指示处理模式：`gain` 仅应用增益调整，`peak` 仅执行峰值限制，`gain_and_peak` 同时应用增益与峰值限制，`none` 禁用所有处理。

```javascript
const result = await fb.replaygain.getMode();
```

## getPreamp()

签名：`fb.replaygain.getPreamp(): Promise<ReplaygainGetPreampResponse>`

返回两个预增益，单位 dB：`withRg` 用于带 ReplayGain 的曲目，`withoutRg` 用于不带 ReplayGain 的曲目。

```javascript
const result = await fb.replaygain.getPreamp();
```

## getSettings()

签名：`fb.replaygain.getSettings(): Promise<ReplaygainGetSettingsResponse>`

一次返回音源模式、处理模式、两个预增益和启用状态：`sourceMode`、`processingMode`、`preampWithRg`、`preampWithoutRg`（dB）和 `active`。音源模式为 `none` 或 `auto`、或处理模式为 `none` 时，`active` 为 `false`。要启动分析，调用 `scan(paths, { mode? })`。

```javascript
const result = await fb.replaygain.getSettings();
```

## scan(paths, opts?)

签名：`fb.replaygain.scan(paths: string[], opts?: { mode?: 'track' | 'album' }): Promise<ReplaygainScanResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| paths | string[] | 是 | 要扫描的文件；空列表返回 `INVALID_PARAMS`。带 `\|subsong:N` 后缀时选子曲目，如 CUE 里的一首 |
| opts | `{ mode?: 'track' \| 'album' }` | 否 | `mode` 默认为 `'track'`，逐文件扫描音轨增益；`'album'` 把所选文件当作一张专辑扫描 |

通过原生菜单命令调用 foobar2000 内置的 ReplayGain 扫描器。扫描任务发起后即返回响应回执，计算结果随后在后台写入文件。响应包含 `scannedCount`、生效的 `mode` 以及指示任务后台执行的提示文本 `note`。

```javascript
await fb.replaygain.scan(['E:\\Music\\one.flac'], { mode: 'track' });

const res = await fb.replaygain.scan(['E:\\Music\\one.flac', 'E:\\Music\\two.flac'], { mode: 'album' });
if (res.success === false) throw new Error(res.error);
console.log(`正在以 ${res.mode} 模式扫描 ${res.scannedCount} 个文件`);
```

## setMode(sourceMode, processingMode?)

签名：`fb.replaygain.setMode(sourceMode: NonNullable<ReplaygainSetModeParams['sourceMode']>, processingMode?: ReplaygainSetModeParams['processingMode']): Promise<ReplaygainSetModeResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| sourceMode | string | 是 | 音源模式：`none`、`track`、`album` 或 `auto`；`byPlaybackOrder` 是 `auto` 的旧写法，同样接受 |
| processingMode | string | 否 | 处理模式：`none`、`gain`、`gain_and_peak` 或 `peak`；省略时保持原值 |

修改音源模式；若指定了 `processingMode`，则同时更新处理模式。返回生效的 `sourceMode`、`processingMode` 以及指示配置是否发生变更的布尔值 `changed`。

```javascript
const result = await fb.replaygain.setMode('track');
```

## setPreamp(withRg?, withoutRg?)

签名：`fb.replaygain.setPreamp(withRg?: number, withoutRg?: number): Promise<ReplaygainSetPreampResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| withRg | number | 否 | 带 ReplayGain 的曲目的预增益，单位 dB，范围 -24 到 24 |
| withoutRg | number | 否 | 不带 ReplayGain 的曲目的预增益，单位 dB，范围 -24 到 24 |

设置 ReplayGain 预增益；未指定的参数保持原值。返回生效的 `withRg`、`withoutRg` 以及指示配置是否发生变更的布尔值 `changed`。

```javascript
const result = await fb.replaygain.setPreamp(6, 0);
```

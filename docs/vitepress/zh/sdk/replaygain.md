# fb.replaygain ReplayGain

`fb.replaygain` 读取 ReplayGain 元数据，控制播放时的 ReplayGain 处理设置，并在宿主端发起扫描或清除。

## clear(paths)

签名：`fb.replaygain.clear(paths: string[]): Promise<ReplaygainClearResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| paths | string[] | 是 | 要清除 ReplayGain 元数据的文件路径，至少一条。带 `\|subsong:N` 后缀时选子曲目，如 CUE 里的一首 |

从文件中移除 ReplayGain 值。响应带 `clearedCount`，即交给标签写入器的文件数。标签在后台重写，要再用 `get()` 读一次文件才能看到值消失。

```javascript
const result = await fb.replaygain.clear(['E:\\Music\\song.flac']);
```

## get(paths)

签名：`fb.replaygain.get(paths: string | string[]): Promise<ReplaygainGetResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| paths | string \| string[] | 是 | 单个文件路径或路径数组；SDK 总是以 `{ paths: string[] }` 发送 |

逐曲目返回结果，音轨和专辑的增益、峰值字段按文件实际存储的情况出现。响应带 `count` 和 `results`，每个请求路径一行，按请求顺序。每行（`ReplayGainTrackInfo`）原样带回请求的 `path`，并有 `success`；读不了的文件是带 `error` 的失败行，不是失败的调用。

读得出的行带 `hasReplayGain`，以及文件存有的那些值：`trackGain`、`albumGain` 是形如 `-7.25 dB` 的文本，`trackPeak`、`albumPeak` 是六位小数的文本；同样的值以数字形式放在 `trackGainRaw`、`albumGainRaw`（dB）和 `trackPeakRaw`、`albumPeakRaw`（线性幅度）中。路径带 `|subsong:N` 后缀时选子曲目，如 CUE 里的一首；不带后缀读第一首。

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

返回 `sourceMode` 和 `processingMode`。`sourceMode` 是播放时应用的增益来源：`none` 不应用，`track`、`album` 用对应的存储值，`auto` 跟随播放顺序（随机时用音轨增益，否则用专辑增益）。`processingMode` 是应用什么：`gain` 只应用增益，`peak` 只做峰值限制，`gain_and_peak` 两者都做，`none` 什么都不做。

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

通过右键菜单命令对文件启动 foobar2000 自带的 ReplayGain 扫描。扫描发起后 Promise 即 resolve，结果随后由扫描器写入文件。响应带 `scannedCount`、扫描使用的 `mode`，以及 `note`：一句固定的英文说明，指出扫描在后台进行。

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

修改音源模式，给了 `processingMode` 时一并修改处理模式。响应报告修改后生效的 `sourceMode` 和 `processingMode`，以及表示这次调用是否改了什么的 `changed`。

```javascript
const result = await fb.replaygain.setMode('track');
```

## setPreamp(withRg?, withoutRg?)

签名：`fb.replaygain.setPreamp(withRg?: number, withoutRg?: number): Promise<ReplaygainSetPreampResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| withRg | number | 否 | 带 ReplayGain 的曲目的预增益，单位 dB，范围 -24 到 24 |
| withoutRg | number | 否 | 不带 ReplayGain 的曲目的预增益，单位 dB，范围 -24 到 24 |

只发送给了值的参数，没给的预增益保持原值。响应报告修改后生效的 `withRg` 和 `withoutRg`，以及表示这次调用是否改了什么的 `changed`。

```javascript
const result = await fb.replaygain.setPreamp(6, 0);
```

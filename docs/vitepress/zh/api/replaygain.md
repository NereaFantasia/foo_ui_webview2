# ReplayGain 回放增益 API

`replaygain` 命名空间的方法：ReplayGain 设置、读取、清除与扫描。

## ReplayGain API

ReplayGain 音量标准化设置。

### replaygain.getSettings

<!-- api-schema:begin replaygain.getSettings -->
读取完整的 ReplayGain 配置：音源模式、处理模式、两个预增益，以及 ReplayGain 是否生效。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `sourceMode` | `"none" \| "track" \| "album" \| "auto"` | 播放时应用的增益来源：`none` 不应用，`track`、`album` 用对应的存储值，`auto` 跟随播放顺序（随机时用音轨增益，否则用专辑增益）。 |
| `processingMode` | `"none" \| "gain" \| "gain_and_peak" \| "peak"` | 应用什么：`gain` 只应用增益，`peak` 只做峰值限制，`gain_and_peak` 两者都做，`none` 什么都不做。 |
| `preampWithRg` | `number` | 对带 ReplayGain 的曲目应用的预增益，单位 dB。 |
| `preampWithoutRg` | `number` | 对不带 ReplayGain 的曲目应用的预增益，单位 dB。 |
| `active` | `boolean` | ReplayGain 是否在改变输出：音源模式为 `none` 或 `auto`、或处理模式为 `none` 时为 `false`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### replaygain.getMode

<!-- api-schema:begin replaygain.getMode -->
读取音源模式与处理模式。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `sourceMode` | `"none" \| "track" \| "album" \| "auto"` | 播放时应用的增益来源：`none` 不应用，`track`、`album` 用对应的存储值，`auto` 跟随播放顺序（随机时用音轨增益，否则用专辑增益）。 |
| `processingMode` | `"none" \| "gain" \| "gain_and_peak" \| "peak"` | 应用什么：`gain` 只应用增益，`peak` 只做峰值限制，`gain_and_peak` 两者都做，`none` 什么都不做。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### replaygain.setMode

<!-- api-schema:begin replaygain.setMode -->
修改音源模式、处理模式或两者；没给的键保持原值。响应报告修改后生效的模式。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `sourceMode` | `"none" \| "track" \| "album" \| "auto" \| "byPlaybackOrder"` | 否 | 要设置的音源模式；`byPlaybackOrder` 是 `auto` 的旧写法。 |
| `processingMode` | `"none" \| "gain" \| "gain_and_peak" \| "peak"` | 否 | 应用什么：`gain` 只应用增益，`peak` 只做峰值限制，`gain_and_peak` 两者都做，`none` 什么都不做。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `sourceMode` | `"none" \| "track" \| "album" \| "auto"` | 播放时应用的增益来源：`none` 不应用，`track`、`album` 用对应的存储值，`auto` 跟随播放顺序（随机时用音轨增益，否则用专辑增益）。 |
| `processingMode` | `"none" \| "gain" \| "gain_and_peak" \| "peak"` | 应用什么：`gain` 只应用增益，`peak` 只做峰值限制，`gain_and_peak` 两者都做，`none` 什么都不做。 |
| `changed` | `boolean` | 这次调用是否改了什么。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('replaygain.setMode', { sourceMode: 'album', processingMode: 'gain' });
```

### replaygain.getPreamp

<!-- api-schema:begin replaygain.getPreamp -->
读取两个预增益，单位 dB。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `withRg` | `number` | 对带 ReplayGain 的曲目应用的预增益，单位 dB。 |
| `withoutRg` | `number` | 对不带 ReplayGain 的曲目应用的预增益，单位 dB。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### replaygain.setPreamp

<!-- api-schema:begin replaygain.setPreamp -->
修改一个或两个预增益；没给的键保持原值。响应报告修改后生效的值。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `withRg` | `number` | 否 | 带 ReplayGain 的曲目的预增益，单位 dB。取值 `-24` 到 `24`（含端点）。 |
| `withoutRg` | `number` | 否 | 不带 ReplayGain 的曲目的预增益，单位 dB。取值 `-24` 到 `24`（含端点）。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `withRg` | `number` | 对带 ReplayGain 的曲目应用的预增益，单位 dB。 |
| `withoutRg` | `number` | 对不带 ReplayGain 的曲目应用的预增益，单位 dB。 |
| `changed` | `boolean` | 这次调用是否改了什么。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('replaygain.setPreamp', { withRg: 3.0, withoutRg: -3.0 });
```

### replaygain.get

<!-- api-schema:begin replaygain.get -->
读取文件里存储的 ReplayGain 值，每个路径一行。读不了的文件是失败的行，不是失败的调用。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 要读取的曲目。带 `\|subsong:N` 后缀时选子曲目，如 CUE 里的一首；不带后缀读第一首。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | `results` 的行数。 |
| `results` | `ReplayGainTrackInfo[]` | 每个请求路径一行，按请求顺序。 |
| `results[].path` | `string` | 请求时给的路径，后缀 `\|subsong:N` 原样保留。 |
| `results[].success` | `boolean` | 文件是否读得出来。 |
| `results[].error` | `string` | 读不出来的原因；`success` 为 `false` 时出现。 |
| `results[].hasReplayGain` | `boolean` | 文件是否存有音轨增益或专辑增益。`success` 为 `true` 时出现。 |
| `results[].trackGain` | `string` | 音轨增益，两位小数加 `dB` 后缀，如 `-7.25 dB`。 |
| `results[].trackGainRaw` | `number` | 音轨增益，单位 dB。 |
| `results[].trackPeak` | `string` | 音轨峰值，六位小数。 |
| `results[].trackPeakRaw` | `number` | 音轨峰值，线性幅度。 |
| `results[].albumGain` | `string` | 专辑增益，两位小数加 `dB` 后缀。 |
| `results[].albumGainRaw` | `number` | 专辑增益，单位 dB。 |
| `results[].albumPeak` | `string` | 专辑峰值，六位小数。 |
| `results[].albumPeakRaw` | `number` | 专辑峰值，线性幅度。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```json
{
    "success": true,
    "count": 1,
    "results": [
        {
            "path": "C:\\Music\\song.flac",
            "success": true,
            "trackGain": "-5.20 dB",
            "trackGainRaw": -5.2,
            "trackPeak": "0.987654",
            "trackPeakRaw": 0.987654,
            "albumGain": "-4.80 dB",
            "albumGainRaw": -4.8,
            "albumPeak": "1.000000",
            "albumPeakRaw": 1.0,
            "hasReplayGain": true
        }
    ]
}
```

### replaygain.clear

<!-- api-schema:begin replaygain.clear -->
从文件中移除 ReplayGain 值。标签在后台重写，再读一次文件才能看到值消失。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 要移除 ReplayGain 值的曲目。带 `\|subsong:N` 后缀时选子曲目，如 CUE 里的一首；不带后缀指第一首。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `clearedCount` | `integer` | 交给标签写入器的文件数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### replaygain.scan

<!-- api-schema:begin replaygain.scan -->
通过右键菜单命令对文件启动 foobar2000 自带的 ReplayGain 扫描。扫描发起后即返回，结果由扫描器写入文件。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 要扫描的曲目。带 `\|subsong:N` 后缀时选子曲目，如 CUE 里的一首；不带后缀指第一首。不能为空。 |
| `mode` | `"track" \| "album"` | 否 | `track` 逐文件扫描音轨增益；`album` 把这些文件当一张专辑扫描。默认 `"track"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `scannedCount` | `integer` | 交给扫描器的文件数。 |
| `mode` | `"track" \| "album"` | 扫描使用的模式。 |
| `note` | `string` | 一句固定的英文说明，指出扫描在后台进行。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
// 扫描单曲 track gain
await fb2k.invoke('replaygain.scan', {
    paths: ['C:\\Music\\song.flac'],
    mode: 'track'
});
// 扫描整张专辑
await fb2k.invoke('replaygain.scan', {
    paths: ['C:\\Music\\01.flac', 'C:\\Music\\02.flac'],
    mode: 'album'
});
```

## 运行时行为说明

- `replaygain.get` 读取每个传入媒体路径；`replaygain.clear` 通过 foobar2000 异步写入 ReplayGain 元数据。`replaygain.scan` 只是请求宿主扫描器开工，不同步返回分析结果。

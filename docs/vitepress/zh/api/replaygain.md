# ReplayGain 回放增益 API

提供 ReplayGain 回放增益模式配置、预增益调节、标签元数据读写与后台扫描能力。

## ReplayGain API

ReplayGain 音量标准化设置。

### replaygain.getSettings

<!-- api-schema:begin replaygain.getSettings -->
获取完整 ReplayGain 配置（包含音源模式、处理模式、两项预增益 dB 值及当前是否实际生效）

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `sourceMode` | `"none" \| "track" \| "album" \| "auto"` | 播放时应用的增益音源模式（none 为不应用；track 与 album 使用对应存储的增益值；auto 跟随播放顺序，随机模式使用音轨增益，其余顺序使用专辑增益） |
| `processingMode` | `"none" \| "gain" \| "gain_and_peak" \| "peak"` | ReplayGain 处理模式（gain 仅应用增益调节；peak 仅执行峰值防削波限制；gain_and_peak 两者兼具；none 不进行任何处理） |
| `preampWithRg` | `number` | 对带有 ReplayGain 标签的曲目应用的预增益数值（单位 dB） |
| `preampWithoutRg` | `number` | 对缺少 ReplayGain 标签的曲目应用的预增益数值（单位 dB） |
| `active` | `boolean` | ReplayGain 是否正在实际调节音频输出（音源模式为 none、auto 或处理模式为 none 时为 false） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### replaygain.getMode

<!-- api-schema:begin replaygain.getMode -->
获取当前配置的 ReplayGain 音源模式与处理模式

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `sourceMode` | `"none" \| "track" \| "album" \| "auto"` | 播放时应用的增益音源模式（none 为不应用；track 与 album 使用对应存储的增益值；auto 跟随播放顺序，随机模式使用音轨增益，其余顺序使用专辑增益） |
| `processingMode` | `"none" \| "gain" \| "gain_and_peak" \| "peak"` | ReplayGain 处理模式（gain 仅应用增益调节；peak 仅执行峰值防削波限制；gain_and_peak 两者兼具；none 不进行任何处理） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### replaygain.setMode

<!-- api-schema:begin replaygain.setMode -->
修改 ReplayGain 音源模式或处理模式（缺省参数保持原值；调用返回修改后实际生效的模式）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `sourceMode` | `"none" \| "track" \| "album" \| "auto" \| "byPlaybackOrder"` | 否 | 待设置的音源模式输入值（byPlaybackOrder 为 auto 的向后兼容旧别名） |
| `processingMode` | `"none" \| "gain" \| "gain_and_peak" \| "peak"` | 否 | ReplayGain 处理模式（gain 仅应用增益调节；peak 仅执行峰值防削波限制；gain_and_peak 两者兼具；none 不进行任何处理） |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `sourceMode` | `"none" \| "track" \| "album" \| "auto"` | 播放时应用的增益音源模式（none 为不应用；track 与 album 使用对应存储的增益值；auto 跟随播放顺序，随机模式使用音轨增益，其余顺序使用专辑增益） |
| `processingMode` | `"none" \| "gain" \| "gain_and_peak" \| "peak"` | ReplayGain 处理模式（gain 仅应用增益调节；peak 仅执行峰值防削波限制；gain_and_peak 两者兼具；none 不进行任何处理） |
| `changed` | `boolean` | 本次设置调用是否实际修改了模式配置 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('replaygain.setMode', { sourceMode: 'album', processingMode: 'gain' });
```

### replaygain.getPreamp

<!-- api-schema:begin replaygain.getPreamp -->
获取带 ReplayGain 与不带 ReplayGain 曲目的预增益数值（单位 dB）

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `withRg` | `number` | 对带有 ReplayGain 标签的曲目应用的预增益数值（单位 dB） |
| `withoutRg` | `number` | 对缺少 ReplayGain 标签的曲目应用的预增益数值（单位 dB） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### replaygain.setPreamp

<!-- api-schema:begin replaygain.setPreamp -->
设置带 ReplayGain 与不带 ReplayGain 曲目的预增益数值（缺省参数保持原值；取值范围 -24 到 24 dB；调用返回修改后实际生效的值）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `withRg` | `number` | 否 | 带 ReplayGain 标签曲目的预增益目标值（单位 dB）取值 `-24` 到 `24`（含端点）。 |
| `withoutRg` | `number` | 否 | 不带 ReplayGain 标签曲目的预增益目标值（单位 dB）取值 `-24` 到 `24`（含端点）。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `withRg` | `number` | 修改后对带 ReplayGain 标签曲目生效的预增益数值（单位 dB） |
| `withoutRg` | `number` | 修改后对不带 ReplayGain 标签曲目生效的预增益数值（单位 dB） |
| `changed` | `boolean` | 本次设置调用是否实际修改了预增益数值 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('replaygain.setPreamp', { withRg: 3.0, withoutRg: -3.0 });
```

### replaygain.get

<!-- api-schema:begin replaygain.get -->
批量读取音频文件标签中存储的 ReplayGain 增益与峰值数据（单文件读取失败仅标记对应结果项失败，不导致整批调用报错）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 待读取的曲目路径列表（支持 \|subsong:N 后缀指定单曲音轨，如 CUE 分轨；缺省后缀时默认读取首个音轨） |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | results 数组包含的结果记录总数 |
| `results` | `ReplayGainTrackInfo[]` | 逐文件查询结果列表（严格保持请求传入顺序） |
| `results[].path` | `string` | 原始请求传入的曲目路径（保留 \|subsong:N 后缀） |
| `results[].success` | `boolean` | 该曲目元数据是否成功读取 |
| `results[].error` | `string` | 元数据读取失败的具体原因描述（仅在 success 为 false 时提供） |
| `results[].hasReplayGain` | `boolean` | 文件中是否记录了音轨增益或专辑增益数据（仅在 success 为 true 时提供） |
| `results[].trackGain` | `string` | 格式化音轨增益字符串（保留两位小数并附带 dB 后缀，例如 -7.25 dB） |
| `results[].trackGainRaw` | `number` | 原始数值形式的音轨增益（单位 dB） |
| `results[].trackPeak` | `string` | 格式化音轨峰值字符串（保留六位小数） |
| `results[].trackPeakRaw` | `number` | 原始线性幅度形式的音轨峰值 |
| `results[].albumGain` | `string` | 格式化专辑增益字符串（保留两位小数并附带 dB 后缀） |
| `results[].albumGainRaw` | `number` | 原始数值形式的专辑增益（单位 dB） |
| `results[].albumPeak` | `string` | 格式化专辑峰值字符串（保留六位小数） |
| `results[].albumPeakRaw` | `number` | 原始线性幅度形式的专辑峰值 |

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
批量从指定音频文件标签中清除 ReplayGain 增益信息（标签在后台异步重写）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 待清除 ReplayGain 信息的曲目路径列表（支持 \|subsong:N 后缀指定子曲目；缺省后缀时指首个音轨）不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `clearedCount` | `integer` | 成功提交至标签写入器的文件总数 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### replaygain.scan

<!-- api-schema:begin replaygain.scan -->
调用原生上下文菜单命令启动 foobar2000 原生 ReplayGain 扫描器（任务启动后即刻返回，扫描结果由原生组件异步回写文件）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 待扫描 ReplayGain 的曲目路径列表（支持 \|subsong:N 后缀指定子曲目；缺省后缀时指首个音轨）不能为空。 |
| `mode` | `"track" \| "album"` | 否 | 扫描模式（track 为逐文件扫描独立音轨增益；album 为将传入文件整体作为单张专辑扫描）默认 `"track"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `scannedCount` | `integer` | 成功提交至扫描队列的文件总数 |
| `mode` | `"track" \| "album"` | 实际执行扫描的模式（track 或 album） |
| `note` | `string` | 固定的英文提示文本（指出扫描任务正在后台异步运行） |

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

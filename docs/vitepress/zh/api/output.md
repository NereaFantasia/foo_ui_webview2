# Output 输出设备 API

提供音频输出设备与输出模块能力查询接口。

## Output API - 音频输出

### output.getDevices

<!-- api-schema:begin output.getDevices -->
获取全部输出模块下的音频设备列表

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `devices` | `OutputDevice[]` | 全部输出模块下的设备列表（按模块归属依次排列） |
| `devices[].guid` | `string` | 输出设备唯一标识符 GUID（格式为 {...}；由于默认设备为全零 GUID，需结合 (entryGuid, guid) 作为唯一复合键） |
| `devices[].name` | `string` | 音频输出设备显示名称 |
| `devices[].entry` | `string` | 提供该设备的输出模块显示名称 |
| `devices[].entryGuid` | `string` | 提供该设备的输出模块 GUID（格式为 {...}） |
| `count` | `integer` | devices 数组包含的设备总数 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> **`guid` 在本端点内不唯一。** foobar2000 用全零 GUID
> `{00000000-0000-0000-0000-000000000000}` 表示某个输出模块的「默认设备」，因此多个模块下会各出现一次全零 GUID。
> 请用 `(entryGuid, guid)` 组合作为设备的唯一键，不要只用 `guid`。

### output.getEntries

<!-- api-schema:begin output.getEntries -->
获取全部输出模块（output entry）及其能力标志列表

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `entries` | `OutputEntry[]` | 全部已安装的输出模块列表 |
| `entries[].guid` | `string` | 输出模块唯一标识符 GUID（格式为 {...}） |
| `entries[].name` | `string` | 输出模块显示名称（可能存在同名或空名称） |
| `entries[].needsBitdepthConfig` | `boolean` | 该模块是否需要显式配置输出位深 |
| `entries[].needsDitherConfig` | `boolean` | 该模块是否需要配置抖动处理（Dithering） |
| `entries[].supportsMultipleStreams` | `boolean` | 该模块是否支持同时播放多路音频流 |
| `entries[].isHighLatency` | `boolean` | 该模块是否声明为高延迟输出 |
| `entries[].isLowLatency` | `boolean` | 该模块是否声明为低延迟输出 |
| `count` | `integer` | entries 数组包含的模块总数 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### output.getSettings

<!-- api-schema:begin output.getSettings -->
获取各输出模块的显示名称（仅供参考展示；输出配置须在 foobar2000 首选项调整，切换设备请调用 config.setOutputDevice）

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `note` | `string` | 固定的英文提示文本（指出首选项中修改输出设置的路径） |
| `availableOutputs` | `string[]` | 各输出模块显示名称列表（名称可能重复或为空且调用间顺序不固定；建议优先使用 output.getEntries 配合 GUID 唯一标识） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 实际输出设置通过 foobar2000 首选项管理。如需切换输出设备，请使用 `config.setOutputDevice`。

> **不建议在新代码中使用 `availableOutputs`。** 它只是一个显示名数组，存在两个已实测的问题：
> 同名模块无法区分（多个后端都叫「默认」），以及某些模块的名称为空字符串。
> 此外该数组的顺序来自服务枚举，**多次调用之间并不稳定**，因此不能依赖数组下标定位模块。
> 需要可编程地识别输出模块时请改用 `output.getEntries`，它为每个名称附带 GUID。

## 运行时行为说明

- `output.getSettings` 仅提供只读发现信息。输出配置由 foobar2000 Preferences 管理，而非本 API。

# Output 输出设备 API

`output` 命名空间的方法：列出输出模块与设备。

## Output API - 音频输出

### output.getDevices

<!-- api-schema:begin output.getDevices -->
列出所有输出模块的设备。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `devices` | `OutputDevice[]` | 所有输出模块的设备，按模块依次排列。 |
| `devices[].guid` | `string` | 设备 GUID，形如 `{...}`。单独不唯一：模块用全零 GUID 表示其默认设备，要用 `(entryGuid, guid)` 作键。 |
| `devices[].name` | `string` | 设备的显示名。 |
| `devices[].entry` | `string` | 提供该设备的输出模块的显示名。 |
| `devices[].entryGuid` | `string` | 该输出模块的 GUID，形如 `{...}`。 |
| `count` | `integer` | `devices` 的条目数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> **`guid` 在本端点内不唯一。** foobar2000 用全零 GUID
> `{00000000-0000-0000-0000-000000000000}` 表示某个输出模块的「默认设备」，因此多个模块下会各出现一次全零 GUID。
> 请用 `(entryGuid, guid)` 组合作为设备的唯一键，不要只用 `guid`。

### output.getEntries

<!-- api-schema:begin output.getEntries -->
列出输出模块（foobar2000 的 output entry）及其能力标志。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `entries` | `OutputEntry[]` | 所有输出模块。 |
| `entries[].guid` | `string` | 模块 GUID，形如 `{...}`。 |
| `entries[].name` | `string` | 模块的显示名。可能有多个模块同名，也有模块报告空名字。 |
| `entries[].needsBitdepthConfig` | `boolean` | 该模块需要配置输出位深。 |
| `entries[].needsDitherConfig` | `boolean` | 该模块需要配置抖动。 |
| `entries[].supportsMultipleStreams` | `boolean` | 该模块能同时播放多路流。 |
| `entries[].isHighLatency` | `boolean` | 该模块声明自己是高延迟的。 |
| `entries[].isLowLatency` | `boolean` | 该模块声明自己是低延迟的。 |
| `count` | `integer` | `entries` 的条目数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### output.getSettings

<!-- api-schema:begin output.getSettings -->
报告各输出模块的显示名。仅供参考：输出设置在 foobar2000 首选项里修改，切换设备用 `config.setOutputDevice`。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `note` | `string` | 一句固定的英文说明，指出输出设置在哪里修改。 |
| `availableOutputs` | `string[]` | 各输出模块的显示名。名字可能重复或为空，顺序在两次调用之间也不稳定；新代码请用 `output.getEntries`，它给出名字与 GUID 的对应。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 实际输出设置通过 foobar2000 首选项管理。如需切换输出设备，请使用 `config.setOutputDevice`。

> **不建议在新代码中使用 `availableOutputs`。** 它只是一个显示名数组，存在两个已实测的问题：
> 同名模块无法区分（多个后端都叫「默认」），以及某些模块的名称为空字符串。
> 此外该数组的顺序来自服务枚举，**多次调用之间并不稳定**，因此不能依赖数组下标定位模块。
> 需要可编程地识别输出模块时请改用 `output.getEntries`，它为每个名称附带 GUID。

## 运行时行为说明

- `output.getSettings` 仅提供只读发现信息。输出配置由 foobar2000 Preferences 管理，而非本 API。

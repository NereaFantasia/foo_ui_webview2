# DSP 音效处理 API

`dsp` 命名空间的方法：DSP 效果器链与预设管理。

## DSP API - 效果器管理

> 注意: `dsp.getActivePreset` / `dsp.setActivePreset` 未在 C++ 层注册，请改用 `config.getActiveDspPreset` / `config.setActiveDspPreset`。

### dsp.getChain

<!-- api-schema:begin dsp.getChain -->
读取当前生效的 DSP 链，以及它当前对应哪个预设（如果有）。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `dsps` | `DspChainEntry[]` | 按处理顺序排列的链。 |
| `dsps[].index` | `integer` | 在链中的位置。 |
| `dsps[].guid` | `string` | 处理器的 GUID，形如 `{...}`。 |
| `dsps[].name` | `string` | 处理器的显示名。 |
| `activePreset` | `string \| null` | 当前选中预设的名称；链不对应任何预设时为 `null`，经 `addDsp`、`removeDsp`、`moveDsp` 或 `setChain` 改过之后就是这样。 |
| `activePresetIndex` | `integer` | 选中预设在 `dsp.getPresets` 中的索引；没有选中时为 `-1`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 手工改链的操作（`addDsp` / `removeDsp` / `moveDsp` / `setChain`）会使活动链脱离预设，此后 `activePreset` 与 `activePresetIndex` 即为 `null` / `-1`。

### dsp.getPresets

<!-- api-schema:begin dsp.getPresets -->
列出配置目录里保存的 DSP 预设，以及当前选中的是哪个。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `presets` | `DspPresetEntry[]` | 全部保存的预设，按列表顺序。 |
| `presets[].index` | `integer` | 在预设列表中的索引，`applyPreset` 用它。 |
| `presets[].name` | `string` | 预设名，也是配置目录 `dsp-presets` 下的文件名。 |
| `presets[].active` | `boolean` | 是否为当前选中的预设。 |
| `count` | `integer` | `presets` 的条目数。 |
| `selectedIndex` | `integer` | 选中预设的索引；没有选中时为 `-1`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 预设本身持久化在 profile 的 `dsp-presets\<名称>.fb2k-dsp`。

### dsp.applyPreset

<!-- api-schema:begin dsp.applyPreset -->
按索引或名称把一个预设设为当前链；两者都给时以 `index` 为准。整条替换当前链，不改写预设文件。名称须完全一致（区分大小写），同名时取第一个。两者都没给以 `INVALID_PARAMS` 失败，索引超出最后一个预设以 `INVALID_INDEX` 失败，没有预设叫这个名称以 `NOT_FOUND` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `index` | `integer` | 否 | 来自 `dsp.getPresets` 的预设索引。不小于 `0`。 |
| `name` | `string` | 否 | 来自 `dsp.getPresets` 的预设名。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `appliedPreset` | `string` | 现在生效的预设名。 |
| `appliedIndex` | `integer` | 现在生效的预设索引。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> `name` 和 `index` 至少提供一个，两者都没有以 `INVALID_PARAMS` 失败；同时提供时 `index` 优先。

```javascript
// 按名称
await fb2k.invoke('dsp.applyPreset', { name: 'My Preset' });
// 按索引
await fb2k.invoke('dsp.applyPreset', { index: 0 });
```

> 应用预设会整条替换当前活动链（含各 DSP 的参数），因此它也是把链恢复到某个已知状态的最可靠方式。预设文件本身不会被改写——本接口只写活动链。

### dsp.getAvailable

<!-- api-schema:begin dsp.getAvailable -->
列出已安装的全部 DSP 处理器。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `dsps` | `DspAvailableEntry[]` | 已安装的全部处理器，按服务枚举顺序。 |
| `dsps[].guid` | `string` | 处理器的 GUID，形如 `{...}`；`addDsp` 与 `setChain` 用它。 |
| `dsps[].name` | `string` | 处理器的显示名。 |
| `dsps[].hasConfig` | `boolean` | 处理器是否有配置对话框。 |
| `count` | `integer` | `dsps` 的条目数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### dsp.addDsp

<!-- api-schema:begin dsp.addDsp -->
用默认设置把一个处理器插入当前链。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `guid` | `string` | 是 | 已安装处理器的 GUID，取自 `dsp.getAvailable`。不能为空。 |
| `position` | `integer` | 否 | 插入位置；`-1` 或超出链尾的位置都表示追加到末尾。默认 `-1`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `addedDsp` | `string` | 加入的处理器的显示名。 |
| `position` | `integer` | 该项最终所在的位置。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
// 获取可用 DSP 列表，然后添加
const available = await fb2k.invoke('dsp.getAvailable');
if (available.success === false) throw new Error(available.error);
const eq = available.dsps.find(d => d.name === 'Equalizer');
if (eq) {
    await fb2k.invoke('dsp.addDsp', { guid: eq.guid });
}
```

### dsp.removeDsp

<!-- api-schema:begin dsp.removeDsp -->
从当前链移除一项。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `index` | `integer` | 是 | 要移除的项的位置。不小于 `0`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `removedDsp` | `string` | 移除的处理器的显示名。 |
| `removedIndex` | `integer` | 它原来所在的位置。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### dsp.moveDsp

<!-- api-schema:begin dsp.moveDsp -->
把当前链里的一项移到另一位置，保留它的设置。重排请用它而不是 `setChain`：`setChain` 会用默认设置重建每一项。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `from` | `integer` | 是 | 该项现在的位置。不小于 `0`。 |
| `to` | `integer` | 是 | 重排后的最终位置。不小于 `0`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `movedDsp` | `string` | 移动的处理器的显示名；`from` 等于 `to`、什么都没动时不出现。 |
| `from` | `integer` | 它原来的位置。 |
| `to` | `integer` | 它现在的位置。 |
| `message` | `string` | `from` 等于 `to` 时为 `No change needed`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 升序、降序移动时返回的 `to` 都与传入值一致。需要重排链请用本接口，不要用 `setChain`（后者只接受 `guid`，不承诺保留参数）。

### dsp.setChain

<!-- api-schema:begin dsp.setChain -->
整条替换当前链，每一项按处理器的默认设置构建。空列表即清空链。任一项解析不到已安装的处理器，整次调用失败且链保持不变。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `dsps` | `DspChainSpec[]` | 是 | 新链，按处理顺序；空即清空链。 |
| `dsps[].guid` | `string` | 是 | 已安装处理器的 GUID，取自 `dsp.getAvailable`。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | 现在链里的条目数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

任意一项无法解析时整次调用被拒绝且**活动链保持不变**，错误信息带出错下标：

| 情况 | `error` | `code` |
| --- | --- | --- |
| `dsps` 缺失 | `dsps is required` | `INVALID_PARAMS` |
| `dsps` 不是数组 | `dsps must be an array` | `INVALID_PARAMS` |
| 元素不是对象 | `dsps[0] must be an object` | `INVALID_PARAMS` |
| 缺 `guid` 或为空串 | `dsps[0].guid is required` | `INVALID_PARAMS` |
| `guid` 不是字符串 | `dsps[0].guid must be a string` | `INVALID_PARAMS` |
| 元素带了别的键 | `unknown parameter 'dsps[0].index'` | `INVALID_PARAMS` |
| GUID 格式非法 | `dsps[0]: Invalid GUID format: <值>` | `INVALID_PARAMS` |
| GUID 合法但该 DSP 未安装 | `dsps[0]: DSP not found or no default preset: <值>` | `NOT_FOUND` |

传入空数组是合法的，表示清空整条链，返回 `count: 0`。每项只能带 `guid`：`getChain` 返回的条目还带 `index` 与 `name`，经 SDK 的 `fb.dsp.setChain` 回传时只发送 `guid`，直接 invoke 时要自己映射。

```javascript
await fb2k.invoke('dsp.setChain', {
    dsps: [
        { guid: '{EQ-GUID-HERE}' },
        { guid: '{LIMITER-GUID-HERE}' }
    ]
});
```

> 传空数组会清空整条链。本接口只接受 `guid`，因此每个 DSP 都按其默认预设加入——参数是否得以保留**取决于该 DSP 的实现**：多数 foobar2000 内置 DSP 把设置存在全局配置里，参数会保留；而按预设实例存参的 DSP（VST 包装器、部分第三方 DSP）会回到默认值。不要依赖此行为，也不要把 `getChain` 的输出直接回灌 `setChain` 来做重排序，重排请用 `dsp.moveDsp`。

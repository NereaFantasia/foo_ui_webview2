# DSP 音效处理 API

`dsp` 命名空间提供 DSP 效果器链检查、重排与预设应用能力。

## DSP API - 效果器管理

> [!NOTE]
> `dsp.getActivePreset` 与 `dsp.setActivePreset` 未在 C++ 端点注册；如需读写当前激活的预设名称，请改用 `config.getActiveDspPreset` 与 `config.setActiveDspPreset`。

### dsp.getChain

<!-- api-schema:begin dsp.getChain -->
获取当前生效的 DSP 效果器链及匹配的活动预设信息（若未匹配预设则返回 null）

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `dsps` | `DspChainEntry[]` | 按音频处理顺序排列的活动 DSP 条目列表 |
| `dsps[].index` | `integer` | 在 DSP 链中的索引位置 |
| `dsps[].guid` | `string` | 效果器处理器的 GUID 标识符（格式形如 `{...}`） |
| `dsps[].name` | `string` | 效果器处理器的显示名称 |
| `activePreset` | `string \| null` | 当前匹配的预设名称；当活动链未匹配任何预设（如经由 `addDsp`、`removeDsp`、`moveDsp` 或 `setChain` 修改后）时为 `null` |
| `activePresetIndex` | `integer` | 选中预设在 `dsp.getPresets` 预设列表中的序号；未选中任何预设时为 `-1` |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 手动修改活动链（调用 `addDsp`、`removeDsp`、`moveDsp` 或 `setChain`）会使当前链脱离预设绑定，此后 `activePreset` 为 `null`，`activePresetIndex` 为 `-1`。

### dsp.getPresets

<!-- api-schema:begin dsp.getPresets -->
获取用户配置目录中已保存的全部 DSP 预设列表及当前选中的预设序号

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `presets` | `DspPresetEntry[]` | 配置目录中保存的全部预设列表（按枚举顺序排列） |
| `presets[].index` | `integer` | 在预设列表中的索引序号（可直接传入 `dsp.applyPreset`） |
| `presets[].name` | `string` | 预设名称（对应用户配置目录 `dsp-presets` 下的文件名） |
| `presets[].active` | `boolean` | 是否为当前处于激活状态的预设 |
| `count` | `integer` | `presets` 列表包含的预设总数 |
| `selectedIndex` | `integer` | 当前选中的预设索引序号；未选中任何预设时为 `-1` |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 预设文件持久化保存在用户配置目录的 `dsp-presets/<名称>.fb2k-dsp`。

### dsp.applyPreset

<!-- api-schema:begin dsp.applyPreset -->
将指定 DSP 预设应用为当前活动链（支持按预设索引或名称指定，两者均传入时以 `index` 为准）；整条替换当前活动效果器链且不改写磁盘预设文件；名称匹配严格区分大小写（存在同名预设时匹配首个）；未指定任何参数以 `INVALID_PARAMS` 失败，索引超出范围以 `INVALID_INDEX` 失败，未查得匹配预设名称以 `NOT_FOUND` 失败

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `index` | `integer` | 否 | 目标预设在 `dsp.getPresets` 中的索引序号不小于 `0`。 |
| `name` | `string` | 否 | 目标预设的完整名称（区分大小写） |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `appliedPreset` | `string` | 成功应用并生效的预设名称 |
| `appliedIndex` | `integer` | 成功应用并生效的预设索引序号 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 必须传入 `name` 或 `index` 至少一项（两者均缺失以 `INVALID_PARAMS` 失败）；同时传入时以 `index` 为准。

```javascript
// 按名称
await fb2k.invoke('dsp.applyPreset', { name: 'My Preset' });
// 按索引
await fb2k.invoke('dsp.applyPreset', { index: 0 });
```

> 应用预设将全量替换当前活动链（包含各效果器的内部参数配置）；该操作仅更新内存活动链，不会改写磁盘预设文件。

### dsp.getAvailable

<!-- api-schema:begin dsp.getAvailable -->
列出宿主中已安装的所有 DSP 音效处理器及其配置支持状态

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `dsps` | `DspAvailableEntry[]` | 宿主中已安装的所有处理器列表（按服务枚举顺序排列） |
| `dsps[].guid` | `string` | 处理器的 GUID 标识符（格式形如 `{...}`，作为 `addDsp` 与 `setChain` 的参数） |
| `dsps[].name` | `string` | 效果器处理器的显示名称 |
| `dsps[].hasConfig` | `boolean` | 该处理器是否提供专属参数配置对话框 |
| `count` | `integer` | 已安装的处理器总数 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### dsp.addDsp

<!-- api-schema:begin dsp.addDsp -->
以默认参数将指定的 DSP 效果器插入当前活动链的指定位置

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `guid` | `string` | 是 | 待添加处理器的 GUID 标识符（取自 `dsp.getAvailable`）不能为空。 |
| `position` | `integer` | 否 | 插入的目标索引位置；传入 `-1` 或超出链尾的数值均表示追加至末尾默认 `-1`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `addedDsp` | `string` | 成功加入的处理器的显示名称 |
| `position` | `integer` | 插入条目在活动链中的实际位置序号 |

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
从当前活动链中移除指定位置的 DSP 效果器条目

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `index` | `integer` | 是 | 待移除条目在活动链中的索引位置不小于 `0`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `removedDsp` | `string` | 成功移除的处理器的显示名称 |
| `removedIndex` | `integer` | 该条目在移除前所处的索引位置 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### dsp.moveDsp

<!-- api-schema:begin dsp.moveDsp -->
将当前活动链中的指定条目移动至目标位置并保留现有参数；重排效果器顺序时优先使用此方法（`setChain` 会以默认参数重建各项）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `from` | `integer` | 是 | 待移动条目的当前索引位置不小于 `0`。 |
| `to` | `integer` | 是 | 条目重排后的目标索引位置不小于 `0`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `movedDsp` | `string` | 移动的处理器的显示名称；当 `from === to` 未发生实际位移时缺省 |
| `from` | `integer` | 移动操作前的原始索引位置 |
| `to` | `integer` | 移动操作完成后的目标索引位置 |
| `message` | `string` | 状态提示文本；当 `from === to` 时固定为 `"No change needed"` |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 无论升序或降序移动，返回的 `to` 均与传入的目标索引一致；重排效果器顺序应始终使用本接口（`setChain` 仅接收 `guid` 且按默认参数重建，无法保留已有配置）。

### dsp.setChain

<!-- api-schema:begin dsp.setChain -->
全量替换当前生效的 DSP 效果器链（各项均按处理器默认配置构建，传入空列表代表清空链）；若任一条目无法解析为已安装的处理器，整次调用以失败返回且保持原活动链不变

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `dsps` | `DspChainSpec[]` | 是 | 替换后的新 DSP 链列表（按音频处理顺序排列，传入空数组代表清空链） |
| `dsps[].guid` | `string` | 是 | 待添加处理器的 GUID 标识符（取自 `dsp.getAvailable`）不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | 链重置后包含的效果器条目总数 |

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

传入空数组可直接清空整条效果器链并返回 `count: 0`；数组元素仅接受 `guid` 属性（`dsp.getChain` 返回的条目包含 `index` 与 `name`，直接通过 `fb2k.invoke` 调用时须提前映射剔除多余字段；SDK 的 `fb.dsp.setChain` 会自动完成清洗映射）。

```javascript
await fb2k.invoke('dsp.setChain', {
    dsps: [
        { guid: '{EQ-GUID-HERE}' },
        { guid: '{LIMITER-GUID-HERE}' }
    ]
});
```

> 传入空数组将清空整条活动链。由于本接口仅接收 `guid`，各效果器均按其默认预设加载；参数能否保留**取决于具体 DSP 组件的内部实现**（多数 foobar2000 内置效果器将参数保存在全局配置，故能保留设置；而 VST 包装器或部分第三方 DSP 按实例存参，将重置回默认值）；禁止将 `dsp.getChain` 的结果直接回传给 `setChain` 执行重排，效果器重排必须使用 `dsp.moveDsp`。

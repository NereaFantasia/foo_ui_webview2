# DSP API

The `dsp` namespace provides methods to inspect installed DSP processors, query and modify the active DSP chain, and apply DSP presets.

## dsp

> `dsp.getActivePreset` and `dsp.setActivePreset` are not registered in the C++ backend. Use `config.getActiveDspPreset` and `config.setActiveDspPreset` instead.

### dsp.addDsp

<!-- api-schema:begin dsp.addDsp -->
Insert a processor into the active chain with its default settings.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `guid` | `string` | Yes | GUID of an installed processor, as `dsp.getAvailable` reports it. Must not be empty. |
| `position` | `integer` | No | Position to insert at; any negative position, or one past the end, appends. Default: `-1`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `addedDsp` | `string` | Display name of the processor added. |
| `position` | `integer` | Position the entry landed at. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('dsp.getAvailable');
if (res.success === false) throw new Error(res.error);
const { dsps } = res;
const eq = dsps.find(d => d.name === 'Equalizer');
if (eq) {
    await fb2k.invoke('dsp.addDsp', { guid: eq.guid });
}
```

### dsp.applyPreset

<!-- api-schema:begin dsp.applyPreset -->
Make a preset the active chain, by index or by name; `index` wins when both are given. Replaces the whole active chain and never writes the preset file. A name matches exactly, case included, and the first preset of that name is taken. Giving neither fails with `INVALID_PARAMS`, an index past the last preset with `INVALID_INDEX`, a name no preset has with `NOT_FOUND`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `index` | `integer` | No | Preset index from `dsp.getPresets`. At least `0`. |
| `name` | `string` | No | Preset name from `dsp.getPresets`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `appliedPreset` | `string` | Name of the preset now active. |
| `appliedIndex` | `integer` | Index of the preset now active. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('dsp.applyPreset', { name: 'Headphones' });
```

Supply either `index` or `name`. At least one is required, and `index` takes precedence when both are provided. Both address the same presets, and the response echoes `appliedPreset` and `appliedIndex`. Applying a preset replaces the entire active chain; it never modifies the stored preset file on disk (`profile\dsp-presets\<name>.fb2k-dsp`).

### dsp.getAvailable

<!-- api-schema:begin dsp.getAvailable -->
List every installed DSP processor.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `dsps` | `DspAvailableEntry[]` | Every installed processor, in service enumeration order. |
| `dsps[].guid` | `string` | GUID of the processor, rendered as `{...}`; what `addDsp` and `setChain` take. |
| `dsps[].name` | `string` | Display name of the processor. |
| `dsps[].hasConfig` | `boolean` | Whether the processor has a configuration dialog. |
| `count` | `integer` | Number of entries in `dsps`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('dsp.getAvailable');
```

### dsp.getChain

<!-- api-schema:begin dsp.getChain -->
Read the active DSP chain and which preset, if any, it currently corresponds to.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `dsps` | `DspChainEntry[]` | The chain in processing order. |
| `dsps[].index` | `integer` | Position in the chain. |
| `dsps[].guid` | `string` | GUID of the processor, rendered as `{...}`. |
| `dsps[].name` | `string` | Display name of the processor. |
| `activePreset` | `string \| null` | Name of the selected preset; `null` when the chain matches no preset, as after any edit through `addDsp`, `removeDsp`, `moveDsp` or `setChain`. |
| `activePresetIndex` | `integer` | Index of the selected preset in `dsp.getPresets`; `-1` when none is selected. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('dsp.getChain');
```

`activePreset` and `activePresetIndex` are always present in the response. When no preset is selected — including immediately after manual chain modifications via `dsp.setChain`, `dsp.addDsp`, `dsp.removeDsp`, or `dsp.moveDsp` — they return `null` and `-1` respectively.

### dsp.getPresets

<!-- api-schema:begin dsp.getPresets -->
List the DSP presets stored in the profile and which one is selected.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `presets` | `DspPresetEntry[]` | Every stored preset, in list order. |
| `presets[].index` | `integer` | Index in the preset list, as `applyPreset` takes it. |
| `presets[].name` | `string` | Preset name, which is also its file name under `dsp-presets` in the profile. |
| `presets[].active` | `boolean` | Whether this is the selected preset. |
| `count` | `integer` | Number of entries in `presets`. |
| `selectedIndex` | `integer` | Index of the selected preset; `-1` when none is selected. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('dsp.getPresets');
```

`selectedIndex` is `-1` when no preset is selected. Stored presets reside in `profile\dsp-presets\<name>.fb2k-dsp`.

### dsp.moveDsp

<!-- api-schema:begin dsp.moveDsp -->
Move one entry of the active chain to another position, keeping its settings. Prefer this over `setChain` for reordering: `setChain` rebuilds every entry from its default settings.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `from` | `integer` | Yes | Current position of the entry. At least `0`. |
| `to` | `integer` | Yes | Final position in the reordered chain. At least `0`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `movedDsp` | `string` | Display name of the processor moved; absent when `from` equals `to` and nothing moved. |
| `from` | `integer` | The position it came from. |
| `to` | `integer` | The position it is at now. |
| `message` | `string` | `No change needed` when `from` equals `to`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('dsp.moveDsp', { from: 2, to: 0 });
```

`to` represents the final index in the reordered chain and matches the value passed. When `from === to`, no processors move and the response returns `message: "No change needed"`. Use `dsp.moveDsp` rather than feeding `dsp.getChain` back into `dsp.setChain` to reorder processors, as `dsp.moveDsp` preserves each processor's configuration. Negative `from` or `to` values fail with `INVALID_PARAMS` (`from is out of range`), while indices at or beyond chain length fail with `INVALID_INDEX` (`Index out of range`).

### dsp.removeDsp

<!-- api-schema:begin dsp.removeDsp -->
Remove one entry from the active chain.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `index` | `integer` | Yes | Position of the entry to remove. At least `0`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `removedDsp` | `string` | Display name of the processor removed. |
| `removedIndex` | `integer` | Position it was removed from. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('dsp.removeDsp', { index: 0 });
```

A negative `index` is rejected by parameter validation with `INVALID_PARAMS` (`index is out of range`). An index at or beyond chain length is rejected with `INVALID_INDEX` (`Index out of range`).

### dsp.setChain

<!-- api-schema:begin dsp.setChain -->
Replace the whole active chain, each entry built from its processor's default settings. An empty list clears the chain. One entry that does not resolve to an installed processor fails the whole call and leaves the chain untouched.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `dsps` | `DspChainSpec[]` | Yes | The new chain in processing order; empty clears the chain. |
| `dsps[].guid` | `string` | Yes | GUID of an installed processor, as `dsp.getAvailable` reports it. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `count` | `integer` | Number of entries in the chain now. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('dsp.getAvailable');
if (res.success === false) throw new Error(res.error);
const { dsps } = res;
const eq = dsps.find(d => d.name === 'Equalizer');
await fb2k.invoke('dsp.setChain', { dsps: [{ guid: eq.guid }] });

// Clear active DSP chain
await fb2k.invoke('dsp.setChain', { dsps: [] });
```

Replaces the entire active chain. Passing `dsps: []` clears it. Each array element must be an object containing only a `guid`: entries returned by `dsp.getChain` also contain `index` and `name`, so map them to `{ guid }` or use the SDK's `fb.dsp.setChain`. Entries are applied in array order.

Every entry must resolve to an installed DSP. The call is rejected as a whole without altering the current chain, and the error message identifies the offending index:

| Condition | Error | `code` |
| --- | --- | --- |
| `dsps` absent | `dsps is required` | `INVALID_PARAMS` |
| `dsps` not an array | `dsps must be an array` | `INVALID_PARAMS` |
| Element is not an object | `dsps[0] must be an object` | `INVALID_PARAMS` |
| `guid` missing or empty | `dsps[0].guid is required` | `INVALID_PARAMS` |
| `guid` not a string | `dsps[0].guid must be a string` | `INVALID_PARAMS` |
| Element carries another key | `unknown parameter 'dsps[0].index'` | `INVALID_PARAMS` |
| `guid` malformed | `dsps[0]: Invalid GUID format: <value>` | `INVALID_PARAMS` |
| `guid` well-formed but DSP not installed | `dsps[0]: DSP not found or no default preset: <guid>` | `NOT_FOUND` |

Because each entry supplies only a `guid`, each DSP is added with its default preset. Whether current settings are retained depends on the DSP implementation: processors with global configuration retain settings, whereas DSPs storing configuration per preset instance (such as VST wrappers and some third-party components) reset to factory defaults. Use `dsp.moveDsp` to reorder existing processors without losing settings.

Editing the chain detaches it from any active preset. Subsequent calls to `dsp.getChain` return `activePreset: null`, and `dsp.getPresets` reports `selectedIndex: -1`.

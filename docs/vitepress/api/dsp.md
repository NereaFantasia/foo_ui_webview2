# DSP API

Methods of the `dsp` namespace.

## dsp

> Note: `dsp.getActivePreset` / `dsp.setActivePreset` are not registered on the C++ side — use `config.getActiveDspPreset` / `config.setActiveDspPreset` instead.

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

Supply either `index` or `name`; at least one is required, and `index` wins when
both are present. Both address the same presets, and the response echoes
`appliedPreset` / `appliedIndex` either way. Applying a preset replaces the whole
active chain; it never writes back to the stored preset, so the files under
`profile\dsp-presets\<name>.fb2k-dsp` are left untouched.

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

`activePreset` and `activePresetIndex` are always present. When no preset is
selected — including right after `dsp.setChain`, `addDsp`, `removeDsp` or
`moveDsp` edit the chain by hand — they report `null` and `-1` respectively
rather than being omitted.

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

`selectedIndex` is `-1` when no preset is selected. Presets live in
`profile\dsp-presets\<name>.fb2k-dsp`.

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

`to` is the final index in the reordered chain and matches the value you passed,
in both directions. When `from === to` nothing moves and the response carries
`message: "No change needed"`. Use this — not `getChain` fed back into
`setChain` — to reorder a chain, because it preserves each DSP's configuration.
A negative `from` or `to` is refused by the parameter reader with `INVALID_PARAMS`
(`from is out of range`); a position at or past the end of the chain is refused with
`INVALID_INDEX` (`Index out of range`).

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

A negative `index` never reaches the chain: the parameter reader refuses it with
`INVALID_PARAMS` (`index is out of range`). An index at or past the end of the chain
is refused with `INVALID_INDEX` (`Index out of range`).

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

// Clear the chain
await fb2k.invoke('dsp.setChain', { dsps: [] });
```

Replaces the entire chain. Passing `dsps: []` clears it. Each element must be an
object carrying a `guid` and nothing else: the entries `getChain` reports also carry
`index` and `name`, so pass them through the SDK's `fb.dsp.setChain`, which sends only
`guid`, or map them yourself. Entries are applied in array order.

Every entry must resolve to an installed DSP — the call is rejected as a whole,
without touching the current chain, and the error names the offending index:

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

Because entries only carry a `guid`, each DSP is added using its default preset.
**Whether that keeps the DSP's current settings depends on the DSP itself** —
many foobar2000 DSPs store configuration globally, so their settings survive, but
a DSP that keeps configuration per preset instance (VST wrappers, some
third-party DSPs) will fall back to factory values. Do not rely on `setChain` to
preserve configuration; use `moveDsp` when you only need to reorder.

Editing the chain this way detaches it from any preset, so `getChain` afterwards
reports `activePreset: null` and `getPresets` reports `selectedIndex: -1`.

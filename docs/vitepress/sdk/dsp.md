# fb.dsp DSP Chain

`fb.dsp` inspects and modifies the active digital signal processing chain and applies saved DSP presets.

## getChain()

Signature: `fb.dsp.getChain(): Promise<DspGetChainResponse>`

Returns the active DSP entries (`DspChainEntry`: `index`, `guid`, `name`) in execution order. A successful result always includes `activePreset` (`null` when no preset is selected) and `activePresetIndex` (`-1` when none). An edit through `addDsp()`, `removeDsp()`, `moveDsp()`, or `setChain()` leaves the chain matching no preset, so both read as unselected afterwards.

```javascript
const res = await fb.dsp.getChain();
if (res.success === false) throw new Error(res.error);
const { dsps, activePreset } = res;
```

## setChain(dsps)

Signature: `fb.dsp.setChain(dsps: ReadonlyArray<Pick<DspChainSpec, 'guid'>>): Promise<DspSetChainResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| dsps | `{ guid: string }[]` | Yes | The new chain in processing order. An empty array clears the chain. |

Replaces the active chain with the supplied entries; each entry carries only a `guid`. The SDK drops every other field before sending, so the rows `getChain()` and `getAvailable()` report can be passed as they are. The response reports the new `count`. The whole call is refused, and the chain left untouched, on the first entry that does not resolve to an installed DSP.

Every entry is rebuilt from its DSP's default settings. To reorder the chain without losing settings, use `moveDsp()`.

```javascript
const available = await fb.dsp.getAvailable();
if (available.success === false) throw new Error(available.error);
const eq = available.dsps.find((d) => d.name === 'Equalizer');
if (eq) await fb.dsp.setChain([{ guid: eq.guid }]);
```

## getPresets()

Signature: `fb.dsp.getPresets(): Promise<DspGetPresetsResponse>`

Returns `presets` (`DspPresetEntry`: `index`, `name`, `active`), `count`, and `selectedIndex` (`-1` when no preset is selected). A preset's `name` is also its file name under `dsp-presets` in the foobar2000 profile.

```javascript
const res = await fb.dsp.getPresets();
if (res.success === false) throw new Error(res.error);
const names = res.presets.map((p) => p.name);
```

## applyPreset(indexOrName)

Signature: `fb.dsp.applyPreset(indexOrName: number | string): Promise<DspApplyPresetResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| indexOrName | number \| string | Yes | Preset `index` or `name` from `getPresets()`. A number is sent as `index`, a string as `name`. |

Applies a saved preset by numeric index or display name. The preset replaces the whole active chain; the preset file is not written. The response echoes `appliedPreset` and `appliedIndex`.

```javascript
await fb.dsp.applyPreset('Headphones');
await fb.dsp.applyPreset(0); // by index
```

## getAvailable()

Signature: `fb.dsp.getAvailable(): Promise<DspGetAvailableResponse>`

Returns discoverable DSP entries in `dsps`, in service enumeration order, and their `count`. Each entry (`DspAvailableEntry`) has `guid`, `name`, and `hasConfig` fields; `hasConfig` is `true` when the DSP has a configuration dialog. The `guid` is what `addDsp()` and `setChain()` take.

```javascript
const res = await fb.dsp.getAvailable();
if (res.success === false) throw new Error(res.error);
const configurable = res.dsps.filter((d) => d.hasConfig);
```

## addDsp(guid, position?)

Signature: `fb.dsp.addDsp(guid: string, position?: number): Promise<DspAddDspResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| guid | string | Yes | GUID of an installed DSP, as `getAvailable()` reports it |
| position | number | No | Chain index to insert at. Omitted, negative, or at or past the end of the chain, the DSP is appended. |

Adds the DSP identified by `guid` with its default settings, optionally at a specific chain position (`-1` or omitted appends). The response carries `addedDsp` and the `position` it landed at.

```javascript
await fb.dsp.addDsp('{00000000-0000-0000-0000-000000000000}', 0);
```

## removeDsp(index)

Signature: `fb.dsp.removeDsp(index: number): Promise<DspRemoveDspResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| index | number | Yes | Index of the entry in the active chain, as `getChain()` reports it |

Removes the DSP at the supplied active-chain index. The response carries `removedDsp` and `removedIndex`.

```javascript
const res = await fb.dsp.removeDsp(2);
if (res.success === false) throw new Error(res.error);
console.log(`Removed ${res.removedDsp}`);
```

## moveDsp(from, to)

Signature: `fb.dsp.moveDsp(from: number, to: number): Promise<DspMoveDspResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| from | number | Yes | Current index of the entry |
| to | number | Yes | Index of the entry in the reordered chain |

Moves an active DSP from one chain index to another and keeps its settings, so prefer it over `setChain()` for reordering. The response echoes `from` and `to`, where `to` is the final landing index, and names the moved DSP in `movedDsp`. When `from === to` nothing moves: `movedDsp` is absent and `message` is present.

```javascript
await fb.dsp.moveDsp(2, 0);
```

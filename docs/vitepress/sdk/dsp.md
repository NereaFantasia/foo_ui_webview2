# fb.dsp DSP Chain

`fb.dsp` inspects and modifies the active digital signal processing chain and applies saved DSP presets.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## Additional methods

### getChain()

Signature: `fb.dsp.getChain(): Promise<DspGetChainResponse>`

Returns the active DSP entries in execution order. A successful result always includes `activePreset` (`null` when no preset is selected) and `activePresetIndex` (`-1` when none).

```javascript
const { dsps, activePreset } = await fb.dsp.getChain();
```

### setChain(dsps)

Signature: `fb.dsp.setChain(dsps: unknown[]): Promise<BaseResponse & { count?: number }>`

Replaces the active chain with the supplied DSP descriptors.

### getPresets()

Signature: `fb.dsp.getPresets(): Promise<DspGetPresetsResponse>`

Returns `presets`, `count`, and `selectedIndex`. Each preset includes its normal `DspPreset` fields and may include an `active` flag.

### applyPreset(indexOrName)

Signature: `fb.dsp.applyPreset(indexOrName: number | string): Promise<BaseResponse & { appliedPreset?: string; appliedIndex?: number }>`

Applies a saved preset by numeric index or display name.

```javascript
await fb.dsp.applyPreset('Headphones');
```

### getAvailable()

Signature: `fb.dsp.getAvailable(): Promise<DspGetAvailableResponse>`

Returns discoverable DSP entries in `dsps`. Each entry has `guid`, `name`, and `hasConfig` fields.

### addDsp(guid, position?)

Signature: `fb.dsp.addDsp(guid: string, position?: number): Promise<BaseResponse & { addedDsp?: string }>`

Adds the DSP identified by `guid`, optionally at a specific chain position.

```javascript
await fb.dsp.addDsp('{00000000-0000-0000-0000-000000000000}', 0);
```

### removeDsp(index)

Signature: `fb.dsp.removeDsp(index: number): Promise<BaseResponse & { removedDsp?: string }>`

Removes the DSP at the supplied active-chain index.

### moveDsp(from, to)

Signature: `fb.dsp.moveDsp(from: number, to: number): Promise<BaseResponse & { movedDsp?: string; from?: number; to?: number; message?: string }>`

Moves an active DSP from one chain index to another. The response echoes `from` and `to`, where `to` is the final landing index; `message` is present when `from === to` and nothing moved.

```javascript
await fb.dsp.moveDsp(2, 0);
```

<!-- END AUTO-GENERATED SDK STUBS -->

# Audio Extensions

Covers the `fb.dsp`, `fb.output`, and `fb.replaygain` namespaces.

## fb.dsp DSP Management

### getChain()

Returns the active DSP chain in execution order, along with best-effort active-preset information.

```javascript
const chain = await fb.dsp.getChain();
// chain.dsps: [{ index, guid, name }, ...]
```

### setChain(dsps)

Replaces the active DSP chain.

| Parameter | Type | Description |
| --- | --- | --- |
| dsps | unknown[] | DSP configuration array passed to `dsp.setChain`; each entry is read for its `guid` only |

```javascript
await fb.dsp.setChain([{ guid: '{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}' }]);
```

### getPresets()

Returns all DSP presets, their count, and the selected preset index.

```javascript
const r = await fb.dsp.getPresets();
// r.presets: [{ name, index }, ...]
```

### applyPreset(indexOrName)

Applies a DSP preset by numeric index or string name.

```javascript
await fb.dsp.applyPreset(0);           // By index
await fb.dsp.applyPreset('My Preset'); // By name
```

### getAvailable()

Returns the available DSP entries. Each entry includes `guid`, `name`, and `hasConfig`.

```javascript
const r = await fb.dsp.getAvailable();
// r.dsps: [{ guid, name, hasConfig }, ...]
```

### addDsp(guid, position?)

Adds a DSP to the active chain.

| Parameter | Type | Description |
| --- | --- | --- |
| guid | string | DSP entry GUID |
| position | number | Optional insertion position; omission appends to the chain |

```javascript
await fb.dsp.addDsp('{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}');
```

### removeDsp(index)

Removes the DSP at the specified chain index.

```javascript
await fb.dsp.removeDsp(2); // Remove the third DSP
```

### moveDsp(from, to)

Moves a DSP within the active chain.

```javascript
await fb.dsp.moveDsp(0, 2); // Move the first DSP to the third position
```

## fb.output Output Devices

### getDevices()

Returns a flat array of available audio output devices. The SDK unwraps the host's `{ devices, count }` envelope.

```javascript
const devices = await fb.output.getDevices();
// [{ guid, name, entry, entryGuid }, ...]
```

### getEntries()

Returns output-module entries and their capabilities.

```javascript
const r = await fb.output.getEntries();
```

### getSettings()

Returns the available output module names and any host note. This endpoint does not return the active device configuration.

```javascript
const settings = await fb.output.getSettings();
```

## fb.replaygain ReplayGain

### get(paths)

Returns ReplayGain metadata for one path or an array of paths. The SDK always sends a `{ paths: string[] }` request.

```javascript
const r = await fb.replaygain.get('E:\\Music\\song.flac');
const r2 = await fb.replaygain.get(['song1.flac', 'song2.flac']);
```

### getMode()

Returns the current source and processing modes.

```javascript
const r = await fb.replaygain.getMode();
// r.sourceMode, r.processingMode
```

### setMode(sourceMode, processingMode?)

Updates the ReplayGain source mode and, optionally, the processing mode.

| Parameter | Type | Description |
| --- | --- | --- |
| sourceMode | string | Source mode |
| processingMode | string | Optional processing mode |

```javascript
await fb.replaygain.setMode('track');
```

### getPreamp()

Returns the preamp values for tracks with and without ReplayGain metadata.

```javascript
const r = await fb.replaygain.getPreamp();
// r.withRg, r.withoutRg (dB)
```

### setPreamp(withRg?, withoutRg?)

Updates either or both ReplayGain preamp values, in dB.

```javascript
await fb.replaygain.setPreamp(6.0, 0.0);
```

### getSettings()

Returns the complete ReplayGain settings snapshot.

```javascript
const settings = await fb.replaygain.getSettings();
```

### scan(paths)

Starts a ReplayGain scan through the host context-menu pipeline. The default mode is `'track'`; pass `{ mode: 'album' }` to treat the selection as one album.

```javascript
await fb.replaygain.scan(['E:\\Music\\song1.flac', 'E:\\Music\\song2.flac']);
```

### clear(paths)

Clears ReplayGain metadata from the specified files.

```javascript
await fb.replaygain.clear(['E:\\Music\\song.flac']);
```

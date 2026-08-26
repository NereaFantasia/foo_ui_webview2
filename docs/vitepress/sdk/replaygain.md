# fb.replaygain ReplayGain

`fb.replaygain` reads ReplayGain metadata, controls playback processing settings, and starts host-side scan or clear operations.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## Additional methods

> This block records SDK method coverage and may later be expanded with complete examples and best practices.

### clear()

Signature: `fb.replaygain.clear(paths: string[]): Promise<BaseResponse & { clearedCount?: number }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| paths | string[] | Yes | File paths whose ReplayGain metadata should be cleared |

Returns the underlying clear result and may include `clearedCount`.

```javascript
const result = await fb.replaygain.clear(['E:\\Music\\song.flac']);
```

### get()

Signature: `fb.replaygain.get(paths: string | string[]): Promise<ReplayGainGetResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| paths | string \| string[] | Yes | One file path or an array of paths; the SDK always sends `{ paths: string[] }` |

Returns per-track results with optional track/album gain and peak fields.

```javascript
const result = await fb.replaygain.get('E:\\Music\\song.flac');
```

### getMode()

Signature: `fb.replaygain.getMode(): Promise<ReplayGainGetModeResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns `sourceMode` and `processingMode` when available.

```javascript
const result = await fb.replaygain.getMode();
```

### getPreamp()

Signature: `fb.replaygain.getPreamp(): Promise<ReplayGainGetPreampResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns `withRg` and `withoutRg` preamp values in dB when available.

```javascript
const result = await fb.replaygain.getPreamp();
```

### getSettings()

Signature: `fb.replaygain.getSettings(): Promise<ReplayGainGetSettingsResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns the source mode, processing mode, both preamp values, and active state. To start analysis, call `scan(paths, { mode? })`; the default mode is `'track'`, while `'album'` treats the selection as one album.

```javascript
const result = await fb.replaygain.getSettings();
await fb.replaygain.scan(['E:\\Music\\one.flac'], { mode: 'track' });
```

### setMode()

Signature: `fb.replaygain.setMode(sourceMode: string, processingMode?: string): Promise<BaseResponse & { changed?: boolean }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| sourceMode | string | Yes | ReplayGain source mode |
| processingMode | string | No | ReplayGain processing mode |

Returns the update result and may include `changed`.

```javascript
const result = await fb.replaygain.setMode('track');
```

### setPreamp()

Signature: `fb.replaygain.setPreamp(withRg?: number, withoutRg?: number): Promise<BaseResponse & { changed?: boolean }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| withRg | number | No | Preamp in dB for tracks with ReplayGain metadata |
| withoutRg | number | No | Preamp in dB for tracks without ReplayGain metadata |

Only defined arguments are sent. Returns the update result and may include `changed`.

```javascript
const result = await fb.replaygain.setPreamp(6, 0);
```

<!-- END AUTO-GENERATED SDK STUBS -->

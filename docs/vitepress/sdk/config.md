# `fb.config` configuration

## get(key) 

Returns the portable configuration value as `{ value }`.

| Parameter | Type | Description |
| --- | --- | --- |
| `key` | `string` | Configuration key |

The SDK wrapper accepts only the key; it does not expose the native `default` parameter.

```javascript
const result = await fb.config.get('theme');
console.log(result.value);   // 'dark'
```

## set(key, value) 

Persists a portable configuration value.

```javascript
await fb.config.set('theme', 'dark');
await fb.config.set('volume', 0.8);
```

## remove(key) 

Removes a portable configuration item. The response may include `existed`.

```javascript
await fb.config.remove('theme');
```

## getAll() 

Returns `{ success, items, configs, count }`. `items` and `configs` are aliases for the same key-value snapshot.

## export() 

Returns `{ success, data, json, count }`, including the serialized JSON snapshot.

## Host information

### getVersionInfo() 

Returns foobar2000 and plugin version information, architecture, portable-mode state, and profile path.

```javascript
const ver = await fb.config.getVersionInfo();
console.log(ver.versionFull, ver.is64bit ? 'x64' : 'x86');
```

### getComponents() 

Returns loaded components as `ComponentInfo[]`. Each entry includes `name`, `version`, and optional `filename` / `fileName` aliases.

```javascript
const comps = await fb.config.getComponents();
```

## Output devices

### getOutputDevices() 

Returns `OutputDevice[]`, including `name`, `outputId`, `deviceId`, and `isCurrent`.

### getOutputConfig() 

Returns the current `OutputConfig`. `bufferLength` is expressed in seconds.

### setOutputDevice(outputId, deviceId) 

Selects an output driver/device pair.

### setOutputBuffer(ms) 

Sets the output buffer. A numeric argument is interpreted as milliseconds. The object form accepts `milliseconds` or native `bufferLength` seconds; if both are present, the host prefers `milliseconds`. The resolved value must be in the range `0.05..2.0` seconds.

```javascript
const devices = await fb.config.getOutputDevices();
const current = devices.find(d => d.isCurrent);
await fb.config.setOutputDevice(devices[0].outputId, devices[0].deviceId);
await fb.config.setOutputBuffer(1000); // milliseconds
await fb.config.setOutputBuffer({ bufferLength: 0.5 }); // seconds
```

## Advanced configuration

### getAdvancedConfig() 

Returns the top-level `AdvancedConfigItem[]`; branch entries expose nested `children`.

### getAdvancedConfigValue(guid) 

Returns one `AdvancedConfigValueResponse`.

### setAdvancedConfigValue(guid, value) 

Sets an advanced configuration value.

### resetAdvancedConfig(guid) 

Resets an advanced configuration entry to its default.

```javascript
const all = await fb.config.getAdvancedConfig();
const val = await fb.config.getAdvancedConfigValue('{some-guid}');
await fb.config.setAdvancedConfigValue('{some-guid}', 'new-value');
await fb.config.resetAdvancedConfig('{some-guid}');
```

## Preferences

### getPreferencesPages() 

Returns flattened preferences pages and branch nodes as `PreferencesPage[]`.

### getPreferencesStandardGuids() 

Returns standard preferences-page GUIDs such as `root`, `core`, `display`, `playback`, `output`, `mediaLibrary`, and `advanced`.

## Media-library configuration

### getLibraryStatus() 

Returns `LibraryStatus`.

### getLibraryFilePatterns() 

Returns optional `tracks` and `images` file-pattern descriptors.

### showLibraryPreferences() 

Opens the media-library preferences page.

## DSP presets

### getDspPresets() 

Returns DSP presets as `{ index, name }[]`.

### getActiveDspPreset() 

Returns `{ index, name, isActive }`; `index` and `name` are `null` when no preset is active.

### setActiveDspPreset(index) 

Activates a DSP preset by index.

```javascript
const presets = await fb.config.getDspPresets();
console.log(presets.map(p => p.name));
await fb.config.setActiveDspPreset(0);
```

### getCursorFollowPlayback() / setCursorFollowPlayback(enabled) 

Gets or sets cursor-follow-playback.

### getPlaybackFollowCursor() / setPlaybackFollowCursor(enabled) 

Gets or sets playback-follow-cursor.

### getReplaygainMode() / setReplaygainMode(mode) 

Gets or sets the ReplayGain source mode. The getter returns numeric `mode` and compatibility alias `value`. The setter accepts either `0..3` or `'none' | 'track' | 'album' | 'byPlaybackOrder' | 'auto'`; `'auto'` resolves to mode `3`.

| Value | Mode |
| --- | --- |
| 0 | None |
| 1 | Track |
| 2 | Album |
| 3 | By playback order |

```javascript
const r = await fb.config.getReplaygainMode();
console.log(r.mode); // 0
await fb.config.setReplaygainMode(2); // Album mode

await fb.config.setCursorFollowPlayback(true);
await fb.config.setPlaybackFollowCursor(false);
```

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## Additional methods

> This block completes SDK method coverage and may later be expanded with richer examples and guidance.

### export()

Signature: `fb.config.export(): Promise<ConfigExportResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns the `config.export` result.

```javascript
const result = await fb.config.export();
```

### getActiveDspPreset()

Signature: `fb.config.getActiveDspPreset(): Promise<ActiveDspPresetInfo>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns the `config.getActiveDspPreset` result.

```javascript
const result = await fb.config.getActiveDspPreset();
```

### getAll()

Signature: `fb.config.getAll(): Promise<ConfigGetAllResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns the `config.getAll` result.

```javascript
const result = await fb.config.getAll();
```

### getCursorFollowPlayback()

Signature: `fb.config.getCursorFollowPlayback(): Promise<{ enabled: boolean }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns the `config.getCursorFollowPlayback` result.

```javascript
const result = await fb.config.getCursorFollowPlayback();
```

### getLibraryFilePatterns()

Signature: `fb.config.getLibraryFilePatterns(): Promise<LibraryFilePatternsResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns the `config.getLibraryFilePatterns` result.

```javascript
const result = await fb.config.getLibraryFilePatterns();
```

### getLibraryStatus()

Signature: `fb.config.getLibraryStatus(): Promise<LibraryStatus>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns the `config.getLibraryStatus` result.

```javascript
const result = await fb.config.getLibraryStatus();
```

### getOutputConfig()

Signature: `fb.config.getOutputConfig(): Promise<OutputConfig>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns the `config.getOutputConfig` result.

```javascript
const result = await fb.config.getOutputConfig();
```

### getPlaybackFollowCursor()

Signature: `fb.config.getPlaybackFollowCursor(): Promise<{ enabled: boolean }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns the `config.getPlaybackFollowCursor` result.

```javascript
const result = await fb.config.getPlaybackFollowCursor();
```

### getPreferencesPages()

Signature: `fb.config.getPreferencesPages(): Promise<PreferencesPagesResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns the `config.getPreferencesPages` result.

```javascript
const result = await fb.config.getPreferencesPages();
```

### getPreferencesStandardGuids()

Signature: `fb.config.getPreferencesStandardGuids(): Promise<PreferencesStandardGuids>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns the `config.getPreferencesStandardGuids` result.

```javascript
const result = await fb.config.getPreferencesStandardGuids();
```

### showLibraryPreferences()

Signature: `fb.config.showLibraryPreferences(): Promise<BaseResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns the `config.showLibraryPreferences` result.

```javascript
const result = await fb.config.showLibraryPreferences();
```

<!-- END AUTO-GENERATED SDK STUBS -->

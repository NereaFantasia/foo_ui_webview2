# `fb.config` configuration

## get(key, defaultValue?)

Returns the portable configuration value as `{ success, key, value, found }`.

| Parameter | Type | Description |
| --- | --- | --- |
| `key` | `string` | Configuration key |
| `defaultValue` | `JsonValue` (optional) | Value to return when the key is not stored; passed as host parameter `default`. Omitted from the call when `undefined` |

A key that is not stored is not considered an error: `found` is `false` and `value` resolves to `defaultValue`, or `null` when no default is specified. Falsy defaults such as `0`, `false`, and `''` are sent as provided.

```javascript
const result = await fb.config.get('theme');
if (result.success === false) throw new Error(result.error);
console.log(result.value);   // 'dark'

const layout = await fb.config.get('layout', { columns: 3 });
if (layout.success === false) throw new Error(layout.error);
console.log(layout.found);   // false when the key is not stored
console.log(layout.value);   // { columns: 3 } when the key is not stored
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

Reads the whole store. Resolves with `{ items, configs, count }`: `items` maps every key to its value, `configs` is the backward-compatible alias, and `count` is the number of keys. Fails with `OPERATION_FAILED` when the store cannot be read.

```javascript
const res = await fb.config.getAll();
if (res.success === false) throw new Error(res.error);
for (const [key, value] of Object.entries(res.items)) console.log(key, value);
```

## export()

Reads the whole store as both a key-value map and serialized JSON text. Resolves with `{ data, json, count }`: `data` matches `items` from `getAll()`, and `json` is `data` serialized as compact JSON text. Fails with `OPERATION_FAILED` when the store cannot be read.

```javascript
const res = await fb.config.export();
if (res.success === false) throw new Error(res.error);
await navigator.clipboard.writeText(res.json);
```

## Host information

### getVersionInfo()

Returns foobar2000 and plugin version information, architecture, portable-mode state, and profile path.

```javascript
const ver = await fb.config.getVersionInfo();
if (ver.success === false) throw new Error(ver.error);
console.log(ver.versionFull, ver.is64bit ? 'x64' : 'x86');
```

### getComponents()

Resolves with `{ components, count }`, the loaded components as `ComponentInfo[]`, or with a failure envelope. Each entry includes `name`, `version`, and optional `filename` / `fileName` aliases.

```javascript
const res = await fb.config.getComponents();
if (res.success === false) throw new Error(res.error);
console.log(res.components.length);
```

## Output devices

### getOutputDevices()

Resolves with `{ devices, count }`; each device carries `name`, `outputId`, `deviceId`, and `isCurrent`.

### getOutputConfig()

Reports the active output configuration as `OutputConfig`: `outputId` and `deviceId` (GUID strings), `bufferLength` in seconds, `bitDepth` in bits, `useDither`, `useFades`, and optional `outputName` and `deviceName`. `outputName` is omitted when no installed module matches `outputId`; `deviceName` is omitted when the module does not provide a device name.

```javascript
const out = await fb.config.getOutputConfig();
if (out.success === false) throw new Error(out.error);
console.log(out.deviceName ?? out.deviceId, `${out.bufferLength * 1000} ms`);
```

### setOutputDevice(outputId, deviceId)

Selects an output driver/device pair.

### setOutputBuffer(ms)

Sets the output buffer. A numeric argument is interpreted as milliseconds. The object form accepts `milliseconds` or native `bufferLength` seconds; if both are present, the host prefers `milliseconds`. The resolved value must be in the range `0.05..2.0` seconds.

```javascript
const res = await fb.config.getOutputDevices();
if (res.success === false) throw new Error(res.error);
const current = res.devices.find(d => d.isCurrent);
const first = res.devices[0];
if (first) await fb.config.setOutputDevice(first.outputId, first.deviceId);
await fb.config.setOutputBuffer(1000); // milliseconds
await fb.config.setOutputBuffer({ bufferLength: 0.5 }); // seconds
```

## Advanced configuration

### getAdvancedConfig()

Resolves with `{ entries, count }`, the top-level `AdvancedConfigItem[]`; branch entries expose nested `children`.

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

Lists all preferences pages and branches in foobar2000. Resolves with `{ pages, count }`, where `pages` is a `PreferencesPage[]` array listing all regular pages first, followed by branches. Each entry includes `name`, `guid`, `parentGuid` (GUID of the parent page or branch), and `sortPriority` (lower numbers sort earlier; `0` sorts by name); branch entries include `isBranch: true`.

### getPreferencesStandardGuids()

Reports the GUIDs of standard foobar2000 preferences pages (17 predefined keys, such as `root`, `core`, `display`, `playback`, `output`, `mediaLibrary`, `advanced`, `tools`, `dsp`, and `keyboardShortcuts`). A top-level page matches one of these as its `parentGuid`.

```javascript
const [pagesRes, std] = await Promise.all([
  fb.config.getPreferencesPages(),
  fb.config.getPreferencesStandardGuids(),
]);
if (pagesRes.success === false || std.success === false) throw new Error('preferences unavailable');
const displayPages = pagesRes.pages.filter((p) => p.parentGuid === std.display);
```

## Media-library configuration

### getLibraryStatus()

Reports the media library's state as `LibraryStatus`: `enabled` (at least one library folder is configured), `initialized` (loading has finished; `true` when the host cannot tell) and `itemCount`. The count walks the whole library on every call, so avoid calling it often on a large library.

### getLibraryFilePatterns()

Reports folder organization rules for newly converted, copied, or moved tracks. Resolves with optional `tracks` and `images` rules, each structured as `{ directory, format }`: `directory` is the destination folder, and `format` is the title formatting pattern for subdirectories and filenames. Unconfigured rules are omitted; when neither is configured, the result contains only `success: true`.

### showLibraryPreferences()

Opens foobar2000's Media Library preferences page.

```javascript
const status = await fb.config.getLibraryStatus();
if (status.success === false) throw new Error(status.error);
if (!status.enabled) {
  await fb.config.showLibraryPreferences();
} else {
  const patterns = await fb.config.getLibraryFilePatterns();
  if (patterns.success && patterns.tracks) console.log(patterns.tracks.directory);
}
```

## DSP presets

### getDspPresets()

Resolves with `{ presets, count }`, the DSP presets as `{ index, name }[]`.

### getActiveDspPreset()

Reports the selected DSP preset as `{ index, name, isActive }`. When none is selected or presets are unavailable, `index` and `name` are `null` and `isActive` is `false`.

### setActiveDspPreset(index)

Selects a DSP preset by index, replacing the active DSP chain with the preset's chain. Selecting a preset cannot be undone to restore an unselected state.

```javascript
const res = await fb.config.getDspPresets();
if (res.success === false) throw new Error(res.error);
console.log(res.presets.map(p => p.name));
await fb.config.setActiveDspPreset(0);

const active = await fb.config.getActiveDspPreset();
if (active.success && active.isActive) console.log(active.name);
```

### getCursorFollowPlayback() / setCursorFollowPlayback(enabled)

Reads or sets foobar2000's "cursor follows playback" setting. The getter resolves with `{ enabled, value }` (`value` is the legacy alias of `enabled`); the setter resolves with the assigned `enabled`. Setting changes broadcast `playback:cursorFollowChanged` with `{ enabled }`.

### getPlaybackFollowCursor() / setPlaybackFollowCursor(enabled)

Reads or sets foobar2000's "playback follows cursor" setting, using the same return format. Setting changes broadcast `playback:followCursorChanged` with `{ enabled }`.

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
if (r.success === false) throw new Error(r.error);
console.log(r.mode); // 0
await fb.config.setReplaygainMode(2); // Album mode

const cursor = await fb.config.getCursorFollowPlayback();
if (cursor.success && !cursor.enabled) await fb.config.setCursorFollowPlayback(true);
const follow = await fb.config.getPlaybackFollowCursor();
if (follow.success && follow.enabled) await fb.config.setPlaybackFollowCursor(false);
```


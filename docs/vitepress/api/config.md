# Config API

Methods of the `config` namespace.

## config

### config.export

<!-- api-schema:begin config.export -->
Read the whole store both as a map and as one JSON text.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `data` | `Record<string, any>` | Every stored key with its value; the map `config.getAll` reports. |
| `json` | `string` | `data` serialized as one compact JSON text. |
| `count` | `integer` | Number of keys in the store. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('config.export');
```

### config.get

<!-- api-schema:begin config.get -->
Read the value stored under a key. A key that is not there is not an error: `found` is `false` and `value` is `default`, or `null` when no default was given.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `key` | `string` | Yes | Key to read. Must not be empty. |
| `default` | `any` | No | Value to answer with when the key is absent. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `key` | `string` | The key asked for. |
| `value` | `any` | The stored value; when the key is absent, `default` if it was given, otherwise `null`. |
| `found` | `boolean` | Whether the key is in the store. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('config.get', { key: 'theme' });
if (res.success === false) throw new Error(res.error);
const { value, found } = res;
```

### config.getActiveDspPreset

<!-- api-schema:begin config.getActiveDspPreset -->
Report which DSP preset is selected. `index` and `name` are `null` and `isActive` is `false` when none is selected or presets are unavailable.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `index` | `integer \| null` | Position of the selected preset; `null` when none is selected. |
| `name` | `string \| null` | Name of the selected preset; `null` when none is selected. |
| `isActive` | `boolean` | Whether a preset is selected. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('config.getActiveDspPreset');
```

### config.getAdvancedConfig

<!-- api-schema:begin config.getAdvancedConfig -->
List foobar2000's Advanced preferences as a tree, from the root or from below `parentGuid`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `parentGuid` | `string` | No | GUID of the branch to list, rendered as `{...}`; the root of the tree when omitted or empty. A well-formed GUID that names no branch lists nothing. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `entries` | `AdvancedConfigItem[]` | Entries directly below the requested branch; each branch carries its own entries in `children`. |
| `entries[].name` | `string` | Display name. |
| `entries[].guid` | `string` | Entry GUID rendered as `{...}`, as `config.getAdvancedConfigValue`, `config.setAdvancedConfigValue` and `config.resetAdvancedConfig` take it. |
| `entries[].sortPriority` | `number` | Sort priority the entry reports. |
| `entries[].type` | `"branch" \| "checkbox" \| "radio" \| "integer" \| "string" \| "unknown"` | Kind of an Advanced preferences entry: `branch` holds other entries, `checkbox` and `radio` hold a boolean, `integer` and `string` hold text, `unknown` is any other kind. |
| `entries[].value` | `any` | Current value: a boolean on `checkbox` and `radio`; the stored text on `integer` and `string`, so an integer entry reads as its decimal text here while `config.getAdvancedConfigValue` reports a number; absent on `branch` and `unknown`. |
| `entries[].defaultValue` | `any` | Default value in the same form as `value`; present only when the entry reports one. |
| `entries[].isSigned` | `boolean` | Whether the entry is flagged as a signed integer; present on `integer` and `string`. |
| `entries[].isFilePath` | `boolean` | Whether the entry is flagged as holding a file path; present on `integer` and `string`. |
| `entries[].isFolderPath` | `boolean` | Whether the entry is flagged as holding a folder path; present on `integer` and `string`. |
| `entries[].children` | `AdvancedConfigItem[]` | The entries inside a branch; present on `branch` only. The tree stops eleven levels below the requested parent: a branch on that level is listed with an empty `children`. |
| `count` | `integer` | Number of entries in `entries`; nested entries are not counted. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const entries = await fb2k.invoke('config.getAdvancedConfig');
```

### config.getAdvancedConfigValue

<!-- api-schema:begin config.getAdvancedConfigValue -->
Read one Advanced preferences entry by GUID. An integer entry's value comes back as a number here, while `config.getAdvancedConfig` lists it as its decimal text.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `guid` | `string` | Yes | Entry GUID rendered as `{...}`. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `name` | `string` | Display name of the entry. |
| `guid` | `string` | The GUID as it was sent; its letter case is not normalized. |
| `type` | `"branch" \| "checkbox" \| "radio" \| "integer" \| "string" \| "unknown"` | Kind of an Advanced preferences entry: `branch` holds other entries, `checkbox` and `radio` hold a boolean, `integer` and `string` hold text, `unknown` is any other kind. |
| `value` | `any` | Current value: a boolean on `checkbox` and `radio`, a number on `integer` (`0` when the stored text does not start with a number), a string on `string`, `null` on `branch` and `unknown`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const entry = await fb2k.invoke('config.getAdvancedConfigValue', { guid: '{some-guid}' });
```

### config.getAll

<!-- api-schema:begin config.getAll -->
Read the whole store: every key with its value.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `items` | `Record<string, any>` | Every stored key with its value. |
| `configs` | `Record<string, any>` | The same map as `items`, under its older name. |
| `count` | `integer` | Number of keys in the store. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('config.getAll');
```

### config.getComponents

<!-- api-schema:begin config.getComponents -->
List the installed components with their versions.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `components` | `ConfigComponentInfo[]` | Every installed component, in enumeration order. |
| `components[].name` | `string` | Name the component reports. |
| `components[].version` | `string` | Version string the component reports. |
| `components[].filename` | `string` | File name the component reports for its module; absent when it reports none. |
| `components[].fileName` | `string` | The same as `filename`. |
| `count` | `integer` | Number of entries in `components`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('config.getComponents');
```

### config.getCursorFollowPlayback

<!-- api-schema:begin config.getCursorFollowPlayback -->
Report foobar2000's "cursor follows playback" setting.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `enabled` | `boolean` | Whether the setting is on. |
| `value` | `boolean` | The same as `enabled`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('config.getCursorFollowPlayback');
```

### config.getDspPresets

<!-- api-schema:begin config.getDspPresets -->
List the DSP presets stored in the profile. A host without DSP preset support reports an empty list.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `presets` | `ConfigDspPreset[]` | Every stored preset, in list order. |
| `presets[].index` | `integer` | Position in the preset list, as `config.setActiveDspPreset` takes it. |
| `presets[].name` | `string` | Preset name. |
| `count` | `integer` | Number of entries in `presets`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('config.getDspPresets');
```

### config.getLibraryFilePatterns

<!-- api-schema:begin config.getLibraryFilePatterns -->
Report where foobar2000 puts newly encoded, copied or moved tracks and album images. A pattern that is not configured is absent, so with neither configured the response is `{ success: true }` alone.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `tracks` | `ConfigLibraryFilePattern` | Pattern for newly encoded, copied or moved tracks; absent when not configured. |
| `tracks.directory` | `string` | Target folder as configured. |
| `tracks.format` | `string` | Title formatting pattern for the subfolders and the file name below that folder. |
| `images` | `ConfigLibraryFilePattern` | Pattern for newly encoded, copied or moved album images; absent when not configured. |
| `images.directory` | `string` | Target folder as configured. |
| `images.format` | `string` | Title formatting pattern for the subfolders and the file name below that folder. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('config.getLibraryFilePatterns');
```

### config.getLibraryStatus

<!-- api-schema:begin config.getLibraryStatus -->
Report whether the media library is enabled and loaded, and how many items it holds. The count walks the whole library on every call.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `enabled` | `boolean` | Whether the media library is enabled, which foobar2000 takes to mean that at least one library folder is configured. |
| `itemCount` | `integer` | Number of items in the media library, counted on every call. |
| `initialized` | `boolean` | Whether the media library has finished loading; `true` on a host that cannot tell. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('config.getLibraryStatus');
```

### config.getOutputConfig

<!-- api-schema:begin config.getOutputConfig -->
Report the output settings in effect: module, device, buffer length, bit depth, dithering and fades.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `outputId` | `string` | GUID of the output module in effect, rendered as `{...}`. |
| `deviceId` | `string` | GUID of the device in effect, rendered as `{...}`. |
| `bufferLength` | `number` | Output buffer length in seconds. |
| `bitDepth` | `integer` | Output bit depth setting, in bits. |
| `useDither` | `boolean` | Whether dithering is on. |
| `useFades` | `boolean` | Whether fades are on. |
| `outputName` | `string` | Display name of the output module; absent when no installed module has that GUID. |
| `deviceName` | `string` | Display name of the device; absent when the module does not name it. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('config.getOutputConfig');
```

### config.getOutputDevices

<!-- api-schema:begin config.getOutputDevices -->
List the devices of every output module and flag the one in effect. These are the devices `output.getDevices` lists, in a flat shape that pairs each with the GUID of its module.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `devices` | `ConfigOutputDevice[]` | Devices of all output modules. |
| `devices[].name` | `string` | Display name: the full name the output manager reports, or `<module>: <device>` when the device list comes from the output modules themselves. |
| `devices[].id` | `string` | The same as `deviceId`. |
| `devices[].outputId` | `string` | GUID of the output module, rendered as `{...}`. |
| `devices[].deviceId` | `string` | Device GUID rendered as `{...}`. Not unique on its own: a module reports its default device with the all-zero GUID, so key devices by `(outputId, deviceId)`. |
| `devices[].isCurrent` | `boolean` | Whether this module and device are the ones in effect, as `config.getOutputConfig` reports them. |
| `count` | `integer` | Number of entries in `devices`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('config.getOutputDevices');
```

### config.getPlaybackFollowCursor

<!-- api-schema:begin config.getPlaybackFollowCursor -->
Report foobar2000's "playback follows cursor" setting.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `enabled` | `boolean` | Whether the setting is on. |
| `value` | `boolean` | The same as `enabled`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('config.getPlaybackFollowCursor');
```

### config.getPreferencesPages

<!-- api-schema:begin config.getPreferencesPages -->
List every registered preferences page and preferences branch: the pages first, then the branches.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `pages` | `ConfigPreferencesPage[]` | Every page, then every branch. |
| `pages[].name` | `string` | Display name. |
| `pages[].guid` | `string` | GUID of the page or branch, rendered as `{...}`. |
| `pages[].parentGuid` | `string` | GUID of the page or branch it sits under, rendered as `{...}`; a standard parent from `config.getPreferencesStandardGuids` for a top-level page. |
| `pages[].sortPriority` | `number` | Sort priority it reports: lower sorts first and `0` sorts by name; `0` as well when it reports none. |
| `pages[].isBranch` | `boolean` | `true` on a branch; absent on a page. |
| `count` | `integer` | Number of entries in `pages`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('config.getPreferencesPages');
```

### config.getPreferencesStandardGuids

<!-- api-schema:begin config.getPreferencesStandardGuids -->
Report the GUIDs of foobar2000's standard preferences parents, which registered pages and branches name as their parent.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `root` | `string` | `preferences_page::guid_root`, rendered as `{...}`: the root of the preferences tree. |
| `hidden` | `string` | `preferences_page::guid_hidden`, rendered as `{...}`. |
| `tools` | `string` | `preferences_page::guid_tools`, rendered as `{...}`. |
| `core` | `string` | `preferences_page::guid_core`, rendered as `{...}`. |
| `display` | `string` | `preferences_page::guid_display`, rendered as `{...}`. |
| `playback` | `string` | `preferences_page::guid_playback`, rendered as `{...}`. |
| `visualisations` | `string` | `preferences_page::guid_visualisations`, rendered as `{...}`. |
| `input` | `string` | `preferences_page::guid_input`, rendered as `{...}`. |
| `tagWriting` | `string` | `preferences_page::guid_tag_writing`, rendered as `{...}`. |
| `mediaLibrary` | `string` | `preferences_page::guid_media_library`, rendered as `{...}`. |
| `tagging` | `string` | `preferences_page::guid_tagging`, rendered as `{...}`. |
| `output` | `string` | `preferences_page::guid_output`, rendered as `{...}`. |
| `advanced` | `string` | `preferences_page::guid_advanced`, rendered as `{...}`. |
| `components` | `string` | `preferences_page::guid_components`, rendered as `{...}`. |
| `dsp` | `string` | `preferences_page::guid_dsp`, rendered as `{...}`. |
| `shell` | `string` | `preferences_page::guid_shell`, rendered as `{...}`. |
| `keyboardShortcuts` | `string` | `preferences_page::guid_keyboard_shortcuts`, rendered as `{...}`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('config.getPreferencesStandardGuids');
```

### config.getReplaygainMode

<!-- api-schema:begin config.getReplaygainMode -->
Report the ReplayGain source mode as a number. `replaygain.getMode` reports the same setting by name.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `mode` | `integer` | Source mode: `0` none, `1` track, `2` album, `3` by playback order. |
| `value` | `integer` | The same as `mode`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('config.getReplaygainMode');
```

### config.getVersionInfo

<!-- api-schema:begin config.getVersionInfo -->
Report the foobar2000 version, this component's version and the profile folder.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `version` | `string` | foobar2000's version string as the core reports it; it includes the product name. |
| `foobar2000` | `string` | The same as `version`. |
| `versionFull` | `string` | What `core_version_info_v2::get_name()` reports, which the foobar2000 SDK documents as the product name `foobar2000`; despite the key, not a longer form of `version`. |
| `is64bit` | `boolean` | Whether this is a 64-bit build. |
| `isPortable` | `boolean` | Whether foobar2000 runs as a portable installation. |
| `plugin` | `object` | This component. |
| `plugin.name` | `string` | Always `foo_ui_webview2`. |
| `plugin.version` | `string` | This component's version, `major.minor.patch`. |
| `profilePath` | `string` | The profile folder as a display path. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('config.getVersionInfo');
```

### config.remove

<!-- api-schema:begin config.remove -->
Delete a key from the store. Deleting a key that is not there succeeds with `existed: false`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `key` | `string` | Yes | Key to delete. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `key` | `string` | The key asked for. |
| `existed` | `boolean` | Whether the key was in the store before the call. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('config.remove', { key: 'theme' });
```

### config.resetAdvancedConfig

<!-- api-schema:begin config.resetAdvancedConfig -->
Return one Advanced preferences entry to its default value.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `guid` | `string` | Yes | Entry GUID rendered as `{...}`. Must not be empty. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('config.resetAdvancedConfig', { guid: '{some-guid}' });
```

### config.set

<!-- api-schema:begin config.set -->
Store a value under a key in this component's own key-value store, replacing any value the key held. The store is kept in foobar2000's configuration store, so it survives a restart. Fails with `OPERATION_FAILED` when the store cannot be read or the value cannot be saved.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `key` | `string` | Yes | Key to store under. Every page of this component shares one store, so give your keys a prefix of your own. Must not be empty. |
| `value` | `any` | Yes | Value to store, any JSON value. A top-level `null` counts as missing and is refused, so clear a key with `config.remove`; a `null` inside an object or an array is kept. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `key` | `string` | The key written. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('config.set', { key: 'theme', value: { mode: 'dark' } });
```

### config.setActiveDspPreset

<!-- api-schema:begin config.setActiveDspPreset -->
Select a DSP preset by index, which replaces the whole active DSP chain with the preset's; the same operation as `dsp.applyPreset` by index. Nothing selects "no preset" again.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `index` | `integer` | Yes | Position of the preset in `config.getDspPresets`. At least `0`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('config.setActiveDspPreset', { index: 0 });
```

### config.setAdvancedConfigValue

<!-- api-schema:begin config.setAdvancedConfigValue -->
Write one Advanced preferences entry. The value has to suit the entry: a boolean for a `checkbox` or `radio` entry; a string or a number for an `integer` or `string` entry, where a number is stored as its decimal text (an `integer` entry drops the fraction, a `string` entry keeps six decimals). A `branch` or an entry of another kind cannot be written.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `guid` | `string` | Yes | Entry GUID rendered as `{...}`. Must not be empty. |
| `value` | `any` | Yes | Value to write; what each kind of entry accepts is in the method description. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('config.setAdvancedConfigValue', { guid: '{some-guid}', value: true });
```

### config.setCursorFollowPlayback

<!-- api-schema:begin config.setCursorFollowPlayback -->
Turn foobar2000's "cursor follows playback" setting on or off.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `enabled` | `boolean` | Yes | `true` turns the setting on, `false` turns it off. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `enabled` | `boolean` | The value written. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('config.setCursorFollowPlayback', { enabled: true });
```

### config.setOutputBuffer

<!-- api-schema:begin config.setOutputBuffer -->
Set the output buffer length, in seconds (`bufferLength`) or in milliseconds (`milliseconds`). One of the two is required; `milliseconds` wins when both are given, and `bufferLength` is range-checked even then.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `bufferLength` | `number` | No | Buffer length in seconds. Ignored when `milliseconds` is given, but range-checked all the same. Between `0.05` and `2` inclusive. |
| `milliseconds` | `number` | No | Buffer length in milliseconds; takes precedence over `bufferLength`. Between `50` and `2000` inclusive. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
// milliseconds is converted to seconds; the effective range is 0.05-2.0 seconds
await fb2k.invoke('config.setOutputBuffer', { milliseconds: 1000 });
```

### config.setOutputDevice

<!-- api-schema:begin config.setOutputDevice -->
Switch output to a device of an output module; take both GUIDs from `config.getOutputDevices`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `outputId` | `string` | Yes | GUID of the output module, as `config.getOutputDevices` reports it in `outputId`. Must not be empty. |
| `deviceId` | `string` | Yes | GUID of the device, as `config.getOutputDevices` reports it in `deviceId`. Must not be empty. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('config.setOutputDevice', { outputId: '{output-guid}', deviceId: '{device-guid}' });
```

### config.setPlaybackFollowCursor

<!-- api-schema:begin config.setPlaybackFollowCursor -->
Turn foobar2000's "playback follows cursor" setting on or off.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `enabled` | `boolean` | Yes | `true` turns the setting on, `false` turns it off. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `enabled` | `boolean` | The value written. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('config.setPlaybackFollowCursor', { enabled: true });
```

### config.setReplaygainMode

<!-- api-schema:begin config.setReplaygainMode -->
Set the ReplayGain source mode by number (`mode`) or by name (`sourceMode`). One of the two is required; `mode` wins when both are given. `replaygain.setMode` changes the same setting by name.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `mode` | `integer` | No | Source mode by number, as `config.getReplaygainMode` reports it; takes precedence over `sourceMode`. Between `0` and `3` inclusive. |
| `sourceMode` | `"none" \| "track" \| "album" \| "byPlaybackOrder" \| "auto"` | No | Source mode by name: `none` is `0`, `track` `1`, `album` `2`, and `byPlaybackOrder` or its other spelling `auto` is `3`. Read only when `mode` is absent. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `mode` | `integer` | The source mode now in effect, as a number. |
| `value` | `integer` | The same as `mode`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('config.setReplaygainMode', { sourceMode: 'album' });
```

### config.showLibraryPreferences

<!-- api-schema:begin config.showLibraryPreferences -->
Open foobar2000's Media Library preferences page.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('config.showLibraryPreferences');
```

## Storage and preference semantics

`config.set`, `config.get`, `config.remove`, `config.getAll`, and
`config.export` operate on the component's persistent configuration object.
`config.get` requires `key`; when the key is absent it returns `found: false`
and uses the optional `default` value when supplied. `config.set` requires both
`key` and `value`; the value can be any JSON value.

Output and advanced-preference methods use foobar2000 services. In particular,
`config.setOutputDevice` requires valid `outputId` and `deviceId` GUIDs, while
`config.setOutputBuffer` accepts either seconds in `bufferLength` or
milliseconds in `milliseconds`. Advanced entries require a valid `guid`; their
accepted `value` type depends on the entry type rather than a single universal
schema.

The cursor-follow and ReplayGain setters accept their documented compatibility
forms. For ReplayGain, `mode` and `value` are numeric forms, while
`sourceMode` accepts `track`, `album`, `auto`, `byPlaybackOrder`, or `none`.
The handler returns `INVALID_PARAMS` for an unknown string source mode.

# Config API

English API reference for the `config` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## config

### config.export


_No parameters._

**Returns**: `{"count":0,"data":"...","json":"...","success":true}`

```js
const result = await fb2k.invoke('config.export');
```

### config.get


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `default` | `json` | No | Optional; omitted by default. |
| `key` | `string` | Yes | Configuration key to read; a missing or empty value returns `key is required`. |

**Returns**: `{"error":"...","found":"...","key":"...","success":true,"value":"..."}`

```js
const { value, found } = await fb2k.invoke('config.get', { key: 'theme' });
```

### config.getActiveDspPreset


_No parameters._

**Returns**: `{"index":0,"isActive":true,"name":"..."}`

```js
const result = await fb2k.invoke('config.getActiveDspPreset');
```

### config.getAdvancedConfig


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `parentGuid` | `string` | No | Optional; defaults to the advanced-preferences root branch. |

**Returns**: JSON object from the runtime handler.

```js
const entries = await fb2k.invoke('config.getAdvancedConfig');
```

### config.getAdvancedConfigValue


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `guid` | `string` | Yes | Advanced-preferences entry GUID; a missing or empty value fails with `guid is required`. |

**Returns**: `{"guid":"...","name":"...","type":"...","value":"..."}`

```js
const entry = await fb2k.invoke('config.getAdvancedConfigValue', { guid: '{some-guid}' });
```

### config.getAll


_No parameters._

**Returns**: `{"configs":"...","count":"...","items":"...","success":true}`

```js
const result = await fb2k.invoke('config.getAll');
```

### config.getComponents


_No parameters._

**Returns**: JSON object from the runtime handler.

```js
const result = await fb2k.invoke('config.getComponents');
```

### config.getCursorFollowPlayback


_No parameters._

**Returns**: `{"enabled":"...","value":"..."}`

```js
const result = await fb2k.invoke('config.getCursorFollowPlayback');
```

### config.getDspPresets


_No parameters._

**Returns**: JSON object from the runtime handler.

```js
const result = await fb2k.invoke('config.getDspPresets');
```

### config.getLibraryFilePatterns


_No parameters._

**Returns**: `{"images":[],"tracks":[]}`

```js
const result = await fb2k.invoke('config.getLibraryFilePatterns');
```

### config.getLibraryStatus


_No parameters._

**Returns**: `{"enabled":true,"initialized":"...","itemCount":"..."}`

```js
const result = await fb2k.invoke('config.getLibraryStatus');
```

### config.getOutputConfig


_No parameters._

**Returns**: `{"bitDepth":"...","bufferLength":"...","deviceId":"...","deviceName":"...","outputId":"...","outputName":"...","useDither":"...","useFades":"..."}`

```js
const result = await fb2k.invoke('config.getOutputConfig');
```

### config.getOutputDevices


_No parameters._

**Returns**: JSON object from the runtime handler.

```js
const result = await fb2k.invoke('config.getOutputDevices');
```

### config.getPlaybackFollowCursor


_No parameters._

**Returns**: `{"enabled":"...","value":"..."}`

```js
const result = await fb2k.invoke('config.getPlaybackFollowCursor');
```

### config.getPreferencesPages


_No parameters._

**Returns**: JSON object from the runtime handler.

```js
const result = await fb2k.invoke('config.getPreferencesPages');
```

### config.getPreferencesStandardGuids


_No parameters._

**Returns**: `{"advanced":"...","components":"...","core":"...","display":"...","dsp":"...","hidden":"...","input":"...","keyboardShortcuts":"...","mediaLibrary":"...","output":"...","playback":"...","root":"...","shell":"...","tagWriting":"...","tagging":"...","tools":"...","visualisations":"..."}`

```js
const result = await fb2k.invoke('config.getPreferencesStandardGuids');
```

### config.getReplaygainMode


_No parameters._

**Returns**: `{"mode":"...","value":"..."}`

```js
const result = await fb2k.invoke('config.getReplaygainMode');
```

### config.getVersionInfo


_No parameters._

**Returns**: `{"foobar2000":"...","is64bit":true,"isPortable":true,"plugin":"...","profilePath":"...","version":"...","versionFull":"..."}`

```js
const result = await fb2k.invoke('config.getVersionInfo');
```

### config.remove


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `key` | `string` | Yes | Configuration key to delete; a missing or empty value returns `key is required`. |

**Returns**: `{"error":"...","existed":"...","key":"...","success":true}`

```js
await fb2k.invoke('config.remove', { key: 'theme' });
```

### config.resetAdvancedConfig


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `guid` | `string` | Yes | Advanced-preferences entry GUID; a missing or empty value fails with `guid is required`. |

**Returns**: `{"success":true}`

```js
await fb2k.invoke('config.resetAdvancedConfig', { guid: '{some-guid}' });
```

### config.set


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `key` | `string` | Yes | Configuration key to write; a missing or empty value returns `key is required`. |
| `value` | `json` | Yes | Required. |

**Returns**: `{"error":"...","key":"...","success":true}`

```js
await fb2k.invoke('config.set', { key: 'theme', value: { mode: 'dark' } });
```

### config.setActiveDspPreset


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `index` | `integer` | Yes | Required. |

**Returns**: `{"success":true}`

```js
await fb2k.invoke('config.setActiveDspPreset', { index: 0 });
```

### config.setAdvancedConfigValue


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `guid` | `string` | Yes | Advanced-preferences entry GUID; a missing or empty value fails with `guid is required`. |
| `value` | `boolean` | Yes | Required. |

**Returns**: `{"success":true}`

```js
await fb2k.invoke('config.setAdvancedConfigValue', { guid: '{some-guid}', value: true });
```

### config.setCursorFollowPlayback


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `enabled` | `boolean` | No | Optional; default false. |
| `value` | `boolean` | No | Optional; default false. |

**Returns**: `{"enabled":"...","success":true}`

```js
await fb2k.invoke('config.setCursorFollowPlayback', { enabled: true });
```

### config.setOutputBuffer


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `bufferLength` | `number` | No | Optional; default 0. |
| `milliseconds` | `number` | No | Optional; default 0. |

**Returns**: `{"success":true}`

```js
// milliseconds is converted to seconds; the effective range is 0.05-2.0 seconds
await fb2k.invoke('config.setOutputBuffer', { milliseconds: 1000 });
```

### config.setOutputDevice


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `deviceId` | `string` | Yes | Device GUID; a missing or empty value fails with `outputId and deviceId are required`. |
| `outputId` | `string` | Yes | Output-driver GUID; a missing or empty value fails with `outputId and deviceId are required`. |

**Returns**: `{"success":true}`

```js
await fb2k.invoke('config.setOutputDevice', { outputId: '{output-guid}', deviceId: '{device-guid}' });
```

### config.setPlaybackFollowCursor


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `enabled` | `boolean` | No | Optional; default false. |
| `value` | `boolean` | No | Optional; default false. |

**Returns**: `{"enabled":"...","success":true}`

```js
await fb2k.invoke('config.setPlaybackFollowCursor', { enabled: true });
```

### config.setReplaygainMode


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `mode` | `integer` | No | Optional; default -1. |
| `sourceMode` | `string` | No | Optional; omitted by default. |
| `value` | `integer` | No | Optional; default -1. |

**Returns**: `{"code":"...","error":"...","mode":"...","success":true,"value":"..."}`

```js
await fb2k.invoke('config.setReplaygainMode', { sourceMode: 'album' });
```

### config.showLibraryPreferences


_No parameters._

**Returns**: `{"success":true}`

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

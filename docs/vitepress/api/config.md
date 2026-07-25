# Config API

English API reference for the `config` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## config

### config.export

Public API method. Runtime authority: `src/api/ConfigApi.cpp:923`.

_No parameters._

**Returns**: `{"count":0,"data":"...","json":"...","success":true}`

```js
const result = await fb2k.invoke('config.export');
```

### config.get

Public API method. Runtime authority: `src/api/ConfigApi.cpp:920`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `default` | `json` | No | Optional; default omitted. |
| `key` | `string` | No | Optional; default . |

**Returns**: `{"error":"...","found":"...","key":"...","success":true,"value":"..."}`

```js
const result = await fb2k.invoke('config.get', { default: /* value */, key: /* value */ });
```

### config.getActiveDspPreset

Public API method. Runtime authority: `src/api/ConfigApi.cpp:950`.

_No parameters._

**Returns**: `{"index":0,"isActive":true,"name":"..."}`

```js
const result = await fb2k.invoke('config.getActiveDspPreset');
```

### config.getAdvancedConfig

Public API method. Runtime authority: `src/api/ConfigApi.cpp:932`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `parentGuid` | `string` | No | Optional; default . |

**Returns**: JSON object from the runtime handler.

```js
const result = await fb2k.invoke('config.getAdvancedConfig', { parentGuid: /* value */ });
```

### config.getAdvancedConfigValue

Public API method. Runtime authority: `src/api/ConfigApi.cpp:933`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `guid` | `string` | No | Optional; default . |

**Returns**: `{"guid":"...","name":"...","type":"...","value":"..."}`

```js
const result = await fb2k.invoke('config.getAdvancedConfigValue', { guid: /* value */ });
```

### config.getAll

Public API method. Runtime authority: `src/api/ConfigApi.cpp:922`.

_No parameters._

**Returns**: `{"configs":"...","count":"...","items":"...","success":true}`

```js
const result = await fb2k.invoke('config.getAll');
```

### config.getComponents

Public API method. Runtime authority: `src/api/ConfigApi.cpp:940`.

_No parameters._

**Returns**: JSON object from the runtime handler.

```js
const result = await fb2k.invoke('config.getComponents');
```

### config.getCursorFollowPlayback

Public API method. Runtime authority: `src/api/ConfigApi.cpp:954`.

_No parameters._

**Returns**: `{"enabled":"...","value":"..."}`

```js
const result = await fb2k.invoke('config.getCursorFollowPlayback');
```

### config.getDspPresets

Public API method. Runtime authority: `src/api/ConfigApi.cpp:949`.

_No parameters._

**Returns**: JSON object from the runtime handler.

```js
const result = await fb2k.invoke('config.getDspPresets');
```

### config.getLibraryFilePatterns

Public API method. Runtime authority: `src/api/ConfigApi.cpp:945`.

_No parameters._

**Returns**: `{"images":[],"tracks":[]}`

```js
const result = await fb2k.invoke('config.getLibraryFilePatterns');
```

### config.getLibraryStatus

Public API method. Runtime authority: `src/api/ConfigApi.cpp:944`.

_No parameters._

**Returns**: `{"enabled":true,"initialized":"...","itemCount":"..."}`

```js
const result = await fb2k.invoke('config.getLibraryStatus');
```

### config.getOutputConfig

Public API method. Runtime authority: `src/api/ConfigApi.cpp:927`.

_No parameters._

**Returns**: `{"bitDepth":"...","bufferLength":"...","deviceId":"...","deviceName":"...","outputId":"...","outputName":"...","useDither":"...","useFades":"..."}`

```js
const result = await fb2k.invoke('config.getOutputConfig');
```

### config.getOutputDevices

Public API method. Runtime authority: `src/api/ConfigApi.cpp:926`.

_No parameters._

**Returns**: JSON object from the runtime handler.

```js
const result = await fb2k.invoke('config.getOutputDevices');
```

### config.getPlaybackFollowCursor

Public API method. Runtime authority: `src/api/ConfigApi.cpp:956`.

_No parameters._

**Returns**: `{"enabled":"...","value":"..."}`

```js
const result = await fb2k.invoke('config.getPlaybackFollowCursor');
```

### config.getPreferencesPages

Public API method. Runtime authority: `src/api/ConfigApi.cpp:938`.

_No parameters._

**Returns**: JSON object from the runtime handler.

```js
const result = await fb2k.invoke('config.getPreferencesPages');
```

### config.getPreferencesStandardGuids

Public API method. Runtime authority: `src/api/ConfigApi.cpp:939`.

_No parameters._

**Returns**: `{"advanced":"...","components":"...","core":"...","display":"...","dsp":"...","hidden":"...","input":"...","keyboardShortcuts":"...","mediaLibrary":"...","output":"...","playback":"...","root":"...","shell":"...","tagWriting":"...","tagging":"...","tools":"...","visualisations":"..."}`

```js
const result = await fb2k.invoke('config.getPreferencesStandardGuids');
```

### config.getReplaygainMode

Public API method. Runtime authority: `src/api/ConfigApi.cpp:960`.

_No parameters._

**Returns**: `{"mode":"...","value":"..."}`

```js
const result = await fb2k.invoke('config.getReplaygainMode');
```

### config.getVersionInfo

Public API method. Runtime authority: `src/api/ConfigApi.cpp:941`.

_No parameters._

**Returns**: `{"foobar2000":"...","is64bit":true,"isPortable":true,"plugin":"...","profilePath":"...","version":"...","versionFull":"..."}`

```js
const result = await fb2k.invoke('config.getVersionInfo');
```

### config.remove

Public API method. Runtime authority: `src/api/ConfigApi.cpp:921`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `key` | `string` | No | Optional; default . |

**Returns**: `{"error":"...","existed":"...","key":"...","success":true}`

```js
const result = await fb2k.invoke('config.remove', { key: /* value */ });
```

### config.resetAdvancedConfig

Public API method. Runtime authority: `src/api/ConfigApi.cpp:935`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `guid` | `string` | No | Optional; default . |

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('config.resetAdvancedConfig', { guid: /* value */ });
```

### config.set

Public API method. Runtime authority: `src/api/ConfigApi.cpp:919`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `key` | `string` | No | Optional; default . |
| `value` | `json` | Yes | Required. |

**Returns**: `{"error":"...","key":"...","success":true}`

```js
const result = await fb2k.invoke('config.set', { key: /* value */, value: /* value */ });
```

### config.setActiveDspPreset

Public API method. Runtime authority: `src/api/ConfigApi.cpp:951`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `index` | `integer` | Yes | Required. |

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('config.setActiveDspPreset', { index: /* value */ });
```

### config.setAdvancedConfigValue

Public API method. Runtime authority: `src/api/ConfigApi.cpp:934`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `guid` | `string` | No | Optional; default . |
| `value` | `boolean` | Yes | Required. |

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('config.setAdvancedConfigValue', { guid: /* value */, value: /* value */ });
```

### config.setCursorFollowPlayback

Public API method. Runtime authority: `src/api/ConfigApi.cpp:955`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `enabled` | `boolean` | No | Optional; default false. |
| `value` | `boolean` | No | Optional; default false. |

**Returns**: `{"enabled":"...","success":true}`

```js
const result = await fb2k.invoke('config.setCursorFollowPlayback', { enabled: /* value */, value: /* value */ });
```

### config.setOutputBuffer

Public API method. Runtime authority: `src/api/ConfigApi.cpp:929`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `bufferLength` | `number` | No | Optional; default 0. |
| `milliseconds` | `number` | No | Optional; default 0. |

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('config.setOutputBuffer', { bufferLength: /* value */, milliseconds: /* value */ });
```

### config.setOutputDevice

Public API method. Runtime authority: `src/api/ConfigApi.cpp:928`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `deviceId` | `string` | No | Optional; default . |
| `outputId` | `string` | No | Optional; default . |

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('config.setOutputDevice', { deviceId: /* value */, outputId: /* value */ });
```

### config.setPlaybackFollowCursor

Public API method. Runtime authority: `src/api/ConfigApi.cpp:957`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `enabled` | `boolean` | No | Optional; default false. |
| `value` | `boolean` | No | Optional; default false. |

**Returns**: `{"enabled":"...","success":true}`

```js
const result = await fb2k.invoke('config.setPlaybackFollowCursor', { enabled: /* value */, value: /* value */ });
```

### config.setReplaygainMode

Public API method. Runtime authority: `src/api/ConfigApi.cpp:961`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `mode` | `integer` | No | Optional; default -1. |
| `sourceMode` | `string` | No | Optional; default omitted. |
| `value` | `integer` | No | Optional; default -1. |

**Returns**: `{"code":"...","error":"...","mode":"...","success":true,"value":"..."}`

```js
const result = await fb2k.invoke('config.setReplaygainMode', { mode: /* value */, sourceMode: /* value */, value: /* value */ });
```

### config.showLibraryPreferences

Public API method. Runtime authority: `src/api/ConfigApi.cpp:946`.

_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('config.showLibraryPreferences');
```

## Contract supplements

The sections below close public-contract findings from the strict parameter audit without replacing existing explanations.

<!-- phase3-supplement:config.set -->
### Contract supplement: `config.set`

Verified contract supplement. Runtime authority: `src/api/ConfigApi.cpp:821-840`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `key` | `string` | No | `` | Optional; default . |
| `value` | `json` | Yes | none | Required. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |
| `key` | `json` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('config.set', { key: /* value */, value: /* value */ });
```
<!-- phase3-supplement:config.setAdvancedConfigValue -->
### Contract supplement: `config.setAdvancedConfigValue`

Verified contract supplement. Runtime authority: `src/api/ConfigApi.cpp:321-387`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `guid` | `string` | No | `` | Optional; default . |
| `value` | `boolean` | Yes | none | Required. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('config.setAdvancedConfigValue', { guid: /* value */, value: /* value */ });
```
<!-- phase3-supplement:config.setCursorFollowPlayback -->
### Contract supplement: `config.setCursorFollowPlayback`

Verified contract supplement. Runtime authority: `src/api/ConfigApi.cpp:689-697`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `enabled` | `boolean` | No | `false` | Optional; default false. |
| `value` | `boolean` | No | `false` | Optional; default false. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `enabled` | `json` | No |
| `success` | `boolean` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('config.setCursorFollowPlayback', { enabled: /* value */, value: /* value */ });
```
<!-- phase3-supplement:config.setOutputBuffer -->
### Contract supplement: `config.setOutputBuffer`

Verified contract supplement. Runtime authority: `src/api/ConfigApi.cpp:138-164`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `bufferLength` | `number` | No | `0` | Optional; default 0. |
| `milliseconds` | `number` | No | `0` | Optional; default 0. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('config.setOutputBuffer', { bufferLength: /* value */, milliseconds: /* value */ });
```
<!-- phase3-supplement:config.setPlaybackFollowCursor -->
### Contract supplement: `config.setPlaybackFollowCursor`

Verified contract supplement. Runtime authority: `src/api/ConfigApi.cpp:702-710`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `enabled` | `boolean` | No | `false` | Optional; default false. |
| `value` | `boolean` | No | `false` | Optional; default false. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `enabled` | `json` | No |
| `success` | `boolean` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('config.setPlaybackFollowCursor', { enabled: /* value */, value: /* value */ });
```
<!-- phase3-supplement:config.setReplaygainMode -->
### Contract supplement: `config.setReplaygainMode`

Verified contract supplement. Runtime authority: `src/api/ConfigApi.cpp:720-744`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `mode` | `integer` | No | `-1` | Optional; default -1. |
| `sourceMode` | `string` | No | omitted | Optional; default omitted. |
| `value` | `integer` | No | `-1` | Optional; default -1. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `code` | `string` | Yes |
| `error` | `string` | Yes |
| `mode` | `json` | No |
| `success` | `boolean` | No |
| `value` | `json` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('config.setReplaygainMode', { mode: /* value */, sourceMode: /* value */, value: /* value */ });
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

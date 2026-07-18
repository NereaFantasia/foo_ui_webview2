# Misc API

English API reference for the `clipboard`, `console`, `log`, `menu`, `misc`, `panel`, `system`, `test` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## clipboard

### clipboard.read

Public API method. Runtime authority: `src/api/ClipboardApi.cpp:319`.

_No parameters._

**Returns**: `{"files":[],"hasFiles":true,"hasImage":true,"hasText":true,"success":true,"text":"..."}`

```js
const result = await fb2k.invoke('clipboard.read');
```

### clipboard.write

Public API method. Runtime authority: `src/api/ClipboardApi.cpp:322`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `text` | `string` | No | Optional; default . |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('clipboard.write', { text: /* value */ });
```

### clipboard.writeFiles

Public API method. Runtime authority: `src/api/ClipboardApi.cpp:328`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `array` | Yes | Required. |

**Returns**: `{"error":"...","fileCount":"...","success":true}`

```js
const result = await fb2k.invoke('clipboard.writeFiles', { paths: /* value */ });
```

### clipboard.writeHTML

Public API method. Runtime authority: `src/api/ClipboardApi.cpp:325`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `html` | `string` | No | Optional; default . |
| `plainText` | `string` | No | Optional; default . |

**Returns**: `{"error":"...","htmlWritten":"...","success":true,"textWritten":"..."}`

```js
const result = await fb2k.invoke('clipboard.writeHTML', { html: /* value */, plainText: /* value */ });
```

## console

### console.error

Public API method. Runtime authority: `src/api/ConsoleApi.cpp:308`.

Provide one of `message` or `args`. Empty payloads fail with `message is required`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `string` | No | Optional log text. Non-string values are serialized. |
| `args` | `array` | No | Optional argument list joined with spaces when `message` is omitted. |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('console.error', { message: 'failed to load artwork' });
```

### console.log

Public API method. Runtime authority: `src/api/ConsoleApi.cpp:302`.

Provide one of `message` or `args`. Empty payloads fail with `message is required`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `string` | No | Optional log text. Non-string values are serialized. |
| `args` | `array` | No | Optional argument list joined with spaces when `message` is omitted. |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('console.log', { message: 'track started' });
```

### console.warn

Public API method. Runtime authority: `src/api/ConsoleApi.cpp:305`.

Provide one of `message` or `args`. Empty payloads fail with `message is required`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `string` | No | Optional log text. Non-string values are serialized. |
| `args` | `array` | No | Optional argument list joined with spaces when `message` is omitted. |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('console.warn', { args: ['retry', 3] });
```

## log

### log.clear

Public API method. Runtime authority: `src/api/ConsoleApi.cpp:317`.

_No parameters._

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('log.clear');
```

### log.read

Public API method. Runtime authority: `src/api/ConsoleApi.cpp:314`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `lines` | `integer` | No | Optional; default 100. |

**Returns**: `{"content":"...","error":"...","lineCount":"...","lines":"...","success":true,"totalLines":"..."}`

```js
const result = await fb2k.invoke('log.read', { lines: /* value */ });
```

### log.write

Public API method. Runtime authority: `src/api/ConsoleApi.cpp:311`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `string` | No | Optional; default omitted. |
| `args` | `array` | No | Optional; default []. |
| `file` | `string` | No | Optional; default omitted. |
| `level` | `string` | No | Optional; default info. |
| `append` | `boolean` | No | Optional; default true. |
| `timestamp` | `boolean` | No | Optional; default true. |

**Returns**: `{"error":"...","path":"...","success":true}`

```js
const result = await fb2k.invoke('log.write', { message: /* value */, args: /* value */, append: /* value */, file: /* value */, level: /* value */, timestamp: /* value */ });
```

## menu

### menu.close

Public API method. Runtime authority: `src/api/MenuApi.cpp:1141-1146`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `reason` | `string` | No | `api` | Optional close reason passed to the overlay host; default `api`. |

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('menu.close', { reason: 'api' });
```

### menu.getContextMenu

Public API method. Runtime authority: `src/api/MenuApi.cpp:1318`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `handles` | `array` | No | Optional; default []. |
| `i18n` | `boolean` | No | Optional; default true. |
| `locale` | `string` | No | Optional; default auto. |
| `mode` | `string` | No | Optional; `auto`, `selection`, `playlist`, `nowPlaying`, or `handles`; default `auto`. |
| `withAvailability` | `boolean` | No | Optional; default true. |

**Returns**: `{"Failed to initialize context menu":"...","error":"...","i18n":"...","items":"...","locale":"...","mode":"...","success":true,"withAvailability":"..."}`

```js
const result = await fb2k.invoke('menu.getContextMenu', { i18n: /* value */, locale: /* value */, mode: /* value */, withAvailability: /* value */, handles: /* value */ });
```

### menu.getMainMenu

Public API method. Runtime authority: `src/api/MenuApi.cpp:1317`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `i18n` | `boolean` | No | Optional; default true. |
| `locale` | `string` | No | Optional; default auto. |
| `root` | `string` | No | Optional; default . |
| `withAvailability` | `boolean` | No | Optional; default true. |

**Returns**: `{"error":"...","fallback":"...","items":[],"success":true}`

```js
const result = await fb2k.invoke('menu.getMainMenu', { i18n: /* value */, locale: /* value */, root: /* value */, withAvailability: /* value */ });
```

### menu.runContextCommand

Public API method. Runtime authority: `src/api/MenuApi.cpp:1316`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `command` | `string` | No | Optional; default . |

**Returns**: `{"error":"...","guid":"...","itemCount":"...","success":true}`

```js
const result = await fb2k.invoke('menu.runContextCommand', { command: /* value */ });
```

### menu.runContextCommandById

Public API method. Runtime authority: `src/api/MenuApi.cpp:1319`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `id` | `integer` | No | Optional; default -1. |
| `mode` | `string` | No | Optional; default auto. |
| `handles` | `array` | No | Optional; default []. |

**Returns**: `{"Failed to initialize context menu":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('menu.runContextCommandById', { id: /* value */, mode: /* value */, handles: /* value */ });
```

### menu.runMainMenuCommand

Public API method. Runtime authority: `src/api/MenuApi.cpp:1315`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `command` | `string` | No | Optional; default . |

**Returns**: `{"error":"...","guid":"...","success":true}`

```js
const result = await fb2k.invoke('menu.runMainMenuCommand', { command: /* value */ });
```

### menu.show

Public API method. Runtime authority: `src/api/MenuApi.cpp:1321`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `items` | `array` | No | Optional; default omitted. |
| `x` | `integer` | No | Optional; default -1. |
| `y` | `integer` | No | Optional; default -1. |

**Returns**: `{"error":"...","menuId":"...","success":true}`

```js
const result = await fb2k.invoke('menu.show', { items: /* value */, x: /* value */, y: /* value */ });
```

### menu.showNativePopup

Public API method. Runtime authority: `src/api/MenuApi.cpp:1320`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `handles` | `array` | No | Optional; default []. |
| `mode` | `string` | No | Optional; `auto`, `selection`, `playlist`, `nowPlaying`, or `handles`; default `auto`. |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('menu.showNativePopup', { handles: /* value */, mode: /* value */ });
```

## misc

### misc.exit

Public API method. Runtime authority: `src/api/MiscApi.cpp:132`.

_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('misc.exit');
```

### misc.getComponentPath

Public API method. Runtime authority: `src/api/MiscApi.cpp:126`.

_No parameters._

**Returns**: `{"path":"...","value":"..."}`

```js
const result = await fb2k.invoke('misc.getComponentPath');
```

### misc.getFoobarPath

Public API method. Runtime authority: `src/api/MiscApi.cpp:124`.

_No parameters._

**Returns**: `{"path":"...","value":"..."}`

```js
const result = await fb2k.invoke('misc.getFoobarPath');
```

### misc.getProfilePath

Public API method. Runtime authority: `src/api/MiscApi.cpp:125`.

_No parameters._

**Returns**: `{"path":"...","value":"..."}`

```js
const result = await fb2k.invoke('misc.getProfilePath');
```

### misc.restart

Public API method. Runtime authority: `src/api/MiscApi.cpp:131`.

_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('misc.restart');
```

### misc.showConsole

Public API method. Runtime authority: `src/api/MiscApi.cpp:127`.

_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('misc.showConsole');
```

### misc.showLibrarySearch

Public API method. Runtime authority: `src/api/MiscApi.cpp:129`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `query` | `string` | No | Optional; default . |

**Returns**: `{"query":"...","success":true}`

```js
const result = await fb2k.invoke('misc.showLibrarySearch', { query: /* value */ });
```

### misc.showPopupMessage

Public API method. Runtime authority: `src/api/MiscApi.cpp:130`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `string` | No | Optional; default . |
| `msg` | `string` | No | Optional; default . |
| `title` | `string` | No | Optional; default Message. |

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('misc.showPopupMessage', { message: /* value */, msg: /* value */, title: /* value */ });
```
## Owner-family behavior and limits

- `clipboard.writeFiles` accepts media-read-authorized paths. `clipboard.writeHTML` writes HTML plus a plain-text fallback; `clipboard.read` reports only formats currently available from the Windows clipboard.
- `console.log`, `console.warn`, `console.error`, and `log.write` require one of `message` or `args`. `log.write.file`, when accepted, is only a leaf `.log` or `.txt` filename under the profile directory; paths, traversal and Windows reserved device names are rejected by the runtime.
- `menu.getContextMenu`, `menu.runContextCommandById`, and `menu.showNativePopup` use `mode` to select `handles`, `nowPlaying`, `selection`, or `playlist` context. `auto` tries those sources in that order, ending with playlist context. In `handles` mode, every path is media-access validated before a handle is created. `menu.showNativePopup` uses screen cursor coordinates and returns before the native menu is displayed.
- `menu.show` opens the self-drawn overlay after resource validation. `menu.close` only closes the active overlay; `menu.__*` endpoints are internal and are not public APIs.
- `misc.showPopupMessage` accepts `message`, falling back to `msg`, and defaults `title` to `"Message"`. The restart and exit methods request the corresponding foobar2000 standard command.
- `panel.setConfig` changes only its documented panel fields. `system.*` reports registered runtime and plugin information; `test.*` is diagnostic surface rather than application behavior.

### misc.showPreferences

Public API method. Runtime authority: `src/api/MiscApi.cpp:128`.

_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('misc.showPreferences');
```

## panel

### panel.getConfig

Public API method. Runtime authority: `src/api/WindowApi.cpp:2470`.

_No parameters._

**Returns**: `{"config":{},"success":true}`

```js
const result = await fb2k.invoke('panel.getConfig');
```

### panel.setConfig

Public API method. Runtime authority: `src/api/WindowApi.cpp:2471`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `enableDragDrop` | `boolean` | No | Optional; default omitted. |
| `grabFocus` | `boolean` | No | Optional; default omitted. |
| `panelName` | `string` | No | Optional; default omitted. |
| `transparentBackground` | `boolean` | No | Optional; default omitted. |

**Returns**: `{"changed":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('panel.setConfig', { enableDragDrop: /* value */, grabFocus: /* value */, panelName: /* value */, transparentBackground: /* value */ });
```

## system

### system.getApiStats

Public API method. Runtime authority: `src/api/PluginRegistry.cpp:490`.

_No parameters._

**Returns**: `{"registered":"...","success":true}`

```js
const result = await fb2k.invoke('system.getApiStats');
```

### system.getApisByNamespace

Public API method. Runtime authority: `src/api/PluginRegistry.cpp:454`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `namespace` | `string` | No | Optional; default . |

**Returns**: JSON object from the runtime handler.

```js
const result = await fb2k.invoke('system.getApisByNamespace', { namespace: /* value */ });
```

### system.getDPI

Public API method. Runtime authority: `src/api/WindowApi.cpp:2426`.

_No parameters._

**Returns**: `{"dpi":"...","scale":"..."}`

```js
const result = await fb2k.invoke('system.getDPI');
```

### system.getLocale

Public API method. Runtime authority: `src/api/WindowApi.cpp:2429`.

_No parameters._

**Returns**: `{"country":"...","language":"...","locale":"..."}`

```js
const result = await fb2k.invoke('system.getLocale');
```

### system.getRegisteredPlugins

Public API method. Runtime authority: `src/api/PluginRegistry.cpp:496`.

_No parameters._

**Returns**: `{"registered":"...","success":true}`

```js
const result = await fb2k.invoke('system.getRegisteredPlugins');
```

### system.getTheme

Public API method. Runtime authority: `src/api/WindowApi.cpp:2425`.

_No parameters._

**Returns**: `{"accentColor":"...","darkMode":"...","isDark":"...","transparency":"..."}`

```js
const result = await fb2k.invoke('system.getTheme');
```

### system.isPluginRegistered

Public API method. Runtime authority: `src/api/PluginRegistry.cpp:509`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `namespace` | `string` | No | Optional; default . |

**Returns**: `{"registered":"...","success":true}`

```js
const result = await fb2k.invoke('system.isPluginRegistered', { namespace: /* value */ });
```

### system.listAvailableApis

Public API method. Runtime authority: `src/api/PluginRegistry.cpp:438`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `includeExternal` | `boolean` | No | Optional; default true. |
| `includeInternal` | `boolean` | No | Optional; default true. |

**Returns**: JSON object from the runtime handler.

```js
const result = await fb2k.invoke('system.listAvailableApis', { includeExternal: /* value */, includeInternal: /* value */ });
```

### system.searchApis

Public API method. Runtime authority: `src/api/PluginRegistry.cpp:472`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `query` | `string` | No | Optional; default . |

**Returns**: JSON object from the runtime handler.

```js
const result = await fb2k.invoke('system.searchApis', { query: /* value */ });
```

## test

### test.echo

Public API method. Runtime authority: `src/api/PlaybackApi.cpp:697`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `json` | No | Optional; default omitted. |

**Returns**: `{"echo":"...","input":"...","success":true}`

```js
const result = await fb2k.invoke('test.echo', { message: /* value */ });
```

### test.ping

Public API method. Runtime authority: `src/api/PlaybackApi.cpp:698`.

_No parameters._

**Returns**: `{"pong":"...","timestamp":"..."}`

```js
const result = await fb2k.invoke('test.ping');
```

## Phase 3 contract supplements

The sections below close public-contract findings from the strict parameter audit without replacing existing explanations.

<!-- phase3-supplement:log.write -->
### Contract supplement: `log.write`

Phase 3 verified contract supplement. Runtime authority: `src/api/ConsoleApi.cpp:140-221`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `message` | `string` | No | omitted | Optional; default omitted. |
| `args` | `array` | No | `[]` | Optional; default []. |
| `file` | `string` | No | omitted | Optional; default omitted. |
| `level` | `string` | No | `info` | Optional; default info. |
| `append` | `boolean` | No | `true` | Optional; default true. |
| `timestamp` | `boolean` | No | `true` | Optional; default true. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |
| `path` | `json` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('log.write', { message: /* value */, args: /* value */, file: /* value */, level: /* value */, append: /* value */, timestamp: /* value */ });
```
<!-- phase3-supplement:menu.getContextMenu -->
### Contract supplement: `menu.getContextMenu`

Phase 3 verified contract supplement. Runtime authority: `src/api/MenuApi.cpp:980-1024`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `handles` | `array` | No | `[]` | Optional; default []. |
| `i18n` | `boolean` | No | `true` | Optional; default true. |
| `locale` | `string` | No | `auto` | Optional; default auto. |
| `mode` | `string` | No | `auto` | Optional; default auto. |
| `withAvailability` | `boolean` | No | `true` | Optional; default true. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |
| `i18n` | `json` | No |
| `items` | `json` | No |
| `locale` | `json` | No |
| `mode` | `json` | No |
| `withAvailability` | `json` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('menu.getContextMenu', { handles: /* value */, i18n: /* value */, locale: /* value */, mode: /* value */, withAvailability: /* value */ });
```
<!-- phase3-supplement:menu.runContextCommandById -->
### Contract supplement: `menu.runContextCommandById`

Phase 3 verified contract supplement. Runtime authority: `src/api/MenuApi.cpp:1024-1045`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `id` | `integer` | No | `-1` | Optional; default -1. |
| `mode` | `string` | No | `auto` | Optional; default auto. |
| `handles` | `array` | No | `[]` | Optional; default []. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('menu.runContextCommandById', { id: /* value */, mode: /* value */, handles: /* value */ });
```
<!-- phase3-supplement:misc.showPopupMessage -->
### Contract supplement: `misc.showPopupMessage`

Phase 3 verified contract supplement. Runtime authority: `src/api/MiscApi.cpp:97-105`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `message` | `string` | No | `` | Optional; default . |
| `msg` | `string` | No | `` | Optional; default . |
| `title` | `string` | No | `Message` | Optional; default Message. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `success` | `boolean` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('misc.showPopupMessage', { message: /* value */, msg: /* value */, title: /* value */ });
```

# Misc API

English API reference for the `clipboard`, `console`, `log`, `menu`, `misc`, `panel`, `system`, `test` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## clipboard

### clipboard.read

Reads the Windows clipboard and reports which of text, file list, and image data it currently holds.

_No parameters._

**Returns**: `{"files":[],"hasFiles":true,"hasImage":true,"hasText":true,"success":true,"text":"..."}`

```js
const result = await fb2k.invoke('clipboard.read');
```

### clipboard.write

Replaces the clipboard contents with Unicode text.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `text` | `string` | Yes | Text to place on the clipboard. An empty string fails with `text is required`. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('clipboard.write', { text: 'copied text' });
```

### clipboard.writeFiles

Places a file list on the clipboard so that Explorer and other shell targets can paste it.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `array` | Yes | Array of absolute file paths. A missing, non-array, or empty value fails. |

**Returns**: `{"error":"...","fileCount":"...","success":true}`

```js
await fb2k.invoke('clipboard.writeFiles', { paths: ['C:\\Music\\song.flac'] });
```

### clipboard.writeHTML

Writes rich text to the clipboard in `HTML Format`, together with a plain-text fallback.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `html` | `string` | Yes | HTML fragment to write. An empty string fails with `html is required`. |
| `plainText` | `string` | No | Plain-text fallback. Defaults to the `html` string itself. |

**Returns**: `{"error":"...","htmlWritten":"...","success":true,"textWritten":"..."}`

```js
await fb2k.invoke('clipboard.writeHTML', { html: '<b>Now playing</b>' });
```

## console

### console.error

Prints a message to the foobar2000 console with an `ERROR` marker.

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

Prints a message to the foobar2000 console.

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

Prints a message to the foobar2000 console with a `WARN` marker.

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

Truncates the component log file in the profile directory.

_No parameters._

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('log.clear');
```

### log.read

Reads the tail of the component log file.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `lines` | `integer` | No | Number of trailing lines to return. Defaults to `100`. Negative values fail with `lines must be non-negative`. |

**Returns**: `{"content":"...","error":"...","lineCount":"...","lines":"...","success":true,"totalLines":"..."}`

```js
const { content } = await fb2k.invoke('log.read', { lines: 50 });
```

### log.write

Appends a line to a log file in the profile directory.

Provide one of `message` or `args`. Empty payloads fail with `message is required`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `string` | No | Log text. Non-string values are serialized. |
| `args` | `array` | No | Argument list joined with spaces when `message` is omitted. |
| `file` | `string` | No | Leaf `.log` or `.txt` filename under the profile directory. Defaults to `webview_ui.log`. |
| `level` | `string` | No | Level tag written in upper case before the message. Defaults to `info`. |
| `append` | `boolean` | No | Appends when `true` (the default); truncates the file first when `false`. |
| `timestamp` | `boolean` | No | Prefixes each line with a timestamp. Defaults to `true`. |

**Returns**: `{"error":"...","path":"...","success":true}`

```js
await fb2k.invoke('log.write', { message: 'panel initialized' });
```

## menu

### menu.close

Closes the active self-drawn menu overlay, if one is open.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `reason` | `string` | No | `api` | Optional close reason passed to the overlay host; default `api`. |

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('menu.close', { reason: 'api' });
```

### menu.getContextMenu

Returns the context menu tree that foobar2000 would build for the selected context.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `handles` | `array` | No | Track paths, or `{ path, subsong }` objects, used as the menu context. Required when `mode` is `handles`. |
| `i18n` | `boolean` | No | Translates item labels into `displayLabel`. Defaults to `true`. |
| `locale` | `string` | No | Translation locale. Defaults to `auto`, which keeps the host's own labels. |
| `mode` | `string` | No | One of `auto`, `selection`, `playlist`, `nowPlaying`, or `handles`. Defaults to `auto`, which is also used for any other value. |
| `withAvailability` | `boolean` | No | Includes per-submenu availability counters. Defaults to `true`. |

**Returns**: `{"Failed to initialize context menu":"...","error":"...","i18n":"...","items":"...","locale":"...","mode":"...","success":true,"withAvailability":"..."}`

```js
const { items } = await fb2k.invoke('menu.getContextMenu', { mode: 'selection' });
```

### menu.getMainMenu

Returns the main menu tree, falling back to a flat command list when the host cannot build a tree.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `i18n` | `boolean` | No | Translates item labels into `displayLabel`. Defaults to `true`. |
| `locale` | `string` | No | Translation locale. Defaults to `auto`, which keeps the host's own labels. |
| `root` | `string` | No | Restricts the tree to one top-level menu, such as `View`. Defaults to the whole menu. |
| `withAvailability` | `boolean` | No | Includes per-submenu availability counters. Defaults to `true`. |

**Returns**: `{"error":"...","fallback":"...","items":[],"success":true}`

```js
const { items } = await fb2k.invoke('menu.getMainMenu', { root: 'View' });
```

### menu.runContextCommand

Runs a context menu command against the current selection or the now playing track.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `command` | `string` | Yes | Command name or GUID string. An empty value fails with `command is required`. |

**Returns**: `{"error":"...","guid":"...","itemCount":"...","success":true}`

```js
await fb2k.invoke('menu.runContextCommand', { command: 'Playback Statistics/Rating/5' });
```

### menu.runContextCommandById

Runs a context menu command by the numeric id reported by `menu.getContextMenu`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `id` | `integer` | Yes | Command id from the built context menu. Missing, non-integer, or negative values fail with `id is required`. |
| `mode` | `string` | No | One of `selection`, `playlist`, `nowPlaying`, `handles`, or `auto`. Defaults to `auto`, which is also used for any other value. |
| `handles` | `array` | No | Track paths, or `{ path, subsong }` objects, used as the menu context. Required when `mode` is `handles`. |

**Returns**: `{"Failed to initialize context menu":"...","error":"...","success":true}`

```js
await fb2k.invoke('menu.runContextCommandById', { id: 3 });
```

### menu.runMainMenuCommand

Runs a main menu command.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `command` | `string` | Yes | GUID, leaf command name, or slash-separated path. An empty value fails with `command is required`. |
| `subGuid` | `string` | No | GUID of a dynamic child command, paired with its owning command GUID. A malformed value fails with `Invalid subGuid format`. |

**Returns**: `{"error":"...","guid":"...","success":true}`

`command` accepts a GUID, a leaf command name, or a slash-separated path. Prefer
the GUID: it is the only form that is stable across hosts. A localized
foobar2000 build reports localized command labels, so an English name or path
will not resolve there.

Name and path forms are matched exactly per segment. When a name matches more
than one command the call fails with `MENU_MATCH_AMBIGUOUS` and lists the
candidates, rather than picking one.

Failure is always reported as `success: false` with a `code`:

| `code` | Meaning |
| --- | --- |
| `MENU_ITEM_DISABLED` | Command exists but is currently greyed out. |
| `MENU_MATCH_AMBIGUOUS` | Name matched several commands; see `candidates`. |
| `MENU_COMMAND_NOT_FOUND` | No command matched. |

```js
// Preferred: address by GUID.
const result = await fb2k.invoke('menu.runMainMenuCommand', {
    command: '{11213A01-9F36-4E69-A1BB-7A72F418DE3A}',
});

// A dynamic child command needs its owning command GUID plus subGuid.
await fb2k.invoke('menu.runMainMenuCommand', {
    command: '{41D98AF1-8C4F-4F0E-8B7A-1A4B0F7B1234}',
    subGuid: '{A222D5A9-2903-AA8C-EEAE-4B9230558B55}',
});
```

### menu.show

Opens the self-drawn menu overlay and returns its id; the user's choice arrives later through the `menu:select` and `menu:dismiss` events.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `items` | `array` | No | Menu rows, each with `id`, `label`, and optional `type`, `enabled`, `checked`, and nested `submenu`. Defaults to an empty menu. |
| `x` | `integer` | No | Screen-space X anchor in pixels. Defaults to the cursor X, which is also used for negative values. |
| `y` | `integer` | No | Screen-space Y anchor in pixels. Defaults to the cursor Y, which is also used for negative values. |

**Returns**: `{"error":"...","menuId":"...","success":true}`

```js
const { menuId } = await fb2k.invoke('menu.show', {
    items: [
        { id: 'play', label: 'Play' },
        { type: 'separator' },
        { id: 'props', label: 'Properties' },
    ],
});
```

### menu.showNativePopup

Opens the native foobar2000 context menu at the current cursor position.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `handles` | `array` | No | Track paths, or `{ path, subsong }` objects, used as the menu context. Required when `mode` is `handles`. |
| `mode` | `string` | No | One of `auto`, `selection`, `playlist`, `nowPlaying`, or `handles`. Defaults to `auto`, which is also used for any other value. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('menu.showNativePopup', { mode: 'nowPlaying' });
```

## misc

### misc.exit

Requests the foobar2000 exit command.

_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('misc.exit');
```

### misc.getComponentPath

Returns the directory the component DLL was loaded from.

_No parameters._

**Returns**: `{"path":"...","value":"..."}`

```js
const result = await fb2k.invoke('misc.getComponentPath');
```

### misc.getFoobarPath

Returns the foobar2000 installation directory.

_No parameters._

**Returns**: `{"path":"...","value":"..."}`

```js
const result = await fb2k.invoke('misc.getFoobarPath');
```

### misc.getProfilePath

Returns the foobar2000 profile directory.

_No parameters._

**Returns**: `{"path":"...","value":"..."}`

```js
const result = await fb2k.invoke('misc.getProfilePath');
```

### misc.restart

Requests the foobar2000 restart command.

_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('misc.restart');
```

### misc.showConsole

Opens the foobar2000 console window.

_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('misc.showConsole');
```

### misc.showLibrarySearch

Opens the media library search window, optionally pre-filled with a query.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `query` | `string` | No | Initial search query. Defaults to an empty query. |

**Returns**: `{"query":"...","success":true}`

```js
await fb2k.invoke('misc.showLibrarySearch', { query: 'artist has Radiohead' });
```

### misc.showPopupMessage

Shows a foobar2000 popup message dialog. Always reports `success: true`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `string` | No | Message body. Defaults to an empty message. |
| `msg` | `string` | No | Legacy alias, used only when `message` is absent. Prefer `message`. |
| `title` | `string` | No | Dialog title. Defaults to `Message`. |

**Returns**: `{"success":true}`

```js
await fb2k.invoke('misc.showPopupMessage', {
    message: 'Playlist exported.',
    title: 'Now Playing',
});
```
## Owner-family behavior and limits

- `clipboard.writeFiles` accepts media-read-authorized paths. `clipboard.writeHTML` writes HTML plus a plain-text fallback; `clipboard.read` reports only formats currently available from the Windows clipboard.
- `console.log`, `console.warn`, `console.error`, and `log.write` require one of `message` or `args`. `log.write.file`, when accepted, is only a leaf `.log` or `.txt` filename under the profile directory; paths, traversal and Windows reserved device names are rejected by the runtime.
- `menu.getContextMenu`, `menu.runContextCommandById`, and `menu.showNativePopup` use `mode` to select `handles`, `nowPlaying`, `selection`, or `playlist` context. `auto` tries those sources in that order, ending with playlist context. In `handles` mode, every path is media-access validated before a handle is created. `menu.showNativePopup` uses screen cursor coordinates and returns before the native menu is displayed.
- `menu.show` opens the self-drawn overlay after resource validation. `menu.close` only closes the active overlay; `menu.__*` endpoints are internal and are not public APIs.
- `misc.showPopupMessage` accepts `message`, falling back to `msg`, and defaults `title` to `"Message"`. The restart and exit methods request the corresponding foobar2000 standard command.
- `panel.setConfig` changes only its documented panel fields. `system.*` reports registered runtime and plugin information; `test.*` is diagnostic surface rather than application behavior.

### misc.showPreferences

Opens the foobar2000 preferences dialog.

_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('misc.showPreferences');
```

## panel

### panel.getConfig

Returns the calling panel's configuration.

_No parameters._

**Returns**: `{"config":{},"success":true}`

```js
const result = await fb2k.invoke('panel.getConfig');
```

### panel.setConfig

Updates the calling panel's configuration. Only the fields below may be changed from JavaScript; omitted fields keep their current value.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `enableDragDrop` | `boolean` | No | Enables drag-and-drop onto the panel. |
| `grabFocus` | `boolean` | No | Lets the panel take keyboard focus on click. |
| `panelName` | `string` | No | Display name of the panel. |
| `transparentBackground` | `boolean` | No | Renders the panel with a transparent background. |

**Returns**: `{"changed":"...","error":"...","success":true}`

```js
await fb2k.invoke('panel.setConfig', { grabFocus: true });
```

## system

### system.getApiStats

Returns counters describing the registered API surface.

_No parameters._

**Returns**: `{"registered":"...","success":true}`

```js
const result = await fb2k.invoke('system.getApiStats');
```

### system.getApisByNamespace

Lists the registered APIs belonging to one namespace.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `namespace` | `string` | Yes | Namespace to list, such as `playback`. An empty value fails with `namespace is required`. |

**Returns**: JSON object from the runtime handler.

```js
const apis = await fb2k.invoke('system.getApisByNamespace', { namespace: 'playback' });
```

### system.getDPI

Returns the DPI and scale factor of the panel's display.

_No parameters._

**Returns**: `{"dpi":"...","scale":"..."}`

```js
const result = await fb2k.invoke('system.getDPI');
```

### system.getLocale

Returns the current Windows user locale, language, and country.

_No parameters._

**Returns**: `{"country":"...","language":"...","locale":"..."}`

```js
const result = await fb2k.invoke('system.getLocale');
```

### system.getRegisteredPlugins

Lists the external plugins registered with the bridge.

_No parameters._

**Returns**: `{"registered":"...","success":true}`

```js
const result = await fb2k.invoke('system.getRegisteredPlugins');
```

### system.getTheme

Returns the Windows theme state: dark mode, accent color, and transparency.

_No parameters._

**Returns**: `{"accentColor":"...","darkMode":"...","isDark":"...","transparency":"..."}`

```js
const result = await fb2k.invoke('system.getTheme');
```

### system.isPluginRegistered

Reports whether a plugin namespace is registered.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `namespace` | `string` | Yes | Plugin namespace to check. An empty value fails with `namespace is required`. |

**Returns**: `{"registered":"...","success":true}`

```js
const { registered } = await fb2k.invoke('system.isPluginRegistered', { namespace: 'myplugin' });
```

### system.listAvailableApis

Lists every API currently available through the bridge.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `includeExternal` | `boolean` | No | Includes APIs registered by external plugins. Defaults to `true`. |
| `includeInternal` | `boolean` | No | Includes APIs built into the component. Defaults to `true`. |

**Returns**: JSON object from the runtime handler.

```js
const apis = await fb2k.invoke('system.listAvailableApis', { includeExternal: false });
```

### system.searchApis

Searches registered APIs by name or description.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `query` | `string` | Yes | Search text. An empty value fails with `query is required`. |

**Returns**: JSON object from the runtime handler.

```js
const apis = await fb2k.invoke('system.searchApis', { query: 'playlist' });
```

## test

### test.echo

Echoes the request back, for round-trip diagnostics.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `json` | No | Any JSON value, returned in `echo`. When omitted, the whole params object is echoed instead. |

**Returns**: `{"echo":"...","input":"...","success":true}`

```js
const { echo } = await fb2k.invoke('test.echo', { message: 'ping' });
```

### test.ping

Returns a liveness marker and the host's current Unix timestamp.

_No parameters._

**Returns**: `{"pong":"...","timestamp":"..."}`

```js
const result = await fb2k.invoke('test.ping');
```

## Contract supplements

The sections below close public-contract findings from the strict parameter audit without replacing existing explanations.

<!-- phase3-supplement:log.write -->
### Contract supplement: `log.write`

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `message` | `string` | No | omitted | Log text. Non-string values are serialized. |
| `args` | `array` | No | `[]` | Argument list joined with spaces when `message` is omitted. |
| `file` | `string` | No | omitted | Leaf `.log` or `.txt` filename under the profile directory. |
| `level` | `string` | No | `info` | Level tag written in upper case before the message. |
| `append` | `boolean` | No | `true` | Appends when `true`; truncates the file first when `false`. |
| `timestamp` | `boolean` | No | `true` | Prefixes each line with a timestamp. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |
| `path` | `json` | No |

Semantics: omitted optional parameters use handler defaults. One of `message` or `args` must be present.

```js
await fb2k.invoke('log.write', { message: 'startup complete', level: 'warn' });
```
<!-- phase3-supplement:menu.getContextMenu -->
### Contract supplement: `menu.getContextMenu`

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `handles` | `array` | No | `[]` | Track paths, or `{ path, subsong }` objects, used as the menu context. |
| `i18n` | `boolean` | No | `true` | Translates item labels into `displayLabel`. |
| `locale` | `string` | No | `auto` | Translation locale; `auto` keeps the host's own labels. |
| `mode` | `string` | No | `auto` | One of `auto`, `selection`, `playlist`, `nowPlaying`, or `handles`. |
| `withAvailability` | `boolean` | No | `true` | Includes per-submenu availability counters. |

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

Semantics: omitted optional parameters use handler defaults. `mode: 'handles'` additionally requires a non-empty `handles` array.

```js
const { items } = await fb2k.invoke('menu.getContextMenu', { mode: 'nowPlaying' });
```
<!-- phase3-supplement:menu.runContextCommandById -->
### Contract supplement: `menu.runContextCommandById`

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `id` | `integer` | Yes | — | Command id from the built context menu. Missing, non-integer, or negative values fail with `id is required`. |
| `mode` | `string` | No | `auto` | One of `selection`, `playlist`, `nowPlaying`, `handles`, or `auto`. |
| `handles` | `array` | No | `[]` | Track paths, or `{ path, subsong }` objects, used as the menu context. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |

Semantics: omitted optional parameters use handler defaults. `mode: 'handles'` additionally requires a non-empty `handles` array.

```js
await fb2k.invoke('menu.runContextCommandById', { id: 3, mode: 'selection' });
```
<!-- phase3-supplement:misc.showPopupMessage -->
### Contract supplement: `misc.showPopupMessage`

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `message` | `string` | No | `` | Message body. |
| `msg` | `string` | No | `` | Legacy alias, used only when `message` is absent. |
| `title` | `string` | No | `Message` | Dialog title. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `success` | `boolean` | No |

Semantics: omitted optional parameters use handler defaults. There is no failure branch; the call always reports `success: true`.

```js
await fb2k.invoke('misc.showPopupMessage', { message: 'Export finished.' });
```

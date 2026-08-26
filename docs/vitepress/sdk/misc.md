# Menus and Miscellaneous APIs

This page covers the `fb.menu`, `fb.console`, `fb.log`, `fb.lyrics`, `fb.notification`, `fb.panel`, `fb.misc`, and `fb.dnd` namespaces.

## fb.menu Menu Commands

### getMainMenu(root?)

Returns the main-menu tree. The optional `root` scopes the returned subtree, for example `'Main'` or `'View'`.

```javascript
const menu = await fb.menu.getMainMenu();
const viewMenu = await fb.menu.getMainMenu('View');
```

### getContextMenu(options?)

Returns a context-menu tree.

| Parameter | Type | Description |
| --- | --- | --- |
| `options.mode` | `'auto' \| 'selection' \| 'playlist' \| 'nowPlaying' \| 'handles'` | Selects the context source. |
| `options.handles` | `unknown[]` | Handle list used with `mode: 'handles'`. |
| `options.path` | `string` | Optional track path. |
| `options.subsong` | `number` | Optional subsong index. |
| `options.locale` | `string` | Locale selector; defaults to `'auto'`. |
| `options.i18n` | `boolean` | Enables localized labels. |
| `options.withAvailability` | `boolean` | Includes availability metadata. |

```javascript
const ctx = await fb.menu.getContextMenu({ mode: 'nowPlaying' });
```

Use `selection` for selected tracks. `playlist` requests playlist-level commands.

### runMainMenuCommand(command)

Executes a main-menu command by GUID, leaf name, or path.

Prefer the GUID: a localized foobar2000 build reports localized command labels,
so an English name or path will not resolve there. See
[Misc API](../api/misc.md#menu-runmainmenucommand) for the failure codes.

```javascript
await fb.menu.runMainMenuCommand('{11213A01-9F36-4E69-A1BB-7A72F418DE3A}');
```

### runContextCommand(command, options?)

Executes a context-menu command against the default context (the now-playing
track, falling back to the active playlist selection).

| Parameter | Type | Description |
| --- | --- | --- |
| `command` | `string` | GUID or command name. |
| `options.subGuid` | `string` | Node GUID of a dynamically generated child. Without it the owning container is targeted, which runs nothing. |

```javascript
await fb.menu.runContextCommand('Properties');

// A dynamic child needs the owning GUID plus its node GUID.
await fb.menu.runContextCommand('{5B69B9E3-1C7C-4C63-A9B0-1D0C0D0F0E0D}', {
    subGuid: '{A222D5A9-2903-AA8C-EEAE-4B9230558B55}',
});
```

### runContextCommandById(id, options?)

Executes a context-menu command by numeric ID.

| Parameter | Type | Description |
| --- | --- | --- |
| `id` | `number` | Command ID. |
| `options` | `Omit<MenuRunContextCommandByIdParams, 'id'>` | Optional `mode`, `handles`, `path`, and `subsong` context. |

### showNativePopup(options?)

Schedules a native popup menu using `MenuShowNativePopupParams`. The default
`auto` mode tries handles, now playing, playlist selection, then playlist context.

```javascript
await fb.menu.showNativePopup({ mode: 'selection' });
```

For the WebView-rendered `menu.show`, `menu.close`, and `menu.popup` APIs, see [fb.menu](./menu).

## fb.console Console Output

Writes messages to the foobar2000 console. Each method returns a `Promise<BaseResponse>`.

### log(message)

```javascript
await fb.console.log('Debug information');
```

### warn(message)

```javascript
await fb.console.warn('Warning message');
```

### error(message)

```javascript
await fb.console.error('Error message');
```

## fb.log Log File

### write(message, options?)

Writes a message with optional `LogWriteParams`, including `level`, `append`, `timestamp`, `file`, and `args`. The response may include the log-file `path`.

```javascript
await fb.log.write('Operation completed', { level: 'info' });
```

### read(lines?)

Reads log lines. The default host limit is 100 when `lines` is omitted.

```javascript
const { lines } = await fb.log.read(100);
```

### clear()

Clears the log file.

```javascript
await fb.log.clear();
```

## fb.lyrics Lyrics

### get(path?, options?)

Returns lyrics for `path`, or for the current track when `path` is omitted.

| Parameter | Type | Description |
| --- | --- | --- |
| `path` | `string` | Optional track path. |
| `options.source` | `'embedded' \| 'file' \| 'any'` | Source filter; defaults to `'any'`. |
| `options.type` | `'synced' \| 'unsynced' \| 'any'` | Synchronization filter; defaults to `'any'`. |
| `options.format` | `'lrc' \| 'txt' \| 'any'` | File-format filter; defaults to `'any'`. |

```javascript
const current = await fb.lyrics.get();
const embedded = await fb.lyrics.get(path, { source: 'embedded' });
const synced = await fb.lyrics.get(undefined, { type: 'synced' });
```

### exists(path)

Checks whether lyrics are available for a track path.

```javascript
const r = await fb.lyrics.exists('E:\\Music\\song.flac');
console.log(r.exists);
```

### save(path, lyrics, options?)

Saves lyrics to one or more configured targets.

| Parameter | Type | Description |
| --- | --- | --- |
| `path` | `string` | Track path. |
| `lyrics` | `string` | Lyrics text. |
| `options.target` | `string[]` | Target list forwarded by `LyricsSaveParams`. |
| `options.filename` | `string` | Optional custom sidecar filename. |
| `options.tagName` | `string` | Embedded tag name; defaults to `'LYRICS'`. |
| `options.format` | `string` | Sidecar format; defaults to `'lrc'`. |

```javascript
await fb.lyrics.save('E:\\Music\\song.flac', '[00:00.00]Lyrics...');
await fb.lyrics.save(path, text, { target: ['file', 'config'] });
await fb.lyrics.save(path, text, {
    target: ['embedded'],
    tagName: 'SYNCEDLYRICS',
});
```

The SDK return type is `BaseResponse & { results?: Array<{ target, success, error? }>; savedTo?: string[] }`.

## fb.notification Notifications

### show(options)

Shows a host notification. `UiShowNotificationParams` uses `body`, not `message`.

```javascript
await fb.notification.show({ title: 'Notice', body: 'Operation completed' });
```

### hide()

Hides the current notification.

### showCustomMenu(options)

Shows a host custom menu and returns optional `selectedId`.

```javascript
const { selectedId } = await fb.notification.showCustomMenu({
    items: [
        { label: 'Option A', id: 'a' },
        { label: 'Option B', id: 'b' },
    ],
});
```

### showToast(options)

Shows a toast.

```javascript
await fb.notification.showToast({ message: 'Added to the playlist' });
```

## fb.panel Panel Configuration

### getConfig()

Returns the current panel configuration envelope.

```javascript
const { config } = await fb.panel.getConfig();
```

### setConfig(options)

Updates fields supported by `PanelSetConfigParams`: `panelName`, `transparentBackground`, `grabFocus`, and `enableDragDrop`.

```javascript
await fb.panel.setConfig({ panelName: 'Library', enableDragDrop: true });
```

## fb.misc Host Actions

### exit()

Exits foobar2000.

### restart()

Restarts foobar2000.

```javascript
await fb.misc.restart();
```

### getComponentPath()

Returns `{ path }` for the component DLL directory.

```javascript
const r = await fb.misc.getComponentPath();
console.log(r.path);
```

### getFoobarPath()

Returns `{ path }` for the foobar2000 executable directory.

### getProfilePath()

Returns `{ path }` for the profile directory.

### showConsole()

Shows the foobar2000 console window.

### showLibrarySearch(query?)

Opens Media Library Search with an optional initial query.

```javascript
await fb.misc.showLibrarySearch('artist IS Beatles');
```

### showPopupMessage(message, title?)

Shows a popup message box.

```javascript
await fb.misc.showPopupMessage('Operation completed', 'Notice');
```

### showPreferences()

Opens foobar2000 Preferences.

## fb.dnd Drag and Drop

External file drop, documented on its own page: [fb.dnd](./dnd.md).

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## Additional methods

> This block maintains SDK-facing method coverage and may be expanded with complete examples and best practices.

### exit()

Signature: `fb.misc.exit(): Promise<BaseResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| None | — | — | This method takes no arguments. |

Returns the `misc.exit` response envelope.

```javascript
const result = await fb.misc.exit();
```

### getFoobarPath()

Signature: `fb.misc.getFoobarPath(): Promise<{ path: string }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| None | — | — | This method takes no arguments. |

Returns the foobar2000 path in `{ path }`.

```javascript
const result = await fb.misc.getFoobarPath();
```

### getProfilePath()

Signature: `fb.misc.getProfilePath(): Promise<{ path: string }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| None | — | — | This method takes no arguments. |

Returns the profile path in `{ path }`.

```javascript
const result = await fb.misc.getProfilePath();
```

### showConsole()

Signature: `fb.misc.showConsole(): Promise<BaseResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| None | — | — | This method takes no arguments. |

Returns the `misc.showConsole` response envelope.

```javascript
const result = await fb.misc.showConsole();
```

### showPreferences()

Signature: `fb.misc.showPreferences(): Promise<BaseResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| None | — | — | This method takes no arguments. |

Returns the `misc.showPreferences` response envelope.

```javascript
const result = await fb.misc.showPreferences();
```

<!-- END AUTO-GENERATED SDK STUBS -->

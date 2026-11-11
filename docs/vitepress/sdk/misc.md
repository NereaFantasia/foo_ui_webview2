# fb.misc Host Actions

`fb.misc` provides foobar2000 host lifecycle and environment actions: querying installation, profile, and component paths, opening native console, Preferences, and Media Library Search dialogs, displaying popup messages, and restarting or exiting the host process.

## exit()

Signature: `fb.misc.exit(): Promise<MiscExitResponse>`

Requests foobar2000 to terminate the host process gracefully.

```javascript
const res = await fb.misc.exit();
if (res.success === false) throw new Error(res.error);
```

## getComponentPath()

Signature: `fb.misc.getComponentPath(): Promise<MiscGetComponentPathResponse>`

Resolves with the native absolute directory path where the component DLL is located, populated in the `path` field. The `value` field contains the identical path for backward compatibility.

```javascript
const r = await fb.misc.getComponentPath();
if (r.success === false) throw new Error(r.error);
console.log(r.path);
```

## getFoobarPath()

Signature: `fb.misc.getFoobarPath(): Promise<MiscGetFoobarPathResponse>`

Resolves with the native absolute path of the foobar2000 installation directory, returned in both `path` and `value` fields.

```javascript
const res = await fb.misc.getFoobarPath();
if (res.success === false) throw new Error(res.error);
const { path } = res;
```

## getProfilePath()

Signature: `fb.misc.getProfilePath(): Promise<MiscGetProfilePathResponse>`

Resolves with the native absolute path of the foobar2000 user profile directory (storing configurations and components), returned in both `path` and `value` fields.

```javascript
const res = await fb.misc.getProfilePath();
if (res.success === false) throw new Error(res.error);
const { path } = res;
```

## restart()

Signature: `fb.misc.restart(): Promise<MiscRestartResponse>`

Requests foobar2000 to restart the host process.

```javascript
await fb.misc.restart();
```

## showConsole()

Signature: `fb.misc.showConsole(): Promise<MiscShowConsoleResponse>`

Opens the native foobar2000 console window. To output log lines to the console, use [fb.console](./console.md).

```javascript
await fb.misc.showConsole();
```

## showLibrarySearch(query?)

Signature: `fb.misc.showLibrarySearch(query?: string): Promise<MiscShowLibrarySearchResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `query` | `string` | No | Search query pre-populated in the search input; empty or omitted opens an empty search window |

Opens the native foobar2000 Media Library Search window. The `query` field in the response reflects the query expression applied upon initialization.

```javascript
await fb.misc.showLibrarySearch('artist IS Beatles');
```

## showPopupMessage(message, title?)

Signature: `fb.misc.showPopupMessage(message: string, title?: string): Promise<MiscShowPopupMessageResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `string` | Yes | Body text for the dialog; must not be an empty string |
| `title` | `string` | No | Title bar text; defaults to `"Message"` when empty or omitted |

Displays a native foobar2000 popup message dialog. The call resolves immediately once the dialog is created and displayed, without blocking until dismissal. Passing an empty string for `message` fails with `INVALID_PARAMS`.

```javascript
await fb.misc.showPopupMessage('Operation completed', 'Notice');
```

## showPreferences()

Signature: `fb.misc.showPreferences(): Promise<MiscShowPreferencesResponse>`

Opens the native foobar2000 Preferences dialog.

```javascript
await fb.misc.showPreferences();
```

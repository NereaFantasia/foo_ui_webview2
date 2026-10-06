# fb.misc Host Actions

`fb.misc` runs a few foobar2000 host actions: it reports the installation, profile, and component directories, opens the console, Preferences, and Media Library Search windows, shows a popup message, and restarts or exits foobar2000.

## exit()

Signature: `fb.misc.exit(): Promise<MiscExitResponse>`

Asks foobar2000 to exit.

```javascript
const res = await fb.misc.exit();
if (res.success === false) throw new Error(res.error);
```

## getComponentPath()

Signature: `fb.misc.getComponentPath(): Promise<MiscGetComponentPathResponse>`

Reports the directory the component DLL was loaded from, as a native path in `path`. `value` holds the same directory for themes that read that field.

```javascript
const r = await fb.misc.getComponentPath();
if (r.success === false) throw new Error(r.error);
console.log(r.path);
```

## getFoobarPath()

Signature: `fb.misc.getFoobarPath(): Promise<MiscGetFoobarPathResponse>`

Reports the foobar2000 installation directory as `path` (and `value`, the same directory).

```javascript
const res = await fb.misc.getFoobarPath();
if (res.success === false) throw new Error(res.error);
const { path } = res;
```

## getProfilePath()

Signature: `fb.misc.getProfilePath(): Promise<MiscGetProfilePathResponse>`

Reports the foobar2000 profile directory, where configuration and components live, as `path` (and `value`, the same directory).

```javascript
const res = await fb.misc.getProfilePath();
if (res.success === false) throw new Error(res.error);
const { path } = res;
```

## restart()

Signature: `fb.misc.restart(): Promise<MiscRestartResponse>`

Asks foobar2000 to restart.

```javascript
await fb.misc.restart();
```

## showConsole()

Signature: `fb.misc.showConsole(): Promise<MiscShowConsoleResponse>`

Opens the foobar2000 console window. To write lines to the console, use [fb.console](./console.md).

```javascript
await fb.misc.showConsole();
```

## showLibrarySearch(query?)

Signature: `fb.misc.showLibrarySearch(query?: string): Promise<MiscShowLibrarySearchResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `query` | `string` | No | Query filled into the search box. Empty or omitted opens a blank search. |

Opens the Media Library Search window. The response's `query` is the query the window opened with.

```javascript
await fb.misc.showLibrarySearch('artist IS Beatles');
```

## showPopupMessage(message, title?)

Signature: `fb.misc.showPopupMessage(message: string, title?: string): Promise<MiscShowPopupMessageResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `string` | Yes | Body text of the dialog; must not be empty. |
| `title` | `string` | No | Title bar text; the host uses `"Message"` when it is empty or omitted. |

Shows a foobar2000 popup message dialog. The call returns once the dialog is shown, not when it is closed. An empty `message` fails with `INVALID_PARAMS`.

```javascript
await fb.misc.showPopupMessage('Operation completed', 'Notice');
```

## showPreferences()

Signature: `fb.misc.showPreferences(): Promise<MiscShowPreferencesResponse>`

Opens the foobar2000 Preferences dialog.

```javascript
await fb.misc.showPreferences();
```

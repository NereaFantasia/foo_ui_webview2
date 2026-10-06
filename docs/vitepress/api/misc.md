# Misc API

Methods of the `misc` namespace.

## misc

### misc.exit

<!-- api-schema:begin misc.exit -->
Ask foobar2000 to exit.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('misc.exit');
```

### misc.getComponentPath

<!-- api-schema:begin misc.getComponentPath -->
Report the directory the component DLL was loaded from.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `path` | `string` | The directory, as a native path. |
| `value` | `string` | The same directory; kept for themes that read `value`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('misc.getComponentPath');
```

### misc.getFoobarPath

<!-- api-schema:begin misc.getFoobarPath -->
Report the foobar2000 installation directory.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `path` | `string` | The directory, as a native path. |
| `value` | `string` | The same directory; kept for themes that read `value`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('misc.getFoobarPath');
```

### misc.getProfilePath

<!-- api-schema:begin misc.getProfilePath -->
Report the foobar2000 profile directory, where configuration and components live.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `path` | `string` | The directory, as a native path. |
| `value` | `string` | The same directory; kept for themes that read `value`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('misc.getProfilePath');
```

### misc.restart

<!-- api-schema:begin misc.restart -->
Ask foobar2000 to restart.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('misc.restart');
```

### misc.showConsole

<!-- api-schema:begin misc.showConsole -->
Open the foobar2000 console window.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('misc.showConsole');
```

### misc.showLibrarySearch

<!-- api-schema:begin misc.showLibrarySearch -->
Open the media library search window, optionally with a query filled in.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `query` | `string` | No | Query filled into the search box; empty opens a blank search. Default: `""`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `query` | `string` | The query the window opened with. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('misc.showLibrarySearch', { query: 'artist has Radiohead' });
```

### misc.showPopupMessage

<!-- api-schema:begin misc.showPopupMessage -->
Show a foobar2000 popup message dialog. The call returns when the dialog is shown, not when it is closed.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `string` | Yes | Body text of the dialog. Must not be empty. |
| `title` | `string` | No | Title bar text. Default: `"Message"`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('misc.showPopupMessage', {
    message: 'Playlist exported.',
    title: 'Now Playing',
});
```

### misc.showPreferences

<!-- api-schema:begin misc.showPreferences -->
Open the foobar2000 Preferences dialog.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('misc.showPreferences');
```

## Owner-family behavior and limits

- `misc.showPopupMessage` requires a non-empty `message` and defaults `title` to `"Message"`; a call that still sends `msg` is refused with `INVALID_PARAMS` (`unknown parameter 'msg'`). `misc.showConsole`, `misc.showPreferences`, `misc.restart` and `misc.exit` run the corresponding foobar2000 standard command and fail with `OPERATION_FAILED` when foobar2000 declines to run it.

## Contract supplements

The sections below close public-contract findings from the strict parameter audit without replacing existing explanations.

<!-- phase3-supplement:misc.showPopupMessage -->
### Contract supplement: `misc.showPopupMessage`

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `message` | `string` | Yes | — | Message body; must not be empty. |
| `title` | `string` | No | `Message` | Dialog title. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `success` | `boolean` | No |

Semantics: an omitted `title` uses the handler default. A missing or empty `message` is refused with `INVALID_PARAMS` (`message is required`); a call that sends `msg` is refused as an unknown parameter. Otherwise the dialog is shown and the call reports `success: true` without waiting for it to close.

```js
await fb2k.invoke('misc.showPopupMessage', { message: 'Export finished.' });
```

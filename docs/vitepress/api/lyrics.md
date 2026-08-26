# Lyrics API

English API reference for the `lyrics` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## lyrics

### lyrics.exists


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path. Accepts `path\|subsong:N`. |

**Returns**: `{"error":"...","exists":"...","sources":"...","success":true}`

```js
const { exists, sources } = await fb2k.invoke('lyrics.exists', {
	path: 'C:\\Music\\song.flac',
});
```

### lyrics.get


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `path` | `string` | No | — | Falls back to the current playing track. |
| `source` | `string` | No | `any` | Accepts `embedded`, `file`, or `any`. |
| `type` | `string` | No | `any` | Accepts `synced`, `unsynced`, or `any`. |
| `format` | `string` | No | `any` | Accepts `lrc`, `txt`, or `any`. |

**Returns**: `{"available":true,"lyrics":"...","path":"...","source":"...","sourcePath":"...","success":true,"synced":"..."}`

```js
const { available, lyrics, synced } = await fb2k.invoke('lyrics.get', {
	path: 'C:\\Music\\song.flac',
	type: 'synced',
});
```

### lyrics.save


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `path` | `string` | Yes | — | Track path the lyrics belong to. |
| `lyrics` | `string` | Yes | — | Lyrics text to save; must be non-empty. |
| `target` | `string \| string[]` | No | `file` | Accepts `file`, `embedded`, `config`, `all`, or an array of the first three. |
| `format` | `string` | No | `lrc` | Accepts `lrc` or `txt` and selects the sidecar extension. |
| `filename` | `string` | No | — | Plain filename only. Used by the `file` and `config` targets. |
| `tagName` | `string` | No | `LYRICS` | Used by the `embedded` target. |

**Returns**: `{"error":"...","results":"...","success":true}`

```js
await fb2k.invoke('lyrics.save', {
	path: 'C:\\Music\\song.flac',
	lyrics: '[00:12.00]First line\n[00:18.50]Second line\n',
});
```

## Usage notes

- `lyrics.get` uses `path` when supplied and otherwise resolves the current playing track. `source`, `type`, and `format` default to `any`; successful results include `success`, `available`, and `path`, plus `source`, `lyrics`, and `synced` when lyrics are found. File-backed results additionally include `sourcePath`.
- For a `path|subsong:N` container path, file lookup checks the per-track sidecar before the shared sidecar. `lyrics.exists` returns source labels such as `file:song.lrc` and never treats a missing `path` as a current-track request.
- `lyrics.save` requires both `path` and non-empty `lyrics`. `target` defaults to `file` and accepts `file`, `embedded`, `config`, `all`, or an array of the first three values. The `filename` value must be a plain filename; path separators and traversal sequences are rejected.
- The documented SDK helpers `fb.lyrics.get(...)`, `fb.lyrics.exists(...)`, and `fb.lyrics.save(...)` are convenience wrappers. The public Bridge contract on this page remains the three `lyrics.*` methods. The `<fb-lyrics-panel>` component is a consumer, not a registered API method.

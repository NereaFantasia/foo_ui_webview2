# Log API

Methods of the `log` namespace.

## log

### log.clear

<!-- api-schema:begin log.clear -->
Empty `webview_ui.log`.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('log.clear');
```

### log.read

<!-- api-schema:begin log.read -->
Read the last lines of `webview_ui.log`. A file that does not exist yet reads as empty.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `lines` | `integer` | No | How many lines to return from the end of the file. At least `0`. Default: `100`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `content` | `string` | The returned lines joined with `\n`. |
| `lines` | `string[]` | The returned lines. Absent when the file does not exist. |
| `lineCount` | `integer` | Number of lines returned. |
| `totalLines` | `integer` | Number of lines in the whole file. Absent when the file does not exist. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('log.read', { lines: 50 });
if (res.success === false) throw new Error(res.error);
const { content } = res;
```

### log.write

<!-- api-schema:begin log.write -->
Write a line to a log file in the foobar2000 profile directory: `webview_ui.log`, or the file named by `file`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `any` | No | What to write. A string is written as it is; any other JSON value as its JSON text. Takes precedence over `args`. |
| `args` | `any[]` | No | Values to write, separated by spaces: strings as they are, other values as their JSON text. Used only when `message` is absent. |
| `level` | `string` | No | Level written in brackets before the message, upper-cased. Default: `"info"`. |
| `append` | `boolean` | No | Append to the file; `false` replaces its content with this line. Default: `true`. |
| `timestamp` | `boolean` | No | Prefix the line with the local time, as `[YYYY-MM-DD HH:MM:SS.mmm]`. Default: `true`. |
| `file` | `string` | No | Name of another file in the profile directory. It must be a plain file name ending in `.log` or `.txt` and not a Windows device name such as `CON`; any other value is ignored and the line goes to `webview_ui.log`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `path` | `string` | Full path of the file that was written. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Provide one of `message` or `args`; a call with neither fails with `INVALID_PARAMS` (`message is required`).

```js
await fb2k.invoke('log.write', { message: 'panel initialized' });
```

## Owner-family behavior and limits

- `console.log`, `console.warn`, `console.error`, and `log.write` require one of `message` or `args`. `log.write.file`, when accepted, is only a leaf `.log` or `.txt` filename under the profile directory; paths, traversal and Windows reserved device names are rejected by the runtime.

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

# Clipboard API

Methods of the `clipboard` namespace.

## clipboard

### clipboard.read

<!-- api-schema:begin clipboard.read -->
Report what the Windows clipboard holds: text, a file list, an image.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `hasText` | `boolean` | The clipboard holds text (`CF_UNICODETEXT` or `CF_TEXT`). |
| `hasImage` | `boolean` | The clipboard holds a bitmap (`CF_DIB` or `CF_BITMAP`). The image itself is not returned. |
| `hasFiles` | `boolean` | The clipboard holds a file list (`CF_HDROP`) with at least one file. |
| `text` | `string` | The text; empty when the clipboard holds none. |
| `files` | `string[]` | The file paths, present only when `hasFiles` is `true`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('clipboard.read');
```

### clipboard.write

<!-- api-schema:begin clipboard.write -->
Replace the clipboard contents with Unicode text.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `text` | `string` | Yes | Text to place on the clipboard. Must not be empty. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('clipboard.write', { text: 'copied text' });
```

### clipboard.writeFiles

<!-- api-schema:begin clipboard.writeFiles -->
Place a file list on the clipboard (`CF_HDROP`) for Explorer and other shell targets to paste.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | Absolute file paths, in the order they are placed on the clipboard. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `fileCount` | `integer` | Number of paths placed on the clipboard. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('clipboard.writeFiles', { paths: ['C:\\Music\\song.flac'] });
```

### clipboard.writeHTML

<!-- api-schema:begin clipboard.writeHTML -->
Replace the clipboard contents with rich text in `HTML Format`, plus a plain-text fallback.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `html` | `string` | Yes | HTML fragment to write as `HTML Format`. Must not be empty. |
| `plainText` | `string` | No | Plain-text fallback written as `CF_UNICODETEXT`; when omitted or empty, the HTML text itself. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `htmlWritten` | `boolean` | The `HTML Format` data was placed on the clipboard. |
| `textWritten` | `boolean` | The plain-text fallback was placed on the clipboard. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('clipboard.writeHTML', { html: '<b>Now playing</b>' });
```

## Owner-family behavior and limits

- `clipboard.writeFiles` accepts media-read-authorized paths. `clipboard.writeHTML` writes HTML plus a plain-text fallback; `clipboard.read` reports only formats currently available from the Windows clipboard.

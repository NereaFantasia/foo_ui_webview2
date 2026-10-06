# Dialog API

Methods of the `dialog` namespace.

## dialog

### dialog.confirm

<!-- api-schema:begin dialog.confirm -->
Show a modal task dialog with custom buttons. Escape and the close button do not dismiss it.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `title` | `string` | No | Window title. Defaults to "Confirm", localized to the UI language. |
| `message` | `string` | No | Body text. Default: `""`. |
| `type` | `"info" \| "warning" \| "error" \| "question"` | No | Icon. The task dialog shows `question` with the information icon. Default: `"question"`. |
| `buttons` | `string[]` | No | Button labels, in order. Defaults to OK and Cancel, localized to the UI language. |
| `defaultButton` | `integer` | No | Index of the initially focused button. Default: `0`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `response` | `integer` | Zero-based index of the clicked button in `buttons`. `-1` only when not even the fallback message box could be shown. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('dialog.confirm', {
	title: 'Remove Track',
	message: 'Remove this track from the playlist?',
});
if (res.success === false) throw new Error(res.error);
const { response } = res;
```

### dialog.openFile

<!-- api-schema:begin dialog.openFile -->
Show the system dialog for opening files.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `title` | `string` | No | Window title. Defaults to "Open File", localized to the UI language. |
| `multiple` | `boolean` | No | Allow selecting several files. Default: `false`. |
| `defaultPath` | `string` | No | Folder the dialog opens in, every time it is shown. `%music%` expands to the Music folder. Ignored when the path does not resolve to a folder. Default: `""`. |
| `filters` | `FileFilter[]` | No | File type list. When omitted or empty, the dialog shows a single "All Files" entry. |
| `filters[].name` | `string` | No | Label shown in the file type list. Defaults to "Files", localized to the UI language. |
| `filters[].extensions` | `string[]` | No | Extensions without the leading dot, such as `flac`; `*` matches every file. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `canceled` | `boolean` | `true` when the user closed the dialog without choosing. |
| `filePaths` | `string[]` | Chosen files; empty when canceled. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('dialog.openFile', {
	title: 'Add Audio Files',
	multiple: true,
	filters: [{ name: 'Audio', extensions: ['flac', 'mp3', 'm4a'] }],
});
if (res.success === false) throw new Error(res.error);
const { canceled, filePaths } = res;
```

### dialog.openFolder

<!-- api-schema:begin dialog.openFolder -->
Show the system dialog for choosing a folder.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `title` | `string` | No | Window title. Defaults to "Select Folder", localized to the UI language. |
| `defaultPath` | `string` | No | Folder the dialog opens in, every time it is shown. `%music%` expands to the Music folder. Ignored when the path does not resolve to a folder. Default: `""`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `canceled` | `boolean` | `true` when the user closed the dialog without choosing. |
| `folderPath` | `string` | Chosen folder, which need not be `defaultPath`; empty when canceled. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('dialog.openFolder', {
	title: 'Choose Music Folder',
	defaultPath: 'D:\\Music',
});
if (res.success === false) throw new Error(res.error);
const { canceled, folderPath } = res;
```

### dialog.saveFile

<!-- api-schema:begin dialog.saveFile -->
Show the system dialog for saving a file.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `title` | `string` | No | Window title. Defaults to "Save File", localized to the UI language. |
| `defaultName` | `string` | No | File name filled in when the dialog opens. Default: `""`. |
| `filters` | `FileFilter[]` | No | File type list. When omitted or empty, the dialog shows a single "All Files" entry. |
| `filters[].name` | `string` | No | Label shown in the file type list. Defaults to "Files", localized to the UI language. |
| `filters[].extensions` | `string[]` | No | Extensions without the leading dot, such as `flac`; `*` matches every file. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `canceled` | `boolean` | `true` when the user closed the dialog without choosing. |
| `filePath` | `string` | Chosen file; empty when canceled. The system asks before an existing file is overwritten. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('dialog.saveFile', {
	defaultName: 'export.m3u8',
	filters: [{ name: 'Playlist', extensions: ['m3u8'] }],
});
if (res.success === false) throw new Error(res.error);
const { canceled, filePath } = res;
```

## Dialog boundaries

- Native dialog cancellation returns `canceled: true` with an empty result path/list. Dialog initialization failures add `error` and set `canceled: false`.

# fb.dialog Native Dialogs

`fb.dialog` opens host-native file, folder, save, and confirmation dialogs.

## confirm(options?)

Signature: `fb.dialog.confirm(options?: DialogConfirmParams): Promise<DialogConfirmResponse>`

Shows a modal confirmation dialog with custom buttons.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `options.title` | `string` | No | Dialog title; defaults to `Confirm` in the UI language |
| `options.message` | `string` | No | Confirmation message |
| `options.type` | `string` | No | Icon: `info`, `warning`, `error` or `question`; defaults to `question`, which the task dialog shows with the information icon |
| `options.defaultButton` | `number` | No | Index of the initially focused button; defaults to `0` |
| `options.buttons` | `string[]` | No | Custom button labels, in order |

`response` is the zero-based index of the clicked button. With the default buttons `['OK', 'Cancel']`, `0` means confirmed and `1` cancelled; there is no `confirmed` flag. Escape and the close button do not dismiss the dialog, so every result comes from a button click. `-1` appears only when not even the host's fallback message box could be shown.

```javascript
const res = await fb.dialog.confirm({
	title: 'Remove track',
	message: 'Remove the selected track?'
});
if (res.success === false) throw new Error(res.error);
const { response } = res;
if (response === 0) {
	// confirmed
}
```

## openFile(options?)

Signature: `fb.dialog.openFile(options?: DialogOpenFileParams): Promise<DialogOpenFileResponse>`

Opens the native file picker. Returns `{ success: true, canceled, filePaths }`, or the failure envelope `{ success: false, error, code }` when the dialog cannot be shown. Set `multiple` to allow multiple selections; other options include `title`, `defaultPath` (honored on every open), and `filters`, an array of `{ name, extensions }` entries. Write `extensions` without the leading dot, such as `flac`; `*` matches every file.

```javascript
const result = await fb.dialog.openFile({
	title: 'Choose audio files',
	multiple: true,
	filters: [{ name: 'Audio', extensions: ['flac', 'mp3'] }]
});
```

## openFolder(options?)

Signature: `fb.dialog.openFolder(options?: DialogOpenFolderParams): Promise<DialogOpenFolderResponse>`

Opens the native folder picker. Accepts an optional `title` and an optional `defaultPath` — the folder the dialog opens in, every time it is shown; silently ignored when the path does not resolve to a folder, and `%music%` expands to the user's Music folder. Returns `{ success: true, canceled, folderPath }` (or the failure envelope `{ success: false, error, code }` when the dialog cannot be shown); `folderPath` is whatever the user confirmed, which need not be `defaultPath`.

```javascript
const result = await fb.dialog.openFolder({
	title: 'Choose a music folder',
	defaultPath: 'D:\\Music'
});
```

## saveFile(options?)

Signature: `fb.dialog.saveFile(options?: DialogSaveFileParams): Promise<DialogSaveFileResponse>`

Opens the native save picker. Returns `{ success: true, canceled, filePath }`, or the failure envelope `{ success: false, error, code }` when the dialog cannot be shown. Options include `title`, `defaultName`, and `filters`, an array of `{ name, extensions }` entries.

```javascript
const result = await fb.dialog.saveFile({ defaultName: 'playlist.m3u8' });
```

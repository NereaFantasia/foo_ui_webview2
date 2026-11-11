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

`response` returns the zero-based index of the clicked button. Default button choices are OK and Cancel, where `0` indicates confirmation and `1` indicates cancellation; the response does not contain a `confirmed` boolean property. Esc and the window close button cannot dismiss the dialog; return values always originate from button clicks. A `response` of `-1` occurs only when even the host's fallback message box fails to display.

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

Opens the native file picker dialog. Returns `{ success: true, canceled, filePaths }` on success, or a failure envelope when the dialog fails to display. Set `multiple: true` to permit multi-selection; additional options include `title`, `defaultPath` (applied on every invocation), and `filters`. `filters` is an array of `{ name, extensions }` objects, where `extensions` contains file extensions without leading dots (such as `flac`); `*` matches all files.

```javascript
const result = await fb.dialog.openFile({
	title: 'Choose audio files',
	multiple: true,
	filters: [{ name: 'Audio', extensions: ['flac', 'mp3'] }]
});
```

## openFolder(options?)

Signature: `fb.dialog.openFolder(options?: DialogOpenFolderParams): Promise<DialogOpenFolderResponse>`

Opens the native folder picker dialog. Accepts optional `title` and `defaultPath` parameters: `defaultPath` designates the initial directory each time the dialog opens, which is ignored if the path does not resolve to a valid folder; path variables like `%music%` expand to the current user Music directory. Resolves with `{ success: true, canceled, folderPath }` on success, or a failure envelope if the dialog fails to display. `folderPath` contains the directory path confirmed by the user.

```javascript
const result = await fb.dialog.openFolder({
	title: 'Choose a music folder',
	defaultPath: 'D:\\Music'
});
```

## saveFile(options?)

Signature: `fb.dialog.saveFile(options?: DialogSaveFileParams): Promise<DialogSaveFileResponse>`

Opens the native file save dialog. Returns `{ success: true, canceled, filePath }` on success, or a failure envelope when the dialog fails to display. Options include `title`, `defaultName` (suggested filename), and `filters` (array of `{ name, extensions }` objects).

```javascript
const result = await fb.dialog.saveFile({ defaultName: 'playlist.m3u8' });
```

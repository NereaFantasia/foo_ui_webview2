# fb.clipboard Clipboard

`fb.clipboard` reads and writes text, HTML, and file-list clipboard data through the host process.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## Additional methods

### read()

Signature: `fb.clipboard.read(): Promise<{ text: string }>`

Reads clipboard text.

```javascript
const { text } = await fb.clipboard.read();
```

### write(text)

Signature: `fb.clipboard.write(text: string): Promise<BaseResponse>`

Writes plain text to the clipboard.

```javascript
await fb.clipboard.write('Copied from the theme');
```

### writeFiles(paths)

Signature: `fb.clipboard.writeFiles(paths: string[]): Promise<BaseResponse & { fileCount?: number }>`

Writes a list of file paths for pasting into file-aware applications.

```javascript
const result = await fb.clipboard.writeFiles([
	'C:\\Music\\one.flac',
	'C:\\Music\\two.flac'
]);
```

### writeHTML(html, plainText?)

Signature: `fb.clipboard.writeHTML(html: string, plainText?: string): Promise<BaseResponse & { htmlWritten?: boolean; textWritten?: boolean }>`

Writes HTML and, when a non-empty `plainText` value is supplied, a plain-text fallback.

```javascript
await fb.clipboard.writeHTML('<strong>Now playing</strong>', 'Now playing');
```

<!-- END AUTO-GENERATED SDK STUBS -->

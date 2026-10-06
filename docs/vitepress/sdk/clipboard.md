# fb.clipboard Clipboard

`fb.clipboard` reads and writes text, HTML, and file-list clipboard data through the host process.

## read()

Signature: `fb.clipboard.read(): Promise<ClipboardReadResponse>`

Reads the clipboard: which of text, a file list and an image it holds, plus the text and the file paths.

```javascript
const res = await fb.clipboard.read();
if (res.success === false) throw new Error(res.error);
const { text } = res;
```

## write(text)

Signature: `fb.clipboard.write(text: string): Promise<ClipboardWriteResponse>`

Writes plain text to the clipboard.

```javascript
await fb.clipboard.write('Copied from the theme');
```

## writeFiles(paths)

Signature: `fb.clipboard.writeFiles(paths: string[]): Promise<ClipboardWriteFilesResponse>`

Writes a list of file paths for pasting into file-aware applications.

```javascript
const result = await fb.clipboard.writeFiles([
	'C:\\Music\\one.flac',
	'C:\\Music\\two.flac'
]);
```

## writeHTML(html, plainText?)

Signature: `fb.clipboard.writeHTML(html: string, plainText?: string): Promise<ClipboardWriteHTMLResponse>`

Writes HTML to the clipboard as `HTML Format`, together with a plain-text fallback: `plainText` when it is non-empty, otherwise the HTML text itself.

```javascript
await fb.clipboard.writeHTML('<strong>Now playing</strong>', 'Now playing');
```

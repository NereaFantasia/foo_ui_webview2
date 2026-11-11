# fb.clipboard Clipboard

`fb.clipboard` reads and writes text, HTML, and file-list clipboard data through the host process.

## read()

Signature: `fb.clipboard.read(): Promise<ClipboardReadResponse>`

Reads system clipboard content, returning format availability flags (text, file list, image) along with extracted text and file paths.

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

Writes a list of file paths to the clipboard in native file drop format for pasting into File Explorer and file-aware applications.

```javascript
const result = await fb.clipboard.writeFiles([
	'C:\\Music\\one.flac',
	'C:\\Music\\two.flac'
]);
```

## writeHTML(html, plainText?)

Signature: `fb.clipboard.writeHTML(html: string, plainText?: string): Promise<ClipboardWriteHTMLResponse>`

Writes rich text to the clipboard as `HTML Format` with a plain-text fallback: uses `plainText` when provided and non-empty; defaults to the raw HTML source text when omitted or empty.

```javascript
await fb.clipboard.writeHTML('<strong>Now playing</strong>', 'Now playing');
```

# fb.console Host Console

`fb.console` writes messages to the foobar2000 console window. It is separate from the browser's global `console` object.

Each method writes one line. With only `message`, the line is the message as it is. Any values after `message` are appended to the same line, separated by spaces: strings as they are, any other JSON value (number, boolean, `null`, array, object) as its JSON text, like the browser console. The SDK then sends everything as the host's `args`, because the host reads `args` only when `message` is absent.

## error(message, ...args)

Signature: `fb.console.error(message: string, ...args: NonNullable<ConsoleErrorParams['args']>): Promise<ConsoleErrorResponse>`

Writes an error-level line, prefixed with `[WebView][ERROR]`.

```javascript
await fb.console.error('Artwork loading failed');
```

## log(message, ...args)

Signature: `fb.console.log(message: string, ...args: NonNullable<ConsoleLogParams['args']>): Promise<ConsoleLogResponse>`

Writes an informational line, prefixed with `[WebView]`.

```javascript
await fb.console.log('Theme initialized');

// Writes: [WebView] Loaded 42 tracks {"view":"albums"}
const r = await fb.console.log('Loaded', 42, 'tracks', { view: 'albums' });
if (r.success === false) {
    console.warn('console.log failed:', r.error);
}
```

## warn(message, ...args)

Signature: `fb.console.warn(message: string, ...args: NonNullable<ConsoleWarnParams['args']>): Promise<ConsoleWarnResponse>`

Writes a warning-level line, prefixed with `[WebView][WARN]`.

```javascript
await fb.console.warn('Using fallback artwork');
await fb.console.warn('Cover missing for', 'C:\\Music\\a.flac');
```

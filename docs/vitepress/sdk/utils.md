# fb.utils Utilities

## ping()

Tests the bridge connection. Resolves to `{ pong: boolean }` from `test.ping`.

```javascript
const r = await fb.utils.ping(); // { pong: true }
```

## echo(message)

Echoes a message through `test.echo` and resolves to `{ message: string }`.

```javascript
const { message } = await fb.utils.echo('Hello');
```

## formatTitle(pattern, path?)

Evaluates a foobar2000 Title Formatting expression through `titleformat.eval`.

| Parameter | Type | Description |
| --- | --- | --- |
| pattern | string | Title Formatting pattern |
| path | string | Optional track path; omission evaluates against the current track |

The SDK preserves the host response envelope `{ result: string }`; it does not unwrap the string.

```javascript
const { result } = await fb.utils.formatTitle('%artist% - %title%');
console.log(result); // "The Beatles - Let It Be"

// Evaluate against a specific track
const r2 = await fb.utils.formatTitle('%codec% %bitrate%kbps', 'E:\\Music\\song.flac');
```

## getFileInfo(path)

Reads file metadata through `metadata.read` and returns its structured response.

| Parameter | Type | Description |
| --- | --- | --- |
| path | string | Audio file path |

```javascript
const info = await fb.utils.getFileInfo('E:\\Music\\song.flac');
// {success, path, tags: {TITLE, ARTIST, ...}, info: {duration, bitrate, sampleRate, channels, codec}}
```

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## SDK Method Stub

> This block records SDK method coverage and may later be expanded with complete examples and best practices.

### echo()

Signature: `fb.utils.echo(message: string): Promise<{ message: string }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| message | string | Yes | Message forwarded as `{ message }` to `test.echo` |

Returns the result of the underlying `test.echo` call.

```javascript
const result = await fb.utils.echo('Hello');
```

<!-- END AUTO-GENERATED SDK STUBS -->

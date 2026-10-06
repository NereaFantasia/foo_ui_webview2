# fb.utils Utilities

## ping()

Tests the bridge connection. Resolves to `{ pong, timestamp }` from `test.ping`; `timestamp` is the host's Unix time in seconds.

```javascript
const { pong } = await fb.utils.ping(); // pong === true
```

## echo(message)

Echoes a message through `test.echo`. The message comes back as `echo`; `input` holds the parameters the host received.

Signature: `fb.utils.echo(message: string): Promise<TestEchoResponse>`

| Parameter | Type | Description |
| --- | --- | --- |
| message | string | Message sent to `test.echo` as `{ message }` |

```javascript
const { echo } = await fb.utils.echo('Hello'); // echo === 'Hello'
```

## formatTitle(pattern, path?)

Evaluates a foobar2000 Title Formatting expression through `titleformat.eval`.

| Parameter | Type | Description |
| --- | --- | --- |
| pattern | string | Title Formatting pattern |
| path | string | Optional track path; omission evaluates against the current track |

The SDK preserves the host response envelope `{ result: string }`; it does not unwrap the string.

```javascript
const res = await fb.utils.formatTitle('%artist% - %title%');
if (res.success === false) throw new Error(res.error);
const { result } = res;
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

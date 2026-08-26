# fb.rating Track Ratings

`fb.rating` reads and writes integer track ratings on a 0-5 scale.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## Additional methods

> This block records SDK method coverage and may later be expanded with complete examples and best practices.

### get()

Signature: `fb.rating.get(path: string): Promise<{ rating: number }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| path | string | Yes | Absolute file path; may include a `\|subsong:N` suffix for a CUE entry |

Returns `{ rating }`. Use `set(path, rating, { cueIndex? })` to write a rating; `rating` must be an integer from 0 to 5, where 0 clears it. An explicit `cueIndex` takes precedence over a subsong suffix. The set response may identify the `menuPath`, storage backend, or a fallback note.

```javascript
const result = await fb.rating.get('E:\\Music\\song.flac');
await fb.rating.set('E:\\Music\\song.flac', 4);
```

<!-- END AUTO-GENERATED SDK STUBS -->

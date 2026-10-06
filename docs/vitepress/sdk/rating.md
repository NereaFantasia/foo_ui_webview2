# fb.rating Track Ratings

`fb.rating` reads and writes integer track ratings on a 0-5 scale.

## get(path, opts?)

Signature: `fb.rating.get(path: string, opts?: { cueIndex?: number }): Promise<RatingGetResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| path | string | Yes | Absolute file path; may include a `\|subsong:N` suffix for a CUE entry |
| opts.cueIndex | number | No | Explicit CUE subsong index; takes precedence over a `\|subsong:N` suffix |

Returns `{ path, rating, storage }`; `storage` is `stats` for a foo_playcount rating and `file` otherwise. A foo_playcount `%rating%` between 1 and 5 wins; otherwise the file's `RATING` tag is read and clamped to 0-5. `0` means unrated. `storage` is also `file` when neither source has a rating, so it cannot tell a missing tag from a stored `0`. A path that cannot be opened fails with `OPERATION_FAILED`.

```javascript
const result = await fb.rating.get('E:\\Music\\song.flac');
if (result.success === false) throw new Error(result.error);
console.log(result.rating, result.storage); // 0-5
```

## set(path, rating, opts?)

Signature: `fb.rating.set(path: string, rating: number, opts?: { cueIndex?: number }): Promise<RatingSetResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| path | string | Yes | Absolute file path; may include a `\|subsong:N` suffix for a CUE entry |
| rating | number | Yes | Integer from 0 to 5; `0` clears the rating |
| opts.cueIndex | number | No | Explicit CUE subsong index; takes precedence over a `\|subsong:N` suffix |

Sets the track's rating through foo_playcount's rating command in the context menu. When that command is not available, the host dispatches a write of the file's `RATING` tag instead. A rating outside 0-5 fails with `INVALID_PARAMS`.

Returns `{ path, rating, storage }`. `storage` is `stats` when foo_playcount's command ran, and the response then names it in `menuPath`; `storage` is `file` when the tag write was dispatched, with the reason in `note`.

```javascript
await fb.rating.set('E:\\Music\\song.flac', 4);
await fb.rating.set('E:\\Music\\song.flac', 0); // Clear the rating
```

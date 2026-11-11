# fb.rating Track Ratings

`fb.rating` reads and writes integer track ratings on a 0-5 scale.

## get(path, opts?)

Signature: `fb.rating.get(path: string, opts?: { cueIndex?: number }): Promise<RatingGetResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| path | string | Yes | Absolute file path; may include a `\|subsong:N` suffix for a CUE entry |
| opts.cueIndex | number | No | Explicit CUE subsong index; takes precedence over a `\|subsong:N` suffix |

Returns `{ path, rating, storage }`; `storage` is `'stats'` when read from the foo_playcount database and `'file'` when read from audio tags. A foo_playcount `%rating%` between 1 and 5 takes precedence; otherwise the file's `RATING` tag is read and clamped to 0–5. `0` indicates unrated. If neither source specifies a rating, `storage` defaults to `'file'`. A path that cannot be opened fails with `OPERATION_FAILED`.

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

Sets track rating via foo_playcount context menu commands. When that command is not available, the host falls back to asynchronously writing the file's `RATING` tag. Ratings outside 0–5 fail with `INVALID_PARAMS`.

Returns `{ path, rating, storage }`. `storage` is `'stats'` when updated through foo_playcount (with the menu path recorded in `menuPath`); `storage` is `'file'` when falling back to tag writing (with the reason noted in `note`).

```javascript
await fb.rating.set('E:\\Music\\song.flac', 4);
await fb.rating.set('E:\\Music\\song.flac', 0); // Clear the rating
```

# fb.playcount Playback Statistics

`fb.playcount` reads playback statistics supplied by `foo_playcount`. The namespace provides single-track and batch reads plus library-wide aggregates.

## getBatch(paths)

Signature: `fb.playcount.getBatch(paths: string[]): Promise<PlaycountGetBatchResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | Track paths to inspect |

Returns `{ success, count, results }`. Each `PlaycountInfo` result carries its own `success` flag and may include `playCount`, `firstPlayed`, `lastPlayed`, `added`, `rating`, and `inLibrary`.

The host treats this call exactly like `get()`: it resolves paths in request order, echoing each `path` verbatim (including any `|subsong:N` suffix). `playCount` is `0` for unplayed tracks; `rating` ranges from 1 to 5 and is omitted when unrated; timestamp fields are omitted when unrecorded. Paths that fail to resolve are marked with `success: false` and an `error` description. When foo_playcount is not installed, play counts read as `0` and dates are omitted.

```javascript
const result = await fb.playcount.getBatch([
	'E:\\Music\\one.flac',
	'E:\\Music\\two.flac',
]);
```

## getStats()

Signature: `fb.playcount.getStats(): Promise<PlaycountGetStatsResponse>`

Returns aggregate fields including `totalTracks`, `playedTracks`, `unplayedTracks`, `ratedTracks`, `totalPlayCount`, `maxPlayCount`, `averagePlayCount`, and `averageRating`. The totals cover the whole media library. `averagePlayCount` is the mean over played tracks and `averageRating` the mean over rated tracks; each is `0` when there are none.

```javascript
const stats = await fb.playcount.getStats();
```

## set(path, count)

Signature: `fb.playcount.set(path: string, count: number): Promise<PlaycountSetResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path |
| `count` | `number` | Yes | Compatibility argument accepted by the SDK facade |

::: warning Deprecated
This method is deprecated. The underlying component provides no interface to modify playback statistics, so the call always fails with `code: 'NOT_SUPPORTED'` and the promise resolves with `{ success: false }`. The SDK sends only the `path` parameter, ignoring `count`. Use `fb.rating.set()` to update ratings; play counts update automatically during playback.
:::

```javascript
const result = await fb.playcount.set('E:\\Music\\song.flac', 10);
// result.success === false, result.code === 'NOT_SUPPORTED'
```

## Single-track Read

### get(path)

Signature: `fb.playcount.get(path: string): Promise<PlaycountGetResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path; a `\|subsong:N` suffix selects a CUE subsong |

`fb.playcount.get()` invokes `playcount.get` with `paths: [path]`, resolving with `{ count, results }` where `results[0]` holds the track details; failed calls resolve with a failure envelope. Entry properties match `getBatch()`: `success`, `playCount`, `rating` (omitted when unrated), `inLibrary`, and timestamp fields `firstPlayed`, `lastPlayed`, and `added` when recorded.

```javascript
const res = await fb.playcount.get('E:\\Music\\song.flac');
const info = res.success ? res.results[0] : undefined;
if (info?.success) {
	console.log(info.playCount, info.lastPlayed);
}
```

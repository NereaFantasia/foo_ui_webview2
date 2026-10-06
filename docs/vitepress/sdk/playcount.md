# fb.playcount Playback Statistics

`fb.playcount` reads playback statistics supplied by `foo_playcount`. The namespace provides single-track and batch reads plus library-wide aggregates.

## getBatch(paths)

Signature: `fb.playcount.getBatch(paths: string[]): Promise<PlaycountGetBatchResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | Track paths to inspect. |

Returns `{ success, count, results }`. Each `PlaycountInfo` result carries its own `success` flag and may include `playCount`, `firstPlayed`, `lastPlayed`, `added`, `rating`, and `inLibrary`.

The host treats this call exactly like `get()`: one row per path in request order, each echoing its `path` as given, `|subsong:N` suffix included. `playCount` is `0` for a track never played, `rating` runs from 1 to 5 and is absent when the track is unrated, and each date is absent when unknown. A path that cannot be resolved gets `success: false` and an `error`. Without foo_playcount installed, counts read as `0` and the dates are absent.

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
| `path` | `string` | Yes | Track path. |
| `count` | `number` | Yes | Compatibility argument accepted by the SDK facade. |

::: warning Deprecated
This method is deprecated. foo_playcount offers no way to change playback statistics, so the host always fails the call with `code: 'NOT_SUPPORTED'` and the promise resolves `{ success: false }`. The SDK sends only `path`; `count` is kept for signature compatibility and never reaches the host. Use `fb.rating.set()` for ratings, and let actual playback update play counts.
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
| `path` | `string` | Yes | Track path; a `\|subsong:N` suffix selects a CUE subsong. |

`fb.playcount.get()` calls the registered `playcount.get` handler with `paths: [path]` and resolves with its envelope `{ count, results }`, so the track's entry is `results[0]`; a failed call resolves with a failure envelope. The entry has the fields described under `getBatch()`: `success`, `playCount`, `rating` (absent when unrated), `inLibrary`, and the dates `firstPlayed`, `lastPlayed`, and `added` when known.

```javascript
const res = await fb.playcount.get('E:\\Music\\song.flac');
const info = res.success ? res.results[0] : undefined;
if (info?.success) {
	console.log(info.playCount, info.lastPlayed);
}
```

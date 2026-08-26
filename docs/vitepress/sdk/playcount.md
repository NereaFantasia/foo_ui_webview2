# fb.playcount Playback Statistics

`fb.playcount` reads playback statistics supplied by `foo_playcount`. The namespace provides single-track and batch reads plus library-wide aggregates.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## Additional methods

> This block maintains SDK-facing method coverage and may be expanded with complete examples and best practices.

### getBatch()

Signature: `fb.playcount.getBatch(paths: string[]): Promise<PlaycountGetResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | Track paths to inspect. |

Returns `{ success, count, results }`. Each `PlaycountInfo` result carries its own `success` flag and may include `playCount`, `firstPlayed`, `lastPlayed`, `added`, `rating`, and `inLibrary`.

```javascript
const result = await fb.playcount.getBatch([
	'E:\\Music\\one.flac',
	'E:\\Music\\two.flac',
]);
```

### getStats()

Signature: `fb.playcount.getStats(): Promise<PlaycountStats>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| None | — | — | This method takes no arguments. |

Returns aggregate fields including `totalTracks`, `playedTracks`, `unplayedTracks`, `ratedTracks`, `totalPlayCount`, `maxPlayCount`, `averagePlayCount`, and `averageRating`.

```javascript
const stats = await fb.playcount.getStats();
```

### set()

Signature: `fb.playcount.set(path: string, count: number): Promise<BaseResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path. |
| `count` | `number` | Yes | Compatibility argument accepted by the SDK facade. |

This method is deprecated. The host does not support direct play-count mutation and returns `{ success: false }`; the `count` argument is not applied. Use `fb.rating.set()` for ratings, and let actual playback update play counts.

```javascript
const result = await fb.playcount.set('E:\\Music\\song.flac', 10);
```

<!-- END AUTO-GENERATED SDK STUBS -->

## Single-track Read

`fb.playcount.get(path: string): Promise<PlaycountInfo | null>` calls the registered `playcount.get` handler with `paths: [path]` and unwraps the first result. It resolves to `null` when the host returns an empty or failed envelope.

```javascript
const info = await fb.playcount.get('E:\\Music\\song.flac');
if (info?.success) {
	console.log(info.playCount, info.lastPlayed);
}
```

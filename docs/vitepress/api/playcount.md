# Playcount API

Methods of the `playcount` namespace.

## playcount

### playcount.get

<!-- api-schema:begin playcount.get -->
Read the foo_playcount statistics of tracks. Without foo_playcount installed the counts read as zero and the dates are absent.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | Track paths; a `\|subsong:N` suffix selects a CUE subsong. A suffix whose index cannot be read, such as `\|subsong:abc`, is dropped and the path means the first track. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `count` | `integer` | Number of rows in `results`. |
| `results` | `PlaycountRow[]` | One row per requested path, in request order. |
| `results[].path` | `string` | The path as requested, including any `\|subsong:N` suffix, on successful and failed rows alike. |
| `results[].success` | `boolean` | Whether the track could be resolved. |
| `results[].error` | `string` | Why the track could not be resolved; present when `success` is `false`. |
| `results[].playCount` | `integer` | Number of plays; `0` when never played. Present when `success` is `true`. |
| `results[].firstPlayed` | `string` | Time of the first play, as foo_playcount formats it. Absent when unknown. |
| `results[].lastPlayed` | `string` | Time of the most recent play. Absent when unknown. |
| `results[].added` | `string` | Time the track was added to the media library. Absent when unknown. |
| `results[].rating` | `integer` | Rating from 1 to 5. Absent when the track is unrated, never `0`. |
| `results[].inLibrary` | `boolean` | Whether the track is in the media library. Present when `success` is `true`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('playcount.get', {
    paths: ['C:\\Music\\song.flac']
});
if (res.success === false) throw new Error(res.error);
const { results } = res;
```

### playcount.getBatch

<!-- api-schema:begin playcount.getBatch -->
Same as `playcount.get`, under the name batch callers expect.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | Track paths; a `\|subsong:N` suffix selects a CUE subsong. A suffix whose index cannot be read, such as `\|subsong:abc`, is dropped and the path means the first track. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `count` | `integer` | Number of rows in `results`. |
| `results` | `PlaycountRow[]` | One row per requested path, in request order. |
| `results[].path` | `string` | The path as requested, including any `\|subsong:N` suffix, on successful and failed rows alike. |
| `results[].success` | `boolean` | Whether the track could be resolved. |
| `results[].error` | `string` | Why the track could not be resolved; present when `success` is `false`. |
| `results[].playCount` | `integer` | Number of plays; `0` when never played. Present when `success` is `true`. |
| `results[].firstPlayed` | `string` | Time of the first play, as foo_playcount formats it. Absent when unknown. |
| `results[].lastPlayed` | `string` | Time of the most recent play. Absent when unknown. |
| `results[].added` | `string` | Time the track was added to the media library. Absent when unknown. |
| `results[].rating` | `integer` | Rating from 1 to 5. Absent when the track is unrated, never `0`. |
| `results[].inLibrary` | `boolean` | Whether the track is in the media library. Present when `success` is `true`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playcount.getBatch', { paths: ['C:\\Music\\song.flac'] });
```

### playcount.getStats

<!-- api-schema:begin playcount.getStats -->
Summarise play counts and ratings over the whole media library.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `totalTracks` | `integer` | Tracks in the media library. |
| `playedTracks` | `integer` | Tracks played at least once. |
| `unplayedTracks` | `integer` | Tracks never played. |
| `ratedTracks` | `integer` | Tracks with a rating. |
| `totalPlayCount` | `integer` | Sum of all play counts. |
| `maxPlayCount` | `integer` | Highest play count of any track. |
| `averagePlayCount` | `number` | Mean play count of the played tracks; `0` when none was played. |
| `averageRating` | `number` | Mean rating of the rated tracks; `0` when none is rated. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playcount.getStats');
```

### playcount.set

<!-- api-schema:begin playcount.set -->
Placeholder: foo_playcount offers no way to change statistics, so this always fails with `NOT_SUPPORTED`. Change ratings with `rating.set`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Path of the track. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playcount.set', { path: 'C:\\Music\\song.flac' });
```

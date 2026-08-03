# Stats reference

Cross-page reference for playcount and rating surfaces. Primary owners remain:

- [Playcount API](/api/playcount)
- [Metadata / Rating API](/api/metadata)

## rating.set

Sets a track rating. Prefers the foo_playcount context-menu path when available; otherwise falls back to writing the file `RATING` tag.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | string | No | Audio path. Supports `path\|subsong:N`. Omit to target the now-playing item when the handler allows it. |
| `rating` | number | Yes | `0`–`5`; `0` clears the rating. |
| `cueIndex` | number | No | CUE subsong index. Takes priority over `\|subsong:N` embedded in `path`. |

::: warning Dependency
Install [foo_playcount](https://www.foobar2000.org/components/view/foo_playcount) for stats-backed ratings.
:::


## rating.get

Reads a track rating. Prefers foo_playcount (`%rating%` titleformat) and falls back to the file `RATING` tag.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | string | Yes | Audio path. Supports `path\|subsong:N`. |
| `cueIndex` | number | No | CUE subsong index. |

Example return:

```json
{
  "success": true,
  "path": "C:\\Music\\song.flac",
  "rating": 4,
  "storage": "stats"
}
```

| Field | Type | Description |
| --- | --- | --- |
| `rating` | number | `0`–`5`; `0` means unrated |
| `storage` | string | `"stats"` (foo_playcount) or `"file"` (file tag) |


## playcount.get

Returns playcount statistics from foo_playcount. Full parameter/return contract: [Playcount API](/api/playcount#playcount-get).

## playcount.getBatch

Alias of `playcount.get` with the same parameters and return shape. See [Playcount API](/api/playcount#playcount-getbatch).

## playcount.getStats

Returns whole-library playcount aggregates. See [Playcount API](/api/playcount#playcount-getstats).

## playcount.set

Registered placeholder only. It requires `path` but always returns `success: false` and directs callers to `rating.set` for ratings. It does not mutate play counts.

## Related titleformat fields

When foo_playcount is installed, titleformat fields such as `%play_count%`, `%rating%`, and `%last_played%` may be available through the Titleformat API.

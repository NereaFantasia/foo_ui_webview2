# Rating API

Methods of the `rating` namespace. For a track inside a CUE sheet or another container, see [Addressing a track inside a container](./metadata.md#subsong-addressing).

## rating

### rating.get

<!-- api-schema:begin rating.get -->
Read a track's rating from 0 to 5: a foo_playcount `%rating%` between 1 and 5 wins, otherwise the file's `RATING` tag clamped to 0..5. `0` means unrated. A file whose tags cannot be read, a missing file included, is not an error: it reads as having no `RATING` tag.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path; a `\|subsong:N` suffix or a trailing `#N` selects a subsong. Must not be empty. |
| `cueIndex` | `integer` | No | Subsong index that wins over a suffix in `path`; negative values are ignored. Default: `-1`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `path` | `string` | The path as given, suffix included. |
| `rating` | `integer` | Rating from 0 to 5; `0` means unrated. |
| `storage` | `"stats" \| "file"` | `stats` for a foo_playcount rating; `file` for the `RATING` tag, and also when neither source has a rating. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('rating.get', {
	path: 'C:\\Music\\song.flac',
});
if (res.success === false) throw new Error(res.error);
const { rating, storage } = res;
```

### rating.set

<!-- api-schema:begin rating.set -->
Set a track's rating through foo_playcount's context menu, or write the file's `RATING` tag when that menu is not available. Without `path` the playing track is rated, or else the active playlist's selection; with neither the call fails with `NO_ACTIVE_ITEM`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | No | Track path; a `\|subsong:N` suffix or a trailing `#N` selects a subsong. Omit it for the playing track, or else the active playlist's selection. Must not be empty. |
| `rating` | `integer` | Yes | Rating from 0 to 5; `0` clears it. Between `0` and `5` inclusive. |
| `cueIndex` | `integer` | No | Subsong index that wins over a suffix in `path`; negative values are ignored. Default: `-1`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `path` | `string` | The path as given; `(current)` when `path` was omitted and foo_playcount took the rating. |
| `rating` | `integer` | The rating that was set. |
| `storage` | `"stats" \| "file"` | `stats` when foo_playcount's menu ran; `file` when the `RATING` tag write was dispatched. |
| `menuPath` | `string` | The context-menu path that ran; present when `storage` is `stats`. |
| `note` | `string` | Why the tag was written instead; present when `storage` is `file`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('rating.set', {
	path: 'C:\\Music\\song.flac',
	rating: 5,
});
```

## Usage notes

- `rating.set` accepts values from `0` through `5`; `0` removes the rating. It uses foo_playcount when a matching context-menu command is available and otherwise writes the `RATING` file tag. `rating.get` reports its source through `storage` as either `stats` or `file`.

# Lyrics API

Methods of the `lyrics` namespace.

## lyrics

### lyrics.exists

<!-- api-schema:begin lyrics.exists -->
List the known lyrics tags and sidecar files that exist for a track. File candidates and access checks match `lyrics.get`, but contents are not read: empty tags and files still count as existing.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path; a `\|subsong:N` suffix selects a subsong. Must not be empty. |
| `filename` | `string` | No | Exact file name, including any extension, next to the audio. Replaces the automatic file candidates; embedded tags are still checked. Empty means automatic lookup. The same Windows filename rules and `INVALID_PARAMS` failure as `lyrics.get` apply. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `exists` | `boolean` | Whether any source has lyrics. |
| `sources` | `string[]` | Every source found: `embedded` for a lyrics tag, then `file:<name>` for each sidecar file, in the same candidate order as `lyrics.get` with `format=any`, or just the explicit file. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('lyrics.exists', {
	path: 'C:\\Music\\song.flac',
});
if (res.success === false) throw new Error(res.error);
const { exists, sources } = res;
```

### lyrics.get

<!-- api-schema:begin lyrics.get -->
Read non-empty lyrics from the five known lyrics tags, then files next to the audio. Automatic lookup tries the audio file's stem only for a verified single-track file, followed by `<first artist> - <title>`; subsongs use only the latter. Unknown input layouts skip the audio-stem candidate. No numbered or shared album fallback is used. Results are cached for 1200 ms per track, filename and filters. Successful file saves and metadata changes invalidate the cache.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | No | Track path; a `\|subsong:N` suffix selects a subsong. Omit it for the playing track; with nothing playing the call fails with `NO_ACTIVE_ITEM`. The playing track keeps its subsong. Must not be empty. |
| `filename` | `string` | No | Exact file name, including any extension, next to the audio. A non-empty value replaces all automatic file candidates and ignores `format`; `source` and `type` still apply. Empty means automatic lookup. Must be a valid single Windows filename, without path syntax, device names or trailing dots/spaces; invalid names fail with `INVALID_PARAMS`. |
| `source` | `"embedded" \| "file" \| "any"` | No | Where to look: the embedded tags, the sidecar files, or both in that order. Default: `"any"`. |
| `type` | `"synced" \| "unsynced" \| "any"` | No | Keep only synced lyrics (with LRC timestamps) or only unsynced ones. Default: `"any"`. |
| `format` | `"lrc" \| "txt" \| "any"` | No | Sidecar extension to try; `.lrc` candidates are tried before `.txt` with `any`. Applies to automatic sidecar lookup only; a non-empty `filename` takes precedence. Default: `"any"`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `available` | `boolean` | Whether lyrics were found. |
| `path` | `string` | The path looked up: as given, or the playing track's path including its subsong suffix. |
| `source` | `"embedded" \| "file"` | Where the lyrics came from; present when `available` is `true`. |
| `sourcePath` | `string` | Full path of the sidecar file; present when `source` is `file`. |
| `tagName` | `string` | The tag that matched a `type` filter, one of `LYRICS`, `UNSYNCED LYRICS`, `UNSYNCEDLYRICS`, `SYNCEDLYRICS`, `SYNCED LYRICS`; present when `source` is `embedded`, `type` was not `any` and a known tag matched. |
| `lyrics` | `string` | The lyrics text; present when `available` is `true`. |
| `synced` | `boolean` | Whether the text carries LRC timestamps; present when `available` is `true`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('lyrics.get', {
	path: 'C:\\Music\\song.flac',
	type: 'synced',
});
if (res.success === false) throw new Error(res.error);
const { available, lyrics, synced } = res;
```

### lyrics.save

<!-- api-schema:begin lyrics.save -->
Save lyrics to a sidecar file, an embedded tag, or both. A file save invalidates the read cache; an embedded save dispatches an asynchronous write and metadata changes invalidate the cache. Without `filename`, verified single-track files use the audio stem; other tracks use `<first artist> - <title>`. Missing tags in the latter case fail that target with `INVALID_PARAMS`. Existing files are overwritten, so use distinct filenames when tracks have the same artist and title. With one target the result describes that write; with several, each outcome is under `results` and `OPERATION_FAILED` means all failed.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path the lyrics belong to; a `\|subsong:N` suffix selects a subsong. Must not be empty. |
| `lyrics` | `string` | Yes | Lyrics text to save. Must not be empty. |
| `target` | `string[]` | No | Where to write: `file` (a sidecar next to the audio file), `embedded` (a tag in the file), or `all` for both; several entries write to each. Any other value fails with `INVALID_PARAMS`. Omitted means `file`. Must not be empty. |
| `filename` | `string` | No | Exact file name, including any extension, for the `file` target. A non-empty value replaces the derived name and ignores `format`; empty means the default name. The same Windows filename rules and `INVALID_PARAMS` failure as `lyrics.get` apply. |
| `tagName` | `string` | No | Tag the `embedded` target writes. Default: `"LYRICS"`. |
| `format` | `"lrc" \| "txt"` | No | Extension for the default sidecar name; ignored when `filename` is non-empty. Default: `"lrc"`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `savedTo` | `string` | Full path of the file written; present for a single `file` target. |
| `dispatched` | `boolean` | `true` when a single `embedded` target dispatched the tag write; the final outcome arrives as `metadata:writeComplete`. |
| `path` | `string` | The path as given; present for a single `embedded` target. |
| `handlePath` | `string` | Path of the handle the tag write targets; present for a single `embedded` target. |
| `subsong` | `integer` | Subsong the tag write targets; present for a single `embedded` target. |
| `tagsApplied` | `Record<string, string>` | The tag written, keyed by its upper-case name; present for a single `embedded` target. |
| `tagsSet` | `integer` | Number of tags set; present for a single `embedded` target. |
| `tagsRemoved` | `integer` | Number of tags removed; present for a single `embedded` target. |
| `note` | `string` | A note about the dispatch; present for a single `embedded` target. |
| `results` | `Record<string, any>` | With several targets: each target's own envelope (`{ success, savedTo }`, the tag write receipt, or `{ success: false, error, code }`) under `file` or `embedded`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('lyrics.save', {
	path: 'C:\\Music\\song.flac',
	lyrics: '[00:12.00]First line\n[00:18.50]Second line\n',
});
```

## Sidecar lookup {#sidecar-lookup}

Only the audio file's directory is searched. The profile folder and subdirectories are not searched, and names are not matched by similarity.

| Order | File name | When used |
| --- | --- | --- |
| 1 | The audio stem, such as `song.lrc` for `song.wav` | The input reader confirms exactly one track, with subsong ID 0 |
| 2 | `<first artist> - <title>` | Both tags exist; invalid filename characters become `_` |

All `.lrc` candidates are tried before any `.txt` candidates. An unknown input layout skips the audio-stem candidate. Missing artist or title skips the tagged candidate. Multiple artist values are not joined, and remix or live-version names are not treated as interchangeable.

`lyrics.get` returns the first readable, non-empty file matching `type`. `lyrics.exists` uses the same candidates and access checks but reports all existing sources, such as `file:song.lrc`. Empty files and known empty tags count as existing, so `exists: true` does not guarantee `get.available: true`.

Portable `file-relative://` paths are supported. Archive entries (`archive://`, `unpack://`) do not identify a directory in which sidecars can be accessed.

## Containers and subsongs

A subsong is an input plugin's track identifier, not an ordinal that can always be incremented to obtain a track number. Even subsong 0 in a multi-track container skips the shared audio-stem file.

Subsongs use the artist-title name for automatic reads and default saves. If either tag is missing, the file save fails with `INVALID_PARAMS`; specify `filename` instead. Tracks with identical artist and title may share a filename. Saves overwrite existing content, so use distinct names when needed.

### Explicit filenames

All three methods accept `filename`, including its extension. A non-empty value restricts file access to that one name with no fallback, and overrides `format`; other extensions are allowed. `source` and `type` still apply. An empty string means the default lookup or name.

With `source: 'any'`, embedded tags still take priority; use `source: 'file'` to read only the selected file. `exists` still checks embedded tags. The name must be a valid single Windows filename, without path separators, reserved device names, invalid characters or trailing dots/spaces.

```javascript
const path = 'D:\\album.flac|subsong:2';
const filename = 'chosen-track.lrc';

const saved = await fb.lyrics.save(path, '[00:01.00]Track lyrics', {
    target: 'file',
    filename,
});
if (saved.success === false) throw new Error(saved.error);

const found = await fb.lyrics.exists(path, { filename });
if (found.success === false) throw new Error(found.error);

const result = await fb.lyrics.get(path, { source: 'file', filename });
if (result.success === false) throw new Error(result.error);
if (result.available) console.log(result.lyrics);
```

## Save targets and cache

The Bridge takes `target` as an array, defaulting to `['file']`; the SDK also accepts a single string. `file` writes beside the audio, `embedded` dispatches a tag write, and `all` tries both and reports each outcome. Support for writing a container's tags depends on its foobar2000 input plugin.

Successful file saves and metadata changes invalidate the 1200 ms read cache. An embedded save is asynchronous: subscribe to `metadata:writeComplete` before saving, then read after completion. Dispatch is not proof that the file was written; see [Metadata API](/api/metadata) for event subscription.

### Migrating older callers

The `config` save target has been removed; `all` now means `file` and `embedded` only. Existing profile lyrics are neither deleted nor searched.

Numbered `.NN.lrc` and shared album-stem files are no longer selected automatically for subsongs. If you know which track an existing file belongs to, pass its exact `filename` without renaming it.

## Related

- [`artwork.getLyrics`](/api/artwork#artwork-getlyrics) reads only the five known lyrics tags.
- [`<fb-lyrics-panel>`](/components/media) uses these APIs to load lyrics.

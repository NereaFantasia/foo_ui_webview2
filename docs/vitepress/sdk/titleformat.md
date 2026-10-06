# fb.titleformat Title Formatting

`fb.titleformat` evaluates foobar2000 Title Formatting expressions for one or more tracks, and lists commonly used fields through `getBuiltinFields()`.

## eval(pattern, path?)

Signature: `fb.titleformat.eval(pattern: string, path?: string): Promise<TitleformatEvalResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| pattern | string | Yes | Title Formatting pattern, such as `%artist% - %title%` |
| path | string | No | Track path; a `\|subsong:N` suffix selects a subsong. Omit it to evaluate the playing track |

Evaluates one pattern against one track and returns `{ path, pattern, result, infoAvailable }`, where `result` is the formatted text. Without `path` it evaluates the playing track; see "Evaluating the playing track" below. An invalid pattern fails with `INVALID_PARAMS`, and a path that cannot be opened fails with `INVALID_PATH`.

```javascript
const r = await fb.titleformat.eval('%artist% - %title%', 'E:\\Music\\song.flac');
if (r.success === false) throw new Error(r.error);
console.log(r.result); // 'Artist - Title'
```

## evalBatch(pattern, paths)

Signature: `fb.titleformat.evalBatch(pattern: string, paths: string[]): Promise<TitleformatEvalBatchResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| pattern | string | Yes | Title Formatting pattern applied to every path |
| paths | string[] | Yes | Track paths; a `\|subsong:N` suffix selects a subsong |

Evaluates one pattern against several tracks, compiling it once. Returns `pattern`, the counters `total`, `successCount`, and `errorCount`, and `results`: one row per path in request order, with `path`, `success`, and either `result` and `infoAvailable` or `error`. A path that cannot be opened gets a failed row instead of failing the call; an invalid pattern fails the whole call with `INVALID_PARAMS`.

```javascript
const batch = await fb.titleformat.evalBatch('%artist% - %title%', [
	'E:\\Music\\one.flac',
	'E:\\Music\\two.flac',
]);
if (batch.success === false) throw new Error(batch.error);
for (const row of batch.results) {
	console.log(row.path, row.success ? row.result : row.error);
}
```

## evalFields(path, fields)

Signature: `fb.titleformat.evalFields(path: string, fields: TitleformatFieldMap): Promise<TitleformatEvalFieldsResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| path | string | Yes | Track path; a `\|subsong:N` suffix selects a subsong |
| fields | Record<string, string> | Yes | Map from each output key to its Title Formatting pattern |

Evaluates several named patterns against one track in a single pass. Each key of `fields` comes back as a top-level field of the response holding its formatted text, beside `path` and `infoAvailable`; there is no nested `values` object. See "What the flag does not cover" below for key names to avoid. A path that cannot be opened fails with `INVALID_PATH`.

```javascript
const tags = await fb.titleformat.evalFields('E:\\Music\\song.flac', {
	artist: '%artist%',
	year: '$year(%date%)',
});
if (tags.success === false) throw new Error(tags.error);
console.log(tags.artist, tags.year);
```

## evalFieldsBatch(paths, fields)

Signature: `fb.titleformat.evalFieldsBatch(paths: string[], fields: TitleformatFieldMap): Promise<TitleformatEvalFieldsBatchResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| paths | string[] | Yes | Track paths to evaluate |
| fields | Record<string, string> | Yes | Map from each output key to its Title Formatting pattern |

Returns aggregate counts and one result per path. The host compiles the merged expression once before applying it across the batch. Each row carries `path`, `success`, and either one field per key of `fields` plus `infoAvailable`, or `error` when the path cannot be opened. A merged pattern that does not compile fails the whole call with `INVALID_PARAMS`.

```javascript
const result = await fb.titleformat.evalFieldsBatch(
	['E:\\Music\\one.flac', 'E:\\Music\\two.flac'],
	{ artist: '%artist%', title: '%title%' }
);
```

## getBuiltinFields()

Signature: `fb.titleformat.getBuiltinFields(): Promise<TitleformatGetBuiltinFieldsResponse>`

Returns `fields`, a map from a readable name to a Title Formatting pattern, such as `year` → `$year(%date%)` or `displayTitle` → `$if(%title%,%title%,%filename%)`. It is a fixed list of commonly used fields, not every field foobar2000 knows. The entries for play counts, ratings, and play dates need the foo_playcount component.

```javascript
const builtin = await fb.titleformat.getBuiltinFields();
if (builtin.success === false) throw new Error(builtin.error);
const year = await fb.titleformat.eval(builtin.fields.year, 'E:\\Music\\song.flac');
```

## Evaluating the playing track

`eval()` without a path, or with an empty one, evaluates the playing track through foobar2000's playback formatter. That also fills dynamic fields such as `%playback_time%`, `%isplaying%` and live-stream titles, which stay empty when a track is evaluated by path. `path` in the reply is then the playing track's path. With nothing playing the call resolves `success: false` with `code: 'NO_ACTIVE_ITEM'`. The other three methods always take paths.

```javascript
const now = await fb.titleformat.eval('%artist% - %title% [%playback_time%]');
if (now.success) {
	console.log(now.result);
}
```

## Info Availability

Every successful evaluation reports `infoAvailable`. foobar2000 formats a track from its metadb cache, where a local file it has not loaded (in no playlist or library) holds only a placeholder, so the host reads such files from disk first. The read runs synchronously on foobar2000's main thread, once per file that is not loaded, so a large batch of such files pauses the UI for as long as the reads take.

| Value | Meaning |
| --- | --- |
| `true` | The host had the track's info, from foobar2000's cache or read from the file, so tag-derived output is trustworthy. |
| `false` | The host could not get the track's info: a remote path with nothing cached, or a file that cannot be read. A placeholder `file_info` was rendered instead, so tag-derived output such as `%bitrate%` or `%codec%` cannot be trusted. |
| absent | The flag was never set. The exact cases differ per method — see "When the flag is absent" below. |

```javascript
const one = await fb.titleformat.eval('%bitrate%', 'E:\\Music\\one.flac');
if (one.success === false) throw new Error(one.error);
if (one.infoAvailable === false) {
	// Unreadable, or remote with nothing cached — show the value as unknown instead of empty.
}

const many = await fb.titleformat.evalFieldsBatch(
	['E:\\Music\\one.flac', 'E:\\Music\\two.flac'],
	{ bitrate: '%bitrate%', rating: '%rating%' }
);
if (many.success === false) throw new Error(many.error);
for (const row of many.results) {
	console.log(row.path, row.bitrate, row.infoAvailable);
}
```

### When the flag is absent

The two single-track methods and the two batch methods behave differently, so do not assume a missing flag means the same thing everywhere.

- `eval()` and `evalFields()` omit the flag on every failure envelope (`success: false`). `evalFields()` **also** omits it on a `success: true` envelope when `fields` contained no string-valued patterns, or when the merged pattern failed to compile — in both cases no evaluation ran.
- `evalBatch()` and `evalFieldsBatch()` omit it only on rows with `success: false`; a successful row always carries it. Neither ever returns a successful row without the flag — a pattern that fails to compile fails the whole call with a top-level `success: false` and no `results` at all. `evalFieldsBatch()` additionally returns `results: []`, with no rows to carry a flag, when `fields` contains no string-valued patterns. (`evalBatch()` takes a single `pattern` and has no `fields` argument, so that second case cannot arise for it.)

### What the flag does not cover

`evalFields()` and `evalFieldsBatch()` merge every requested pattern into one script and evaluate it in a single pass, so one boolean covers the entire row. It cannot separate an untrustworthy `%bitrate%` from a trustworthy `%rating%`.

foo_playcount virtual fields — `%rating%`, `%play_count%`, `%added%`, `%first_played%`, `%last_played%` — are resolved by a display-field provider instead of the track's own tags, so they remain valid even when the flag is `false`. Read `infoAvailable: false` as "tag-derived fields are untrustworthy", never as "the whole row is wrong".

Don't name a key of the `fields` map `success`, `path` or `infoAvailable`: the response fields of those names take precedence and the key is dropped. A key named `error` or `code` is kept, so check `success`, not `error`, to tell a failure.

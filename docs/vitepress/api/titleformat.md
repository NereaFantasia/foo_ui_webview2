# Titleformat API

Methods of the `titleformat` namespace.

## titleformat

### titleformat.eval

<!-- api-schema:begin titleformat.eval -->
Evaluate one title formatting pattern against one track, or against the playing track when no path is given.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | No | Path of the track to evaluate against; a `\|subsong:N` suffix selects a subsong, such as one track of a CUE sheet. Omit it to evaluate against the playing track, which also fills dynamic fields such as `%playback_time%` and stream titles; the call then fails with `NO_ACTIVE_ITEM` when nothing is playing. Must not be empty. |
| `pattern` | `string` | Yes | Title formatting pattern, such as `%artist% - %title%`. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `path` | `string` | The path that was evaluated; the playing track's path when the request gave none. |
| `pattern` | `string` | The pattern that was evaluated. |
| `result` | `string` | Formatted text. |
| `infoAvailable` | `boolean` | `false` when the host could not get the track's info (a remote path with nothing cached, or a file that cannot be read), so tag-derived output is untrustworthy. A local file that foobar2000 has not loaded is read from disk and reports `true`. Does not cover foo_playcount fields such as `%rating%`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('titleformat.eval', {
    path: 'C:\\Music\\song.flac',
    pattern: '%artist% - %title%'
});
if (res.success === false) throw new Error(res.error);
const { result } = res;
```

### titleformat.evalBatch

<!-- api-schema:begin titleformat.evalBatch -->
Evaluate one pattern against many tracks. The pattern is compiled once.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | Paths of the tracks to evaluate against; a `\|subsong:N` suffix selects a subsong. |
| `pattern` | `string` | Yes | Title formatting pattern applied to every path. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `pattern` | `string` | The pattern that was evaluated. |
| `total` | `integer` | Number of paths received. |
| `successCount` | `integer` | Rows that evaluated. |
| `errorCount` | `integer` | Rows that failed. |
| `results` | `EvalBatchRow[]` | One row per path, in request order. |
| `results[].path` | `string` | The path of this row. |
| `results[].success` | `boolean` | Whether this row evaluated. |
| `results[].result` | `string` | Formatted text; present when `success` is `true`. |
| `results[].infoAvailable` | `boolean` | Same meaning as in `titleformat.eval`; present when `success` is `true`. |
| `results[].error` | `string` | Why this row failed; present when `success` is `false`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('titleformat.evalBatch', {
    paths: ['C:\\Music\\song.flac', 'C:\\Music\\other.mp3'],
    pattern: '%codec% %bitrate%kbps'
});
if (res.success === false) throw new Error(res.error);
const { results } = res;
```

### titleformat.evalFields

<!-- api-schema:begin titleformat.evalFields -->
Evaluate several named patterns against one track in a single pass. The patterns are compiled as one script, so a pattern that fails to compile does not fail the call: the result then has no field keys and no `infoAvailable`, unlike `titleformat.evalFieldsBatch`, which fails with `INVALID_PARAMS`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Path of the track to evaluate against; a `\|subsong:N` suffix selects a subsong. Must not be empty. |
| `fields` | `Record<string, string>` | Yes | Map from output key to title formatting pattern, such as `{ "year": "$year(%date%)" }`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `path` | `string` | The path that was evaluated. |
| `infoAvailable` | `boolean` | `false` under the same conditions as in `titleformat.eval`, so tag-derived values are untrustworthy. One flag covers the whole request. Absent when `fields` is empty. |
| `[each key]` | `string` | One entry per key of `fields`, holding the formatted text. Keys named `success`, `path` or `infoAvailable` are dropped because the response fields of those names take precedence. Keys named `error` or `code` are kept, so check `success` to tell a failure. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('titleformat.evalFields', {
    fields: { title: '%title%', artist: '%artist%', length: '%length%' },
    path: 'C:\\Music\\song.flac'
});
```

### titleformat.evalFieldsBatch

<!-- api-schema:begin titleformat.evalFieldsBatch -->
Evaluate several named patterns against many tracks. The merged pattern is compiled once.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | Paths of the tracks to evaluate against; a `\|subsong:N` suffix selects a subsong. |
| `fields` | `Record<string, string>` | Yes | Map from output key to title formatting pattern. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `total` | `integer` | Number of paths received, or 0 when `fields` is empty. |
| `successCount` | `integer` | Rows that evaluated. |
| `errorCount` | `integer` | Rows that failed. |
| `results` | `(EvalFieldsBatchRow & Record<string, string>)[]` | One row per path, in request order. |
| `results[].path` | `string` | The path of this row. |
| `results[].success` | `boolean` | Whether this row evaluated. |
| `results[].infoAvailable` | `boolean` | Same meaning as in `titleformat.evalFields`; present when `success` is `true`. |
| `results[].error` | `string` | Why this row failed; present when `success` is `false`. |
| `results[].[each key]` | `string` | One entry per key of `fields`, holding the formatted text. Keys named `path`, `success` or `infoAvailable` are dropped because the row's own fields of those names take precedence. A key named `error` is kept, so check `success` to tell a failed row. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('titleformat.evalFieldsBatch', {
    fields: { title: '%title%', album: '%album%' },
    paths: ['C:\\Music\\song.flac', 'C:\\Music\\other.mp3']
});
if (res.success === false) throw new Error(res.error);
const { results } = res;
```

### titleformat.getBuiltinFields

<!-- api-schema:begin titleformat.getBuiltinFields -->
List commonly used title formatting fields, keyed by a readable name.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `fields` | `Record<string, string>` | Map from readable name to pattern, such as `"year": "$year(%date%)"`. Entries under playcount need the foo_playcount component. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('titleformat.getBuiltinFields');
```

## Evaluation semantics

- Parameters are checked before the handler runs. A missing or empty required string, a value of the wrong type, or a key the method does not declare returns `success: false` with `code: "INVALID_PARAMS"`.
- `titleformat.eval` requires a non-empty `path` and `pattern`. Invalid patterns and unavailable files return `success: false` with `error`.
- `titleformat.evalBatch` requires an array `paths` and a non-empty `pattern`; per-item failures are retained in `results`, while `successCount` and `errorCount` summarize the batch.
- `titleformat.evalFields` and `titleformat.evalFieldsBatch` require an object `fields` whose values are pattern strings. Each key becomes a response property. A non-string value fails the whole call with `INVALID_PARAMS`.
- `titleformat.getBuiltinFields` is a convenience reference generated by the runtime. It does not guarantee that optional component fields such as play-count data are populated for every installation or track.

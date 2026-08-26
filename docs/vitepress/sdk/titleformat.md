# fb.titleformat Titleformat

`fb.titleformat` evaluates foobar2000 Title Formatting expressions for one or more tracks. The namespace also exposes `eval()`, `evalBatch()`, `evalFields()`, and `getBuiltinFields()`.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## Additional methods

> This block records SDK method coverage and may later be expanded with complete examples and best practices.

### evalFieldsBatch()

Signature: `fb.titleformat.evalFieldsBatch(paths: string[], fields: Record<string, string>): Promise<TitleformatFieldsBatchResult>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| paths | string[] | Yes | Track paths to evaluate |
| fields | Record<string, string> | Yes | Map from each output key to its Title Formatting pattern |

Returns aggregate counts and one result per path. The host compiles the merged expression once before applying it across the batch.

```javascript
const result = await fb.titleformat.evalFieldsBatch(
	['E:\\Music\\one.flac', 'E:\\Music\\two.flac'],
	{ artist: '%artist%', title: '%title%' }
);
```

<!-- END AUTO-GENERATED SDK STUBS -->

## Info Availability

Every successful evaluation reports `infoAvailable`, taken from the host's `format_title` return value.

| Value | Meaning |
| --- | --- |
| `true` | The track's metadb info was ready, so tag-derived output is trustworthy. |
| `false` | The track's metadb info was not ready. A placeholder `file_info` was rendered instead, so tag-derived output such as `%bitrate%` or `%codec%` cannot be trusted. |
| absent | The flag was never set. The exact cases differ per method — see "When the flag is absent" below. |

```javascript
const one = await fb.titleformat.eval('%bitrate%', 'E:\\Music\\one.flac');
if (one.infoAvailable === false) {
	// Not in the metadb yet — show the value as unknown instead of empty.
}

const many = await fb.titleformat.evalFieldsBatch(
	['E:\\Music\\one.flac', 'E:\\Music\\two.flac'],
	{ bitrate: '%bitrate%', rating: '%rating%' }
);
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

A key named `infoAvailable` inside the `fields` map overwrites the flag with your own pattern output, matching the existing behaviour of the `path` and `success` keys.

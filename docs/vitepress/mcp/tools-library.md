# Library Tools

Four tools search and summarize the media library.

## fb2k_library_search

Searches the media library with foobar2000 query syntax, for example `artist IS Mili` or `%rating% GREATER 3`.

- **Bridge method**: `library.search`

| Parameter | Type | Required | Constraints |
| --- | --- | --- | --- |
| `query` | string | Yes | foobar2000 query expression |
| `offset` | integer | No | Declared minimum `0`; the Bridge default is `0` |
| `limit` | integer | No | Declared range `1` to `500`; the Bridge default is `100` |

The MCP declaration describes the runtime default but does not inject a `default` value. When `limit` is omitted, the request reaches `library.search`, whose handler uses `100`.

**Example result:**

```json
{
  "tracks": [
    { "title": "Redo", "artist": "Mili", "album": "Millennium Mother", "path": "..." }
  ],
  "total": 42
}
```

> Multi-value tags in `artist` / `albumArtist` / `genre` / `composer` (only the fields this API actually returns) are joined with `, ` in their original order, without de-duplication.

::: tip Query examples
- `artist IS Mili` — exact match
- `title HAS love` — substring match
- `%rating% GREATER 3` — numeric comparison
- `artist IS Mili AND album IS "Miracle Milk"` — combined expression

:::

## fb2k_library_get_albums

Gets all album names in the media library.

- **Parameters**: none
- **Bridge method**: `library.getAlbums`

## fb2k_library_get_artists

Gets all artist names in the media library.

- **Parameters**: none
- **Bridge method**: `library.getArtists`

## fb2k_library_get_stats

Gets media-library statistics.

- **Parameters**: none
- **Bridge method**: `library.getStats`

**Example result:**

```json
{
  "totalTracks": 12345,
  "totalAlbums": 678,
  "totalArtists": 234,
  "totalDuration": 3456789
}
```

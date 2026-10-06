# Changelog

All notable changes to foo-ui-webview2-mcp will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.2.0] - 2026-10-06

Released together with foo_ui_webview2 2.0.0, whose host methods these tools
call.

### Changed

- **Breaking:** the 101 bridge tools are now 10. Each covers one namespace, or
  one kind of change within it, and takes the host method to call as `action`,
  with that method's parameters beside it:

  | Tool | Actions |
  | --- | --- |
  | `fb2k_playback_read` | the `playback.get*` methods and `queue.get` |
  | `fb2k_playback_control` | transport, seeking, volume, mute, playback order, stop-after-current, `playback.playPath` and `playlist.playTrack` |
  | `fb2k_playlist_read` | reading playlists, their tracks, selection, focus, lock and autoplaylist state |
  | `fb2k_playlist_manage` | creating, removing, renaming, duplicating and reordering playlists, and autoplaylists |
  | `fb2k_playlist_edit` | adding, removing, moving, sorting and replacing a playlist's tracks, undo and redo |
  | `fb2k_playlist_select` | the active playlist, selection and focus |
  | `fb2k_library_read` | `library.search`, `getAlbums`, `getArtists` and `getStats` |
  | `fb2k_queue_edit` | every change to the playback queue |
  | `fb2k_track_read` | reading tags, ratings and artwork of track files |
  | `fb2k_track_write` | writing tags, ratings and artwork |

  An old call moves to the tool that holds its method, with the method as
  `action` and the same parameters: `fb2k_playback_get_current_track {}`
  becomes `fb2k_playback_read { "action": "playback.getCurrentTrack" }`, and
  `fb2k_playlist_play_track { "index": 0 }` becomes
  `fb2k_playback_control { "action": "playlist.playTrack", "index": 0 }`. The
  tool descriptions list every action with its parameters.
- **Breaking:** the UI-testing tools are now `fb2k_page_inspect`, whose
  `action` is `screenshot`, `domSnapshot` or `consoleMessages`, and
  `fb2k_page_evaluate`, which replaces `fb2k_evaluate` and is still registered
  only with `FB2K_ENABLE_EVAL`.
- **Breaking:** an argument the chosen method does not declare is refused
  before the call, including one that belongs to another action of the same
  tool; the error lists the parameters the method takes. Undeclared top-level
  arguments used to be passed on to the host, which refused them from 2.0.0 on.
- **Breaking:** `playback.setPosition` takes the target in `position`
  (seconds) instead of `seconds`, the only key the host accepts from 2.0.0 on.
- **Breaking:** `playback.setPlaybackOrder` takes either `order`, an index from
  0 to 6, or `name`, one of `default`, `repeat-playlist`, `repeat-track`,
  `random`, `shuffle-tracks`, `shuffle-albums` and `shuffle-folders`; give
  exactly one. `order` no longer accepts a name.
- **Breaking:** `metadata.embedArtwork` takes `target` as an array: any of
  `embedded`, `file` and `all`, for example `["file"]`. Omitted, the host
  writes `embedded`.
- Results are compact JSON rather than indented, and a result longer than
  `FB2K_MAX_RESPONSE_CHARS` characters (100000 by default) is cut, with a note
  suggesting a smaller page or fewer fields.
- **Breaking:** `artwork.getCurrent` and `artwork.getForTrack` return the cover
  as an MCP image, with the other fields as JSON text, instead of a `dataUrl`
  inside the JSON. A cover that is not PNG, JPEG, GIF or WebP, or is larger
  than `FB2K_MAX_IMAGE_BYTES` bytes (3932160, 3.75 MiB, by default), is left
  out, and the text says why.
- The parameters come from the host method declarations, so their types,
  ranges, defaults and descriptions match the host:
  - A path must not be empty and a list of paths needs at least one entry;
    both are refused before the call.
  - Declared defaults are filled in when an argument is omitted, for example
    `cueIndex` `-1` and `limit` `100`.
  - The artwork `type` also accepts `cover_front` and `cover_back`, the
    foobar2000 spellings of `front` and `back`.
  - A negative playlist index, row or queue position is refused before the
    call: `playlist`, `track` and each of `tracks` of `queue.add`, `playlist`
    of `queue.addPaths`, each of `items` of `playlist.removeTracks` and
    `playlist.moveTracks`, of `indices` of `playlist.setSelection` and
    `queue.remove`, and of `newOrder` of `playlist.reorder` and
    `playlist.reorderPlaylists`. The host itself now fails such a call with
    `INVALID_PARAMS` instead of dropping the negative items.
  - Each `handles` entry of `playlist.insertTracks` and `playlist.addHandles`
    is a path string or a `{ path, subsong }` object.
  - An `items` entry of `metadata.writeBatch`, `queue.setContents` or
    `queue.insertNext` with a key its method does not declare is refused
    before the call. `queue.setContents` no longer checks the entry shape
    itself: an entry that is neither `{ queueIndex }` nor `{ playlist, item }`
    reaches the host, which refuses the whole call before changing the queue.
- `queue.get` reports `playlist` and `playlistItem` as `null` for an entry
  without a playlist position; both keys are always present.
- The server reports the version of the package it ships in; it used to
  report `0.1.0`.

### Added

- Tool annotations. The read tools are marked read-only; `fb2k_playlist_manage`,
  `fb2k_playlist_edit`, `fb2k_queue_edit` and `fb2k_track_write` are marked
  destructive, since some of their actions remove or overwrite data. None of the
  bridge tools reaches beyond the foobar2000 instance.
- `FB2K_READ_ONLY`: set to `1` or `true`, the server registers only the
  read-only tools.
- A rate limit shared by every tool: 40 calls in a burst, then 20 a second. A
  call over it fails with a message saying how long to wait.
- The `queue.setContents`, `queue.insertNext` and `queue.playNow` actions.
  `queue.insertNext` takes `paths`, `items` (`{ playlist, item }` playlist
  positions, which the resulting entries keep so the playback cursor follows
  them), or both; at least one must be non-empty.
- Optional parameters the host methods declare beyond what the old tools took:
  `sort`, `query`, `offset`, `limit`, `includeTracks` and `useCache` on
  `library.getAlbums`; `sort`, `limit` and `includeAlbums` on
  `library.getArtists`; `fields` on `library.search`; `formats` and `fields` on
  `playlist.getTracks`; `muted` on `playlist.playTrack`; `position` on
  `playlist.create`; and `cueIndex` on `metadata.write`. `includeCover` and
  `coverMaxSize` of `library.getAlbums` stay hidden, since a cover would be
  inlined into the client's context as base64. `limit` is capped at 500 on
  `library.search` and `library.getAlbums`, and at 1000, its default, on
  `library.getArtists`.
- `playlistGuid`, the `guid` from `playlist.getAll`, wherever a playlist index
  is taken, except on `playlist.remove`, which requires the index. Unlike an
  index it keeps naming the same playlist when others are added, removed or
  moved.

### Removed

- The tools of methods another action covers, or that were deprecated:
  - `playlist.getCount`, `getTrackCount`, `isLocked` and `isAutoplaylist`: every
    entry of `playlist.getAll` carries `trackCount`, `isLocked` and
    `isAutoplaylist`.
  - `playlist.getAutoplaylistQuery`: `playlist.getAutoplaylistInfo` answers the
    same.
  - `playlist.focusTrack` and `playlist.getFocusTrack`: use
    `playlist.setFocusedTrack` and `playlist.getFocusedTrack`.
  - `queue.getCount`: `queue.get` carries `count`. `queue.flush`: use
    `queue.clear`.
  - `metadata.removeTag` and `metadata.removeField`: `metadata.write` with a
    tag set to `null` removes it.
  - `metadata.readByPath`: `metadata.read` gives the same tags and info,
    structured.
  - `playlist.addPathsAsync`: it reports completion only as an event, which
    MCP clients do not receive; `playlist.addPaths` does the same work and
    returns when it is done.

### Fixed

- `consoleMessages` returns the page's console messages and uncaught
  exceptions, the last 200 kept, including the earlier ones the page still held
  when the server connected. The old `fb2k_console_messages` read a page variable nothing wrote, so
  it always came back empty.

## [0.1.1] - 2026-07-27

### Added

- `fb2k_metadata_read` and `fb2k_metadata_read_by_path` declare the optional
  `cueIndex` parameter, matching `fb2k_metadata_read_raw`. Both tools already
  forwarded the value, but it was absent from the published schema, so clients
  had no way to discover it. `fb2k_metadata_read_batch` intentionally omits it
  because the underlying handler resolves each path's sub-track from the path
  itself.

### Changed

- **Behavior change** — a bridge handler response with `success: false` is now
  reported as an MCP tool error instead of a successful tool result. The host
  `code` and `details` fields are preserved in the returned error content.
  Clients that previously treated every response as success must handle the
  error path.
- Raised the minimum `@modelcontextprotocol/sdk` requirement to 1.23.0, which
  is needed for complete Zod 4 object schema registration.

### Fixed

- Enforce declared tool parameter constraints, including numeric bounds, array
  element schemas, nested required fields, defaults, and open metadata tag
  objects. Invalid arguments are rejected instead of being forwarded to the
  bridge.
- Reject circular or prototype-sensitive tool declarations before the server
  starts, and reject non-JSON, circular, sparse, accessor-backed, or
  prototype-sensitive defaults.
- Reject prototype-sensitive runtime argument keys before MCP SDK normalization
  or bridge invocation.
- Parse stdio JSON through a guarded transport ahead of the MCP SDK schema
  pass, returning `-32602` only for unsafe requests while reporting unsafe
  notifications and responses without generating response-to-response traffic.

## [0.1.0] - 2026-06-20

### Added

- Initial public release of the `foo-ui-webview2-mcp` MCP server.
- Connects to the foobar2000 `foo_ui_webview2` WebView2 instance over the Chrome
  DevTools Protocol (CDP, default `localhost:9222`) and exposes the bridge API as
  Model Context Protocol (MCP) tools over stdio.
- Ships the `foo-ui-webview2-mcp` CLI binary, runnable via `npx -y foo-ui-webview2-mcp`.
- Configurable connection via `FB2K_CDP_HOST` / `FB2K_CDP_PORT` environment variables.

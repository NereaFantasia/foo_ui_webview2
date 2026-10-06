# Changelog

All notable changes to the foo-webview-sdk will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.0.0] - 2026-10-06

> **Breaking changes**: `library.getAlbumTracks(album, albumArtist)` names
> the album by a `library.getAlbums` row, and its result has `albumArtist` in
> place of `artist`. The namespace methods that resolved with a bare list or
> `null`, or threw, now resolve with the host's envelope; without a host
> every call resolves with a `NOT_SUPPORTED` failure instead of
> `{ mock: true, method }`. Every generated `XxxResponse` is
> `XxxSuccess | ApiFailure`, so a result field read without checking
> `success` no longer compiles. Several payload types are narrower,
> `artwork.getAvailableArtwork` requires `path`, and `FbBaseElement._sub`
> accepts only names in `FBEventName`. The track events drop `id` and
> `fullPath`; read `handle`. `LyricsSaveTarget` drops `'config'`. From the
> host: a declared method refuses an undeclared key or a value of the wrong
> type with `INVALID_PARAMS`, `queue.addPaths` on a locked playlist fails
> with `LOCKED` (was `OPERATION_FAILED`), a file drop
> reaches the page only where a `dragover` handler calls `preventDefault()`,
> and a page the host does not trust gets `ORIGIN_DENIED` for every call and
> no events: a popup opened at an `http(s)` URL its opener does not trust, a
> page that navigated away, `file://` pages, and in development mode any page
> not on the configured server.
>
> The "Earlier 2.0.0" entry further down is a May 2026 version that used the
> same number before the SDK version realigned with the component at 1.5.0.
> It was never published to npm.

### Added

- `PlaybackClock` estimates the playback position at any moment between the
  host's updates, for progress bars, word-timed lyrics and media elements that
  follow playback. Exported from `foo-webview-sdk/bridge` and reachable as
  `fb.PlaybackClock` in the `<script>` bundle.
- `fb.media.getStreamUrl(path)` and `fb.media.getContainerInfo(path)`
  (experimental): a URL, valid only in the calling document, for playing or
  reading a local media file, and the tracks, attachments and chapters of an
  MP4/QuickTime or Matroska/WebM file, read without decoding.
- `MediaElementFollower` (experimental) keeps a muted media element on
  foobar2000's playback position through `PlaybackClock`.
  `setSource(path, { timelineOffset })` selects the file; for a chapter that
  foobar2000 plays as a subsong, pass the chapter's `start`. Another subsong of
  the same file keeps the loaded resource.
- `fb.lyrics.get`, `fb.lyrics.exists` and `fb.lyrics.save` accept a
  `filename` naming the lyrics file next to the audio file, in place of the
  automatic candidates; `exists(path, options?)` gains the options argument.
- `fb.metadata.write` and `fb.metadata.writeBatch` write multivalue tags: a
  non-empty string array replaces all values of the tag and an empty array
  removes it. An element that is not a non-empty string without NUL fails that
  track with `INVALID_PARAMS` before any of its tags are written.
- `canPlay(track, limits?)` (experimental), also `fb.media.canPlay`, asks the
  browser whether it can decode a track from `media.getContainerInfo`:
  `supported`, `smooth` and `powerEfficient` from `MediaCapabilities`, each
  `'unknown'` when the browser cannot be asked, plus the `canPlayType` hint.
- `playback.getPosition`, `playback.setPosition`, `playback:seeked`,
  `playback:timeHighRes` and `playback:stateChanged` carry `hostTime`, the
  system time the host read the position at in Unix milliseconds, on the clock
  `Date.now()` reads. On `playback:seeked` it is when foobar2000 reported the
  seek, and `position` is the seek target.
- `fb.webview.getSource()` reports where the host loaded the page from: the
  development server, a URL, a template folder or the built-in page, with the
  mapped folder and template name, and the active template as configured now.
- `fb.file.write`, `fb.file.writeBinary` and `fb.file.writeDataUrl` accept
  `atomic: true`: the host writes a temporary file next to the target and
  swaps it in with one rename, so a reader never sees a partly written file.
- `fb.ui.setMaximizeButtonRegion(region?)` reports where the page draws the main
  window's maximize button, so that on Windows 11 hovering it offers Snap layouts.
  The host passes the mouse input on the button back to the page. The response's
  `snapLayouts` says whether the system offers them.
- `DndEnterPayload`, `DndDropPayload` and the `dnd.getPathsAsync` response carry
  `source: 'self' | 'other-window' | 'external'`: where the drag started, in this
  window's page, in another window of the same foobar2000, or anywhere else. The
  host reads it from a marker any program could imitate, so it is a hint for
  handling a drop, not a security check. On the response it is absent when no
  session was found.
- `unwrap(res)` returns the success branch of a response and `unwrap(res, key)`
  one field of it; for a failure both throw an `ApiCallError` whose `code` and
  `details` come from the envelope. Exported from the package entry and from
  `foo-webview-sdk/bridge`, and reachable as `fb.unwrap` / `fb.ApiCallError`
  in the `<script>` bundle.
- `Track` (and every row type built on it, such as `LibraryTrack`,
  `PlaylistTrack` and `QueueItem`) has `albumArtists: string[]`, the ALBUM
  ARTIST values in tag order; `albumArtists.join(', ')` equals `albumArtist`.
  `TrackInfo` gains it as an optional field. The album `library.getAlbums` files
  a track under is `(album, albumArtists[0])`, or `(album, artists[0])` when the
  array is empty, which is that row's `name` and `albumArtist`. `albumArtists`
  is accepted in the `fields` of `library.query`, `library.search` and
  `playlist.getTracks`.
- `config.get(key, defaultValue?)`: the optional second argument is sent as
  the host's `default`, the value answered with `found: false` when the key
  is not stored. Omitted when `undefined`; falsy JSON values are sent.
- `console.log`, `console.warn` and `console.error` accept values after the
  message (`...args: JsonValue[]`). With any, the call is sent as the host's
  `args` with the message first, and the host writes them on one line
  separated by spaces, non-strings as JSON text. A lone message is sent as
  `message`, as before.
- `library.getAll(start, count, opts)`: `opts.useCache` and `opts.asyncResult`
  (`LibraryGetAllParams`). `useCache` is sent only when given (host default
  `true`); `asyncResult` defaults to `true`, as the wrapper always sent it,
  and `false` makes the host build the page on the main thread. The wrapper
  now listens for `library:getAllResult` before sending the call, so a page
  delivered before the pending answer is handled is no longer missed (the
  call used to wait for the timeout); the listener is removed when the page
  arrives, the call fails or rejects, or the timeout fires.
- `queue.remove` accepts an array of positions, sent as the host's `indices`
  and removed in one call; the response then carries `removedCount`. A
  single number is sent as `index`, as before.
- New entry `foo-webview-sdk/schema` for test doubles and other stand-ins for the
  host: `API_PARAM_SHAPES` lists the parameter keys every declared method accepts,
  which of them are required, and the same for nested objects;
  `findParamKeyProblem(method, params)` checks a call against it the way the host
  does before a handler runs and returns the first key the host would refuse the
  call for (`unknown` or `missing`), or `null`. Keys starting with `_` pass, and a
  key whose value is `null` or `undefined` counts as absent, as on the host, which
  receives the params as JSON. Only key names are checked.
- Every declared method's `XxxParams` type, `ApiParamsMap` (method name to
  params) and `ApiMethodMap` (method name to `[params, response]`) are exported
  from the package entry.
- `window:activated` and `window:dpiChanged` are listed in `FBEventName` and
  `FBEventPayloadMap`, with `WindowActivatedPayload` and
  `WindowDpiChangedPayload`. The host has always sent both to the main
  window's page.
- New exported types `MenuSelectPayload`, `MenuDismissPayload` and
  `MenuValueChangedPayload`, the payloads of `menu:select`, `menu:dismiss` and
  `menu:valueChanged`.
- The payloads of `taskbar:buttonClicked`, `tray:beforeContextMenu`,
  `tray:click`, `tray:doubleClick`, `tray:menuItemClicked` and
  `webview:processFailed` are exported by name: `TaskbarButtonClickedPayload`,
  `TrayBeforeContextMenuPayload`, `TrayClickPayload`, `TrayDoubleClickPayload`,
  `TrayMenuItemClickedPayload` and `WebviewProcessFailedPayload`. They used to
  be reachable only as `FBEventPayloadMap['<event>']`.
- `fb.playlist.getMatchingRows(query, playlist?)` resolves with the rows of a
  playlist whose tracks match a foobar2000 query, in one call however long the
  playlist is, and
  `fb.playlist.getTracksAt(playlist, rows, formats?, fields?)` reads rows
  picked by number, such as those, in the order given. A query the host's
  parser rejects fails with `INVALID_PARAMS` and `details.param: 'query'`, as
  it now does in `library.search` and `library.query`.
- `fb.playlist.reorderPlaylists(order)` takes the new order as GUID strings as
  well as indices. GUIDs are sent as `newOrderGuids`, and one no playlist has
  fails with `NOT_FOUND` instead of moving whichever playlist is at an index.
- `QueueContentRef` and `QueueListRef` accept `{ playlistGuid, item }`, so
  `fb.queue.setContents` and `fb.queue.insertNext` can name a row's playlist
  by GUID.
- Responses that report a playlist by index carry its GUID as `playlistGuid`:
  the `playlist.*` results, `fb.player.getCurrentTrackIndex()` and
  `getPlayingPlaylist()` (`null` without a location), `QueueItem`,
  `queue.addPaths`, `artwork.getByPlaylistItem` and
  `selection.getViewingTrack`. `PlaylistDuplicateResponse` adds
  `sourcePlaylistGuid`. The `playlist:*` event payloads carry `playlistGuid`
  for the playlist they concern; `playlist:created` and `playlist:renamed`
  carry `guid`, `playlist:activated` `newGuid`, `playlist:reordered` the new
  order as `guids`, and `playlist:removed` the `indices` and `guids` of the
  removed playlists.
- `<fb-playlist-view>` accepts a playlist GUID in `playlist`, and its
  `fb-track-select`, `fb-track-play` and `fb-track-context` details carry
  `playlistGuid`. `<fb-playlist-tabs>` puts each playlist's GUID on its tab as
  `data-guid`; `fb-playlist-select` and `fb-playlist-context` carry `guid`, and
  `fb-playlist-reorder` carries `newOrderGuids`. `fb-playlist-pick` of
  `<fb-playlist-selector>` carries `guid`.

### Changed

- **Breaking:** `LyricsSaveTarget` drops `'config'`; the host no longer saves
  lyrics to the configuration folder, and `'all'` writes the file and embedded
  targets.
- **Breaking:** only pages the host trusts can call it or receive its
  events: pages on its own virtual host, pages it wrote itself, and pages on an
  origin it navigated the window to (the development server, a panel's URL, or
  a popup URL its opener already trusted). Calls from any other page reject at
  once with `ORIGIN_DENIED` instead of timing out. A URL carrying a user name
  or password no longer passes for a trusted origin, and `file://` pages and
  other loopback ports are no longer trusted in development mode.
- **Breaking:** `library.getAlbumTracks(album, albumArtist)` names the album by
  a `library.getAlbums` row: pass the row's `name` and `albumArtist`, both
  required and compared byte for byte. It returns exactly the tracks that row
  groups, so `total` equals its `trackCount`, sorted by disc number, then track
  number, then library order. The result has `albumArtist` in place of
  `artist` and the album's `row` (absent when no row matches). The optional
  `artist` parameter, which matched any `album artist` or `artist` value, is
  gone. `<fb-library-tree>` follows: album groups carry their album artist as
  `data-artist`, the `fb-library-select`, `fb-library-play` and
  `fb-library-context` details report it as `artist`, and `addToPlaylist` on
  an album row under an artist adds that artist's tracks on the album.
- **Breaking:** the namespace methods that resolved with a bare list or
  `null`, or threw, now resolve with the host's envelope like every other
  method. None of them throws for a failure the host reports, and none turns
  a failure into an empty array, so a failed call and an empty result stay
  distinguishable. Check `success` first, or pass the answer to the new
  `unwrap` (see Added).

  | Method | Was | Now |
  | --- | --- | --- |
  | `playlist.getAll()` | `PlaylistInfo[]`; threw on failure | `{ playlists, count }` |
  | `playlist.getTracks()` | the rows; `[]` on failure | `{ playlist, start, count, total, tracks }` |
  | `playlist.getSelectedTracks()` | the rows; `[]` on failure | `{ playlist, count, tracks }` |
  | `playlist.getAvailableColumns()` | the columns; threw on failure | `{ columns, count }` |
  | `output.getDevices()` | the devices; threw on failure | `{ devices, count }` |
  | `system.listApis()`, `getApisByNamespace()`, `searchApis()` | `SystemApiInfo[]`; threw on failure | `{ apis }` |
  | `system.getRegisteredPlugins()` | `SystemPluginInfo[]`; threw on failure | `{ plugins }` |
  | `config.getOutputDevices()` | the devices; threw on failure | `{ devices, count }` |
  | `config.getAdvancedConfig()` | the entries; threw on failure | `{ entries, count }` |
  | `config.getPreferencesPages()` | the pages; threw on failure | `{ pages, count }` |
  | `config.getComponents()` | the components; threw on failure | `{ components, count }` |
  | `config.getDspPresets()` | the presets; threw on failure | `{ presets, count }` |
  | `playcount.get(path)` | `PlaycountInfo \| null` | `{ count, results }`; the entry is `results[0]` |
  | `library.getAll()` | a failed or timed-out background list rejected with a plain object | resolves with `OPERATION_FAILED`, `details.requestId` (and `details.timeoutMs` on a timeout) |

  Each also resolves with an `ApiFailure` when the call fails.
  `PlaylistAvailableColumnsResponse`, `AdvancedConfigResponse` and
  `PreferencesPagesResponse` now name the response union rather than the list,
  and `LibraryPagedTracksResponse` has `success: true`.
- `<fb-playlist-selector>`, `<fb-playlist-tabs>`, `<fb-output-selector>`,
  `<fb-dsp-preset-selector>` and `<fb-playlist-view>` keep what they show when
  a call fails, instead of clearing it. `<fb-library-tree>` dispatches
  `fb-library-added` only when the host added the tracks, with the `count` the
  host reports; a locked playlist no longer produces the event.
- **Breaking:** without a host (`window.fb2k` missing, as in a plain browser
  tab or a unit test), `bridge.invoke` and every namespace call resolve after
  100 ms with an `ApiFailure`: `{ success: false, code: 'NOT_SUPPORTED',
  error: 'No foobar2000 host is available', details: { method } }`. They used
  to resolve with `{ mock: true, method }`, which has no `success` field, so a
  `success === false` check took it for a success. The `MockInvokeResponse`
  type is removed. To run a page against fake data, assign a stand-in with
  `invoke`, `on` and `off` to `window.fb2k` before the page subscribes to
  events.
- **Breaking:** every generated `XxxResponse` is now `XxxSuccess | ApiFailure`
  instead of one flat interface with optional `error` and `code`.
  `XxxSuccess` has `success: true` and the method's result fields;
  `ApiFailure` has `success: false`, `error`, `code` and an optional
  `details`. Reading a result field without checking `success` first no
  longer compiles: the flat type let it through, and on a failed call the
  field was `undefined`. Check first, for example
  `if (res.success === false) throw new Error(res.error);`; the `=== false`
  form narrows with or without `strictNullChecks`, `!res.success` only with
  it. A type built on a
  response with `Omit`, `Pick` or an indexed access (`XxxResponse['field']`)
  should start from `XxxSuccess`. `HttpBinaryResponse`, `WindowListResponse`,
  `MenuGetMainMenuResponse` and `MenuGetContextMenuResponse` follow the same
  pattern, and `HttpBinarySuccess` is new. Some failures carry fields of their
  own, such as `candidates` on `MENU_MATCH_AMBIGUOUS`; test for them with
  `in`. The payloads of `http:response`, `http:downloadComplete` and
  `library:getAllResult` keep their flat shape for now.
- New exported types `ApiFailure`, and an `XxxSuccess` for every method with a
  generated `XxxResponse`.
- The payload types of the `keyboard`, `taskbar`, `tray`, `ui`, `system`,
  `app`, `webview`, `plugin`, `api`, `metadb`, `state`, `dnd`, `window`,
  `panel`, `menu`, `jitQueue`, `port`, `file`, `metadata`, `http`,
  `library`, `audio`, `playlist`, `playback` and `selection` events now match
  what the host sends field for field, with a description on every field.
  Apart from the changes to the track events and to the two empty events
  listed below, the events themselves are unchanged. Some types are narrower,
  so a
  comparison with a value the host never sends, or a read of a field it never
  sends, no longer compiles:
  - `WebviewProcessFailedPayload.kind` and `.recoveryAction` are unions of the
    values the host sends, and `kindRaw` is a `number` (was `unknown`).
  - `UiColoursChangedPayload` and `UiFontChangedPayload` are
    `Record<string, never>` (were `JsonObject`), as neither event carries
    fields.
  - `DndDragEndedPayload.code` is
    `'PERMISSION_DENIED' | 'INVALID_PARAMS' | 'OPERATION_FAILED'` (was
    `ApiErrorCode`).
  - `FBEventPayloadMap['state:changed']` types `value` and `previousValue` as
    `JsonValue` (was `unknown`). `StateChangedPayload<T>` keeps its type
    parameter for typing a key's values yourself.
  - `PanelFocusPayload` and `PanelBlurPayload` are `Record<string, never>`
    (declared a `windowId` the host never sent).
  - `WindowHoverStateChangedPayload.hovering` is required and `reason` is
    gone; the host always sends `{ windowId, hovering }`.
  - `WindowBackdropStateChangedPayload.mode` and `.effect`, and
    `WindowBehaviorChangedPayload.profile`, are unions of the values the host
    sends; `WindowBehaviorChangedPayload.resolvedBehavior` has the fields of
    `window.getPopupBehavior` (was `JsonObject`).
  - `MenuValueChangedPayload.itemId` is a `string` (was `unknown`).
  - `JitQueueNeedNextPayload.reason` is `'trackChange'` (was `string`), the
    only value the host sends.
  - `PortMessagePayload.portId` is a `string` (was `unknown`).
  - `MetadataWriteCompletePayload.operation` is `'write' | 'removeTag'` (was
    `string`).
  - `LibraryGetAllResultPayload.offset`, `.limit` and `.fromCache` are
    required; the host always sends them.
  - `PlaybackStartingPayload.command` is
    `'play' | 'next' | 'previous' | 'random' | 'unknown'` (was
    `'default' | 'play' | 'next' | 'prev' | 'random'`): the host has always
    sent `previous` and `unknown`, never `prev` or `default`.
  - `AudioFullWaveformFailedPayload.code` and `AudioPcmFailedPayload.code`
    are unions of the codes each event can carry (were `string` and
    `ApiErrorCode`).
  - `AudioFullWaveformReadyPayload` no longer declares `fileSize` and
    `cacheKey`, which the host never sent; `maxAmplitude`, `duration`,
    `sampleRate`, `channels`, `scale`, `signed` and `cached` are required.
  - `FbPopupMessageDetail` no longer declares `targetWindowId`, which
    `window:message` never carried.
- `WindowMessagePayload.message` is `JsonValue` (was `JsonObject`):
  `window.sendMessage` and `window.broadcast` deliver whatever JSON the sender
  passed.
- `PortMessagePayload.message` is `JsonValue` (was `JsonObject`):
  `port.postMessage` and `port.postMessageTo` deliver whatever JSON the sender
  passed.
- `HttpResponsePayload.status`, `.headers` and `.body` are optional (were
  required): a failed request carries `error` and `code` instead, and
  `status` only when a refused redirect produced one. Check `success` before
  reading them. `HttpResponsePayload` and `HttpDownloadCompletePayload` gain
  `code` and `cancelled`, and `HttpResponsePayload` gains `contentLength`,
  which the host already sent.
- `PlaybackStartingPayload.paused` and `PlaybackStateChangedPayload.position`
  / `.duration` are required; the host always sends them.
- `AudioSpectrumPayload` has the fields of an `audio.getSpectrum` answer:
  `spectrum` is optional (was required), since a bin frame with
  `channels: 'stereo'` carries `left` and `right` instead, which the type now
  declares; `output`, `fftSize`, `scale`, `sampleRate`, `minFrequency`,
  `maxFrequency`, `state`, `streamTime` and `hostTime` are required. It no
  longer extends `SpectrumFrameInfo`.
- `MetadataProbeResultItem.tags` is `Record<string, JsonValue>` (was
  `Record<string, string | string[]>`), the same type as the flat fields of
  `metadata.readBatch`.
- `PanelConfig.edgeStyle` is a `number` (was `string`): the host sends `0`
  (none), `1` (sunken) or `2` (grey).
- `PluginRegisteredPayload` / `PluginUnregisteredPayload` are now aliases of
  `SystemPluginInfo`, and `ApiRegisteredPayload` / `ApiUnregisteredPayload` of
  `SystemApiInfo`, with the same fields as before.
- Every `dnd:*` event is listed in `FBEventName` and `FBEventPayloadMap`
  directly; they used to be merged in from `DndEventPayloadMap`.
- `playback:trackChanged`, `playback:edited` and
  `playback:itemPlayed` now carry the shared `Track`, the same row
  `playback.getCurrentTrack` answers with, and `PlaybackTrackChangedPayload`,
  `PlaybackEditedPayload` and `PlaybackItemPlayedPayload` are aliases of
  `Track`. `id` and `fullPath` are gone: read `handle`, which is what
  `fullPath` held. `artists` and `rating` are new, and every field is always
  present.
- The `track` and `nowPlaying` of `selection:changed` are the
  shared `Track` too, and both are optional in `SelectionChangedPayload`
  (were a required `JsonObject`): `track` comes only when exactly one track
  is selected, `nowPlaying` only while a track is loaded. `handles` is a
  `string[]` (was `JsonObject`) and `type` a union of the selection types.
- `audio:dspPresetChanged` and `playlist:defaultFormatChanged` deliver `{}`,
  as their `Record<string, never>` types always said; the host used to send
  `null`.
- **Breaking:** `artwork.getAvailableArtwork(path)` requires `path`, and
  `metadata.embedArtwork(path, opts)` requires `opts`, which carries the
  required `imageData`. The host refused every call without them with
  `INVALID_PARAMS`; such a call now fails to compile instead.
- `MenuPopupItem`, `WindowPopupBehaviorPatch` and `WindowBackdropPolicyPatch`
  are type aliases instead of interfaces, so they are checked as JSON
  wherever they are sent. Declaration merging into them no longer works.
- `FbMetadbHandle`'s constructor also accepts a `Track` row, or a partial one
  such as the rows of `library.search` with `fields`.
- **Breaking:** `FbBaseElement._sub(event, handler)` accepts only event names
  listed in `FBEventName`; a misspelled or custom name no longer compiles.
  A subclass that listens for a plugin's own event subscribes with
  `fb.on(name, handler)`, which still takes any string, and pushes the
  returned callback onto `_subscriptions` so it is released on disconnect.
- `<fb-playlist-view>` works out the GUID of the playlist it shows and names
  it by that GUID in every host call, so a playlist added or removed in front
  of it cannot turn a selection, a Delete or a double-click into one on
  another playlist. With an index in `playlist` it looks the index up again
  after `playlist:created`, `playlist:removed` and `playlist:reordered`, and
  drops its selection and focus only when the index now names another
  playlist. Rows reload only for the item events of the playlist it shows.
- `<fb-playlist-tabs>` activates, opens the native menu for and reorders
  playlists by GUID, and dispatches `fb-playlist-select` and
  `fb-playlist-reorder` only once the host has done it; a tab whose playlist is
  gone does nothing.
- The multi-step `plman` methods of the SMP layer (`RemovePlaylistSelection`
  with `crop`, `SetPlaylistSelection` with `false`, `AddLocations` and
  `InsertPlaylistItems` with `select`, `GetPlaylistItems` after its first
  page) name the playlist by index in their first call only, and by the GUID
  that call reported afterwards. `MovePlaylist` sends the GUIDs of the cached
  playlists.
- The `options` of `fb.playlist.playTrack` no longer accept `playlistGuid`,
  which conflicted with the playlist given as the first argument; pass the
  GUID as that argument.
- `fb.http.request()` is typed as resolving with the success branch only,
  `HttpGetSuccess` (or `HttpBinarySuccess` with `responseType: 'arraybuffer'`
  or `'binary'`), which is what it always did: every failure rejects. Read
  `status` and `body` without checking `success`; a `res.success === false`
  test on its result no longer compiles.

### Fixed

- `fb.ui.getMode()` in a popup reports the popup's own `windowId`, the one
  `fb.ui.getCurrentWindowId()` gives, instead of `main`. Several panels in the
  same foobar2000 window are no longer taken for one another by `getMode`,
  `getCurrentWindowId`, `fb.panel.getConfig` and `setConfig`.
- The synchronous `fb.file` methods fail a path longer than 259 characters with
  `OPERATION_FAILED` and `details.value` 206. foobar2000 does not opt in to long
  paths, and an existing file past the limit used to be reported as missing.
  `fb.file.copyAsync`, `moveAsync` and `deleteAsync` report such an entry with
  the new reason `path-too-long` instead of `not-found`.
  `fb.file.read` and `fb.file.write` also put the Windows error code in
  `details.value` when the file cannot be opened.
- Components registered with `registerComponents()` from
  `foo-webview-sdk/components` work inside foobar2000 without a `window.fb`
  assignment. They looked the SDK up on `window.fb`, which the ESM entry never
  set and where the host keeps a smaller object of its own, so their first SDK
  call threw a `TypeError`. The ESM `registerComponents()` now binds them to the
  instance `foo-webview-sdk` exports; `dist/components.js` imports it from
  `dist/bridge.js`, so the theme and the components share one bridge.
  `<script>` themes still load `bridge.global.js`; a `window.fb` without the
  SDK's `on` and `invoke` now throws an error that says how to load the SDK.
- `<fb-rating>` reads and sets the rating of a CUE subsong as itself. It sent
  the track's bare `path` to `rating.get` and `rating.set`, which the host
  resolves to the first subsong of the file; it now sends the track's `handle`
  (or `path` with a `|subsong:N` suffix when `handle` is absent). The `path`
  in the `fb-rating-change` detail carries the same key.
- SMP `FbUiSelectionHolder.SetSelection` no longer sends a `type` key that
  `selection.set` does not declare. The host refused it, so every call
  failed; the `type` argument is now ignored.
- `menu.popup` subscribes to `menu:select` and `menu:dismiss` before it sends
  `menu.show`, so a result that reaches the page ahead of the call's response
  still settles the promise. It used to subscribe only after `menu.show`
  answered. When `menu.show` fails or rejects (including the bridge's request
  timeout), both listeners are removed before the promise settles.
- `<fb-titlebar>` and `<fb-window-controls>` follow only their own window.
  `window:stateChanged` reaches every window, so maximizing the main window
  also flipped `maximized` on the components in a popup, and a popup entering
  fullscreen did the same in the main window. They now ask
  `fb.ui.getCurrentWindowId()` on connect and ignore events whose `windowId`
  names another window; events that arrive before the answer are held and the
  latest one for the own window is applied once it comes. An event without
  `windowId` (an older host) is applied as before, and so is every event when
  the id cannot be learned.
- `WindowStateChangedPayload` gains `windowId`, the window whose state changed.
- SMP keeps the `|subsong:N` suffix when it hands a track to the host, so a
  subsong of a multi-track file (a CUE sheet) is no longer treated as the
  file's first subsong: `FbTitleFormat.Eval`, `EvalWithMetadb` and
  `EvalWithMetadbs` (`titleformat.eval` / `evalBatch`, which accept the
  suffix from host builds that resolve it), `FbMetadbHandle.GetFileInfo`
  (`metadata.read`) and `plman.AddItemToPlaybackQueue` (`queue.addPaths`).
  `smpUtils.toHandleId` now also adds the `subsong` of a host track row
  (`absolutePath` / `path` plus `subsong`, as `smp.cache.currentTrack`
  holds) instead of returning the bare path; a path that already ends in
  the suffix is kept as it is.
- `<fb-playlist-view>` marks the row of the playing track with `playing`. It
  used to read a row number from the `playback:trackChanged` payload, which
  carries none, so no row was ever marked. The view now asks
  `fb.player.getCurrentTrackIndex()` on every track change and whenever its
  rows reload, and marks a row only when the track plays from the playlist
  the view shows; a stop clears the mark.
- `<fb-playlist-view>` shows the playlist a changed `playlist` attribute
  names; it used to reload the playlist it already showed. Whenever the
  playlist shown changes, through the attribute or a switch of the followed
  active playlist, the view drops the cached rows, selection and focus of the
  previous one: a kept selection made Delete remove the rows with the same
  numbers from the new playlist. A `playlist:focusChanged` of another
  playlist no longer moves the view's focus.
- SMP `fb.RunContextCommandWithMetadb(command, handle_or_handle_list)` runs
  the command on the given tracks. It used to send `handles` to
  `menu.runContextCommand`, which does not declare it, so every call failed.
  It now builds the context menu for the tracks with `menu.getContextMenu`
  (`mode: 'handles'`), finds the first command whose full path from the top
  of the menu equals `command` ignoring case, as SMP does (for example
  `Playback Statistics/Rating/5`), and runs it with
  `menu.runContextCommandById`. It resolves `false` without running anything
  when the menu has no such command or cannot be built for the tracks, and
  never falls back to the selection or the playing track. A handle list
  passes every track; a CUE subsong keeps its `|subsong:N` suffix.
- SMP `ContextMenuManager.ExecuteByID(id)` resolves `false` for an id
  `BuildMenu` allocated to a row without `commandId`. It used to send that id
  as a raw host command id, running whichever command the host had numbered
  the same. Ids outside the range the last `BuildMenu` allocated still pass
  through as raw host ids.
- `<fb-playlist-view>` following the active playlist shows no rows while there
  is none. It used to fall back to the first playlist and act on it.
- `<fb-playlist-tabs>` and `<fb-playlist-selector>` keep the newest list of
  playlists when an earlier read answers last. `<fb-playlist-selector>`
  redraws after `playlist:reordered`; its options kept the old indices.
- `plman.RemovePlaylistSelection(playlist, true)` and
  `plman.SetPlaylistSelection(playlist, items, false)` return `false` when the
  selection cannot be read. The crop used to clear the whole playlist, and the
  deselection the whole selection.
- `fb.ClearPlaylist()` and `fb.GetFocusItem()` act on the playlist that is
  active when they run, not on the one the SMP cache last saw active.

### Removed

- `menu:show` from `FBEventName` and `FBEventPayloadMap`. Only the host's own
  menu page receives it; a page never did.

### Deprecated

- `DndSessionEventPayload` and `DndEventPayloadMap`. Use `DndLeavePayload` for
  the `sessionId` shape, and index `FBEventPayloadMap` for a `dnd:*` payload.
- `playlist.getTracksPage()`. `playlist.getTracks()` now resolves with the
  same page.

## [1.14.0] - 2026-10-06

> **Breaking changes**: `library.getArtistAlbums` now matches `artist`
> exactly rather than as a substring — pass `match: 'substring'` for the old
> containment test. The exact comparison is byte for byte, so case matters and
> `*` / `?` are literal; `library.getArtistTracks` and
> `library.getAlbumTracks` now use that same comparison, having previously
> pulled in names differing only in case — which made `getAlbumTracks` merge
> two identically titled albums. A name containing a double quote, which used
> to match nothing at all, now resolves under the exact comparison.
> A row's `artist` reports the album artist (the first value
> of `album artist`, falling back to `artist`) instead of the first matching
> track's credit. A call whose `limit` is below the artist's album count
> returns a different *set* of albums, not merely a different order, and
> `limit: 0` now returns none where it used to return one.
> `QueueItem.playlist` and `QueueItem.playlistItem` are required
> `number | null` — `null` for an entry without a playlist position — where
> they used to be optional numbers carrying an out-of-range sentinel; test
> `item.playlist == null` instead of comparing magnitudes.

### Added

- **Dragging tracks out of the window as files** — `dnd.prepareDrag(paths)`
  exchanges a list of locations for a one-shot token *before* the drag
  starts (request it on `pointerdown`), and `dnd.applyDragToken(dataTransfer,
  token)` writes that token into the drag synchronously inside `dragstart`,
  setting `effectAllowed` to `'copy'` as the host requires. The host swaps the
  token for the already-validated files and the drag continues as an ordinary
  page drag; Explorer and other applications receive real files. What lands is
  always a physical file: a `path|subsong:N` entry drags its container, an
  `archive://` / `unpack://` entry drags the archive, and several entries can
  collapse into one. Accepted forms are native paths, `file://`,
  `file-relative://` (a portable install's form for media on the program's
  volume, resolved by foobar2000), `archive://` / `unpack://` and
  `path|subsong:N`; a track's `path` and `absolutePath` are both fine.
  Resolves — never rejects — with `ORIGIN_DENIED` (new code), `NOT_SUPPORTED`,
  `INVALID_PARAMS`, `INVALID_PATH` or `PERMISSION_DENIED`; messages never
  contain a path.
- **`dnd.onDragEnded` / event `dnd:dragEnded`** — fires only when the host
  refuses to attach the files (`PERMISSION_DENIED` for a bad, spent,
  superseded or foreign token; `INVALID_PARAMS` for an `effectAllowed` wider
  than `'copy'`; `OPERATION_FAILED` when the list could not be attached). A
  successful hand-over produces no event; read `dragend`'s
  `dataTransfer.dropEffect` instead (`'copy'` when a target took the files).
  Typed as `DndDragEndedPayload` with `result: 'failed'`.
- `DndCapabilities` and `DndCapabilitiesChangedPayload` gain `dragOut` and
  `dragOutUnavailableReason` (`DndDragOutUnavailableReason`:
  `'not-visual-hosting' | 'runtime-too-old' | 'register-failed'`; the WebView2
  Runtime must be Edge 144 or newer). `dragOut` is independent of `paths`.
  New response type `DndDragToken`.
- **`queue.setContents`, `queue.insertNext` and `queue.playNow`** join the
  `queue` namespace. `setContents(items)` replaces the entire queue with an
  ordered list of `{ queueIndex }` / `{ playlist, item }` references, and
  fails the whole call — leaving the queue untouched — on any unrecognized
  entry, unlike `queue.add`'s per-entry skip. `insertNext(entries, position?)`
  takes file paths, `QueueListRef` playlist positions (`{ playlist, item }`,
  exported) or a mix, inserts them so they play next, moves a track already
  queued to its new spot instead of duplicating it, and reports
  `insertedCount` / `movedCount` / `invalidCount` separately. Path entries
  carry no playlist position and the playback cursor does not follow them;
  position entries carry theirs and it does. A mixed array is split by type
  before it is sent — positions first, then paths — so `[p1, I1, p2]` queues
  as `I1, p1, p2`; position entries deduplicate by track, and one bad
  position entry fails the whole call. `playNow(index?)` plays a queue
  entry immediately, promoting it to the front first when needed; its
  `queueCount` is read right after playback starts and is not guaranteed to
  land before or after the played entry is consumed.
- `library.getArtistAlbums` takes a third `options` argument carrying `sort`
  (`name` | `artist` | `year` | `trackCount`, default `name`) and `match`
  (`'exact' | 'substring'`, default `exact`). An unrecognised `sort` falls
  back to `name`; an unrecognised `match` is refused by the host, so `match`
  is typed as a literal union to catch a misspelling at compile time.
- `LibraryArtistAlbumsResponse` gains `artist`, `total` and `hasMore`.
  `total` counts albums before `limit` truncation. There is no `offset`, so
  `hasMore` means the page can only be widened with a larger `limit`.
- `dialog.openFolder` accepts `defaultPath`, the folder the dialog opens in
  every time it is shown, regardless of where the user last browsed;
  `folderPath` still reports whatever the user confirmed. A value that does
  not resolve to a folder — a missing path, an unplugged drive, a file — is
  silently ignored and the dialog opens where it otherwise would, with no
  `error`. `%music%` expands to the user's Music folder.
  `DialogOpenFolderParams` gains `defaultPath?: string`.
  `dialog.openFile`'s `defaultPath` follows the same rules.
- The self-drawn menu's `rating`, `slider` and `segmented` rows answer the
  mouse wheel. Scrolling up increases, the same direction as the arrow keys,
  and one wheel event is one step — one star, a twentieth of a slider's range,
  or the next enabled segment — regardless of how far the wheel turned. Steps
  report through the same value channel as a click and the menu stays open. A
  disabled row and a slider whose `min` equals its `max` never move, and the
  wheel is ignored while a different row is in editor mode. Applies to
  `tray.setContextMenu` with `render: 'webview'` and to `menu.show`.
- `library.getArtists` accepts `includeAlbums` in its `options` argument
  (`LibraryGetArtistsParams.includeAlbums`). When `true`, every `ArtistInfo`
  row carries `albums: ArtistAlbumRef[]` — the `{ name, artist }` identities
  of the albums that artist is credited on, de-duplicated by that pair and
  sorted by `name`, then `artist`. `(name, artist)` is the identity
  `library.getAlbums` groups by, so each element pairs with exactly one
  `getAlbums` row. The host answers from the same single scan and cache as
  the plain call; `limit` caps the rows, not the `albums` inside them.
- New exported type `ArtistAlbumRef`.
- `fb.audio.subscribeSpectrum` returns an unsubscribe function carrying
  `ready`, a promise that never rejects and settles with whether the host
  registered the subscription and with which values. The callback receives
  only this subscription's frames. The options accept `scale: 'db'`,
  `minFrequency` and `maxFrequency`, and `fftSize` up to 65536.
- `fb.audio.generateFullWaveform` accepts `signal`. Aborting it, or the client
  timeout, cancels the host task; an aborted signal rejects with a
  `DOMException` named `AbortError`. New `fb.audio.cancelFullWaveform(taskId)`.
- `fb.audio.getWaveform` accepts `channels` (`'mix' | 'stereo'`) and `points`.

### Changed

- `QueueItem.playlist` and `QueueItem.playlistItem` are required
  `number | null`: exact indices for an entry that has a playlist position,
  both `null` for one that has none. The SMP `plman.GetPlaybackQueueContents()`
  maps `null` to `-1`, as its contract already stated.
- **`playback:queueChanged` gains `count`**, the queue length after the
  change, so a listener no longer has to call `queue.getCount` on every
  event to know the new size. A rebuild-style change — `queue.setContents`,
  the reorder path of `queue.insertNext`, `queue.moveToTop`, or
  `queue.playNow` with `index > 0` — now broadcasts exactly once instead of
  once per internal write, with `origin: 'unknown'` for that batched event.
- `library.getArtistAlbums` rows now carry every `AlbumInfo` key that
  `library.getAlbums` returns except `coverDataUrl` and `tracks` — notably
  `albumArtist`, `duration`, `discCount`, `genre`, `label`, `firstTrackPath`
  and `firstTrackAbsolutePath`. Pass `firstTrackPath` to
  `artwork.getForTrack` for cover art.
- **`trackCount`, `duration` and `discCount` on those rows count only the
  tracks the artist appears on**, while `library.getAlbums` reports the
  album-wide figures for the same album. The two share these key names
  without sharing their scope, so a row from this endpoint understates an
  album card.
- `library.getArtistAlbums` is typed as `LibraryArtistAlbumsResponse`
  instead of `{ albums: AlbumInfo[] }`.
- `<fb-library-tree>` no longer lists other artists' albums under an artist
  node whose name is a substring of theirs, following the new default
  comparison.
- `ArtistInfo.duration` is now `ArtistInfo.totalDuration`, matching the key
  the host has always sent. Code that read `.duration` on an artist row was
  reading `undefined`; it now fails to compile instead.

### Fixed

- Every `library.getArtistAlbums` response now carries `albums`. Its four
  failure paths omitted the key, so `const { albums } = …` yielded
  `undefined` against a type that declares the field required.
- `AlbumInfo.duration` is required and this endpoint now sends it. Rows
  previously carried four keys, none of them `duration`, so the declared row
  type never described what arrived.
- `library.getArtistAlbums` no longer under-reports `trackCount` when `limit`
  is reached. Grouping used to stop at the cap, freezing the counts of the
  albums collected so far.

## [1.13.0] - 2026-08-26

> **Breaking changes**: `library.search` no longer duplicates its row array —
> the host sends `tracks` only, and `LibrarySearchResponse.items` is now an
> optional `@deprecated` field, so code that destructures `items` and calls
> array methods on it stops type-checking under `strictNullChecks`. On the
> type level, `dialog.confirm` now resolves with `{ response }` (the
> previously declared `confirmed` flag was never populated by the host).
> `file.*` error messages are standardized and no longer echo the rejected
> path — parse `code` and `details` instead of `error` text. A parameter of
> the wrong shape now reports `INVALID_PARAMS` rather than `PERMISSION_DENIED`.

### Added

- **Asynchronous file operations** — `file.copyAsync`, `file.moveAsync`,
  `file.deleteAsync` and `file.cancelOp` run on a host worker thread and
  return a `{ operationId, totalCount }` receipt; results arrive in batches
  on `file:opProgress` followed by one `file:opComplete`. Directory entries
  report once per entry after their whole tree is walked; at most 8
  operations are in flight process-wide.
- **Asynchronous metadata probing** — `metadata.probeBatchAsync` and
  `metadata.cancelProbe`, with progress on `metadata:probeProgress` /
  `metadata:probeComplete`. Each result carries `infoSource`
  (`cached` / `direct` / `none`) and a per-path `failure` reason.
- **Field projection** — `library.query` and `library.search` accept an
  optional `fields` array naming the `TrackInfo` keys each returned row
  should carry. An invalid selection resolves — never rejects — with
  `{ success: false, code: 'INVALID_PARAMS' }`.
- **Drag-and-drop path access** — `dnd.getCapabilities` reports what the
  current window can deliver, `dnd.getPathsAsync` is the reliable way to
  read real filesystem paths inside a `drop` handler, and shortcut (`.lnk`)
  targets are exposed as a parallel `resolvedPaths` array.

### Changed

- `library.query` / `library.search` serialize off the host's main thread;
  apart from the `items` removal above, both resolve the same shapes as
  before. Large result sets no longer freeze the UI — page with
  `offset` / `limit` or project with `fields` to keep single calls small.
- The SMP compatibility layer fetches query hits in one call instead of
  paging the whole library per 500 rows.

### Deprecated

- `LibrarySearchResponse.items` — hosts stopped sending it in 1.13.0; older
  hosts still include it. Read `tracks`, which every host version sends on
  every response shape.

## [1.12.0] - 2026-08-13

> **Breaking changes**: the `dnd` drop-zone registry (`registerDropZone` /
> `unregisterDropZone` / `getDropZones`) is removed and `dnd.startDrag` now
> reports `NOT_SUPPORTED` instead of faking success — see *Removed* and
> *Changed* below for migration. On the type level, `SmpMenuBuildState` gained
> a required `family` field and `DiscoveryContextMenuCommand` is no longer an
> alias of `DiscoveryMainMenuCommand`.

### Added

- **Per-call presentation options for the self-drawn menu** — `menu.show` and
  `menu.popup` take a third `MenuPopupOptions` argument, and keys the caller
  omits are not sent, so the host keeps its own defaults:
  - `windowModel` (`'fullscreen'` default, or `'contentSized'`) —
    `'contentSized'` draws the root and its first-level submenu in separate
    compact windows measured to their content, so each panel carries the real
    DWM backdrop material across its own surface plus the system window shadow.
    It is the recommended model for a context menu; `'fullscreen'` remains the
    compatibility default.
  - `css` (at most 256 KiB) and `cssReplace` — style takeover for the overlay,
    layered over the built-in styles by default, or replacing them entirely so
    the whole look including the entry animation is the theme's.
  - `backdrop` (`'acrylic'` default, `'mica'`, `'mica-alt'`, `'none'`) and
    `backdropDarkMode` (default `true`) — DWM system backdrop for the menu
    window. Acrylic needs Windows 11 22H2 or newer and degrades on Windows 10.
  - `closeAnimationMs` (default `0`, clamped to `0..1000`) — exit fade duration.
    It animates the web content only; the DWM backdrop is a window-level effect
    that snaps in and out with the window, so a fully smooth fade needs
    `backdrop: 'none'` plus a translucent CSS background.
- **Rich items in `MenuPopupItem`** — the `type` union gained `'nowplaying'`,
  `'rating'`, `'slider'`, and `'segmented'` along with the fields they use
  (`value`, `min` / `max` / `orientation`, `segments`, and `cover` / `title` /
  `subtitle`), plus `iconSvg` for an inline monochrome icon on any row. Icons go
  through the runtime's allowlist sanitizer; an illegal or oversized one is
  dropped without failing the row.
- **`menu:valueChanged`** — reports a rating, slider, or segmented change in a
  self-drawn menu as `{ menuId, itemId, value }` and **keeps the menu open**,
  while ordinary rows still report through `menu:select` and close it. Because
  `menu.popup` resolves only on selection or dismissal, subscribe to this event
  separately when a menu contains value controls.
- **Unified menu state vocabulary in `discovery`** — added the exported types
  `MenuNodeState`, `MenuNodeSource`, and `MenuUnaddressableReason`, and applied
  them across `DiscoveryMainMenuCommand`, `DiscoveryContextMenuCommand`,
  `DiscoveryContextMenuTreeNode`, and `DiscoverySearchResult`. Every enumerated
  node now carries the `MenuNodeState` fields — `enabled`, `checked`,
  `radioChecked`, `hidden`, `stateKnown`, and the raw `flags`; command
  enumerations and search results (not tree nodes) additionally carry `source`,
  `executable`, and `unaddressableReason`.
  `stateKnown` is the field to check first on the context-menu side: the SDK
  evaluates display data against a track set, so with nothing selected or
  playing `enabled` / `checked` carry no observation and only `hidden` is
  meaningful.
- **`discovery.searchCommands` covers the context menu** — the wrapper accepts
  `{ scope }` (`'all'` default, or `'mainmenu'` / `'contextmenu'`) and
  `{ includeHidden }`. Hits carry `type: 'mainmenu' | 'contextmenu'` plus the
  state fields above, and the response echoes `scope` / `includeHidden` and adds
  `mainMenuHits` / `contextMenuHits` / `stateKnown`.
- **`discovery.getContextMenuTree` reports truncation** — the response gained
  `truncated`, `depthExceeded`, `childrenExceeded`, `maxDepth`, and
  `maxChildrenPerNode`; each node gained `depth`, `childrenReturned`, and its own
  `truncated` / `depthExceeded` / `childrenExceeded`. A `popup` node's
  `childCount` (the host's real count) can now be reconciled against
  `childrenReturned` (what the response contains).
- `DiscoveryServiceCounts` gained `contextMenuCommands`, and
  `DiscoveryGetAllServicesResponse` gained `contextMenuHiddenFiltered` and
  `stateKnown`.
- `DiscoveryGetContextMenuCommandsResponse` declares the `includeHidden` /
  `hiddenFiltered` / `stateKnown` / `selectionCount` fields the host already
  returns, and `DiscoveryGetMainMenuCommandsResponse` declares `includeHidden`;
  `getContextMenuCommands()` now takes the `includeHidden` option.
- `executeContextMenuCommand()` declares `hidden`, `resolved`, `name`, and
  `force` on its response.
- `menu.runContextCommand` accepts `subGuid`, addressing a dynamically
  generated child (a rating value, a converter preset). Without it the owning
  container is targeted, which runs nothing. The wrapper signature is now
  `runContextCommand(command, options?)`.
- `MenuRunContextCommandResponse.executionConfirmed` distinguishes "the host
  reported the command ran" from "the command was dispatched through an entry
  point that returns nothing". It is absent on early-validation failures, where
  nothing was dispatched at all.
- `splitMenuAddress(value)` — decodes an `idMap` entry of the `mainmenu` family
  back into `{ command, subGuid? }`. Exported because the encoding crosses the
  boundary between the menu builder and the dispatcher.
- `SmpMenuFamily` (`'mainmenu' | 'contextmenu'`) and the matching required
  `family` field on `SmpMenuBuildState`. The two families use disjoint
  identifier spaces, and a shared builder cannot infer which one it is walking.
- **Native drag-drop pipeline (`dnd`)** — the host now observes drag gestures
  itself through a native `IDropTarget` bridge and hands the page what HTML5
  deliberately hides: real filesystem paths. New surface:
  - `dnd.getPathsAsync(sessionId?)` — the reliable way to read a session's
    paths from inside a HTML5 `drop` handler.
  - `dnd.getPaths()` / `dnd.hasFiles()` — synchronous best-effort reads of the
    page-side session snapshot, for optimistic UI during `dragover`.
  - `dnd.getCapabilities()` — whether this window can deliver paths at all;
    subscribe to `dnd:capabilitiesChanged` for withdrawals.
  - Events `dnd:enter` (paths, `hasFiles`, cursor position), `dnd:leave`, and
    `dnd:drop` (final paths, cursor position, `keyState`), correlated by
    `sessionId`. Paths are withheld from untrusted origins while `hasFiles`
    stays accurate.

### Changed

- **Breaking type fix** — `DiscoveryContextMenuCommand` was a type alias for
  `DiscoveryMainMenuCommand`, which claimed fields the context tier never
  returns. It is now an independent interface: context items are registered flat
  and placed by the host, so they have no menu `path` and no dynamic-expansion
  fields. TypeScript code that read `path` / `isDynamic` / `subGuid` off a
  context-menu command was reading a field that was never populated.
- `SmpRawMenuItem` gained `enabled`, `checked`, `stateKnown`, and `subGuid`.
- **`MenuCommand.flags` / `commandId` are now optional** — both are tier
  dependent, and declaring them required made every v1-tier response a type lie:
  the HMENU tier reads state from Win32 and has no SDK `flags`, and the flat
  tier produces neither. `MenuSubmenu.flags` is optional for the same reason.
  `MenuCommand` also extends `MenuNodeState` and gained `source`, `executable`,
  and `unaddressableReason`. Note that `commandId` is a transient Win32 menu id
  that dies with the menu it came from — only `guid` is a durable address.
- **`menu.runMainMenuCommand` accepts `{ subGuid }`** and resolves a name or
  path by exact segment match. An ambiguous name is reported rather than
  resolved: on a localized host three separate commands can share one label, so
  picking the first match would silently run the wrong one.
- **Breaking type change** — `SmpMenuBuildState` gained a required `family`
  field. Only direct callers of `buildMenuItems` are affected;
  `MainMenuManager` / `ContextMenuManager` method signatures are unchanged. At
  runtime an omitted `family` is treated as `'mainmenu'`.
- `buildMenuItems` leaves an id unmapped when the item carries no identifier its
  family can dispatch, instead of storing one the target endpoint rejects.
  `ExecuteByID` then reports failure locally rather than raising a host error.
- **Breaking — `dnd.startDrag` no longer fakes success**: dragging tracks out
  of the window needs a native `IDropSource` this component does not provide.
  The 1.11.0 call answered `success: true` with a `trackCount` while nothing
  was actually dragged; it now resolves with `{ success: false, code:
  'NOT_SUPPORTED' }` (resolves, not rejects — handler error envelopes arrive as
  normal results, so test `success`). The wrapper keeps a single optional
  argument for source compatibility and ignores it; the old second options
  argument is gone.

### Removed

- **Breaking — the drop-zone registry is gone**: `dnd.registerDropZone`,
  `dnd.unregisterDropZone`, and `dnd.getDropZones` have been removed. The old
  flow injected a script that attached HTML5 drag handlers to a CSS selector
  and re-emitted a page-side `dnd:drop` event carrying `File` metadata
  (`name` / `type` / `size`) — never a real filesystem path. Drop observation
  now happens natively in the host, which emits `dnd:enter` / `dnd:leave` /
  `dnd:drop` to the window under the cursor, no registration required; the
  `dnd:drop` payload is redefined accordingly (`sessionId`, `paths`, `x`, `y`,
  `keyState`).
  **Migration**: delete `registerDropZone` / `unregisterDropZone` /
  `getDropZones` calls and any `zoneId` bookkeeping; keep (or add) plain HTML5
  `dragover` / `drop` listeners for visuals and hit-testing; read real paths
  with `await fb.dnd.getPathsAsync()` inside the `drop` handler, or from the
  host-emitted `dnd:drop` payload; gate path-dependent UI on
  `dnd.getCapabilities()`.

### Fixed

- **`MainMenuManager.ExecuteByID` could not dispatch anything** — `buildMenuItems`
  ranked an item's `commandId` above its `guid`, and both main-menu tiers emit a
  `commandId` on every row. Every allocated id therefore mapped to a number,
  while `menu.runMainMenuCommand` declares `command` as a string and rejects a
  number on type, so main-menu dispatch failed for every row rather than for an
  odd few. The two menu families now map only to what their host endpoint
  accepts: the main menu to `guid` (or `path`), the context menu to `commandId`.
- **Dynamic main-menu children were dispatched as their parent** — an item's
  `subGuid` was dropped, so invoking a dynamically generated child ran the
  container slot that owns it. The host documents executing a container slot as
  undefined behaviour. `ExecuteByID` now forwards `subGuid` alongside `command`.
- **`menu.runContextCommand` reported success it had not observed** — resolving
  a command by name dispatched it through an entry point that returns nothing,
  and the handler answered with a hardcoded `success: true` regardless of the
  outcome. The name is now resolved to a GUID and dispatched through the
  result-returning entry point, so `success` reflects what the host reported.
- **`menu.runContextCommand` ran under the wrong caller** — the command was
  executed as `caller_undefined` while enumeration reads item state as the
  playlist-selection or now-playing caller. Components may vary an item's
  visibility and enabled state per caller, so the state a caller was shown did
  not necessarily describe what execution would do. Both sides now use the
  caller matching where the tracks came from.
- **`menu.runMainMenuCommand` no longer leaks host exceptions** — the v2
  menu-tree branch had no exception guard, and `generate_menu()` throws on
  localized foobar2000 builds. The exception escaped to JS as a raw
  host-language `Error` and, worse, skipped every fallback below it, so name and
  path forms failed outright. Failures are now reported as `success: false` with
  a `code`: `MENU_ITEM_DISABLED`, `MENU_MATCH_AMBIGUOUS` (with `candidates`), or
  `MENU_COMMAND_NOT_FOUND`.
- **`menu.getMainMenu` leaves are addressable on localized hosts** — the v1
  HMENU fallback tier, which is the only tier available there, structurally
  cannot produce a GUID: a Win32 menu carries just `wID`. Leaves are now matched
  against the same `get_display()` text the host rendered the menu from and
  backfilled with `guid` / `subGuid` (measured 0/158 → 131/167 on a localized
  host). A leaf whose label is ambiguous is left without a `guid` and marked
  `executable: false` with `unaddressableReason` instead of carrying a guess.
  The tier also began reporting `flags` / `enabled` / `checked` / `hidden`.
- **Disabled main-menu commands are refused** — execution previously reported
  `success: true` for a greyed-out command, and the GUID form skipped the check
  that the name form applied, so the same command was refused by name yet
  "succeeded" by GUID. All three request forms now validate alike. A GUID absent
  from the enumeration is still attempted: the caller may hold a valid address
  this enumeration did not surface.

- **SMP main-menu dispatch** — `buildMenuItems` mapped an allocated menu id to
  the item's `path` before its `guid`. A path has to be matched against a
  generated menu tree, a lookup that fails outright on localized hosts, whereas
  the host resolves a GUID directly; `ExecuteByID` on a main menu therefore did
  not work at all there. GUID is now preferred over path. `commandId` still wins
  for context-menu sessions, where it is authoritative.
- **SMP menu state decoding** — `buildMenuItems` derived `enabled` / `checked`
  by decoding the raw `flags` word, ignoring the normalized booleans the host
  sends. It now prefers `enabled` / `checked`, falling back to `flags` only when
  they are absent, so an older host keeps working. An item marked
  `stateKnown: false` is offered as enabled rather than greyed out: `flags == 0`
  is bit-identical to "enabled, unchecked", so treating unobserved state as
  disabled hid commands the host would have run.
- Corrected the documented `discovery.searchCommands` result taxonomy, which
  claimed a `mainmenu-dynamic` `type` value the host has never emitted. Dynamic
  entries are identified by `isDynamic` / `subGuid`, not by `type`.

## [1.11.0] - 2026-07-27

### Added

- `metadata.read()` and `metadata.readByPath()` accept an optional second
  argument carrying `cueIndex`, matching `metadata.readRaw()`. Use it to select
  a single track inside a CUE sheet or image file; it takes precedence over a
  `|subsong:N` suffix in the path. `metadata.readBatch()` does not accept it —
  that endpoint resolves each path's sub-track from the path itself.
- **`TrayMenuItem.playbackAction`** (`'play-pause' | 'previous' | 'next' |
  'stop'`) — declare a native playback action on a custom tray menu item.
  Appearance (`label` / `icon` / `id`) stays caller-controlled; at composition
  time the host stamps the item as a trusted built-in playback route and runs
  `playback_control` natively. Declared items therefore do **not** emit
  `tray:menuItemClicked` (mirror Electron `MenuItem.role` / Tauri
  `PredefinedMenuItem`). Valid only on a `type: 'normal'` leaf; unknown tokens
  or declarations on separator / submenu / rich controls reject the whole
  `setContextMenu` / `appendMenuItems` call with `INVALID_PARAMS`; nothing is
  applied partially.
  `'exit'` is not accepted (app exit stays the reserved `_sys_exit` item).
  Scope is tray menus only — no effect on `menu.show`. `getMenuItems()`
  round-trips the field. Use this (or built-in `showPlaybackControls` items)
  for background-reliable tray playback while the main page is deep-suspended
  (minimize / tray hide / lock); plain user items that only forward
  `tray:menuItemClicked` to `playback.*` are not guaranteed in those states.
  Requires plugin 1.11.0 or newer; probe
  `config.getVersionInfo().plugin.version` before relying on it. The SDK
  wrapper passes the field through and does not invent a default.
- **`menu.getMainMenu(root?, opts?)`** — added a second options argument
  carrying `locale`, `i18n`, and `withAvailability`. `locale` (default
  `'auto'`) selects the `displayLabel` translation locale, `i18n: false`
  disables label translation entirely, and `withAvailability` (default `true`)
  includes per-submenu command availability counters. The original
  single-argument call keeps its previous behavior.
- **Dynamic main-menu submenus in `discovery`** — `getMainMenuCommands(opts?)`
  and `searchCommands(query, opts?)` accept `{ expandDynamic }`, and
  `executeMainMenuCommand(guid, subGuid?)` accepts the sub-command GUID needed
  to run an expanded entry. `DiscoveryMainMenuCommand` and
  `DiscoverySearchResult` gained `path`, `isDynamic`, `subGuid` (plus
  `isDynamicParent` / `flags` on the former), and the responses now echo
  `expandDynamic` / `dynamicCount`.
- Added `WindowGetBackdropPolicyParams` and `WindowSetBackdropPolicyParams`,
  documenting the `windowId` field that the host resolves through its shared
  window-target resolver. `setBackdropPolicy` requires `backdropPolicy` and
  does not fall back to the main window when no target resolves.
- **SDK-only binary adapters** — added `fb.file.readBinary()`,
  `fb.file.writeBinary()`, `fb.file.writeDataUrl()`,
  `fb.metadata.embedArtworkBytes()`, and
  `fb.metadata.embedArtworkFromDataUrl()`, plus the public
  `FileBinaryWriteOptions` and `MetadataArtworkBytesOptions` types.
- These additive helpers adapt `ArrayBuffer` / `Uint8Array` values and strict
  Base64 Data URLs to the existing `file.read`, `file.write`, and
  `metadata.embedArtwork` wire contracts. They add no Bridge endpoint and do
  not change raw `invoke` or existing facades. Canonical Base64 and Data URL
  validation happens in the SDK before invocation; Host behavior is unchanged.
- Optional trailing `opts` arguments on five wrappers, forwarding parameters
  the Host already read but the SDK had no way to send: `file.delete(path,
  opts?)` and `file.copy(source, destination, opts?)` take `moveToTrash` /
  `overwrite`; `metadata.write(path, tags, opts?)`,
  `metadata.removeField(path, field, opts?)` and
  `metadata.removeTag(path, tags, opts?)` take `cueIndex` to address a single
  track inside a CUE sheet or image file. `metadata.write` gaining `cueIndex`
  closes a real gap in the v1.11.0 CUE work, which wired `cueIndex` into the
  read path only. Omitting `opts` preserves the previous payloads exactly.
- `metadata.readByPath()` now resolves as `MetadataReadByPathResponse &
  JsonObject` instead of a bare `JsonObject`, so the documented fields are
  typed while extra tag keys remain accessible.

### Changed

- Documented that `tray:menuItemClicked` is for ordinary user items / rich
  value controls only. Built-in `showPlaybackControls` / `showSystemItems`
  injections and items declaring `playbackAction` execute natively and do not
  fire the click event; reflect playback button state from `playback:*`.
- `discovery.getMainMenuCommands()` and `discovery.searchCommands()` now expand
  dynamic submenus by default, so results include child commands contributed by
  components that build their menu at runtime (`mainmenu_commands_v2`, e.g.
  ESLyric) in addition to the parent slot. Callers that need the raw static
  registry must pass `{ expandDynamic: false }`. Entries flagged
  `isDynamicParent` are container slots and are not executable on their own.
- `playcount.set()` no longer sends the `count` key. The host never read it, so
  the value silently did nothing; the wire payload is now limited to what the
  handler actually consumes.

### Fixed

- Corrected the declared response types for `ui.isMinimized()` and
  `ui.isAlwaysOnTop()`. The host returns `{ minimized }` and
  `{ enabled, isAlwaysOnTop }`; the previous declarations claimed
  `{ isMinimized }` and `{ alwaysOnTop }`, which never existed on the wire.
  Code written against the old declarations read `undefined` at runtime and now
  fails type-checking instead — read `minimized` / `enabled` (or
  `isAlwaysOnTop`) going forward.
- Corrected `output.getDevices()` element typing and error handling. The
  declared element type previously borrowed the `config.getOutputDevices`
  shape (`id` / `isCurrent` / `outputId` / `deviceId`), none of which exist on
  the wire; devices actually carry `guid`, `name`, `entry`, and `entryGuid`
  (exported as `OutputDeviceInfo`). Device `guid` is all-zero for a backend's
  "default device" row and may repeat across backends, so key rows by the
  `(entryGuid, guid)` pair. The wrapper also silently returned an empty array
  when the host reported a failure; it now throws with the host error message
  instead, since the flat array return type has no error channel.
- `dsp.moveDsp()` now types the `from` / `to` fields echoed in the response;
  `to` reflects the final landing index after the move.
- Corrected `http.*` request dispatch to invoke `http.get` directly instead of
  threading an unused method parameter, and aligned `menu` / `ui` facades with
  the generated parameter and response contracts.

## [1.10.0] - 2026-07-16

### Added

- **`TrayMenuItem.orientation`** (`'horizontal' | 'vertical'`) — slider-only
  axis for the WebView tray menu. Default when omitted: horizontal. Only exact
  `'vertical'` is vertical (min bottom / max top; Up/Right increase; Down/Left
  decrease; Home/End edges). Native backends ignore orientation (stepped
  submenu degrade). Older runtimes ignore the unknown key and keep horizontal
  interaction. Range normalization is shared (`max<min` swap, `max==min`
  constant with no value change, initial clamp, IPC out-of-range reject).
  `getMenuItems()` round-trips orientation. Requires plugin 1.10.0 or newer;
  themes that must support older hosts should probe
  `config.getVersionInfo().plugin.version` first. The SDK wrapper
  passes the field through and does not inject a default.
- **`TrayMenuConfig.layoutMode`** (`'flat' | 'zones'`) — opt-in WebView tray DOM
  structure. Default `'flat'` keeps legacy `#menu > .fb-item` direct children.
  `'zones'` emits `.fb-zone[data-zone]` wrappers for non-empty top / playback /
  bottom containers. Native backends ignore the field; older runtimes ignore the
  unknown key without creating wrappers. Public `menu.show` is unaffected.
  Requires plugin 1.10.0 or newer; themes that must support older hosts should
  probe `config.getVersionInfo().plugin.version` first.
  Stable CSS hooks: `.fb-menu[data-depth]`, `.fb-zone[data-zone]`,
  `.fb-item[data-item-id|data-kind|data-depth|data-zone]`. `data-item-token` is
  internal and not a public CSS contract.

### Changed

- Self-drawn tray menu accessibility: navigation/editor focus modes with roving
  tabindex and real focus; ARIA for menuitem / menuitemcheckbox / slider /
  radiogroup; `checked: false` remains checkable; default enter/exit animations
  honor `prefers-reduced-motion: reduce` (custom CSS is the theme author's
  responsibility; hide protocol / `closeAnimationMs` unchanged).
- Self-drawn menu protected CSS no longer forces visible `#menu { display:block
  !important }`. Themes may set root / zone `display` to flex or grid without
  specificity hacks; hidden menus still cannot be re-shown by user
  `display:* !important`.

### Security

- **Tray / self-drawn menu SVG allowlist** — the `'webview'` menu renderer no
  longer mounts icon markup via raw `innerHTML`. Each `iconSvg` (item and
  segmented option) is parsed with `DOMParser` and cloned through an element /
  attribute allowlist; illegal or oversized icons are dropped and the menu
  continues. There is no “caller already sanitized” bypass. Transform values are
  parsed strictly (no leading/inter-function junk, required arity); live nodes
  must be in the SVG namespace (empty namespace rejected).
- **Tray / self-drawn menu resource preflight** — `tray.setContextMenu`,
  `tray.appendMenuItems`, and `menu.show` now reject oversized menus before any
  persistent tray config is replaced or any overlay is opened. Caps: 512 items,
  `menu.show` depth 8, 64 segmented options, 256 KiB CSS, 256 KiB total SVG
  content. A single SVG over 32 KiB is dropped (menu continues); other breaches
  return `INVALID_PARAMS` with `details: { field, limit, actual }`. Oversized /
  unsafe resource inputs are an intentional incompatibility with previously
  unbounded payloads.
- **Tray action routing fix** — built-ins are routed by trusted internal origin
  metadata instead of public item-id prefixes. The exact, case-sensitive tray
  ID `_sys_exit` remains the documented 1.9.0 compatibility command and exits
  foobar2000. Caller-supplied `_pb_playPause`, `_pb_prev`, `_pb_next`, and
  `_pb_stop` are normal user items and emit `tray:menuItemClicked`; only runtime
  auto-injected playback controls are privileged, and caller items with the same
  IDs do not suppress those injected controls. Similar `_sys_*` IDs and the
  generic `menu.show` API receive no promotion. Selection and rich-value IPC
  still use per-show opaque tokens and validate caller HWND, current menu ID,
  enabled state, control kind, and value range before dispatch.

### Changed

- **`TrayMenuItem.icon` documentation** — corrected the JSDoc: the base64 ICO
  payload is a **reserved field not currently rendered by either backend** (the
  native `TrackPopupMenu` menu is text-only; the `'webview'` menu draws
  `iconSvg`, not this field). Use `iconSvg` for a menu-item icon. No behaviour
  change — the field was already unused by the renderer.

### Fixed

- **`tray:menuItemClicked` for `'segmented'`** — a `'segmented'` pick reports
  `{ id, value }` (the picked zero-based segment index) through the value channel
  and keeps the menu open, matching `'rating'` / `'slider'`. The shared keep-open
  contract and the `TrayMenuItemClickedPayload.value` documentation previously
  listed only `'rating'` / `'slider'`, so a consumer reading the contract could
  treat a segmented pick as a closing click. The `'webview'` runtime already kept
  the menu open; this only aligns the contract and docs with the runtime.
- **Empty tray-menu zones no longer emit an orphan separator** — a menu zone
  whose items are all `visible: false` (or empty) used to leave a leading /
  trailing divider once the hidden items were filtered out downstream. Visible
  filtering now precedes the separator decision, so both the native and
  `'webview'` menus drop the stray separator. Default-behaviour correction.

## [1.9.0] - 2026-06-18

### Added

- **`TrayMenuItem.iconSvg`** — `{ viewBox, content }` inline monochrome SVG icon
  for normal / submenu items, rendered before the label by the `render: 'webview'`
  tray menu only (the native backend ignores it). Drawn with `fill: currentColor`
  so it follows the menu text colour; when any item in a menu layer supplies an
  icon, all normal/submenu items reserve a fixed 16px icon column so labels stay
  left-aligned.
- **`TrayMenuConfig.autoNowPlaying`** — when `true`, `nowplaying` items get any
  empty field (`cover` / `title` / `subtitle`) auto-filled from the current track
  at right-click time (frontend-first, backend-fallback; any value you supply
  wins). `cover` auto-fill is `'webview'`-only and reads the current track's
  front art downscaled to a thumbnail; `title` / `subtitle` use `%title%`
  (filename fallback) / `%artist%`, so live-stream dynamic titles work too.

### Changed

- **`TrayMenuItem.cover`** now also accepts an `http(s)://` URL (in addition to a
  `data:` URL and raw base64) in the `'webview'` tray menu, so streaming
  front-ends can pass a resolved cover URL directly.

## [1.8.0] - 2026-06-10

### Added

- **`fb.menu` self-drawn menus** — `menu.show(...)` / `menu.close()`
  render a context menu inside your own WebView (recursive submenu
  support), so themes can fully style native-style menus instead of
  relying on the OS menu.
- **`fb.tray` owner-mode menu** — the tray context menu now accepts
  `render: 'webview'` so it can be drawn by your WebView, plus
  **`tray.setMenuItemState(...)`** for fine-grained per-item
  enable / check state.

### Fixed

- **Published type bundle lost `HTMLElementTagNameMap` for `fb-*`
  elements.** `rollup-plugin-dts` tree-shook the empty type-only
  `import './generated/global.js'`, dropping the whole `declare global`
  block from `dist/components.d.ts`; npm consumers lost
  `document.createElement('fb-...')` typing and element inference for
  `querySelector` / JSX. The tsup DTS footer now re-injects the
  augmentation so the published `.d.ts` carries it. No source API
  changed.
- **Package root (`foo-webview-sdk`) is now fully typed for runtime
  imports.** The root entry re-exports the aggregate `fb` (default and
  named), every namespace proxy, `bridge` and `state` alongside the
  shared types, so `import fb from 'foo-webview-sdk'` and
  `import { player } from 'foo-webview-sdk'` resolve with full typings.
  Previously only the `foo-webview-sdk/bridge` sub-path carried the
  runtime types while the root resolved to a types-only surface.

## [1.7.0] - 2026-06-06

### Added

- **`fb.taskbar` + `fb.tray` namespaces** — Windows taskbar thumbnail
  toolbar buttons and a system tray icon, including incremental tray
  menu management via the new `TrayMenuConfig` interface.
- **`webview:processFailed` event** — emitted when the WebView2 render
  process crashes or exits, enabling diagnostics and auto-recovery
  handling from the theme side.

### Changed

- **`library.getAll` performance** — cold-cache full serialization is
  now offloaded to a background thread and the redundant double
  deep-copy on cache hits was removed. Large libraries enumerate
  noticeably faster with no API change.

### Fixed

- **`plman.SetPlaylistSelection` (SMP-compat)** no longer ignores the
  host `success` return value. `FbPlaylistView` rating updates no longer
  leave a floating promise.
- 1.7.0 release packaging and generated SDK type fixes.

## [1.6.1] - 2026-05-20

### Added

- **`fb.cursor` namespace** — `cursor.setHidden()` / `cursor.isHidden()`
  plus the `cursor:hiddenChanged` event for cursor visibility control.
- **`fb.http.*` `insecureTls` option** — opt-in (double-gated) bypass of
  TLS certificate verification for development / self-signed endpoints.

### Fixed

- **`fb.http.*` `responseType: 'arraybuffer'`** failing because the
  response was run through strict UTF-8 validation; binary responses now
  pass through unmodified.

## [1.6.0] - 2026-05-11

This release fixes 9 long-standing namespace facade drifts reported from
front-end consumers, adds an internal namespace-coverage audit to guard
against regressions, and ships English defaults for the bundled Web
Components.

### Added

- **`Bridge.setMetricsHook(hook | undefined)`** — install or remove an
  instrumentation hook that fires after every `bridge.invoke()`
  settles, on both the host path and the mock-fallback path. Receives
  a `BridgeInvokeMetrics` snapshot
  (`{ method, durationMs, success, result?, error? }`). Exceptions
  thrown by the hook are caught, logged via `console.warn` (with the
  invoke method name and the original error), and then discarded so
  observability failures cannot destabilise invoke callers.
- **`replaygain.scan(paths, opts?)`** — new optional
  `opts.mode: 'track' | 'album'` controlling whether the scan runs
  per-file track gain or treats the selection as a single album.
- **`rating.set(path, rating, opts?)`** — new optional
  `opts.cueIndex: number` for explicit CUE subsong index (takes
  precedence over `|subsong:N` suffixes in the path).
- **`player.playPaths(paths, options?)`** — accepts either a numeric
  `startIndex` (matching the prior signature) or a
  `{ startIndex?, replace? }` options object. `replace: true` clears
  the active playlist before insertion.
- **`config.setOutputBuffer(value)`** — accepts either numeric
  milliseconds (legacy compatibility) or a
  `{ milliseconds?, bufferLength? }` options object so callers can
  express the buffer in either unit.
- **`REPLAYGAIN_SOURCE_MODE`** constant dictionary and the
  `ReplaygainSourceMode` / `ReplaygainSourceModeName` types — exported
  alongside `config.{get,set}ReplaygainMode` so callers can compare
  against named entries (`REPLAYGAIN_SOURCE_MODE.track`, etc.) instead
  of memorising the integer literals.
- **Internal namespace-coverage audit** — detects drift between
  hand-written namespace facades and generated `*Params` / `*Response`
  interfaces, preventing future facade regressions.
- **`fb.metadata.embedArtwork(path, opts?)`** — new optional fields on
  `opts`:
  - `target: 'embedded' | 'file' | 'all' | string[]` (default
    `'embedded'`) — write into the file's tag container, write a
    sibling image alongside the audio, or both. Solves the long-
    standing CUE limitation where `album_art_editor` rejects the
    container.
  - `filename: string` — override the auto-generated sidecar name
    when `target` includes `'file'`. Path separators and `..` are
    rejected.
  Sidecar naming uses fb2k's default external artwork pattern —
  `front` → `cover.<ext>`, other types → `<type>.<ext>`. The
  extension is inferred from the image magic bytes (JPEG / PNG /
  WebP / GIF / BMP; fallback `.jpg`). CUE / `|subsong:N` paths
  share one sidecar per directory (per-directory model, matches
  fb2k's external artwork lookup). The SDK response type is now
  the generated `MetadataEmbedArtworkResponse` (adds `savedTo`
  and `results`); previous destructures (`{ success, size }`)
  continue to compile.

### Changed (BREAKING)

- **`config.getReplaygainMode()` response shape** — `mode` and
  `value` are now typed as `0 | 1 | 2 | 3` rather than `string`.
  Existing comparisons such as `r.mode === 'track'` no longer
  compile; switch to `r.mode === REPLAYGAIN_SOURCE_MODE.track` or to
  the integer literal directly.
- **`config.setReplaygainMode(mode)` argument type** — accepts
  `ReplaygainSourceMode | ReplaygainSourceModeName` (the integer
  union or the named alias). Arbitrary strings outside the canonical
  alias union now fail to type-check, instead of silently being
  routed to `mode: 0` (none) by the host.
- **`artwork.getFb2kUrl()` / `artwork.getFb2kUrlByPath()` response
  shape** — now returns `ArtworkGetFb2kUrlResponse` /
  `ArtworkGetFb2kUrlByPathResponse` from the generated layer. The
  resolved URL is in the `dataUrl` field, not `url`. The previous
  `<{ url: string }>` annotation on these methods produced
  `r.url === undefined` at runtime; switch to `r.dataUrl`.
- **Bundled Web Components default UI strings switched to English.**
  `FbLibraryTree` / `FbLibraryFilesystemTree` / `FbLyricsPanel` /
  `FbPropertiesPanel` previously rendered Chinese-language placeholders
  (e.g. `所有艺术家`, `(无内容)`, `无歌词`, `编解码器`). They now
  render English defaults (`All Artists`, `(empty)`, `No lyrics`,
  `Codec`, …). Themes that depended on the Chinese copy must override
  the relevant slot or part to restore custom localised text. No
  public component API signature changed; only default text.

### Fixed

- **`Bridge.getNativeFb2k()`** no longer requires
  `window.fb2k._handleResponse` to be defined as a precondition for
  binding the host bridge. The host installs `invoke` and
  `_handleResponse` atomically, so the extra sentinel was redundant
  and rejected legitimate test mocks that only implement the public
  `invoke` / `on` / `off` surface.
- **`config.{get,set}ReplaygainMode`** now exchange data with the
  host in the integer-mode shape it actually expects. The
  earlier SDK silently issued `setReplaygainMode('track')` as
  `{ mode: 'track' }`, which the host parsed as `mode = -1` and
  resolved to `mode = source_mode_none` via the fallback chain —
  effectively turning ReplayGain off for any caller using the named
  string API.

### Migration

`playcount.get` — informational only:
- The earlier SDK's `playcount.get(path)` single-argument variant is
  retained but produces an envelope rather than a bare value. Prefer
  `playcount.getBatch([path])` for new code; the single-argument
  variant remains supported for backwards compatibility.

`artwork.getFb2kUrl`:

```ts
// Before
const r = await fb.artwork.getFb2kUrl();
const src = r.url;       // undefined — silent bug.

// After
const r = await fb.artwork.getFb2kUrl();
const src = r.dataUrl;   // canonical field.
```

`config.{get,set}ReplaygainMode`:

```ts
import { REPLAYGAIN_SOURCE_MODE, fb } from 'foo-webview-sdk';

// Before — silently set mode to 'none' on the host
await fb.config.setReplaygainMode('track');
const r = await fb.config.getReplaygainMode();
if (r.mode === 'track') { /* never true */ }

// After
await fb.config.setReplaygainMode('track');           // OK — named alias
await fb.config.setReplaygainMode(1);                 // OK — integer
const r = await fb.config.getReplaygainMode();
if (r.mode === REPLAYGAIN_SOURCE_MODE.track) { /* matches */ }
```

`Web Components default text`:

If a theme depends on the previous Chinese defaults, override the
relevant slot or part. Each affected component exposes its visible
text through the regular slot / part surface; consult the
component's JSDoc for the exact slot names.

## [1.5.0] - 2026-05-06

Version realigned with the plugin DLL under the unified-versioning policy
(the SDK version now always matches the component version). The numeric
drop from `2.0.0` to `1.5.0` is a deliberate alignment with the plugin
release cadence, **not** a regression of any SDK feature or behavior.

The publishing-layout migration introduced in the earlier 2.0.0 (dist-only
entry, sub-path exports, archived hand-written files) remains in effect. No
SDK-level public API or behavior has changed since that version.

## Earlier 2.0.0 (publishing layout) - 2026-05-05

This is a publishing-layout migration release. The runtime API surface
is **identical** to 1.4.x; consumers using the documented public exports
will not see behavioural changes. The breaking changes are limited to
package layout, deep file paths, and TypeScript module resolution.

### Changed (BREAKING)

- **Publishing entry switched to `./dist/`** — TypeScript SDK source
  (`sdk/src/**/*.ts`) is now built with `tsup` and published from
  `sdk/dist/`. The hand-written legacy top-level files
  (`sdk/bridge.js`, `sdk/index.d.ts`, `sdk/index.mjs`,
  `sdk/components.js`, `sdk/components.d.ts`, `sdk/smp-compat.js`,
  `sdk/smp/**/*.js`) have been moved out of the published package.
- **`package.json` exports rewritten**:
  - `main` / `module` → `./dist/bridge.js`
  - `types` → `./dist/index.d.ts`
  - `browser` → `./dist/bridge.global.js` (IIFE bundle)
  - Sub-paths: `./bridge`, `./components`, `./smp-compat` now resolve to
    `./dist/<name>.js`; new `./bridge.global`, `./components.global`,
    `./smp-compat.global` sub-paths expose the IIFE bundles for direct
    `<script>` consumption.
- **`files` array** reduced to `["dist", "LICENSE", "README.md", "CHANGELOG.md"]`.
- **Deep imports `foo-webview-sdk/smp/<class>.js` removed** — import the
  individual SMP wrapper classes from `foo-webview-sdk/smp-compat`
  instead (e.g. `import { FbMetadbHandle } from 'foo-webview-sdk/smp-compat'`).

### Removed

- The hand-written `sdk/bridge.js`, `sdk/components.js`,
  `sdk/smp-compat.js`, `sdk/smp/*`, `sdk/index.d.ts`, `sdk/index.mjs`,
  and `sdk/components.d.ts` files have been archived for historical
  reference and are no longer included in the npm package.
- The legacy namespace-parity check script has been removed; legacy ↔ TS
  namespace parity verification is no longer required after the migration.

### Migration guide

Most consumers do not need any code changes:

```js
// Continues to work — resolves to ./dist/bridge.js
import fb from 'foo-webview-sdk';
import { player, playlist } from 'foo-webview-sdk';
import 'foo-webview-sdk/components';
```

`<script>` tag consumers must update the file path:

```html
<!-- Before (1.x) -->
<script src="node_modules/foo-webview-sdk/bridge.js"></script>
<script src="node_modules/foo-webview-sdk/components.js"></script>

<!-- After (2.0) -->
<script src="node_modules/foo-webview-sdk/dist/bridge.global.js"></script>
<script src="node_modules/foo-webview-sdk/dist/components.global.js"></script>
```

Deep imports of SMP wrapper classes must be updated:

```js
// Before (1.x)
import { FbMetadbHandle } from 'foo-webview-sdk/smp/FbMetadbHandle.js';

// After (2.0)
import { FbMetadbHandle } from 'foo-webview-sdk/smp-compat';
```

## [1.4.1] - 2026-04-30

### Added
- ESM entry point (`index.mjs`) — supports `import fb from 'foo-webview-sdk'`
- Named exports for all namespaces (tree-shaking friendly)
- npm distribution workflow
- `components.d.ts` TypeScript definitions for Web Components
- Sub-path exports: `foo-webview-sdk/components`, `foo-webview-sdk/smp-compat`

### Changed
- Package renamed from `@foo-ui/webview-sdk` to `foo-webview-sdk`
- Package is no longer private — published to npm public registry
- JSON response keys normalised to camelCase (`albumArtist`, `trackNumber`, `discNumber`, `hasLyrics`)

### Fixed
- Removed deprecated snake_case compatibility fields from TypeScript definitions

## [1.4.0] - 2026-04-29

### Added
- PortHub cross-window communication (`fb.port`, `fb.event`, `fb.sharedState`)
- Web Components library (`components.js`) — 30+ zero-style functional building blocks
- SMP compatibility layer (`smp-compat.js`) for Spider Monkey Panel script migration
- Full TypeScript definitions (`index.d.ts`, `components.d.ts`)

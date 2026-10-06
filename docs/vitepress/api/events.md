# Events API

Runtime event payload reference for foo_ui_webview2. Event names use **colon** format (`namespace:eventName`); method invocations use **dot** format (`namespace.method`).

Each event below is generated from the host's event declarations, so its fields match what the host sends. Events private to the built-in menu overlay are not listed.

## api events

### api:registered

<!-- api-schema:begin event:api:registered -->
An external plugin registered a method, or replaced one it had registered before. `isExternal` is always `true`.

Sent to every window.

**Payload**

The payload is a [SystemApiInfo](../reference/types.md#systemapiinfo).
<!-- api-schema:end -->

### api:unregistered

<!-- api-schema:begin event:api:unregistered -->
An external plugin removed one of its methods. Unregistering the whole plugin sends `plugin:unregistered` instead.

Sent to every window.

**Payload**

The payload is a [SystemApiInfo](../reference/types.md#systemapiinfo).
<!-- api-schema:end -->

## app events

### app:beforeQuit

<!-- api-schema:begin event:app:beforeQuit -->
foobar2000 is exiting. Sent once, before any window or panel of the plugin closes, to every page: the main window's, popups' and panels', whatever the interface and whether or not background mode is on. Best effort only: the event is queued to each page and the host does not wait. The pages close right after, so a handler may not run or finish, and a call it makes to the host is unlikely to be answered.

Sent to every window.

**Payload**

The event carries no fields.
<!-- api-schema:end -->

## audio events

### audio:dspPresetChanged

<!-- api-schema:begin event:audio:dspPresetChanged -->
foobar2000's playback DSP chain changed, for example a DSP was added or a preset was loaded.

Sent to every window.

**Payload**

The event carries no fields.
<!-- api-schema:end -->

### audio:fullWaveformFailed

<!-- api-schema:begin event:audio:fullWaveformFailed -->
An `audio.generateFullWaveform` request that answered `pending` ended without a waveform: decoding failed, or the request was cancelled with `audio.cancelFullWaveform`. Delivered like `audio:fullWaveformReady`.

Sent to the page that made the call.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `taskId` | `string` | Id from the `pending` answer. |
| `path` | `string` | The path the request gave. |
| `error` | `string` | What went wrong. |
| `code` | `"CANCELLED" \| "INVALID_HANDLE" \| "NO_INFO" \| "INVALID_PARAMS" \| "DECODER_FAILED" \| "DECODE_FAILED" \| "UNKNOWN_ERROR"` | `CANCELLED` after `audio.cancelFullWaveform`; otherwise why decoding failed. |
<!-- api-schema:end -->

### audio:fullWaveformReady

<!-- api-schema:begin event:audio:fullWaveformReady -->
An `audio.generateFullWaveform` request that answered `pending` finished decoding. Every request waiting on the same decode gets its own event, rendered with its own `method`, `scale` and `signed`. Sent to the page that made the request, or to a page under the same top-level window when that one cannot be found, or else to the main window's page.

Sent to the page that made the call.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `taskId` | `string` | Id from the `pending` answer. |
| `path` | `string` | The path the request gave. |
| `waveform` | `number[]` | One value per point, in `scale`; in `[-1, 1]` when `signed`, else in `[0, 1]`. |
| `maxAmplitude` | `number` | The largest value of the selected sequence before normalisation, in linear full-scale units: with `linear`, `waveform[i] * maxAmplitude` is the absolute level; with `db`, dBFS is `(v * 60 - 60) + 20 * log10(maxAmplitude)`. |
| `duration` | `number` | Track duration in seconds. |
| `sampleRate` | `integer` | Sample rate of the decoded track in Hz. |
| `channels` | `integer` | Channel count of the decoded track. |
| `resolution` | `integer` | Number of points. |
| `method` | `"rms" \| "peak"` | How each point was aggregated, as requested. |
| `scale` | `"linear" \| "db"` | Scale of the points, as requested. |
| `signed` | `boolean` | The points keep PCM polarity, as requested. |
| `cached` | `boolean` | Always `false`: this event only follows a decode. |
<!-- api-schema:end -->

### audio:outputDeviceChanged

<!-- api-schema:begin event:audio:outputDeviceChanged -->
foobar2000's output configuration changed, such as the output device.

Sent to every window.

**Payload**

The event carries no fields.
<!-- api-schema:end -->

### audio:pcmFailed

<!-- api-schema:begin event:audio:pcmFailed -->
An `audio.decodePcm` task ended without samples: decoding failed, the page origin stopped being trusted, the buffer could not be allocated or posted, or the task was cancelled with `audio.cancelDecodePcm`. Delivered like `audio:pcmReady`.

Sent to the page that owns the subscription or task.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `taskId` | `string` | Id from the `audio.decodePcm` answer. |
| `path` | `string` | The path the request gave. |
| `error` | `string` | What went wrong. |
| `code` | `"CANCELLED" \| "ORIGIN_DENIED" \| "OPERATION_FAILED" \| "INVALID_PARAMS" \| "DECODER_FAILED" \| "DECODE_FAILED" \| "NOT_SUPPORTED" \| "UNKNOWN_ERROR"` | `CANCELLED` after `audio.cancelDecodePcm`; `ORIGIN_DENIED` when the page origin stopped being trusted; `OPERATION_FAILED` when the buffer could not be allocated or posted; otherwise why decoding failed. `INVALID_PARAMS` means `start` is at or beyond the end of the track. |
<!-- api-schema:end -->

### audio:pcmReady

<!-- api-schema:begin event:audio:pcmReady -->
An `audio.decodePcm` task finished. Its samples reach the page as a `sharedbufferreceived` event on `chrome.webview` with the same `taskId` in `additionalData`, before or after this event. Sent to the page that started the task, only while that page still shows the document that started it.

Sent to the page that owns the subscription or task.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `taskId` | `string` | Id from the `audio.decodePcm` answer. |
| `path` | `string` | The path the request gave. |
| `sampleRate` | `integer` | Samples per second per channel. |
| `channels` | `integer` | Channel count in the buffer; `1` when the request set `mono`. |
| `frames` | `integer` | Frames per channel in the buffer. |
| `start` | `number` | Track time of the first frame, in seconds. |
| `end` | `number` | Track time right after the last frame, in seconds. |
| `duration` | `number` | `frames / sampleRate`, in seconds. |
| `truncated` | `boolean` | The audio ran past the size estimated from the track length, or its format changed partway; the buffer holds the part before the cut. |
| `resampled` | `boolean` | Converted to `sampleRate` with foobar2000's resampler. |
<!-- api-schema:end -->

### audio:replaygainModeChanged

<!-- api-schema:begin event:audio:replaygainModeChanged -->
A ReplayGain setting of foobar2000's playback changed, not only the source mode.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `mode` | `integer` | The source mode after the change: `0` none, `1` track, `2` album, `3` by playback order. |
<!-- api-schema:end -->

### audio:spectrum

<!-- api-schema:begin event:audio:spectrum -->
A frame of an `audio.subscribeSpectrum` subscription, at the subscription's `fps` while playing, plus one silence frame when playback pauses or stops. The fields are those of an `audio.getSpectrum` answer for the subscription. Sent to the page that subscribed: by its window id, else by its window handle, else to a page under the same top-level window; when none is found the frame is dropped.

Sent to the page that owns the subscription or task. A subscriber can have it delivered under a name of its own; this is the name otherwise.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `subscriptionId` | `string` | Subscription the frame belongs to; absent when the request named none. |
| `output` | `"bands" \| "bins"` | What the frame carries. |
| `spectrum` | `number[]` | Band frames: one value per band in the frame's `scale`; `weighted` values lie in `[0, 1]`, `db` values have a floor of `-160`. Bin frames with `channels: 'mix'`: one power value in dB per bin. |
| `left` | `number[]` | Bin frames with `channels: 'stereo'`: the first channel, one dB value per bin. |
| `right` | `number[]` | Bin frames with `channels: 'stereo'`: the second channel, or the first again for a mono stream. |
| `bands` | `integer` | Band frames: length of `spectrum`. |
| `firstBin` | `integer` | Bin frames: FFT bin index of the first value; value `i` is centred at `(firstBin + i) * sampleRate / fftSize` Hz. `0`, with empty arrays, when the range holds no bin. |
| `fftSize` | `integer` | FFT size actually used. |
| `scale` | `"weighted" \| "db"` | Scale of the values; always `db` for bin frames. |
| `channels` | `"mix" \| "stereo"` | Bin frames: the subscription's channel layout. |
| `channelCount` | `integer` | Bin frames: channel count of the visualization stream; `0` when unknown. |
| `sampleRate` | `integer` | Sample rate of the visualization stream in Hz; `0` when unknown. |
| `minFrequency` | `number` | Lower edge of the frequency range in Hz. |
| `maxFrequency` | `number` | Upper edge in Hz: the requested one capped at `sampleRate / 2`, or `sampleRate / 2` when none was given. |
| `state` | `"playing" \| "paused" \| "stopped"` | Paused or stopped frames are silence. |
| `streamTime` | `number` | Seconds since the visualization stream last started; the frame is computed from the `fftSize` samples ending at this time. |
| `hostTime` | `number` | Host system time when the frame was computed, Unix epoch milliseconds. |
<!-- api-schema:end -->

### audio:stream

<!-- api-schema:begin event:audio:stream -->
The host stopped writing one epoch of an `audio.subscribeStream` buffer because the sample rate or channel count changed; the next epoch's buffer arrives as its own `sharedbufferreceived` event. Other endings (unsubscribe, page gone, host exit) are not reported this way: the buffer's `ended` flag is the signal. Sent to the page that subscribed.

Sent to the page that owns the subscription or task.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `subscriptionId` | `string` | Id from the `audio.subscribeStream` answer. |
| `type` | `"ended"` | Always `ended`. |
| `epoch` | `integer` | The epoch that ended; its buffer has the `ended` flag set. |
| `reason` | `"format-change"` | Always `format-change`. |
<!-- api-schema:end -->

## cursor events

### cursor:hiddenChanged

<!-- api-schema:begin event:cursor:hiddenChanged -->
The calling window's cursor was hidden or restored by `cursor.setHidden`; a call that leaves the state as it was sends nothing.

Sent to the page that made the call.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `hidden` | `boolean` | Whether the cursor is hidden now. |
<!-- api-schema:end -->

## dnd events

### dnd:enter

<!-- api-schema:begin event:dnd:enter -->
A drag entered the window. Only the window under the cursor gets it, never another one, because real filesystem paths are sensitive. `paths` is empty when the drag carries no `CF_HDROP` file list (browser links, virtual shell objects, archive entries) or when the document origin is not trusted with real paths.

Sent to the page of the window or panel it concerns.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `sessionId` | `string` | Ties the `dnd:enter`, `dnd:leave` and `dnd:drop` of one drag together. Unique across the host process, so it also tells which window the drag belongs to. |
| `paths` | `string[]` | Absolute filesystem paths, in `DataTransfer.files` order; empty when the drag carries no file list or the document origin is not trusted with paths. |
| `resolvedPaths` | `(string \| null)[]` | Target of the `.lnk` shortcut at the same index of `paths`, or `null`; always as long as `paths`, including when both are withheld and empty. Only `.lnk` is resolved: a shortcut to a shell object, a target too long to read back intact (Windows caps it at `MAX_PATH`), an unavailable COM apartment and an entry skipped to keep the drop responsive all report `null`, never an empty string. A broken shortcut reports the path it recorded, so a non-null entry says where the shortcut points, not that a file is there. |
| `hasFiles` | `boolean` | Whether the drag carries a `CF_HDROP` file list; reported truthfully even when `paths` is withheld, since it reveals nothing by itself. |
| `source` | `"self" \| "other-window" \| "external"` | Where the drag came from: `self` when it started in this window's page, `other-window` when it started in another window of the same foobar2000 (the main window or a popup), `external` for everything else, such as Explorer, another application, another foobar2000 process or a drag that started in a Default UI or Columns UI panel. The host reads it from a marker it adds to every drag that starts in a window it hosts; any program can imitate that marker, so treat the value as a hint about the drag's origin, not as a security check. |
| `x` | `integer` | Cursor x in client-area physical pixels; divide by `devicePixelRatio` for CSS pixels. |
| `y` | `integer` | Cursor y in client-area physical pixels; divide by `devicePixelRatio` for CSS pixels. |
<!-- api-schema:end -->

### dnd:leave

<!-- api-schema:begin event:dnd:leave -->
The drag left the window without dropping.

Sent to the page of the window or panel it concerns.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `sessionId` | `string` | The drag's session, as in `dnd:enter`. |
<!-- api-schema:end -->

### dnd:drop

<!-- api-schema:begin event:dnd:drop -->
The drag was dropped on the window. A drop lands only where the page accepts it, by calling `preventDefault()` in its HTML5 `dragover` handler, and elsewhere only when released within half a second of entering, before the page has answered. `paths` is the final list, which the drag source may have changed since `dnd:enter`. The event can arrive before or after the page's own HTML5 `drop` handler runs; call `dnd.getPathsAsync` from that handler when the paths are needed together with the drop.

Sent to the page of the window or panel it concerns.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `sessionId` | `string` | The drag's session, as in `dnd:enter`. |
| `paths` | `string[]` | Absolute filesystem paths, in `DataTransfer.files` order; empty when the drag carries no file list or the document origin is not trusted with paths. |
| `resolvedPaths` | `(string \| null)[]` | Target of the `.lnk` shortcut at the same index of `paths`, or `null`; same length and same rules as the field of this name in `dnd:enter`. |
| `x` | `integer` | Cursor x in client-area physical pixels. |
| `y` | `integer` | Cursor y in client-area physical pixels. |
| `keyState` | `integer` | Win32 modifier and mouse-button mask at drop time (`MK_*` flags), for a page that wants modifier-dependent behaviour. It does not change the drop effect reported to the drag source, which is never move or link. |
| `source` | `"self" \| "other-window" \| "external"` | Where the drag came from: `self` when it started in this window's page, `other-window` when it started in another window of the same foobar2000 (the main window or a popup), `external` for everything else, such as Explorer, another application, another foobar2000 process or a drag that started in a Default UI or Columns UI panel. The host reads it from a marker it adds to every drag that starts in a window it hosts; any program can imitate that marker, so treat the value as a hint about the drag's origin, not as a security check. |
<!-- api-schema:end -->

### dnd:capabilitiesChanged

<!-- api-schema:begin event:dnd:capabilitiesChanged -->
What the window's drag-drop integration can deliver changed after the page loaded, for instance when a navigation changes the document origin. The payload has the same fields as `dnd.getCapabilities` returns.

Sent to the page of the window or panel it concerns.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `html5` | `boolean` | The page receives standard HTML5 drag events; `false` means the window has no drag-drop support at all. |
| `paths` | `boolean` | Real filesystem paths are obtainable through `dnd.getPathsAsync` and the `dnd:*` events. Not fixed for the window's lifetime: a navigation or Chromium re-registering its own drop target can withdraw it, announced by `dnd:capabilitiesChanged`. |
| `hosting` | `"visual" \| "standard"` | How the window hosts its WebView: `visual` for the main and popup windows, `standard` for a DUI or CUI panel. |
| `pathsUnavailableReason` | `"register-failed" \| "forward-unavailable" \| "inner-target-not-found" \| "chain-failed" \| "displaced" \| "origin-untrusted"` | Why `paths` is `false`; its presence always means `paths` is `false`. |
| `dragOut` | `boolean` | Files can be dragged out of the window through `dnd.prepareDrag`. Independent of `paths`. |
| `dragOutUnavailableReason` | `"not-visual-hosting" \| "runtime-too-old" \| "register-failed"` | Why `dragOut` is `false`; its presence always means `dragOut` is `false`. |
<!-- api-schema:end -->

### dnd:dragEnded

<!-- api-schema:begin event:dnd:dragEnded -->
The host attached no files to a drag out of the window, decided at the moment the drag started. The drag itself goes on as an ordinary page drag without files, so a drop inside the page still works; the token text is blanked first, and in the rare case that it cannot be the drag is cancelled instead. It is not a completion notice: when the host accepts the token and hands the files over, nothing follows, and the page's own `dragend` reports the outcome through `dataTransfer.dropEffect` (`copy` when a target took the files, `none` when the drag was cancelled or refused). Only a drag that carried a drag token can cause it; a page dragging its own text, image or link never does.

Sent to the page of the window or panel it concerns.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `result` | `"failed"` | Always `failed`: the host reports only refusals. |
| `code` | `"PERMISSION_DENIED" \| "INVALID_PARAMS" \| "OPERATION_FAILED"` | Why no files were attached: `PERMISSION_DENIED` when the token was unknown, expired, already spent, superseded by a later `prepareDrag` or minted for another window; `INVALID_PARAMS` when `dataTransfer.effectAllowed` was not exactly `copy`, which `dnd.applyDragToken` sets correctly; `OPERATION_FAILED` when the file list could not be attached to the drag. |
| `error` | `string` | Human-readable reason; never contains a filesystem path. |
<!-- api-schema:end -->

## file events

### file:opProgress

<!-- api-schema:begin event:file:opProgress -->
Results of a `file.copyAsync`, `file.moveAsync` or `file.deleteAsync` run, in batches: a batch goes out once 64 results are pending or 100 ms have passed since the previous one, and the last partial batch arrives before `file:opComplete`. Sent to the page that started the run; when that window is gone by then, to the main window's page.

Sent to the page that made the call.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `operationId` | `string` | Id from the receipt of the call that started the run. |
| `op` | `"copy" \| "move" \| "delete"` | Which operation: `copy`, `move` or `delete`. |
| `done` | `integer` | Entries reported so far across all batches, failures included. |
| `total` | `integer` | Entries the call accepted. |
| `results` | `FileOpResultItem[]` | The results of this batch only, in request order. |
| `results[].source` | `string` | The source as requested, path variables unexpanded. |
| `results[].destination` | `string` | The destination as requested; absent for `file.deleteAsync`. |
| `results[].status` | `"ok" \| "skipped" \| "failed"` | `ok` when carried out; `skipped` when deliberately not carried out, because the destination existed or the run was cancelled first; `failed` otherwise. |
| `results[].reason` | `"already-exists" \| "not-found" \| "permission" \| "cross-volume" \| "path-too-long" \| "io-error" \| "cancelled"` | Why: `already-exists` or `cancelled` with `skipped`; `not-found`, `permission`, `path-too-long` or `io-error` with `failed`; `cross-volume` with `ok`, for a move across volumes done as a copy and a delete. `path-too-long` means a path the entry needed, inside a copied folder included, is longer than 259 characters, or a folder it had to create is longer than 247. Absent when the entry was carried out plainly. |
<!-- api-schema:end -->

### file:opComplete

<!-- api-schema:begin event:file:opComplete -->
The last event of a run, sent after a cancel and after failed entries too. It is skipped only when foobar2000 is shutting down or the worker fails unexpectedly, so a listener that cleans up on it should keep its own timeout. Afterwards the `operationId` is gone and `file.cancelOp` reports `cancelled: false` for it. Delivered like `file:opProgress`.

Sent to the page that made the call.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `operationId` | `string` | Id from the receipt of the call that started the run. |
| `op` | `"copy" \| "move" \| "delete"` | Which operation: `copy`, `move` or `delete`. |
| `total` | `integer` | Entries the call accepted. |
| `successCount` | `integer` | Entries reported `ok`. |
| `skippedCount` | `integer` | Entries reported `skipped`. |
| `failureCount` | `integer` | Entries reported `failed`. |
| `cancelled` | `boolean` | `true` when at least one entry was reported with reason `cancelled`. |
<!-- api-schema:end -->

## http events

### http:response

<!-- api-schema:begin event:http:response -->
The outcome of an async `http.get`, `http.post`, `http.put`, `http.delete`, `http.patch` or `http.head`, carrying the receipt's `requestId`. On success it has the fields of the synchronous response; on failure `error` and `code`, like a failed call. Sent to the page that made the request, or to a page under the same top-level window when that one cannot be found, or else to the main window's page.

Sent to the page that made the call.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `requestId` | `string` | Id from the receipt. |
| `success` | `boolean` | Whether a response came back. A response with an error status such as `404` is a success. |
| `status` | `integer` | HTTP status code: on success that of the response; on failure only when a redirect was refused under `redirect: "error"`, that of the redirect. |
| `headers` | `Record<string, string>` | Response headers by name; of repeated headers the last one wins. Present on success. |
| `body` | `string` | Response body, as text or Base64 according to `responseType`; always empty for `http.head`. Present on success. |
| `responseType` | `"text" \| "base64"` | `text` or `base64`, the encoding of `body`. Present on success. |
| `contentLength` | `integer` | The `Content-Length` header as a number; present on success of `http.head` when the server sends one that parses. |
| `error` | `string` | Why the request failed; present on failure. |
| `code` | `"INVALID_PARAMS" \| "PERMISSION_DENIED" \| "CANCELLED" \| "OPERATION_FAILED"` | The error code: `INVALID_PARAMS` for a URL that is not `http` or `https`, `PERMISSION_DENIED` for a local or private network address the host setting does not allow, `CANCELLED` after `http.abort`, `OPERATION_FAILED` otherwise. Present on failure. |
| `cancelled` | `boolean` | `true` when the request was cancelled by `http.abort`; absent otherwise. |
<!-- api-schema:end -->

### http:downloadComplete

<!-- api-schema:begin event:http:downloadComplete -->
The outcome of `http.download` with `async: true`, carrying the receipt's `requestId`. On success it has the fields of a synchronous download; on failure `error` and `code`. Delivered like `http:response`.

Sent to the page that made the call.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `requestId` | `string` | Id from the receipt. |
| `success` | `boolean` | Whether the file was written. A response with an error status such as `404` is saved and counts as a success. |
| `status` | `integer` | HTTP status code: on success that of the response; on failure only when a redirect was refused under `redirect: "error"`, that of the redirect. |
| `bytesWritten` | `integer` | Bytes written; present on success. |
| `path` | `string` | The file written, with path variables expanded; present on success. |
| `error` | `string` | Why the download failed; present on failure. |
| `code` | `"INVALID_PARAMS" \| "PERMISSION_DENIED" \| "CANCELLED" \| "OPERATION_FAILED"` | The error code, as in `http:response`. Present on failure. |
| `cancelled` | `boolean` | `true` when the download was cancelled by `http.abort`; absent otherwise. |
<!-- api-schema:end -->

## jitQueue events

### jitQueue:trackChanged

<!-- api-schema:begin event:jitQueue:trackChanged -->
A track of the shadow playlist started playing, a replay of the current track included. `trackId` and `title` are the session's current track as `jitQueue.playNow` or `jitQueue.enqueueNext` named it. Tracks inserted by `jitQueue.preloadBatch` have no identifier: playing one leaves both unchanged, and both are empty once a batch has started playback. The JIT events go to the page that started the session, with `jitQueue.playNow` or with a `jitQueue.preloadBatch` that started playback; when that page is gone, to a page under the same top-level window, and else to the main window's page. A page whose session another page started anew hears nothing more of it. Before any session has started they go to the main window's page.

Sent to the page that made the call.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `trackId` | `string` | Identifier of the session's current track; empty when it has none. |
| `title` | `string` | Title of the session's current track; empty when it has none or none was given. |
<!-- api-schema:end -->

### jitQueue:needNext

<!-- api-schema:begin event:jitQueue:needNext -->
The host needs the next track: none is buffered and no earlier request is waiting for an answer. It checks when a track of the shadow playlist starts, dropping a request left unanswered during the previous track, and again when 30 seconds of the track remain. The page answers with `jitQueue.enqueueNext`, or with `jitQueue.notifyEmpty` when it has no more tracks. Delivered like `jitQueue:trackChanged`.

Sent to the page that made the call.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `currentTrackId` | `string` | Identifier of the session's current track; empty when it has none. |
| `reason` | `"trackChange"` | Always `trackChange`. |
<!-- api-schema:end -->

### jitQueue:listExhausted

<!-- api-schema:begin event:jitQueue:listExhausted -->
`jitQueue.notifyEmpty` was called, even with no session active, or playback of the shadow playlist reached the end of a track while the playlist held no tracks. The session is now `Exhausted`. Delivered like `jitQueue:trackChanged`.

Sent to the page that made the call.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `lastTrackId` | `string` | Identifier of the session's current track; empty when it has none. |
<!-- api-schema:end -->

### jitQueue:preloadComplete

<!-- api-schema:begin event:jitQueue:preloadComplete -->
`jitQueue.preloadBatch` inserted its tracks into the shadow playlist. Sent before the call returns; a call that fails sends nothing. Delivered like `jitQueue:trackChanged`: a batch that starts playback reports to the page that called it, a batch appended to a playing session reports to that session's page.

Sent to the page that made the call.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `count` | `integer` | How many tracks were inserted; the same as `tracksAdded` of the call. |
| `startIndex` | `integer` | `startIndex` of the call. |
| `replace` | `boolean` | `replace` of the call. |
<!-- api-schema:end -->

### jitQueue:error

<!-- api-schema:begin event:jitQueue:error -->
A track given to `jitQueue.playNow` or `jitQueue.enqueueNext` could not be added to the shadow playlist: its location resolved to no tracks, the shadow playlist was gone, or adding it failed with an exception. The track is added after the call has already succeeded, so the failure arrives only as this event. Delivered like `jitQueue:trackChanged`.

Sent to the page that made the call.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `trackId` | `string` | Identifier the call gave the track. |
| `error` | `string` | What went wrong, in English. |
| `url` | `string` | The location, when it is not a local file path. Exactly one of `url` and `path` is present. |
| `path` | `string` | The location, when it is a local file path. |
<!-- api-schema:end -->

## keyboard events

### keyboard:hotkey

<!-- api-schema:begin event:keyboard:hotkey -->
A hotkey registered with `keyboard.registerHotkey` was pressed. It goes to the window that registered it. A hotkey stays registered after that window closes; its presses then go to the main window's page.

Sent to the page that owns the subscription or task.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `id` | `integer` | The id `keyboard.registerHotkey` returned. |
| `key` | `string` | The combination as it was registered. |
| `action` | `string` | The action name given at registration. |
<!-- api-schema:end -->

## library events

### library:initialized

<!-- api-schema:begin event:library:initialized -->
foobar2000 reported that the Media Library has finished initializing. The host drops its library caches before sending it. Every window receives it; a page that loads afterwards does not.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `timestamp` | `integer` | When the host sent it, in milliseconds since the Unix epoch. |
<!-- api-schema:end -->

### library:itemsAdded

<!-- api-schema:begin event:library:itemsAdded -->
Tracks were added to the Media Library. The host drops its library caches before sending it.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `count` | `integer` | How many tracks. |
| `timestamp` | `integer` | When the host sent it, in milliseconds since the Unix epoch. |
<!-- api-schema:end -->

### library:itemsRemoved

<!-- api-schema:begin event:library:itemsRemoved -->
Tracks were removed from the Media Library. The host drops its library caches before sending it.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `count` | `integer` | How many tracks. |
| `timestamp` | `integer` | When the host sent it, in milliseconds since the Unix epoch. |
<!-- api-schema:end -->

### library:itemsModified

<!-- api-schema:begin event:library:itemsModified -->
Tracks in the Media Library were modified, for example by a tag edit. The host drops its library caches before sending it.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `count` | `integer` | How many tracks. |
| `timestamp` | `integer` | When the host sent it, in milliseconds since the Unix epoch. |
<!-- api-schema:end -->

### library:getAllResult

<!-- api-schema:begin event:library:getAllResult -->
The page of a `library.getAll` that answered `{ pending: true, requestId }`: the whole library in library order, built off the main thread. The list is kept for later calls only when the library did not change meanwhile. Sent to the page that made the call, or to a page under the same top-level window when that one cannot be found, or else to the main window's page.

Sent to the page that made the call.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `requestId` | `string` | Id from the `{ pending: true, requestId }` answer. |
| `tracks` | `LibraryTrack[]` | The whole library in library order; `index` is the position in the library. Empty when `error` is present. |
| `tracks[].…` | [Track](../reference/types.md#track) | Every field of [Track](../reference/types.md#track). |
| `tracks[].index` | `integer` | Row number; the list the row is in says what it counts. |
| `items` | `LibraryTrack[]` | The same list as `tracks`. |
| `items[].…` | [Track](../reference/types.md#track) | Every field of [Track](../reference/types.md#track). |
| `items[].index` | `integer` | Row number; the list the row is in says what it counts. |
| `total` | `integer` | Tracks in the library when the call was made; `0` when `error` is present. |
| `offset` | `integer` | The `offset` of the call, always `0`. |
| `limit` | `integer` | The `limit` of the call. |
| `fromCache` | `boolean` | Always `false`. |
| `error` | `string` | Why the list could not be built; present only then. |
<!-- api-schema:end -->

## menu events

### menu:select

<!-- api-schema:begin event:menu:select -->
A row of a menu opened with `menu.show` was chosen; `menu:dismiss` with reason `select` follows as the menu closes. Sent to the page that called `menu.show`, or to a page under the same top-level window when that one cannot be found, or else to the main window's page.

Sent to the page that made the call.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `menuId` | `string` | Id of the menu, as `menu.show` returned it. |
| `itemId` | `string` | `id` of the chosen row. |
<!-- api-schema:end -->

### menu:dismiss

<!-- api-schema:begin event:menu:dismiss -->
A menu opened with `menu.show` closed, whether or not a row was chosen. Delivered like `menu:select`, also when another page's `menu.show` or `menu.close` closed the menu.

Sent to the page that made the call.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `menuId` | `string` | Id of the menu, as `menu.show` returned it. |
| `reason` | `string` | Why the menu closed: `select` after a row was chosen, `outside` for a click outside it, `escape`, `blur` when another window took the focus, `replaced` when another menu opened, `timeout` when the menu page did not answer in time, or the `reason` given to `menu.close`. |
<!-- api-schema:end -->

### menu:valueChanged

<!-- api-schema:begin event:menu:valueChanged -->
A rating, slider or segmented control in a menu opened with `menu.show` changed value; the menu stays open. Delivered like `menu:select`.

Sent to the page that made the call.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `menuId` | `string` | Id of the menu, as `menu.show` returned it. |
| `itemId` | `string` | `id` of the control's row. |
| `value` | `integer` | The control's new value. |
<!-- api-schema:end -->

## metadata events

### metadata:probeProgress

<!-- api-schema:begin event:metadata:probeProgress -->
Results of a `metadata.probeBatchAsync` run, in batches: a batch goes out once 64 results are pending or 100 ms have passed since the previous one, and the last partial batch arrives before `metadata:probeComplete`. A path the run was cancelled on or never reached is not reported. Sent to the page that started the probe; when that window is gone by then, to the main window's page.

Sent to the page that made the call.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `operationId` | `string` | Id from the `metadata.probeBatchAsync` receipt. |
| `done` | `integer` | Paths reported so far across all batches, failures included. |
| `total` | `integer` | Paths the call accepted, its `totalCount`. |
| `results` | `MetadataProbeResultItem[]` | The results of this batch only, in request order. |
| `results[].path` | `string` | The path as requested, `\|subsong:N` included. |
| `results[].success` | `boolean` | Whether the track was read. |
| `results[].infoSource` | `"cached" \| "direct" \| "none"` | `cached` when the host's complete cached info was used, `direct` when the file was read, `none` with a failure. |
| `results[].failure` | `"not-found" \| "unsupported-format" \| "read-error"` | Why the path was not read; present when `success` is `false`. |
| `results[].info` | `TrackTechnicalInfo` | Technical info of the track, as in `metadata.read`; present when `success` is `true`. |
| `results[].info.duration` | `number` | Length in seconds. |
| `results[].info.bitrate` | `integer` | Bitrate in kbit/s as the decoder reports it; `0` when unknown. |
| `results[].info.sampleRate` | `integer` | Sample rate in Hz; `0` when unknown. |
| `results[].info.channels` | `integer` | Channel count; `0` when unknown. |
| `results[].info.codec` | `string` | Codec name as the decoder reports it, such as `FLAC` or `MP3`; empty when unknown. |
| `results[].tags` | `Record<string, any>` | The flat fields of the track, as `includeTags` describes; present when `success` is `true` and the call did not pass `includeTags: false`. |
<!-- api-schema:end -->

### metadata:probeComplete

<!-- api-schema:begin event:metadata:probeComplete -->
The last event of a probe, sent after a cancel and after a failed run too; only an exception while sending it, in practice running out of memory, skips it. Afterwards the `operationId` is gone and `metadata.cancelProbe` reports `cancelled: false` for it. Delivered like `metadata:probeProgress`.

Sent to the page that made the call.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `operationId` | `string` | Id from the `metadata.probeBatchAsync` receipt. |
| `total` | `integer` | Paths the call accepted. |
| `successCount` | `integer` | Paths reported with `success: true`. |
| `failureCount` | `integer` | Paths reported with `success: false`. |
| `cancelled` | `boolean` | `true` when the probe was stopped early, by `metadata.cancelProbe` or because the popup that started it closed; the two counts then fall short of `total`. |
<!-- api-schema:end -->

### metadata:writeComplete

<!-- api-schema:begin event:metadata:writeComplete -->
A tag write queued by `metadata.write`, `metadata.writeBatch`, `metadata.removeTag` or `metadata.removeField` finished. Every window receives it.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `operation` | `"write" \| "removeTag"` | `write` for `metadata.write` and `metadata.writeBatch`, `removeTag` for `metadata.removeTag` and `metadata.removeField`. |
| `path` | `string` | The path as the call gave it, suffix included. |
| `subsong` | `integer` | The subsong written: `cueIndex` when it is 0 or more, otherwise the one the path selects, otherwise `0`. |
| `code` | `integer` | foobar2000's completion code: `0` success, `1` aborted, `2` errors. |
| `success` | `boolean` | `true` when `code` is `0`. |
| `status` | `"success" \| "aborted" \| "error"` | `success`, `aborted` or `error`, after `code`. |
<!-- api-schema:end -->

## metadb events

### metadb:changed

<!-- api-schema:begin event:metadb:changed -->
The metadata of tracks changed: tags were written or read again, or a component such as `foo_playcount` asked foobar2000 to show its fields anew (`fromHook`). Each entry names its track by `handle`, the key the track rows of other methods carry, so a single track of a cue sheet or a multi-track file can be told from the rest of the file.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `tracks` | `MetadbChangedTrackItem[]` | Up to 50 of the changed tracks, in no particular order. An invalid entry is skipped, so the list can hold fewer than the smaller of `count` and 50. |
| `tracks[].handle` | `string` | The track's key, built the way a track row's `handle` is: the native path (foobar2000's own path for a location that has none), with `\|subsong:N` when `subsong` is not `0`. It equals the `handle` of the same track in rows from `library.query`, `playlist.getTracks` and the like. |
| `tracks[].path` | `string` | The file path in native form, or foobar2000's own path for a location that has none, such as a stream. It carries no subsong, so every track of a cue sheet or a multi-track file has the same `path`; `handle` tells them apart. |
| `tracks[].subsong` | `integer` | Subsong identifier inside the file, as a track row's `subsong`; `0` for a whole file. |
| `tracks[].rating` | `integer` | Rating `0` to `5`, `0` for unrated, read the same way as `rating.get`. When foobar2000 holds no information for the track, only the playback statistics are read, and the key is absent unless they carry a rating. |
| `tracks[].playCount` | `integer` | The file's `PLAY_COUNT` tag, when it has one; not the playback statistics. |
| `tracks[].title` | `string` | The first `TITLE` value, when the file has one. |
| `tracks[].artist` | `string` | All `ARTIST` values joined with `, `, when the file has any. |
| `count` | `integer` | How many tracks changed, at least `1`; can exceed the length of `tracks`. |
| `fromHook` | `boolean` | `true` when the files themselves did not change and a component such as `foo_playcount` asked for its fields to be shown anew. |
| `timestamp` | `integer` | When the event was sent, in milliseconds since the Unix epoch. |
<!-- api-schema:end -->

## panel events

### panel:focus

<!-- api-schema:begin event:panel:focus -->
The panel got keyboard focus. Sent only when the panel's `grabFocus` option is on.

Sent to the page of the window or panel it concerns.

**Payload**

The event carries no fields.
<!-- api-schema:end -->

### panel:blur

<!-- api-schema:begin event:panel:blur -->
The panel lost keyboard focus. Sent only when the panel's `grabFocus` option is on.

Sent to the page of the window or panel it concerns.

**Payload**

The event carries no fields.
<!-- api-schema:end -->

### panel:initialized

<!-- api-schema:begin event:panel:initialized -->
A DUI element or CUI panel finished creating its WebView. It is sent once, as soon as the WebView is ready, which can be before the page has subscribed; a page that needs the mode reads `window.getMode` on startup instead.

Sent to the page of the window or panel it concerns.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `mode` | `"dui" \| "cui"` | `dui` for a Default UI element, `cui` for a Columns UI panel. |
| `panelMode` | `boolean` | Always `true`. |
| `windowId` | `string` | Id of the panel, as `window.getMode` reports it. |
<!-- api-schema:end -->

### panel:visibilityChanged

<!-- api-schema:begin event:panel:visibilityChanged -->
A DUI element was shown or hidden, for instance by switching tabs in a tab container. Columns UI panels do not send it.

Sent to the page of the window or panel it concerns.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `visible` | `boolean` | Whether the element is now visible. |
<!-- api-schema:end -->

### panel:configChanged

<!-- api-schema:begin event:panel:configChanged -->
The panel's configuration changed, through `panel.setConfig`, the panel's settings dialog, or a configuration foobar2000 handed to the element. The payload has the same fields as `config` in `panel.getConfig`.

Sent to the page of the window or panel it concerns.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `panelName` | `string` | Display name of the panel. |
| `templateName` | `string` | Name of the page template the panel loads. |
| `edgeStyle` | `integer` | Edge style of the panel frame: `0` none, `1` sunken, `2` grey. |
| `urlOverride` | `string` | URL loaded instead of the template; empty when none. |
| `transparentBackground` | `boolean` | Whether the panel renders with a transparent background. |
| `grabFocus` | `boolean` | Whether a click gives the panel keyboard focus. |
| `enableDragDrop` | `boolean` | Whether files can be dropped onto the panel. |
| `enableDevTools` | `boolean` | Whether the developer tools are enabled. |
<!-- api-schema:end -->

## playback events

### playback:trackChanged

<!-- api-schema:begin event:playback:trackChanged -->
A track started playing: playback began, or moved on to another track. The payload is that track, the same row `playback.getCurrentTrack` answers with. A stream that announces a new title, as internet radio does, sends `playback:dynamicInfoTrack` instead.

Sent to every window.

**Payload**

The payload is a [Track](../reference/types.md#track).
<!-- api-schema:end -->

### playback:edited

<!-- api-schema:begin event:playback:edited -->
The information of the playing track changed, for example its tags were edited. The payload is the track as it reads now.

Sent to every window.

**Payload**

The payload is a [Track](../reference/types.md#track).
<!-- api-schema:end -->

### playback:itemPlayed

<!-- api-schema:begin event:playback:itemPlayed -->
foobar2000 counted the playing track as played: 60 seconds of it have been played, or it reached its end after at least a third of it was played. This is when foobar2000's playback statistics record a play.

Sent to every window.

**Payload**

The payload is a [Track](../reference/types.md#track).
<!-- api-schema:end -->

### playback:stopAfterCurrentChanged

<!-- api-schema:begin event:playback:stopAfterCurrentChanged -->
foobar2000's Stop after current option changed, through `playback.setStopAfterCurrent`, foobar2000's menu or a component.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `enabled` | `boolean` | The new value of the option. |
<!-- api-schema:end -->

### playback:followCursorChanged

<!-- api-schema:begin event:playback:followCursorChanged -->
foobar2000's Playback follows cursor option changed.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `enabled` | `boolean` | The new value of the option. |
<!-- api-schema:end -->

### playback:cursorFollowChanged

<!-- api-schema:begin event:playback:cursorFollowChanged -->
foobar2000's Cursor follows playback option changed.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `enabled` | `boolean` | The new value of the option. |
<!-- api-schema:end -->

### playback:orderChanged

<!-- api-schema:begin event:playback:orderChanged -->
foobar2000's playback order changed.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `orderIndex` | `integer` | Index of the new order, as `order` in the `playback.getPlaybackOrder` answer. |
| `order` | `integer` | The same as `orderIndex`. |
<!-- api-schema:end -->

### playback:stopped

<!-- api-schema:begin event:playback:stopped -->
Playback stopped: by the user, at the end of the playlist, because another track is starting, or because foobar2000 is shutting down.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `reason` | `"user" \| "eof" \| "starting_another" \| "shutting_down" \| "unknown"` | Why playback stopped: `user`, `eof` (end of the playlist), `starting_another`, `shutting_down`, or `unknown`, as foobar2000 reports it. `playback.next` while playing is `starting_another`; `playlist.playTrack` on another row while playing is `user`. |
<!-- api-schema:end -->

### playback:paused

<!-- api-schema:begin event:playback:paused -->
Playback was paused or resumed.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `paused` | `boolean` | `true` when paused, `false` when resumed. |
<!-- api-schema:end -->

### playback:seeked

<!-- api-schema:begin event:playback:seeked -->
The playing track was seeked.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `position` | `number` | The position seeked to, in seconds. |
| `hostTime` | `number` | Host system time when foobar2000 reported the seek, Unix epoch milliseconds with a fractional part, the same clock as `Date.now()`. `position` is the seek target, not a position read at that time; the `playback:timeHighRes` sent right after can still carry the old position. |
<!-- api-schema:end -->

### playback:volumeChanged

<!-- api-schema:begin event:playback:volumeChanged -->
foobar2000's volume changed, or it was muted or unmuted.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `volume` | `number` | Volume as a linear percentage, `0` to `100`, as `playback.getVolume` reports it. |
| `volumeDb` | `number` | Volume in dB, `0` at full volume and `-100` at the bottom. |
| `muted` | `boolean` | Whether foobar2000 is muted. |
| `isMuted` | `boolean` | The same as `muted`. |
<!-- api-schema:end -->

### playback:time

<!-- api-schema:begin event:playback:time -->
The playback position, about once a second while playing, as foobar2000 reports it for time displays. Not sent while every window is hidden. `playback:timeHighRes` is finer.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `position` | `number` | Coarse playback position in seconds, supplied by the host for time displays, without a sample timestamp. Use `playback:timeHighRes` or `playback.getPosition` for synchronization. |
<!-- api-schema:end -->

### playback:timeHighRes

<!-- api-schema:begin event:playback:timeHighRes -->
The playback position, about every 100 ms while playing and not paused, and once more right after a track starts, a seek or a resume. Not sent while every window is hidden.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `position` | `number` | Position in seconds, with its fractional part, using the same output-buffer and DSP latency compensation and stepwise reading as `playback.getPosition`. |
| `hostTime` | `number` | Host system time when `position` was read, Unix epoch milliseconds with a fractional part, the same clock as `Date.now()`. `Date.now() - hostTime` is how late the event arrived, and `position + (Date.now() - hostTime) / 1000` estimates the current position. |
<!-- api-schema:end -->

### playback:dynamicInfo

<!-- api-schema:begin event:playback:dynamicInfo -->
The decoder reported new dynamic info for the playing stream, such as the bitrate of a VBR file or the title of an internet radio stream.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `bitrate` | `integer` | Current bitrate in kbit/s; `0` when unknown. |
| `streamTitle` | `string` | The first `TITLE` value of the dynamic info, when it has one. |
<!-- api-schema:end -->

### playback:dynamicInfoTrack

<!-- api-schema:begin event:playback:dynamicInfoTrack -->
The playing stream announced a new track, as internet radio does. Not sent when it carries neither an artist nor a title.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `artist` | `string` | Artists of the new track, joined with `, `; absent when there is none. |
| `title` | `string` | The first title of the new track; absent when there is none. |
<!-- api-schema:end -->

### playback:starting

<!-- api-schema:begin event:playback:starting -->
Playback is starting, after a play, next, previous or random command.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `command` | `"play" \| "next" \| "previous" \| "random" \| "unknown"` | The command that started playback. |
| `paused` | `boolean` | Whether playback starts paused. |
<!-- api-schema:end -->

### playback:stateChanged

<!-- api-schema:begin event:playback:stateChanged -->
The playback state changed: `playing` when a track starts and on resume, `paused` on pause or when playback starts paused, `stopped` when playback stops with reason `user` or `eof` (see `playback:stopped`). `playback.next` stops the playing track with reason `starting_another` and sends no `stopped` state, but foobar2000 reports `playlist.playTrack` on another row as a `user` stop, so a `stopped` state arrives before the new track's events. When a track starts, the event sent right after `playback:trackChanged` carries that track's `canSeek`; an earlier one sent with `playback:starting`, before the track is opened, can report `false`.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `state` | `"playing" \| "paused" \| "stopped"` | The new state. |
| `position` | `number` | Playback position in seconds, read when the event is sent, with the same latency compensation and transient behavior as `playback.getPosition`. |
| `duration` | `number` | Length of the playing track in seconds, read when the event is sent. |
| `canSeek` | `boolean` | Whether the playing track can be seeked, read when the event is sent, as `canSeek` in the `playback.getState` answer; always `false` for `stopped`. |
| `hostTime` | `number` | Host system time when `position` was read, Unix epoch milliseconds with a fractional part, the same clock as `Date.now()`. |
<!-- api-schema:end -->

### playback:queueChanged

<!-- api-schema:begin event:playback:queueChanged -->
foobar2000's playback queue changed. A `queue.*` call that changes the queue several times sends one event at the end.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `origin` | `"user_added" \| "user_removed" \| "playback_advance" \| "unknown"` | What changed the queue: `user_added`, `user_removed`, `playback_advance` (playback took the next queued track), or `unknown`. |
| `count` | `integer` | Queue length after the change. |
<!-- api-schema:end -->

## playlist events

### playlist:defaultFormatChanged

<!-- api-schema:begin event:playlist:defaultFormatChanged -->
foobar2000 reported that its default playlist format changed.

Sent to every window.

**Payload**

The event carries no fields.
<!-- api-schema:end -->

### playlist:itemsAdded

<!-- api-schema:begin event:playlist:itemsAdded -->
Tracks were inserted into a playlist. The JIT queue's hidden playlist sends nothing.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `playlist` | `integer` | Index of the playlist. |
| `playlistGuid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `start` | `integer` | Row of the first inserted track. |
| `count` | `integer` | Number of tracks inserted. |
<!-- api-schema:end -->

### playlist:itemsRemoved

<!-- api-schema:begin event:playlist:itemsRemoved -->
Tracks were removed from a playlist. The JIT queue's hidden playlist sends nothing.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `playlist` | `integer` | Index of the playlist. |
| `playlistGuid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `oldCount` | `integer` | Track count before the removal. |
| `newCount` | `integer` | Track count after the removal. |
<!-- api-schema:end -->

### playlist:itemsReordered

<!-- api-schema:begin event:playlist:itemsReordered -->
A playlist's tracks were reordered. The JIT queue's hidden playlist sends nothing.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `playlist` | `integer` | Index of the playlist. |
| `playlistGuid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `count` | `integer` | Track count of the playlist. |
<!-- api-schema:end -->

### playlist:selectionChanged

<!-- api-schema:begin event:playlist:selectionChanged -->
The selection in a playlist changed; read it with `playlist.getSelection`. The JIT queue's hidden playlist sends nothing.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `playlist` | `integer` | Index of the playlist. |
| `playlistGuid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
<!-- api-schema:end -->

### playlist:focusChanged

<!-- api-schema:begin event:playlist:focusChanged -->
The focused row of a playlist moved. The JIT queue's hidden playlist sends nothing.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `playlist` | `integer` | Index of the playlist. |
| `playlistGuid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `from` | `integer` | Row focused before; `-1` when none was. |
| `to` | `integer` | Row focused now; `-1` when none is. |
<!-- api-schema:end -->

### playlist:itemsReplaced

<!-- api-schema:begin event:playlist:itemsReplaced -->
Tracks of a playlist were replaced in place by other tracks. The JIT queue's hidden playlist sends nothing.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `playlist` | `integer` | Index of the playlist. |
| `playlistGuid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `count` | `integer` | Number of tracks replaced. |
<!-- api-schema:end -->

### playlist:created

<!-- api-schema:begin event:playlist:created -->
A playlist was created.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `index` | `integer` | Index of the new playlist. |
| `guid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `name` | `string` | Its name. |
<!-- api-schema:end -->

### playlist:removed

<!-- api-schema:begin event:playlist:removed -->
One or more playlists were removed.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `oldCount` | `integer` | Playlist count before the removal. |
| `newCount` | `integer` | Playlist count after the removal. |
| `indices` | `integer[]` | Indices the removed playlists had before the removal, ascending. |
| `guids` | `string[]` | GUIDs of the removed playlists, in the order of `indices`; empty when foobar2000 reported the removal without announcing it first, so that the GUIDs could no longer be read. |
<!-- api-schema:end -->

### playlist:reordered

<!-- api-schema:begin event:playlist:reordered -->
The playlists were reordered.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `count` | `integer` | Number of playlists. |
| `guids` | `string[]` | GUID of every playlist, in the new order. |
<!-- api-schema:end -->

### playlist:activated

<!-- api-schema:begin event:playlist:activated -->
The active playlist changed.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `oldIndex` | `integer` | Index of the playlist active before; `-1` when none was. |
| `newIndex` | `integer` | Index of the playlist active now; `-1` when none is. |
| `newGuid` | `string \| null` | GUID of the playlist active now; `null` when none is. No GUID is given for the playlist active before: it may be the one just removed. |
<!-- api-schema:end -->

### playlist:renamed

<!-- api-schema:begin event:playlist:renamed -->
A playlist was renamed.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `index` | `integer` | Index of the playlist. |
| `guid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `name` | `string` | The new name. |
<!-- api-schema:end -->

### playlist:lockChanged

<!-- api-schema:begin event:playlist:lockChanged -->
A lock was put on a playlist or taken off it.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `playlist` | `integer` | Index of the playlist. |
| `playlistGuid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `locked` | `boolean` | Whether a lock is on it now. |
<!-- api-schema:end -->

### playlist:addComplete

<!-- api-schema:begin event:playlist:addComplete -->
A `playlist.addPathsAsync` call finished adding its paths. Paths that need expanding (such as `.cue` or `.m3u`) are expanded first; when every path can be added directly, this follows the call at once. A call that was answered with an error sends nothing.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `operationId` | `string` | Id from the call's answer. |
| `playlistGuid` | `string` | GUID of the playlist the call added to, as the call resolved it; still that playlist when it was moved meanwhile, and no longer anyone's when it was removed. |
| `success` | `boolean` | Always `true`. |
| `addedCount` | `integer` | Tracks added, after expanding playlists and cue sheets. Tracks dropped because the playlist was removed or locked before the expansion finished are not counted. |
| `totalCount` | `integer` | Paths the call accepted. |
<!-- api-schema:end -->

## plugin events

### plugin:registered

<!-- api-schema:begin event:plugin:registered -->
An external plugin registered its namespace through the C++ `PluginRegistry`. Registering the same namespace again only updates its details and sends nothing. The plugin has no methods yet at this point, so `apis` is empty and `apiCount` is `0`; each method follows as `api:registered`.

Sent to every window.

**Payload**

The payload is a [SystemPluginInfo](../reference/types.md#systemplugininfo).
<!-- api-schema:end -->

### plugin:unregistered

<!-- api-schema:begin event:plugin:unregistered -->
An external plugin was unregistered. `apis` lists the methods removed with it; they are not announced one by one with `api:unregistered`.

Sent to every window.

**Payload**

The payload is a [SystemPluginInfo](../reference/types.md#systemplugininfo).
<!-- api-schema:end -->

## port events

### port:connected

<!-- api-schema:begin event:port:connected -->
`port.connect` opened a port. Every window receives it, the opener's included.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `portId` | `string` | Id of the new port, as `port.connect` returned it. |
| `name` | `string` | The channel name. |
| `windowId` | `string` | Id of the window that opened the port; `main` when the caller cannot be matched to a window. |
<!-- api-schema:end -->

### port:disconnected

<!-- api-schema:begin event:port:disconnected -->
A port closed: `port.disconnect` closed it, or the page that opened it started a top-level navigation or went away with its WebView (popup closed, panel removed, WebView rebuilt), which closes all of that page's ports. Every window receives it; a page that closes by navigating receives its own ports' events only if they arrive before it unloads.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `portId` | `string` | Id of the port that closed. |
| `name` | `string` | The channel name the port was on. |
| `windowId` | `string` | Id of the window that opened the port. |
<!-- api-schema:end -->

### port:message

<!-- api-schema:begin event:port:message -->
A message reached a port. Each receiving port gets its own event, sent to the window that opened it: `port.postMessage` reaches every other port on the sender's channel, the sender's own window included, and `port.postMessageTo` reaches one port.

Sent to the page that owns the subscription or task.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `portId` | `string` | Id of the receiving port, one this window opened. |
| `sourcePortId` | `string` | Id of the sending port. |
| `sourceWindowId` | `string` | Id of the sending page's window; `main` when the sender cannot be matched to a window. |
| `message` | `any` | The message as the sender passed it; never `null`. |
<!-- api-schema:end -->

## selection events

### selection:changed

<!-- api-schema:begin event:selection:changed -->
foobar2000's global selection changed. A change less than 50 ms after the previous event is dropped, not delayed, so the last of several quick changes may go unannounced; read `selection.get` when the current value matters.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `count` | `integer` | Number of selected tracks. |
| `type` | `"now_playing" \| "active_playlist_selection" \| "active_playlist" \| "playlist_manager" \| "media_library_viewer" \| "unknown"` | Where the selection comes from, as `typeName` in the `selection.getType` answer. |
| `handles` | `string[]` | Handles of the first 100 selected tracks, in selection order: native paths with `\|subsong:N` appended when the subsong is not `0`. |
| `truncated` | `boolean` | Whether more than 100 tracks are selected, so that `handles` lists only the first 100. |
| `track` | [Track](../reference/types.md#track) | The selected track; present only when exactly one track is selected. |
| `nowPlaying` | [Track](../reference/types.md#track) | The track loaded for playback, playing or paused; absent when none is. |
<!-- api-schema:end -->

## state events

### state:changed

<!-- api-schema:begin event:state:changed -->
`state.set` stored a value without `silent`. When the key held a value that had expired but was not swept yet, `previousValue` is that old value, and its expiry is never announced.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `key` | `string` | The key. |
| `value` | `any` | The stored value; never `null`, which `state.set` treats as missing. |
| `previousValue` | `any` | The value the key held before; `null` when the key is new. |
| `sourceWindowId` | `string` | Id of the window that stored it; `main` when the caller cannot be matched to a window. |
| `expiresAt` | `integer` | When the value expires, in milliseconds since the Unix epoch; present only for a value stored with a positive `ttlMs`. |
<!-- api-schema:end -->

### state:deleted

<!-- api-schema:begin event:state:deleted -->
A value left the shared state: `state.delete` removed it, or its lifetime ran out. Expired values are swept only when `state.get` or `state.keys` runs, so the event for an expiry comes then rather than at the expiry time; deleting a value that expired but was not swept yet reports `deleted`.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `key` | `string` | The key. |
| `sourceWindowId` | `string` | Id of the window that deleted it, `main` when the caller cannot be matched to a window; `""` for an expiry. |
| `reason` | `"deleted" \| "expired"` | `deleted` for `state.delete`, `expired` when its lifetime ran out. |
<!-- api-schema:end -->

## system events

### system:themeChanged

<!-- api-schema:begin event:system:themeChanged -->
The colours of foobar2000's Default UI changed, dark mode included. Only pages in this plugin's Default UI panels receive it, together with `ui:coloursChanged`; Columns UI panels and the plugin's own windows do not.

Sent to the page of the window or panel it concerns.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `darkMode` | `boolean` | Whether Default UI now uses its dark mode. It follows foobar2000's own setting, which can differ from the Windows app theme that `system.getTheme` reports. |
<!-- api-schema:end -->

## taskbar events

### taskbar:buttonClicked

<!-- api-schema:begin event:taskbar:buttonClicked -->
A thumbnail toolbar button installed with `taskbar.setThumbnailButtons` was clicked. The default playback buttons shown before a page installs its own run natively and send nothing.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `id` | `string` | The button's `id`. |
<!-- api-schema:end -->

## tray events

### tray:beforeContextMenu

<!-- api-schema:begin event:tray:beforeContextMenu -->
The tray context menu is about to open on a right click. The menu does not wait for the page, so changes a handler makes to it take effect from the next right click. It is sent even when the menu has no visible row and nothing opens.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `x` | `integer` | Screen x of the cursor, where the menu opens. |
| `y` | `integer` | Screen y of the cursor, where the menu opens. |
<!-- api-schema:end -->

### tray:click

<!-- api-schema:begin event:tray:click -->
The tray icon was clicked with the left button, on release. A right click opens the context menu instead and sends `tray:beforeContextMenu`.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `button` | `integer` | Always `0`, the left button. |
| `x` | `integer` | Cursor x in screen coordinates. |
| `y` | `integer` | Cursor y in screen coordinates. |
<!-- api-schema:end -->

### tray:doubleClick

<!-- api-schema:begin event:tray:doubleClick -->
The tray icon was double-clicked with the left button.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `x` | `integer` | Cursor x in screen coordinates. |
| `y` | `integer` | Cursor y in screen coordinates. |
<!-- api-schema:end -->

### tray:menuItemClicked

<!-- api-schema:begin event:tray:menuItemClicked -->
A row of the tray menu was chosen, or a rich control in it changed value. The built-in playback and system rows, and rows that declare `playbackAction`, run natively and send nothing. With `render: 'webview'` a rich control reports every change and the menu stays open; the default native menu closes on each pick, offers a slider as five stops and shows a segmented row as an ordinary row, which reports no `value`.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `id` | `string` | The row's `id`; `""` for a row without one. |
| `value` | `integer` | The new value of a rich control: stars `0` to `5` for `rating` (`0` clears), a value within `min` to `max` for `slider`, the index of the chosen segment for `segmented`. Absent for other rows. |
<!-- api-schema:end -->

## ui events

### ui:coloursChanged

<!-- api-schema:begin event:ui:coloursChanged -->
The colours of foobar2000's Default UI changed. Only pages in this plugin's Default UI panels receive it, together with `system:themeChanged`; Columns UI panels and the plugin's own windows do not.

Sent to the page of the window or panel it concerns.

**Payload**

The event carries no fields.
<!-- api-schema:end -->

### ui:fontChanged

<!-- api-schema:begin event:ui:fontChanged -->
The fonts of foobar2000's Default UI changed. Only pages in this plugin's Default UI panels receive it; Columns UI panels and the plugin's own windows do not.

Sent to the page of the window or panel it concerns.

**Payload**

The event carries no fields.
<!-- api-schema:end -->

### ui:menuItemClicked

<!-- api-schema:begin event:ui:menuItemClicked -->
A row of a menu opened with `ui.showCustomMenu` was chosen. The page that opened the menu gets this in addition to the method's `selectedId`; dismissing the menu sends nothing.

Sent to the page that made the call.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `id` | `string` | The row's `id`; `""` for a row without one. |
| `label` | `string` | The row's `label` as given, without the shortcut hint; `""` for a row without one. |
<!-- api-schema:end -->

### ui:toast

<!-- api-schema:begin event:ui:toast -->
`ui.showToast` asks the calling page to show a toast. The host draws nothing; the page renders it from these fields.

Sent to the page that made the call.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `message` | `string` | Toast text; never empty. |
| `duration` | `integer` | Display time in milliseconds as given, `3000` when omitted. It is not range-checked and can be `0` or negative. |
| `type` | `"info" \| "success" \| "warning" \| "error"` | Toast kind; `info` when omitted. |
| `position` | `string` | Screen corner as given and not checked; `bottom-right` when omitted. |
<!-- api-schema:end -->

## webview events

### webview:processFailed

<!-- api-schema:begin event:webview:processFailed -->
A WebView2 process of this plugin failed. It is sent to every window; when the failure could not be recovered (`recovered` is `false`), the window whose WebView failed is left out.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `kind` | `"browserProcessExited" \| "renderProcessExited" \| "renderProcessUnresponsive" \| "frameRenderProcessExited" \| "utilityProcessExited" \| "sandboxHelperProcessExited" \| "gpuProcessExited" \| "ppapiPluginProcessExited" \| "ppapiBrokerProcessExited" \| "unknownProcessExited"` | Which process failed; a kind this plugin does not know is reported as `unknownProcessExited`. |
| `kindRaw` | `integer` | The `COREWEBVIEW2_PROCESS_FAILED_KIND` value WebView2 reported. |
| `recovered` | `boolean` | Whether the WebView is usable again: `true` after a render process was reloaded and for processes the runtime restarts by itself, `false` when the reload failed or the browser process exited. |
| `recoveryAction` | `"reload" \| "needRebuild" \| "none"` | What the plugin did: `reload` for a render process (see `recovered` for the outcome), `needRebuild` when the browser process exited and the WebView has to be created anew, `none` for processes the runtime restarts by itself. |
<!-- api-schema:end -->

## window events

### window:activated

<!-- api-schema:begin event:window:activated -->
The main window was activated or deactivated. Only the main window's page gets it; popups and panels do not.

Sent to the page of the window or panel it concerns.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `active` | `boolean` | Whether the main window is now active. |
<!-- api-schema:end -->

### window:dpiChanged

<!-- api-schema:begin event:window:dpiChanged -->
The main window's DPI changed, because it moved to another display or the display's scaling changed. Only the main window's page gets it. Sizes are physical pixels at the new DPI; the host also updates the page's titlebar CSS variables.

Sent to the page of the window or panel it concerns.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `dpi` | `integer` | The new DPI; 96 is 100 % scaling. |
| `dpiScale` | `number` | `dpi / 96`. |
| `titlebarHeight` | `integer` | Height of the titlebar. |
| `captionButtonWidth` | `integer` | Width of one caption button. |
| `captionButtonsWidth` | `integer` | Width of the three caption buttons together, `captionButtonWidth * 3`. |
<!-- api-schema:end -->

### window:stateChanged

<!-- api-schema:begin event:window:stateChanged -->
The main window was maximized, minimized, restored, activated or deactivated, or entered or left fullscreen; a popup sends it only when it enters or leaves fullscreen. It goes to every window, and `windowId` says which window changed: a page that only cares about its own window compares it with `window.getCurrentWindowId`. The main window sends it only when one of the four states differs from what it last sent.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `windowId` | `string` | Id of the window that changed: `main` for the main window, else the popup's id. The same value `window.getCurrentWindowId` gives that window's page. |
| `isMaximized` | `boolean` | Whether the window is maximized. |
| `isMinimized` | `boolean` | Whether the window is minimized. |
| `maximized` | `boolean` | Same as `isMaximized`. |
| `minimized` | `boolean` | Same as `isMinimized`. |
| `isActive` | `boolean` | Whether the window is active. |
| `active` | `boolean` | Same as `isActive`. |
| `isFullscreen` | `boolean` | Whether the window is fullscreen. |
| `fullscreen` | `boolean` | Same as `isFullscreen`. |
<!-- api-schema:end -->

### window:backdropStateChanged

<!-- api-schema:begin event:window:backdropStateChanged -->
The backdrop of the main window or a popup changed: it was applied with another effect, switched between its active and inactive variant, or was applied again by a forced refresh. Sent only after the backdrop was applied successfully.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `windowId` | `string` | Id of the window; `main` for the main window. |
| `active` | `boolean` | Whether the window is active. |
| `mode` | `"active" \| "inactive"` | Which variant was applied, matching `active`. |
| `effect` | `"none" \| "system" \| "mica" \| "mica-alt" \| "acrylic"` | The effect in use, after `inherit` is resolved and an effect the window cannot show falls back; `system` when the platform draws the frame. |
<!-- api-schema:end -->

### window:beforeClose

<!-- api-schema:begin event:window:beforeClose -->
A popup created with `beforeClose` is being closed, by its close button or `window.closePopup`. It stays open until its page calls `window.confirmClose` or `window.cancelClose`; with neither within 3 seconds, it closes anyway.

Sent to the page of the window or panel it concerns.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `windowId` | `string` | Id of the popup. |
<!-- api-schema:end -->

### window:behaviorChanged

<!-- api-schema:begin event:window:behaviorChanged -->
`window.setPopupBehavior` changed a popup's preset or overrides. The payload has the same fields as `window.getPopupBehavior` returns.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `windowId` | `string` | Id of the popup. |
| `profile` | `"legacy" \| "standard" \| "miniPlayer" \| "desktopLyrics"` | The behavior preset; `legacy` when the popup was created without one. |
| `behavior` | `Record<string, any>` | The behavior overrides set on the popup. |
| `resolvedBehavior` | `WindowPopupBehaviorState` | The behavior in effect. |
| `resolvedBehavior.showInTaskbar` | `boolean` | Whether the popup shows on the taskbar. |
| `resolvedBehavior.showInAltTab` | `boolean` | Whether the popup shows in Alt+Tab. |
| `resolvedBehavior.keepVisibleOnShowDesktop` | `boolean` | Whether the popup stays visible when the desktop is shown. |
| `resolvedBehavior.allowMinimize` | `boolean` | Whether the popup can be minimized. |
| `resolvedBehavior.owner` | `"none" \| "main"` | `main` keeps the popup above the main window and minimizes it with the main window; `none` makes it independent. |
| `resolvedBehavior.noActivate` | `boolean` | Whether showing or clicking the popup leaves the focus where it was. |
<!-- api-schema:end -->

### window:hoverStateChanged

<!-- api-schema:begin event:window:hoverStateChanged -->
The cursor entered or left a popup that lets clicks through (`window.setClickThrough`). Such a window gets no mouse messages, so the host polls the cursor while click-through is on; turning click-through off sends `hovering: false`.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `windowId` | `string` | Id of the popup. |
| `hovering` | `boolean` | Whether the cursor is over the popup. |
<!-- api-schema:end -->

### window:message

<!-- api-schema:begin event:window:message -->
A page sent a message with `window.sendMessage` or `window.broadcast`. A directed message goes only to the named window; a broadcast goes to every window except the sender's, or to every window when the sender's window cannot be found.

Sent to the window the caller named.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `sourceWindowId` | `string` | Id of the sending page's window: `main`, a popup id or a panel id; `main` when the sender cannot be matched to a window. |
| `message` | `any` | The message as the sender passed it. |
<!-- api-schema:end -->

### window:minimizeSuppressed

<!-- api-schema:begin event:window:minimizeSuppressed -->
A popup whose behavior has `keepVisibleOnShowDesktop` ignored a minimize command.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `windowId` | `string` | Id of the popup. |
| `reason` | `string` | Why the minimize was ignored; currently always `policy.keepVisibleOnShowDesktop`. |
<!-- api-schema:end -->

### window:popupOpened

<!-- api-schema:begin event:window:popupOpened -->
A popup was created. The windows the host uses for its own menus do not send it.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `windowId` | `string` | Id of the popup. |
| `title` | `string` | Title the popup was created with. |
| `url` | `string` | URL the popup was created with. |
<!-- api-schema:end -->

### window:popupClosed

<!-- api-schema:begin event:window:popupClosed -->
A popup closed and its page was destroyed. The windows the host uses for its own menus do not send it.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `windowId` | `string` | Id of the popup. |
<!-- api-schema:end -->

### window:alwaysOnTopChanged

<!-- api-schema:begin event:window:alwaysOnTopChanged -->
foobar2000's own Always on top option changed, from its menu or from a component. It is not about the always-on-top state of this plugin's popups.

Sent to every window.

**Payload**

| Field | Type | Description |
| --- | --- | --- |
| `enabled` | `boolean` | The new value of the option. |
<!-- api-schema:end -->

## Notes

- `playback:stopAfterCurrentChanged` uses `{ enabled }`, the same field as its API.
- Playlist lifecycle payload fields may gain entries in future versions; treat unknown keys as additive.
- The exact emit source and field set for every row follow the C++ `EmitEvent` / `BroadcastEvent` call sites in the component source.

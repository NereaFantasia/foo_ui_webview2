# Changelog

## v1.13.0 (2026-08-26)

::: warning Breaking changes in this release
Four changes may require code edits:

- **`library.search` no longer duplicates its row array as `items`.** The response used to carry the same rows twice, byte-identical, under `items` and `tracks`. It now sends only `tracks` — the key `library.query` has always used. Code reading `result.items` should read `result.tracks` instead; every response shape — on older hosts too — carries `tracks`, so a `tracks`-first reader keeps working against hosts that still send both. This halves the payload of every row-carrying search response.
- **`dialog.confirm` resolves with `{ response }`, not `{ confirmed }`.** The declared type was wrong. The host has always returned the zero-based index of the clicked button in `buttons` and never a `confirmed` flag, so TypeScript code reading `result.confirmed` was reading a field that was never populated. With the default button set `['OK', 'Cancel']`, `0` means confirmed and `1` means cancelled. The task dialog is created without `TDF_ALLOW_DIALOG_CANCELLATION`, so Escape and the close button do not dismiss it and every result comes from an actual button click; `-1` appears only on the host's fallback path, when even a plain message box could not be shown.
- **`file.*` error messages changed, and a rejected path is no longer echoed back.** Every hand-rolled error envelope in the namespace now goes through the standard error envelope, and a `std::filesystem` exception surfaces only its Win32 error number — the exception text and the offending path no longer reach the payload or the host log. A path-security refusal now reads `file.read: path security denied for 'path': Access denied: protected system path`: it names the method, the parameter (with its index for array parameters, as in `items[2].destination`) and the policy reason, so a caller can tell which argument was refused without the host leaking a filesystem location into a payload a page may forward elsewhere. Code that parsed `result.error` for a path or for specific wording has to switch to `result.code` plus `result.details`.
- **A parameter of the wrong shape now returns `INVALID_PARAMS`, not `PERMISSION_DENIED`.** The path-security decorator previously reported a refused path and a malformed argument under the same code. A handler that branched on `PERMISSION_DENIED` to catch type errors will stop seeing them there.

The release stays on a minor version, consistent with this project's version axis (see 1.6.0 and 1.12.0). Pin an exact version if you need to upgrade deliberately.
:::

### Asynchronous file operations

- **New: `file.copyAsync`, `file.moveAsync`, `file.deleteAsync` and `file.cancelOp`.** The work runs on a host worker thread, so copying a large album no longer freezes the UI the way the synchronous `file.copy` does. Each call returns a `{ operationId, totalCount }` receipt immediately; the outcome arrives on `file:opProgress` and is followed by one `file:opComplete`.
- New events `file:opProgress` and `file:opComplete` (payload: `operationId`, `op`, `done`, `total`, `results` / the three counts and `cancelled`). Results are batched, never one event per entry: they accumulate until 64 are pending or 100 ms have passed since the previous batch, so a fast run collapses to roughly `ceil(total / 64)` events while a slow one keeps a progress signal moving. `done / total` is a usable progress fraction, and the final partial batch always arrives before `file:opComplete`.
- One result is reported per requested entry, never per file: a directory entry is reported once its whole tree has been walked. Each result echoes `source` (and `destination`, except for `deleteAsync`) exactly as requested, `%variable%` placeholders included and unexpanded, so it works as a lookup key. `status` is `'ok'` / `'skipped'` / `'failed'`, with `reason` drawn from `already-exists`, `not-found`, `permission`, `cross-volume`, `io-error` and `cancelled`.
- **Path validation is all-or-nothing.** If any entry fails the host's read or write check the whole call is rejected with `PERMISSION_DENIED` and no `operationId` is produced — a partial batch is never dispatched. `copyAsync` checks `source` as `Read` and `destination` as `FileWrite`; `moveAsync` checks both ends as `FileWrite` because a move deletes the source; `deleteAsync` checks every path as `FileWrite`. At most 8 operations may be in flight process-wide.
- Copy and move differ on an existing destination: `copyAsync` merges a directory into an existing directory (files already there are skipped without being reported individually, and the entry still reports `status: 'ok'`), while `moveAsync` reports it as `skipped` / `already-exists`. `overwrite` (default `false`) covers file destinations only — Windows cannot swap a directory in place, so an existing directory destination is never replaced. Note that the synchronous `file.move` always replaces a file destination; the async form does not unless asked.
- Within one volume a move is a rename and costs nothing regardless of size. Across volumes the host falls back to copy-then-delete-source; the entry still reports `status: 'ok'` but carries `reason: 'cross-volume'` so the extra cost is visible.
- `deleteAsync` defaults to `moveToTrash: true`, which hands each path to the shell and therefore needs the main thread — those deletes run there in batches of 16 and yield in between. `moveToTrash: false` deletes on a worker thread and removes non-empty directories, which the synchronous `file.delete` refuses to do in that mode.
- `file.cancelOp` takes effect part-way through a batch rather than at the end of it: a copy or move stops within one file, aborting the file in flight and removing its partial copy, and a delete stops at the next entry. Entries already done keep their results, every remaining entry is reported as `skipped` / `cancelled`, and the run still ends with a `file:opComplete` carrying `cancelled: true`. Closing a popup cancels the operations that popup started; a panel host has no such hook, so its operations run to the end unless cancelled explicitly. `cancelled: false` comes back when the operation had already finished or never existed — the two cases are deliberately indistinguishable.
- Both events go to the window that made the call while that window is alive, which is what makes it safe for `results` to carry real filesystem paths. Once the window is gone the host can no longer resolve it and falls back to the main instance, so a late event may surface in a window that did not start the operation. Two paths skip `file:opComplete` — the host shutting down mid-run, and an unexpected host-side failure — so a listener that must not leak state should carry its own timeout rather than wait on it forever.

### Metadata probing

- **New: `metadata.probeBatchAsync` and `metadata.cancelProbe`.** Reads happen on a host worker thread, so a few hundred paths no longer stall the UI the way `metadata.readBatch` does. The call returns a `{ operationId, totalCount }` receipt; results arrive on `metadata:probeProgress` and are followed by exactly one `metadata:probeComplete`.
- Each result reports where its info came from — `infoSource: 'cached' | 'direct'` — which is what makes the endpoint useful for files the media library has never seen. On failure it reports which of `'not-found'` / `'unsupported-format'` / `'read-error'` applies, a distinction `readBatch` collapses into one generic error string. `includeTags` (default `true`) attaches the flat tag map, with upstream keys upper-cased as in `readBatch`; pass `false` when only technical info is wanted.
- Paths may carry a `|subsong:N` suffix, are resolved independently and are echoed back verbatim so they work as lookup keys. Unlike `metadata.read`, the batch surface does not honour the legacy `#N` subsong spelling, which would mis-split an extensionless filename that happens to end in `#<digits>`.
- Path validation is all-or-nothing: if any path fails the host's media read check the whole call is rejected with `PERMISSION_DENIED` and no `operationId` is produced. Per-path rejection is not available.
- `metadata:probeProgress` batches on the same 64-or-100 ms rule as the file events. `metadata:probeComplete` always arrives, on the cancelled and failed paths too. Cancellation interrupts the in-progress disk read rather than waiting for it; paths not yet reached are never reported and the interrupted path is reported as neither success nor failure, so `successCount + failureCount` falls short of `total` on a cancelled run.

### Drag and drop

- **A dropped shortcut now tells you what it points at.** Windows puts the `.lnk` file itself in a dropped file list, which foobar2000 cannot play, so every path source is now joined by a parallel array of shortcut targets: `resolvedPaths` on the `dnd:enter` / `dnd:drop` payloads and on `dnd.getPathsAsync()`, plus the new `dnd.getResolvedPaths()` for the synchronous snapshot. The two arrays are always the same length, so `paths.map((p, i) => targets[i] ?? p)` gives a playable list.
- An entry is `null` whenever no target is available: the path is not a shortcut, the shortcut names a shell namespace object such as the recycle bin rather than a file, the recorded target is too long to come back intact (Windows caps it at `MAX_PATH`, and a truncated path would name a different file), COM was unavailable, or resolution was skipped to keep the drop responsive. Never an empty string, so a truthiness test is enough.
- A target says where the shortcut points, not that the file is there. A broken shortcut reports the path its `.lnk` recorded rather than `null`, because Windows hands that path back whether or not the target still exists and the host cannot afford a filesystem check on the thread the drag blocks. Expect a non-null entry to occasionally name nothing. Only `.lnk` is resolved — `.url`, `.library-ms` and virtual search results report `null`.
- Reading `resolvedPaths` costs no filesystem access: the targets were resolved once when the drag arrived, so `getPathsAsync()` reads host memory only.
- **Documented limitation** — the page-side snapshot is published to the top-level document only, so `dnd.getPaths()` and `dnd.getResolvedPaths()` answer with an empty array inside an `<iframe>` and the slot stays `null` for the life of that document. A framed page that needs paths has to receive them from the main frame over `postMessage`.
- Dragging tracks **out** of the window is still unsupported and `dnd.startDrag` keeps resolving `{ success: false, code: 'NOT_SUPPORTED' }`. Measurements settled the design question — it needs a dedicated STA thread, because performing the drag on the host's main thread freezes the target application for as long as the gesture lasts — but the work is deliberately not in this release.

### Media library queries

- **Multi-value tags are no longer truncated to their first value.** Since the first release, a track tagged with several artists reported only the first one everywhere outside `metadata.read`. Track objects now join the values with `, ` in tag order, without de-duplication — matching what foobar2000 itself displays — for `artist`, `albumArtist`, `genre` and `composer`, each API keeping its own field set.
- **New: `artists`, the atomic form of `artist`.** A joined string cannot tell one artist named `"A, B"` from two artists `A` and `B`, so track objects from the library namespace — `library.getAll`, `library.query`, `library.search`, `library.getByPath` and the other `library.*` track endpoints — also carry `artists: string[]` with the untouched values; `artists.join(', ') === artist` holds byte for byte. Track objects from other namespaces (`playlist.getTracks`, `playback.getCurrentTrack`, `queue.get`, artwork and event payloads) do not carry the field.
- **Aggregation counts every credited artist.** `library.getArtists` gives each participating artist its own entry, which makes `trackCount` a participation count: the entries add up to more than the number of tracks, and `albumCount` / `totalDuration` are counted per artist the same way. `library.getStats`'s `totalArtists` now matches the entry count of `getArtists`, and `library.getGenres` treats each value of a multi-value `genre` as its own entry.
- **New: field projection on `library.search` and `library.query`.** `library.search` accepts `options.fields` and `library.query` gained a fourth `fields` argument. Every returned row then holds exactly the requested keys and nothing else, so `TrackInfo` is a partial view at runtime while the declared type stays complete. Accepted names are the 20 track keys — `index`, `title`, `artist`, `artists`, `album`, `albumArtist`, `genre`, `date`, `trackNumber`, `discNumber`, `duration`, `path`, `absolutePath`, `fileSize`, `bitrate`, `sampleRate`, `channels`, `codec`, `subsong`, `rating` — matched case-sensitively. Omit the argument to get all 20. A non-array, an empty array, a non-string element or an unknown name resolves (never rejects) with `{ success: false, code: 'INVALID_PARAMS' }` and echoes the offending names under `details.unknownFields`.
- **`library.search` and `library.query` now serialize off the main thread** and write their result straight to the wire instead of building an intermediate object graph. The response pipeline stops deep-copying along the way, and a query with no hits short-circuits. Aside from the `items` removal listed under breaking changes, the JavaScript contract is unchanged: both still resolve the same shape from the same promise.
- **The Spider Monkey Panel compatibility layer stops re-scanning the library once per page.** `fb.GetQueryItems` used to page through hits 500 at a time, and because `library.search` re-scans the whole library on every request, a multi-thousand-hit query paid one full scan per page — 160 scans on a large library. It now issues a one-row probe for the hit count followed by a single request for every hit, narrowed to the five keys `FbMetadbHandle` actually reads. No observable handle property changes. Trade-off: the probed count is stale if the library changes between the two calls, so hits added in between are dropped; the returned set is not guaranteed stable while the library is being modified concurrently.
- **Practical note on very large libraries.** These changes remove copies and payload, not the cost of producing the rows. A full-field whole-library query still occupies the host's main thread for a substantial time on a six-figure library, dominated by the size of the response being handed to the page. Narrow the projection with `fields` when you do not need all 20 keys — it is the single most effective lever available today. On 32-bit hosts also keep the peak in mind: a full-field whole-library result is held in several forms at once while it is parsed, so a six-figure library can reach hundreds of megabytes momentarily.

### Title formatting

- **`titleformat.eval`, `evalBatch`, `evalFields` and `evalFieldsBatch` now report `infoAvailable`.** The host knew whether a track's metadb info was ready and was throwing that signal away, so tag-derived output could come back silently wrong. `infoAvailable: false` means tag-derived values are untrustworthy. Batch variants carry the flag per row, and rows that failed omit it.
- Two limits worth knowing: one flag covers the whole merged script in the `evalFields` forms, so it cannot tell you which individual field was affected, and it never covers foo_playcount virtual fields. A `fields` key literally named `infoAvailable` overwrites the flag, matching the existing behaviour of `path` and `success`.

### Errors and permissions

- **`fb2k.invoke` inside a subframe now fails immediately.** It rejects with an `Error` carrying `code: 'NOT_SUPPORTED'` and the message `fb2k.invoke is unavailable in subframes`, instead of hanging silently until the 30-second timeout. A detached call (`const { invoke } = fb2k`) still surfaces its `TypeError` as a rejection rather than a synchronous throw.
- A cross-volume **directory** move now returns `NOT_SUPPORTED` with `details.reason: 'cross-volume'` rather than a generic failure. A cross-volume *file* move already succeeded, because the underlying rename silently degrades to a copy.
- Path allowlist and blacklist entries are now normalized to the same canonical real-path form the check side uses. An entry written in a different but equivalent spelling — a mapped drive, a junction, a short 8.3 name — previously failed to match the path it was meant to cover.
- Dropping a shortcut, probing metadata and the async file operations are all covered by the permission matrix in the reference; the counts there moved from 67 specs over 64 APIs to 73 over 68.

::: danger FileWrite now accepts media-library watch folders
The `FileWrite` chain — the channel behind every `file.*` write — gained a media-library watch-folder step, so a path inside a watch folder is now writable **even on the system drive**, where previously only the non-system-drive step would have allowed it. `FileWrite` was already the widest write channel exposed to a theme; this widens it further. If you audit themes, this is the channel to look at first, and the watch-folder list is now part of its attack surface.
:::

### Window and menus

- **Collapsing a menu no longer leaves an invisible click trap.** The WebView rendering area is now shrunk in the same step as the menu closing, so no residual transparent region is left behind intercepting clicks meant for the page underneath.

### SDK

- New exported types for the async file surface: `FileOpEntry`, `FileOpAsyncOptions`, `FileDeleteAsyncOptions`, plus payload types for the four new events (`FileOpProgressPayload`, `FileOpCompletePayload`, `MetadataProbeProgressPayload`, `MetadataProbeCompletePayload`) and their supporting unions (`FileOpKind`, `FileOpStatus`, `FileOpResultReason`, `MetadataProbeFailure`, `MetadataProbeInfoSource`).
- `dialog.openFile`, `saveFile` and `openFolder` now document their resolve shapes (`{ canceled, filePaths }` / `{ canceled, filePath }` / `{ canceled, folderPath }`), where the path field is empty on cancellation.
- `dnd.getPathsAsync` is typed against the shared session-paths shape so `resolvedPaths` is visible to TypeScript. `dnd.getResolvedPaths()` pads from `paths`, so a host that predates the field yields nulls of the right length rather than a short array that would silently misalign an index-paired loop.

### Documentation

- Every parameter table in the bilingual API reference now carries a Default column, and filler prose that restated the parameter name has been removed throughout.
- Required-column entries were verified one by one against the C++ handlers' null checks, correcting roughly 50 mismarked parameters across both languages.
- `library.coverMaxSize` is documented in KB, not bytes. Three broken `library` return-field tables were rebuilt against the actual host structures. Twelve findings from an adversarial documentation audit were applied, including two reversed semantics and several misleading descriptions.

## v1.12.0 (2026-08-13)

::: warning Breaking changes in this release
Four changes may require code edits:

- **The `dnd` drop-zone registry is gone.** `dnd.registerDropZone` / `unregisterDropZone` / `getDropZones` no longer exist; the host now observes drags natively and emits `dnd:enter` / `dnd:leave` / `dnd:drop` to the window under the cursor, no registration required. Read real paths with `fb.dnd.getPathsAsync()` or from the `dnd:drop` payload.
- **`dnd.startDrag` no longer fakes success.** Dragging tracks out of the window needs a native `IDropSource` the component does not provide; the call now resolves with `{ success: false, code: 'NOT_SUPPORTED' }` instead of reporting `success: true`.
- **Window size constraints target the calling window.** The six `window.setMinSize` / `getMinSize` / `setMaxSize` / `getMaxSize` / `setResizable` / `isResizable` endpoints no longer fall back to the main window, and a call that resolves no target now fails. A popup that relied on the old fallback was constraining the main window.
- **`DiscoveryContextMenuCommand` is no longer a type alias.** Reading `path` / `isDynamic` / `subGuid` off a context-menu command no longer type-checks — those fields were never populated.

The release stays on a minor version because the project's version axis has carried breaking changes in minor releases before (see 1.6.0). Pin an exact version if you need to upgrade deliberately.
:::

### Drag and drop

- **Drag and drop is now a native pipeline.** The host observes drag gestures itself through a native `IDropTarget` bridge and hands the page what HTML5 deliberately hides: real filesystem paths. Standard HTML5 drag events keep firing as before; `fb.dnd` runs alongside them as a side channel.
- New surface: `dnd.getPathsAsync(sessionId?)` (the reliable read inside a `drop` handler), `dnd.getPaths()` / `dnd.hasFiles()` (synchronous snapshot reads for optimistic UI), and `dnd.getCapabilities()` (whether this window can deliver paths at all).
- Events `dnd:enter` / `dnd:leave` / `dnd:drop` (payload: `sessionId`, `paths`, `x`, `y`, `keyState`) and `dnd:capabilitiesChanged`, correlated by `sessionId` and delivered point-to-point to the window under the cursor.
- Paths are withheld from untrusted origins while `hasFiles` stays accurate; a DUI / CUI panel (`hosting: 'standard'`) cannot receive paths — branch on `getCapabilities()` rather than assuming from the window type.
- **Migration**: delete `registerDropZone` / `unregisterDropZone` / `getDropZones` calls and any `zoneId` bookkeeping; keep (or add) plain HTML5 `dragover` / `drop` listeners for visuals and hit-testing; read real paths with `await fb.dnd.getPathsAsync()` inside the `drop` handler; gate path-dependent UI on `dnd.getCapabilities()`.

### Main menu

- **`menu.runMainMenuCommand` no longer leaks host exceptions.** On localized foobar2000 builds a host exception previously escaped to JavaScript as a raw host-language `Error` and made the name and path forms fail outright. Failures are now reported as `success: false` with a `code`: `MENU_ITEM_DISABLED`, `MENU_MATCH_AMBIGUOUS` (with `candidates`), or `MENU_COMMAND_NOT_FOUND`.
- **`menu.getMainMenu` leaves are addressable on localized hosts.** Leaves are now matched against the same text the host rendered the menu from and backfilled with `guid` / `subGuid`; measured 0 of 158 leaves before, 131 of 167 after on a localized host. `flags`, `enabled`, `checked`, and `hidden` are now reported as well, where previously none of them were.
- **Behavior change** — a disabled command is refused instead of reporting success. Execution previously returned `success: true` for a greyed-out command, and the same command was refused by name yet "succeeded" by GUID. All three request forms now validate alike and return `MENU_ITEM_DISABLED`. A GUID absent from the enumeration is still attempted, because a caller may hold a valid address the enumeration did not surface.
- Name and path resolution matches by exact segment. An ambiguous name is reported rather than resolved: on a localized host three separate commands can share one label, so picking the first match would silently run the wrong command. Address by `guid` to be unambiguous — it is the only form stable across hosts, since a localized build reports localized labels.
- `menu.runMainMenuCommand` accepts `subGuid` to address a dynamic child command, paired with its owning command GUID.
- A leaf that could not be resolved to an address now says so, carrying `executable: false` and `unaddressableReason` rather than appearing as an ordinary command the caller cannot act on. `available` on flat enumeration results is read from live host state instead of being always `true`, so a disabled command is no longer indistinguishable from an enabled one.

### Self-drawn menu

- **Per-call presentation options.** `menu.show` / `menu.popup` take a third `MenuPopupOptions` argument, and keys the caller omits are not sent, so the host keeps its own defaults: `windowModel` (`'fullscreen'` default, or `'contentSized'` — draws the root and its first-level submenu as separate compact windows measured to their content, so each panel carries the real DWM backdrop material and the system window shadow; the recommended model for a context menu), `css` (at most 256 KiB) / `cssReplace` for style takeover, `backdrop` (`'acrylic'` default, `'mica'`, `'mica-alt'`, `'none'`) with `backdropDarkMode` (default `true`), and `closeAnimationMs` (default `0`, clamped to `0..1000`) for an exit fade.
- **Rich items.** `MenuPopupItem.type` gains `'nowplaying'`, `'rating'`, `'slider'`, and `'segmented'` along with the fields they use (`value`, `min` / `max` / `orientation`, `segments`, and `cover` / `title` / `subtitle`), plus `iconSvg` for an inline monochrome icon on any row. Icons go through the runtime's allowlist sanitizer; an illegal or oversized one is dropped without failing the row.
- **New event `menu:valueChanged`** reports a rating, slider, or segmented change as `{ menuId, itemId, value }` and keeps the menu open, while ordinary rows still report through `menu:select` and close it. Because `menu.popup` resolves only on selection or dismissal, subscribe to this event separately when a menu contains value controls.
- **Fixed** — focus-loss dismissal is now reliable, and pooled submenu windows no longer show blank content in the `contentSized` model.

### Discovery menus

- **Behavior change** — `discovery.searchCommands` now searches the context menu as well as the main menu, so result counts increase and `type` carries a new `'contextmenu'` value. Pass `{ scope: 'mainmenu' }` for the previous coverage. The endpoint previously reported `type: 'mainmenu'` on every hit while only ever looking at the main menu, which made right-click commands unfindable.
- **Behavior change** — `discovery.searchCommands` filters entries the host would not show, matching the enumeration endpoints. Pass `{ includeHidden: true }` for the unfiltered superset.
- **Behavior change** — `discovery.getAllServices` counts context-menu commands in `services.contextMenuCommands` and includes them in `totalServices`, so `totalServices` changes value. `contextMenuHiddenFiltered` reports how many entries the filtering removed, and `stateKnown` is false when nothing was selected or playing.
- Search hits carry the same state fields as the enumeration endpoints (`enabled`, `checked`, `radioChecked`, `hidden`, `stateKnown`, `flags`, `source`, `executable`, `unaddressableReason`), so a caller can tell whether a hit is invocable without a second round trip. When the context family was searched without a track selected or playing, the response's `stateKnown` is false and those hits' `enabled` / `checked` must not be filtered on.
- Search case folding is now ASCII-only and can no longer corrupt a UTF-8 sequence. Observable matching behavior for CJK labels is unchanged — they have no case to fold.
- **`discovery.getContextMenuTree` no longer truncates silently.** Children were previously capped at 50 and depth at 10 while still reporting the host's real `childCount`, so `children.length` disagreed with `childCount` with nothing explaining the difference. The limits are now depth 16 and 512 children per node, and any clipping is reported: a `popup` node gives both `childCount` and `childrenReturned`, and a node whose subtree was clipped carries `truncated` with `depthExceeded` / `childrenExceeded` naming the cause. The flags propagate upward, so the response's top-level `truncated` covers the whole tree; `maxDepth` and `maxChildrenPerNode` echo the applied limits.
- `discovery.getContextMenuTree` nodes now report state — `enabled`, `checked`, `radioChecked`, `hidden`, `stateKnown`, `flags` — and `depth`. Separators carry only their kind, which is all that is meaningful for them.
- `discovery.executeContextMenuCommand` returns `hidden` and `resolved` on the success path, not only when refusing, so a caller can tell "was not refused" apart from "this build does not report the field". `resolved` is false when no registered item owns the GUID, in which case there was no state to evaluate.
- `discovery.getMainMenuCommands` entries expanded from a dynamic submenu now carry `stateKnown`, `executable`, and `unaddressableReason`, matching the static slots.

### SDK

- **Breaking type fix** — `DiscoveryContextMenuCommand` was a type alias for `DiscoveryMainMenuCommand` and therefore claimed fields the context tier never returns. It is now an independent interface: context items are registered flat and placed by the host, so they have no menu `path` and no dynamic-expansion fields. TypeScript code that read `path` / `isDynamic` / `subGuid` off a context-menu command was reading a field that was never populated.
- Added the exported `MenuNodeState`, `MenuNodeSource`, and `MenuUnaddressableReason` types, shared by every menu enumeration result.
- `fb.discovery.searchCommands()` accepts `{ scope, includeHidden }`; `fb.discovery.getContextMenuCommands()` accepts `{ includeHidden }`.
- Fixed SMP main-menu dispatch. A menu id was previously resolved by `path` before `guid`, and path matching fails outright on localized hosts, so `ExecuteByID` on a main menu did not work there at all. `guid` is now preferred (the host resolves it directly); `commandId` still wins for context-menu sessions.
- Fixed SMP menu state decoding. `enabled` / `checked` were previously re-derived from the raw `flags` word, ignoring the normalized booleans the host sends. Those booleans are now preferred, with `flags` used only when they are absent. An item marked `stateKnown: false` is offered as enabled rather than greyed out, because "state unknown" and "enabled, unchecked" are indistinguishable in `flags`, and treating unobserved state as disabled hid commands the host would have run.

### Tray and menus

- **Fixed** — the tray menu's "show main window" path no longer depends on the main page. Once the window was minimized or hidden to the tray, the page was deep-suspended, so no `tray:menuItemClicked` handler could run and nothing could call `window.focus` to bring the window back. A left click on the icon was equally silent. Native items (`_sys_exit`, `playbackAction`) kept working throughout.
- **Behavior change** — `showSystemItems` (default `true`) now injects `_sys_show` ("Show Main Window") before `_sys_exit` in the bottom zone. It restores and foregrounds the main window natively, preserving its maximized / normal placement, and like every native item it does **not** fire `tray:menuItemClicked`. Menus built with `showSystemItems: true` therefore gain one row; pass `showSystemItems: false` to opt out.
- `_sys_show` joins `_sys_exit` in the exact, case-sensitive reserved-id allowlist, so a frontend that renders its own "show main window" row gets the same native route with its `label` / `icon` preserved, and the injection is skipped for it. Lookalikes such as `_sys_show_alt` or `_SYS_SHOW` remain ordinary user items and do not suppress the injection. `playbackAction` still rejects `'show-main-window'` and `'exit'`: system routes stay exclusive to the reserved ids.

### Window

- **Behavior change** — the six size-constraint endpoints (`window.setMinSize` / `getMinSize` / `setMaxSize` / `getMaxSize` / `setResizable` / `isResizable`) now target the calling window, or an explicit `windowId`, and no longer fall back to the main window. A popup that set its own minimum size previously constrained the main window instead. When no target resolves the call fails rather than reporting another window's constraint. DUI / CUI panel callers are refused with `panelMode: true`.
- **Documentation fix** — sizes on these endpoints are physical pixels, not DIP as previously documented. A caller that scaled by the device pixel ratio was applying the factor twice on a high-DPI display. A value round-tripped through a setter and getter is accurate to ±1px.

### Interface language

- The plugin's native UI now follows foobar2000's presentation language instead of the Windows UI language, so a localized foobar2000 no longer shows an English preferences page. The language is detected from the host's own strings and falls back to the Windows UI language when detection is not possible.
- Added the *Interface Language* preference: *Auto (follow foobar2000)* (default), *English*, or *中文*. Newly opened dialogs apply the change immediately; menu titles and panel descriptions are registered with the host once and need a foobar2000 restart to refresh.

### Playlists

- **Playlist files from third-party components now expand correctly.** Adding a playlist URL or file through the API used a hardcoded list of eight wrapper extensions (`.pls` / `.m3u` / `.m3u8` / `.asx` / `.wpl` / `.xspf` / `.fpl` / `.cue`); a playlist format registered by another installed component was treated as a single track. The host's playlist-loader registry is now consulted at runtime, so any format the host can load is expanded.

### Performance and stability

- **Fixed** — the window no longer stays blank after the WebView2 *browser* process dies. Only the render process had a recovery path, so an external cause such as a third-party hook injected into the browser process left the window blank with no way to recover; one observed session ran that way for ten hours. The window is now rebuilt, capped at 3 rebuilds within a 10-minute window so a reproducible crash cannot degenerate into a rebuild→crash→rebuild loop. The window expires and the count resets, so an unrelated later failure is still recovered. This is separate from the v1.11.0 render-process handling, which reloads the page and rebuilds the WebView.

## v1.11.0 (2026-07-27)

### Tray and menus

- Added `TrayMenuItem.playbackAction` (`'play-pause' | 'previous' | 'next' | 'stop'`) so a custom tray item can declare a playback action the plugin runs natively. Appearance stays caller-controlled; declared items do **not** emit `tray:menuItemClicked` (same pattern as Electron `role` / Tauri `PredefinedMenuItem`). Valid only on a `type:'normal'` leaf; unknown tokens or declarations on separator / submenu / rich controls reject the whole `setContextMenu` / `appendMenuItems` call with `INVALID_PARAMS`. `'exit'` is not accepted. Tray-only — no effect on `menu.show`. `getMenuItems` round-trips the field. Prefer this (or built-in `showPlaybackControls`) for background-reliable tray playback while the main page is hidden; plain click→`playback.*` handlers are not guaranteed then. Available from v1.11.0; probe `config.getVersionInfo().plugin.version` if you must support older hosts.
- Clarified that `tray:menuItemClicked` covers ordinary user items and rich value controls only. Built-in playback / system injections and items declaring `playbackAction` execute natively without the click event; reflect button state from `playback:*`.
- `menu.getMainMenu` accepts `locale`, `i18n`, and `withAvailability`. `locale` (default `'auto'`) selects the `displayLabel` translation locale and keeps the host's native labels untranslated by default; `i18n: false` disables label translation entirely; `withAvailability` (default `true`) includes per-submenu command availability counters. The SDK signature is now `getMainMenu(root?, opts?)`.
- Fixed UTF-8 serialization and context-mode selection for custom menus.

### DSP and output

- **`dsp.*` and `output.*` now actually work.** The eleven handlers (`dsp.getChain` / `getPresets` / `getAvailable` / `addDsp` / `removeDsp` / `moveDsp` / `applyPreset` / `setChain`, `output.getDevices` / `getEntries` / `getSettings`) were documented but their source files had never been added to the build, so every call failed as an unregistered method. They are compiled and registered from this release on. The published `fb.dsp.*` and `fb.output.*` SDK wrappers were already shipping and start working against this plugin version; older plugins reject them regardless of SDK version.
- Fixed a crash in `output.getDevices`. Some output backends report a device name with a "length unknown" sentinel instead of a real length; the handler used that value verbatim and read far past the end of the string, terminating foobar2000.
- Fixed `dsp.moveDsp` moving items to the wrong slot. Upward moves landed one position short, so moving an item up by one did nothing and no item could reach the end of the chain. Downward moves were already correct. The returned `to` now reports the real final index.
- **Behavior change** — `dsp.setChain` rejects a call that contains any unusable entry instead of silently skipping it. Previously a chain built from three entries could apply only two and still report `success: true`. Each per-entry failure now returns an index-tagged reason: `dsps[0] must be an object` (also for a non-object element such as a bare string or number), `dsps[0]: guid is required` (missing, empty, or not a string), `dsps[0]: Invalid GUID format: …`, or `dsps[0]: DSP not found or no default preset: …` (a well-formed GUID for a DSP that is not installed). A missing or non-array `dsps` still fails with `dsps array is required`. The chain is left untouched whenever a call is rejected.
- `dsp.getPresets` reports `selectedIndex: -1` when no preset is selected. It previously returned the internal sentinel `18446744073709551615`, which is not representable as a JavaScript number and arrived as an unusable float.
- `dsp.getChain` always includes `activePreset` and `activePresetIndex`, using `null` / `-1` when no preset is selected or the host does not support presets. The keys were previously absent in those cases, so callers had to probe for them.

### Discovery

- **Behavior change** — `discovery.getMainMenuCommands` and `discovery.searchCommands` now expand components that build their submenu at runtime (`mainmenu_commands_v2`, e.g. ESLyric), so results include child commands in addition to the parent slot. Pass `{ expandDynamic: false }` for the previous static-registry-only result.
- New entry fields: `path`, `isDynamic`, `isDynamicParent`, `subGuid`, and `flags`. `getMainMenuCommands` echoes `expandDynamic` and `dynamicCount`, and `discovery.getAllServices` adds `mainMenuDynamicCommands`. An entry flagged `isDynamicParent` is a container slot and is not executable on its own.
- `discovery.executeMainMenuCommand` accepts an optional `subGuid` for commands expanded from a dynamic submenu; without it only the static command GUID is dispatched. The response echoes `subGuid` and `dynamic`.

### Performance and stability

- **Behavior change** — while the page is hidden (minimized, covered, tray-hidden, or locked), high-rate regenerable streams stop at the source: `audio:spectrum`, `playback:time`, and `playback:timeHighRes` are not produced, and resume on the next tick once the page is visible again. `window:hoverStateChanged` and `cursor:hiddenChanged` are naturally silent while hidden. Every other event is delivered reliably and in order — nothing is dropped or merged — including async replies such as `http:response`, `library:getAllResult`, and `audio:fullWaveformReady`, and one-off facts such as `playback:itemPlayed`. Themes that draw a spectrum or a seek position should read a gap as "page hidden", not as "playback stopped".
- Added deep suspend while minimized, covered, locked, or tray-hidden: renderer timers and animations are frozen so the OS can reclaim memory. Controlled by the new advanced-preferences option *Deep-suspend WebView when hidden (TrySuspend; frees renderer memory)* (default on; turning it off falls back to the previous low-memory path).
- Added the advanced-preferences option *Keep WebView active in background while CDP remote debugging is on (tray/minimize/lock)* (default on), so screenshot and timing automation over the DevTools Protocol stays stable instead of being suspended.
- Hardened recovery after a WebView2 crash: a failed render process no longer leaves an unresponsive blank window. The page is reloaded up to a bounded number of attempts and the WebView is rebuilt after repeated failures.
- Improved album-art delivery: fixed cache entries that could serve another track's image, tightened request and parameter validation, and moved image decoding off the interface thread so large covers no longer make the window unresponsive. `artwork.*` request and response shapes are unchanged.

### Metadata

- Fixed `metadata.read`, `metadata.readByPath`, and `metadata.readBatch` ignoring the track index inside a container. A `|subsong:N` suffix was neither stripped nor honored, so reading a single track out of a CUE sheet, ISO image, or multi-track file either failed outright or returned the first track's tags. This is why such files could be read in the foobar2000 UI but not through the API. `metadata.readRaw` was already correct.
- `metadata.read` and `metadata.readByPath` accept `cueIndex` to address a track explicitly, matching `metadata.readRaw`. It takes precedence over a `|subsong:N` suffix in the path. The `fb.metadata.read()` / `readByPath()` wrappers take it as a second `opts` argument, and the `fb2k_metadata_read` / `fb2k_metadata_read_by_path` MCP tools declare it. `metadata.readBatch` does not accept it — address per-track reads there with a `|subsong:N` suffix.

### SDK

- Added SDK-only additive binary adapters: `fb.file.readBinary()`, `fb.file.writeBinary()`, `fb.file.writeDataUrl()`, `fb.metadata.embedArtworkBytes()`, and `fb.metadata.embedArtworkFromDataUrl()`, plus `FileBinaryWriteOptions` and `MetadataArtworkBytesOptions`.
- These helpers adapt `ArrayBuffer` / `Uint8Array` values and strict Base64 Data URLs to the existing `file.read`, `file.write`, and `metadata.embedArtwork` wire contracts. They add no new Bridge endpoints and do not change raw `invoke` or existing facades. Canonical Base64 and Data URL validation occurs in the SDK before invocation; Host validation and behavior are unchanged.
- **Breaking type fix** — the published response types for `ui.isMinimized()` and `ui.isAlwaysOnTop()` were wrong and now match the wire contract: `isMinimized` resolves with `{ minimized }` (there is no `isMinimized` alias), and `isAlwaysOnTop` resolves with `{ enabled, isAlwaysOnTop }` (both carry the same value). Runtime behavior is unchanged; TypeScript code written against the old declarations must be updated.
- `fb.playcount.set()` no longer sends the `count` key, which the host never read. No behavior change; the wire payload is simply smaller.
- `fb.http.request()` now dispatches through the documented `http.get` endpoint; a stale internal parameter could previously forward a mismatched method name. The verb helpers (`fb.http.post()` / `put()` / `delete()` / `patch()`) keep dispatching to their own endpoints, including for binary responses.
- Added `windowId` parameter typings for `window.getBackdropPolicy` and `window.setBackdropPolicy`. `setBackdropPolicy` requires `backdropPolicy` and does not fall back to the main window when no target resolves.
- Added optional trailing `opts` arguments to five wrappers whose host handlers already read the corresponding keys: `fb.file.delete(path, opts?)` (`moveToTrash`), `fb.file.copy(source, destination, opts?)` (`overwrite`), and `fb.metadata.write(path, tags, opts?)` / `removeField(path, field, opts?)` / `removeTag(path, tags, opts?)` (`cueIndex`). `metadata.write` accepting `cueIndex` closes a real gap: v1.11.0 wired `cueIndex` into the metadata *read* path only, so writing a tag to a single track inside a CUE sheet or image file was not expressible through the SDK. Existing call sites are unaffected — every new argument is optional.
- `fb.metadata.readByPath()` now resolves with `MetadataReadByPathResponse` instead of a bare `JsonObject`.
- Corrected two published type declarations that did not match the host contract: `dsp.setChain` takes a **required** `dsps` array of `{ guid }` objects (it was typed `dsps?: string[]`, wrong in optionality, element type, and shape — the host rejects the call unless `dsps` is an array, and reads `guid` off each entry), and the `playlist:created` / `playlist:renamed` payloads keep `name: string`. TypeScript code written against the old `dsp.setChain` declaration must be updated.

## v1.10.0 (2026-07-16)

- Added `TrayMenuItem.orientation` for `type:'slider'` with `'horizontal' | 'vertical'`; horizontal is the default. Only the exact value `vertical` selects vertical behavior (min at the bottom / max at the top; Up/Right increase, Down/Left decrease, and Home/End select the bounds). `native` ignores the field and keeps the tiered submenu; older runtimes ignore the unknown field and remain horizontal. Range normalization swaps `max<min`; `max==min` is constant and emits no value; the initial value is clamped; out-of-range IPC values are rejected. `getMenuItems` round-trips the field. Available from v1.10.0; themes that must support older hosts should probe `config.getVersionInfo().plugin.version`.
- Changed custom-menu focus to two modes: navigation with roving tabindex and real focus, and rich-control editing. ARIA uses `menuitem`, `menuitemcheckbox`, an internal `role=slider`, and a segmented `radiogroup`; `checked:false` remains checkable. Default entrance and exit transform/transition effects are disabled under `prefers-reduced-motion: reduce` without changing the hide protocol or `closeAnimationMs`.
- Added `TrayMenuConfig.layoutMode` with `'flat' | 'zones'`. The default `'flat'` preserves direct `#menu > .fb-item` children. Explicit `'zones'` creates `.fb-zone[data-zone]` wrappers for non-empty top / playback / bottom sections. `native` ignores the field; older runtimes ignore the unknown field and create no wrapper; `menu.show` is unaffected. Available from v1.10.0; themes that must support older hosts should probe `config.getVersionInfo().plugin.version`.
- Changed protected custom-menu CSS so the visible state no longer forces `#menu { display:block !important }`. Themes can make the root menu or zones flex or grid containers, but cannot use `display:* !important` to reveal a hidden menu.
- Hardened custom-menu SVG icons by replacing raw `innerHTML` injection with DOMParser plus allowlisted element and attribute cloning. Invalid or individually oversized icons are discarded while the menu continues to render. `transform` is parsed strictly, rejecting prefixes, inter-function junk, and empty arguments, and nodes must be in the SVG namespace.
- Added transactional resource-limit validation to `tray.setContextMenu`, `tray.appendMenuItems`, and `menu.show` before persistent configuration is written or an overlay opens: item ≤ 512, `menu.show` depth ≤ 8, segmented options ≤ 64, CSS ≤ 256 KiB, and aggregate SVG ≤ 256 KiB. A single SVG over 32 KiB is discarded without rejecting the whole menu. Other invalid or oversized input returns `INVALID_PARAMS` with `field` / `limit` / `actual` in `details`; this is an intentional incompatibility for unsafe input.
- Hardened tray and custom-menu built-in action routing to use trusted internal provenance instead of a public id prefix. The sole compatibility exception is the exact, case-sensitive `_sys_exit` in the tray API, preserving the real exit behavior from 1.9.0. Caller-supplied `_pb_playPause` / `_pb_prev` / `_pb_next` / `_pb_stop` remain ordinary user items and cannot suppress runtime-injected trusted playback items through same-id deduplication. Opaque tokens distinguish duplicate public IDs, and public `menu.show` does not elevate them. Each selection or value change carries an unpredictable one-shot token validated against the current menu index; unknown or expired tokens, disabled items, and out-of-range rich values for rating / slider / segmented are rejected. Internal `menu.__*` IPC also verifies that the caller is the overlay window and that select / dismiss / ready / submenuPanel / valueChanged match the current menu id; external callers and stale or forged menuId values are rejected without changing menu state.
- Custom tray `ContentSized` now measures the root and every first-level submenu offscreen after fonts are ready, then waits for stable dimensions across two consecutive frames. C++ uses 64-bit-safe slot allocation. The fixed HWND region covers only the currently visible root/submenu panel, so reserved space for unopened panels no longer creates a blank acrylic/mica area and the caller's configured backdrop is not silently disabled.
- Clarified that a `segmented` value change in a custom tray menu follows the keep-open contract. A segment change emits `tray:menuItemClicked` with `{ id, value }`, where `value` is the zero-based selected-segment index, and does **not close** the menu, matching `rating` and `slider`. The `webview` runtime already kept the menu open; this corrects the shared contract and event documentation that previously listed only `rating` / `slider`.
- Fixed extra separators for completely hidden or empty tray-menu sections. Previously, filtering all items with `visible:false` could leave a leading or trailing separator. Visibility is now filtered before separator decisions, so neither native nor webview menus render that separator.
- Corrected the documentation for `TrayMenuItem.icon`: base64 ICO remains reserved and neither backend renders it (`native` is text-only and webview renders `iconSvg`). Use `iconSvg` for menu-item icons.

## v1.9.0 (2026-06-18)

- Added icons for normal and submenu items in custom tray menus (`render: 'webview'`) through `TrayMenuItem.iconSvg = { viewBox, content }`. Inline monochrome SVG follows menu text color through `currentColor` and uses a fixed 8px left-aligned gap. When any peer has an icon, all normal and submenu items reserve a 16px icon column for text alignment. `native` menus ignore it.
- Added `config.autoNowPlaying` to `tray.setContextMenu`. When enabled, empty cover/title/subtitle fields on a `nowplaying` item fall back to the current track when the context menu opens; caller-provided values take precedence. The `cover` fallback is `webview`-only and uses a thumbnail of current artwork. title/subtitle use `%title%` with filename fallback and `%artist%`, including dynamic streaming titles.
- Extended `TrayMenuItem.cover` to accept `http(s)://` URLs in addition to existing `data:` values and raw base64, allowing streaming frontends to pass live artwork directly.
- Updated the SDK package to `1.9.0`.

## v1.8.0 (2026-06-10)

- Added custom-menu rendering through `menu.show` / `menu.close`, with WebView-rendered content and recursive submenus. The menu window uses a content-sized fixed-window strategy to prevent expansion flicker.
- Added `render: 'webview'` to `tray.*`, allowing tray context menus to use custom rendering consistent with the theme.
- Added `tray.setMenuItemState` to update one menu item's state without rebuilding the entire menu.
- Fixed clicks on always-on-top popups such as desktop lyrics occasionally bringing the main window to the foreground and making it topmost (the rollback path inserted z-order into the topmost band and formed a sink restoration reference loop).
- Fixed missing global `HTMLElementTagNameMap` declarations in the published SDK so npm consumers regain type completion for `fb-*` custom elements.
- Fixed package-script compatibility with newer PowerShell versions when generating `.fb2k-component` archives.
- Hardened the HttpApi asynchronous-request exception boundary and fixed a NUL string-handling defect in LibraryApi.
- Updated the SDK package to `1.8.0`; `bump-version.ps1` now also synchronizes `sdk/package-lock.json` and the VitePress navigation version.

## v1.7.0 (2026-06-06)

- Added Taskbar & Tray capabilities. `taskbar.*` can configure thumbnail-toolbar buttons, progress, overlay icons, and flash notifications; `tray.*` can create a system-tray icon, balloon notifications, and context menus.
- Added incremental menu management to `tray.*` through `appendMenuItems` / `removeMenuItems` / `clearMenuItems` / `getMenuItems`, allowing `top` / `playback` / `bottom` sections to be maintained without rebuilding the entire menu.
- Added `taskbar:buttonClicked`, `tray:click`, `tray:doubleClick`, `tray:menuItemClicked`, and `tray:beforeContextMenu` events for taskbar-thumbnail and tray interaction.
- Added `webview:processFailed`, which broadcasts diagnostics for WebView2 render-process failures and works with automatic render-process recovery to reduce blank-window failures.
- Added high-resolution playback-position event `playback:timeHighRes`, driven by a dedicated WinAPI timer for sub-second lyrics and progress updates.
- Moved cold-cache full serialization for `library.getAll` to a background thread. The SDK waits for `library:getAllResult` and correlates it by `requestId` so large-library queries do not block the UI.
- Fixed the window restoration path after hiding to tray, including WebView surface recovery for `window.focus` / hidden restore, reducing blank surfaces after minimize, tray hide, or Alt+Tab restoration.
- Fixed corrupt base64 for the taskbar-thumbnail pause icon and corrected HICON ownership, preventing malformed playback buttons and explorer.exe crashes.
- Updated the SDK package to `1.7.0`, including the new Taskbar & Tray types and event declarations.
- Added a Taskbar & Tray API page to VitePress and synchronized Cursor, high-frequency Playback events, and related examples.

## v1.6.1 (2026-05-20)

- Added the `cursor.*` namespace: `cursor.setHidden(hidden)` / `cursor.isHidden()` explicitly control client-area cursor visibility, addressing unreliable CSS `cursor: none` behavior under Visual Hosting.
- Added per-window `cursor:hiddenChanged` events.
- Added the `insecureTls` parameter to `fb.http.*` behind two gates: the global `Allow self-signed / invalid TLS certificates` setting must be ON and the request must specify `insecureTls: true`. This allows explicitly authorized access to self-signed intranet services such as Plex / Jellyfin / Lidarr.
- Added `responseType: 'arraybuffer' | 'binary'` to `fb.http.*`; the body is base64-decoded to an `ArrayBuffer`, so binary artwork and fonts no longer fail strict UTF-8 validation.
- Updated the VitePress cursor.md / http.md / events.md documentation for these changes.

## v1.6.0 (2026-05-11)

- Removed `duration` from `playlist.getAll` to avoid reading every track solely to calculate duration; `playlist.getActive` / `playlist.getPlaying` still return it.
- Changed `http.get` / `http.post` / `http.head` to asynchronous by default. Pass `async: false` explicitly for a synchronous call.

## v1.1.17 (2026-02-06) 

- Added full multi-window support.
- Added `window.createPopup` / `closePopup` / `closeAllPopups` / `getAllWindows`.
- Added `window.sendMessage` / `window.broadcast` for inter-window messaging.
- Added asynchronous close, frameless windows, and transparent backgrounds.

## v1.1.16 (2026-02-06)

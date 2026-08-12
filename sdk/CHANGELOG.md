# Changelog

All notable changes to the foo-webview-sdk will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.12.0] - 2026-08-12

> **Breaking changes**: the `dnd` drop-zone registry (`registerDropZone` /
> `unregisterDropZone` / `getDropZones`) is removed and `dnd.startDrag` now
> reports `NOT_SUPPORTED` instead of faking success — see *Removed* and
> *Changed* below for migration. On the type level, `SmpMenuBuildState` gained
> a required `family` field and `DiscoveryContextMenuCommand` is no longer an
> alias of `DiscoveryMainMenuCommand`.

### Added

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
("SDK 版本与插件版本保持统一"). The numeric
drop from `2.0.0` to `1.5.0` is a deliberate alignment with the plugin
release cadence, **not** a regression of any SDK feature or behavior.

The publishing-layout migration introduced in [2.0.0] (dist-only entry,
sub-path exports, archived hand-written files) remains in effect. No
SDK-level public API or behavior has changed since [2.0.0].

## [2.0.0] - 2026-05-05

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

# Changelog

## v1.11.0 (2026-07-25)

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

- **Behavior change** — event delivery is gated while the page is hidden (minimized, covered, tray-hidden, or locked). Control-plane events (`menu:*`, `jitQueue:*`, `tray:*`, `port:*`, `state:changed`, `state:deleted`, `window:message`, `window:beforeClose`, `app:beforeQuit`, `keyboard:hotkey`, `taskbar:buttonClicked`, `ui:menuItemClicked`, `webview:processFailed`) always pass through. High-rate regenerable streams (`audio:spectrum`, `playback:time`, `playback:timeHighRes`, `window:hoverStateChanged`, `cursor:hiddenChanged`) are dropped while hidden and resume on the next tick. Remaining events are coalesced to their latest payload per event name and replayed in arrival order when the page becomes visible again. Themes that relied on a continuous background event stream should re-read state after becoming visible.
- Added deep suspend while minimized, covered, locked, or tray-hidden: renderer timers and animations are frozen so the OS can reclaim memory. Controlled by the new advanced-preferences option *Deep-suspend WebView when hidden (TrySuspend; frees renderer memory)* (default on; turning it off falls back to the previous low-memory path).
- Added the advanced-preferences option *Keep WebView active in background while CDP remote debugging is on (tray/minimize/lock)* (default on), so screenshot and timing automation over the DevTools Protocol stays stable instead of being suspended.
- Hardened recovery after a WebView2 crash: a failed render process no longer leaves an unresponsive blank window. The page is reloaded up to a bounded number of attempts and the WebView is rebuilt after repeated failures.
- Improved album-art delivery: fixed cache entries that could serve another track's image, tightened request and parameter validation, and moved image decoding off the interface thread so large covers no longer make the window unresponsive. `artwork.*` request and response shapes are unchanged.

### Metadata

- Fixed `metadata.read`, `metadata.readByPath`, and `metadata.readBatch` ignoring the track index inside a container. A `|subsong:N` suffix was neither stripped nor honored, so reading a single track out of a CUE sheet, ISO image, or multi-track file either failed outright or returned the first track's tags. This is why such files could be read in the foobar2000 UI but not through the API. `metadata.readRaw` was already correct.
- `metadata.read` and `metadata.readByPath` accept `cueIndex` to address a track explicitly, matching `metadata.readRaw`. It takes precedence over a `|subsong:N` suffix in the path.

### SDK

- Added SDK-only additive binary adapters: `fb.file.readBinary()`, `fb.file.writeBinary()`, `fb.file.writeDataUrl()`, `fb.metadata.embedArtworkBytes()`, and `fb.metadata.embedArtworkFromDataUrl()`, plus `FileBinaryWriteOptions` and `MetadataArtworkBytesOptions`.
- These helpers adapt `ArrayBuffer` / `Uint8Array` values and strict Base64 Data URLs to the existing `file.read`, `file.write`, and `metadata.embedArtwork` wire contracts. They add no new Bridge endpoints and do not change raw `invoke` or existing facades. Canonical Base64 and Data URL validation occurs in the SDK before invocation; Host validation and behavior are unchanged.
- **Breaking type fix** — the published response types for `ui.isMinimized()` and `ui.isAlwaysOnTop()` were wrong and now match the wire contract: `isMinimized` resolves with `{ minimized }` (there is no `isMinimized` alias), and `isAlwaysOnTop` resolves with `{ enabled, isAlwaysOnTop }` (both carry the same value). Runtime behavior is unchanged; TypeScript code written against the old declarations must be updated.
- `fb.playcount.set()` no longer sends the `count` key, which the host never read. No behavior change; the wire payload is simply smaller.
- `fb.http.request()` now dispatches through the documented `http.get` endpoint; a stale internal parameter could previously forward a mismatched method name. The verb helpers (`fb.http.post()` / `put()` / `delete()` / `patch()`) keep dispatching to their own endpoints, including for binary responses.
- Added `windowId` parameter typings for `window.getBackdropPolicy` and `window.setBackdropPolicy`. `setBackdropPolicy` requires `backdropPolicy` and does not fall back to the main window when no target resolves.

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

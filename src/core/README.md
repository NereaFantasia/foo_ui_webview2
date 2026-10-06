English | [中文](./README.zh-CN.md)

# src/core/ — WebView Lifecycle & Runtime Core

`core/` is the runtime foundation of the component: it defines the WebView2 base class shared by all windows/panels, the multi-instance routing table, and the JIT streaming queue. Both `window/` and `panels/` are built on top of this module. foobar2000's UI entry and the background mode live in [`ui/`](../README.md#supporting-modules), the preferences pages in [`prefs/`](../README.md#supporting-modules), and the library cache and directory-tree index in [`domain/library/`](../README.md#supporting-modules).

---

## Overview

`core/` answers three questions:

1. **Who is the UI entry?** `WebViewUI` in `ui/` (which implements foobar2000's `user_interface`) creates the main window when fb2k starts; the window it creates is built on the base class and registry below.
2. **Where do WebView2's common capabilities live?** `WebViewPanel` consolidates initialization, API registration, callback initialization, message handling, and config hot-reload into a base class, inherited by `MainWindow` / `WebViewDuiElement` / `WebViewCuiPanel`.
3. **How do multiple WebView instances find each other?** `WebViewContext` registers all instances keyed by HWND, supporting cross-instance event broadcasting and directed routing by window ID.

Besides these three, `core/` holds one runtime service: the JIT streaming queue.

---

## Key Files / Classes

| File | Responsibility |
|------|------|
| `WebViewPanel.h/.cpp` | WebView panel base class: `InitializeWebView`, `RegisterAllApis`, `InitializeCallbacks`, navigation/reload, `ApplyConfig` config hot-reload, holds `PanelConfig`, holds `SelectionHolder`; the mode enum `Standalone/DuiPanel/CuiPanel`; defines overridable virtual functions (`OnWebViewReady`, etc.) |
| `PanelBootstrap.cpp` | Defines `WebViewPanel::RegisterAllApis` / `InitializeCallbacks`; the only TU that includes every `api/` and `callbacks/` module header |
| `PanelCrashDiagnostics.h/.cpp` | The `kind` and `recoveryAction` fields of the `webview:processFailed` payload |
| `WebViewContext.h/.cpp` | Multi-instance manager (singleton): `RegisterInstance`/`UnregisterInstance` (by HWND), `GetBridge`/`GetWebViewHost`/`GetPanelByHwnd`, `BroadcastEvent`/`BroadcastEventExcept`, `SendEventTo` by windowId and reverse lookup |
| `QueueManager.h/.cpp` | JIT streaming queue (singleton): a frontend logical queue + a backend "shadow playlist" (only current+next), the state machine `Idle/Active/WaitingNext/Exhausted`, just-in-time URL resolution, auto-buffering, `jitQueue:needNext` prefetch, shadow-list lock protection. `QueueManager.cpp` holds the singleton, the public commands, the state queries, and the playback callbacks |
| `QueueShadowPlaylist.cpp` | `QueueManager` members for the shadow playlist: the `ShadowPlaylistLock` lock and its callbacks, finding or creating the playlist, detecting playback from it, cleaning up played tracks, removing a stale buffered next, and starting playback on it |
| `QueueSources.cpp` | `QueueManager` members for track sources: local-path detection, `PreloadBatch`, subsong-suffix parsing, URL-to-handle conversion, and adding streams or local files to the shadow playlist asynchronously |
| `QueueManagerInternal.h` | The `Announce` event helper shared by `QueueManager.cpp` and `QueueSources.cpp` |
| `FrontendDirectoryResolver.h/.cpp` | Builds the candidate frontend directories from the current settings; the main window, popups, DUI/CUI panels and the panel settings dialog all resolve through it |
| `FrontendDirectoryPolicy.h/.cpp` | Picks the first candidate that has an `index.html`, in the order panel template → active template → `foo_ui_webview2_resources\dist` under the component folder → the `default` template; independent of the foobar2000 SDK, so unit tests link it directly |

---

## How It Works / Data Flow

### WebView Lifecycle

```
fb2k startup
   └─ WebViewUI::init()                       (ui/UserInterface)
        └─ new MainWindow → Create()          (window/)
             └─ WebViewPanel::InitializeWebView(hwnd, Standalone)
                   ├─ WebViewHost::Initialize()         (webview/, shared warmed-up environment)
                   ├─ RegisterAllApis()                 (api/)
                   ├─ InitializeCallbacks()             (callbacks/)
                   ├─ WebViewContext::RegisterInstance(hwnd, host, bridge, id, panel)
                   └─ OnWebViewReady() → load frontend page
```

Panel modes (DUI/CUI) follow the same `WebViewPanel` path, only with a different `mode_`, each holding a per-instance `BridgeCore`, and registering the instance into `WebViewContext`.

### Event Broadcasting & Directing

Events produced by `callbacks/` go out through the helpers in `api/EventEmit.h`, which call into `WebViewContext`: most are pushed to all instances via `BroadcastEvent(event, data)`; when the sender needs to be excluded, use `BroadcastEventExcept`; for point-to-point, use `SendEventTo(windowId, ...)`. `WebViewContext` also supports tracing back from a child-window HWND to the top-level window (`GetHostByHwnd`).

### JIT Streaming Queue

The frontend maintains the full logical queue, while the backend `QueueManager` keeps only the current + next track in a hidden "shadow playlist". Near the end of a track (30s remaining by default), it emits `jitQueue:needNext` so the frontend resolves the next track's URL and then `enqueueNext`, achieving seamless streaming; `PlaybackCallback` forwards fb2k's track-change/stop/progress callbacks to `QueueManager` to advance the state machine.

---

## Dependencies

- **Depends on**: `webview/` (`WebViewHost`/`WebViewEnvironment`), `api/` (registration and bridging), `callbacks/` (event source), `selection/` (`SelectionHolder`), `panels/PanelConfig`, `prefs/` (`WebViewPanel` reads `webview_prefs::*`), `utils/`, and the foobar2000 SDK.
- **Depended on by**: `window/` (`MainWindow`/`PopupWindow` inherit `WebViewPanel`), `panels/` (DUI/CUI instances inherit `WebViewPanel`), `api/` (`QueueApi` calls the queue, and handlers broadcast events through `WebViewContext`), `ui/` (the main-menu command counts instances through `WebViewContext`), and `prefs/` (reloads panels and applies the default zoom through `WebViewContext`/`WebViewPanel`).

---

## Extension Guide

- **Add a runtime service**: prefer making it a singleton (see `QueueManager`, or `domain/library/LibraryCache`), use `mutex`/`shared_mutex` for thread safety, and clarify the invalidation/cleanup timing (trigger `Invalidate` in the corresponding `callbacks/`). A service that needs nothing from `api/`, windows, or UI belongs in `domain/` instead.
- **Add an overridable lifecycle hook**: add a virtual function (such as `OnXxx`) to `WebViewPanel` with an empty base implementation, and let `MainWindow`/panels override it as needed, keeping the three modes' behavior consistent.
- **Add a preference**: add a cfg_var and UI control in `prefs/PreferencesPage` (or the matching sub-page), and expose accessor functions through `webview_prefs::*` / `security_config::*`, avoiding reading global variables directly in the business layer.

---

See also: the repository root [README.md](../../README.md), [../api/README.md](../api/README.md), and [../window/README.md](../window/README.md).

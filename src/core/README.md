English | [中文](./README.zh-CN.md)

# src/core/ — WebView Lifecycle & Runtime Core

`core/` is the runtime foundation of the component: it defines foobar2000's UI entry, the WebView2 base class shared by all windows/panels, the multi-instance routing table, and runtime services such as the library cache, directory-tree index, JIT streaming queue, background mode, and preferences page. Both `window/` and `panels/` are built on top of this module.

---

## Overview

`core/` answers three questions:

1. **Who is the UI entry?** `WebViewUI` (which implements foobar2000's `user_interface`) creates the main window when fb2k starts.
2. **Where do WebView2's common capabilities live?** `WebViewPanel` consolidates initialization, API registration, callback initialization, message handling, and config hot-reload into a base class, inherited by `MainWindow` / `WebViewDuiElement` / `WebViewCuiPanel`.
3. **How do multiple WebView instances find each other?** `WebViewContext` registers all instances keyed by HWND, supporting cross-instance event broadcasting and directed routing by window ID.

The remaining files are runtime services revolving around these three things (cache, queue, background, preferences, security config).

---

## Key Files / Classes

| File | Responsibility |
|------|------|
| `UserInterface.h/.cpp` | `WebViewUI : user_interface` — foobar2000's main UI entry (GUID, `init`/`shutdown`/`activate`/`hide`), creates and holds `MainWindow`, singleton `GetInstance()` |
| `WebViewPanel.h/.cpp` | WebView panel base class: `InitializeWebView`, `RegisterAllApis`, `InitializeCallbacks`, navigation/reload, `ApplyConfig` config hot-reload, holds `PanelConfig`, holds `SelectionHolder`; the mode enum `Standalone/DuiPanel/CuiPanel`; defines overridable virtual functions (`OnWebViewReady`, etc.) |
| `WebViewContext.h/.cpp` | Multi-instance manager (singleton): `RegisterInstance`/`UnregisterInstance` (by HWND), `GetBridge`/`GetWebViewHost`/`GetPanelByHwnd`, `BroadcastEvent`/`BroadcastEventExcept`, `SendEventTo` by windowId and reverse lookup |
| `LibraryCache.h/.cpp` | In-memory library cache (singleton): multi-level cache for albums/tracks/artists/genres/stats/cover, `shared_mutex` read-write lock, tracks use `shared_ptr<const json>` to avoid deep copies, cover cache capped at 100MB, `Invalidate()` on library changes |
| `LibraryTreeIndex.h/.cpp` | Media-root and directory-tree indexer (singleton): infers the real media roots using `library_manager::get_relative_path` + path-tail comparison, built lazily and thread-safe; serves `library.getRoots` / `library.browseTree` |
| `QueueManager.h/.cpp` | JIT streaming queue (singleton): a frontend logical queue + a backend "shadow playlist" (only current+next), the state machine `Idle/Active/WaitingNext/Exhausted`, just-in-time URL resolution, auto-buffering, `jitQueue:needNext` prefetch, shadow-list lock protection |
| `BackgroundService.h/.cpp` | Background mode: when another UI (Default UI, etc.) is in use, keeps WebView2 running in the background to preserve API access; the window can stay invisible throughout |
| `PreferencesPage.h/.cpp` | Preferences page (Preferences → Display → WebView2 UI): template management, window settings, DWM background effects, developer options; exposes `webview_prefs::*` config accessor functions |
| `SecurityConfig.h` | Security config access interface (`security_config::*`, implemented in `main.cpp`): toggles for DevTools, CDP remote debugging, local network, plaintext HTTP, self-signed TLS, background mode, and the HMR dev server |

---

## How It Works / Data Flow

### WebView Lifecycle

```
fb2k startup
   └─ WebViewUI::init()                       (core/UserInterface)
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

Most events produced by `callbacks/` are pushed to all instances via `WebViewContext::BroadcastEvent(event, data)`; when the sender needs to be excluded, use `BroadcastEventExcept`; for point-to-point, use `SendEventTo(windowId, ...)`. `WebViewContext` also supports tracing back from a child-window HWND to the top-level window (`GetHostByHwnd`).

### Library Cache & Index

`LibraryApi` reads from `LibraryCache` first (on a hit it returns a `shared_ptr` handle with zero deep copies); directory-tree/root browsing goes through `LibraryTreeIndex` (built synchronously and cached on first access). When `callbacks/LibraryCallback` detects library changes, it calls `Invalidate()` to invalidate both, so they are rebuilt on the next access.

### JIT Streaming Queue

The frontend maintains the full logical queue, while the backend `QueueManager` keeps only the current + next track in a hidden "shadow playlist". Near the end of a track (30s remaining by default), it emits `jitQueue:needNext` so the frontend resolves the next track's URL and then `enqueueNext`, achieving seamless streaming; `PlaybackCallback` forwards fb2k's track-change/stop/progress callbacks to `QueueManager` to advance the state machine.

---

## Dependencies

- **Depends on**: `webview/` (`WebViewHost`/`WebViewEnvironment`), `api/` (registration and bridging), `callbacks/` (event source), `selection/` (`SelectionHolder`), `panels/PanelConfig`, `utils/`, and the foobar2000 SDK.
- **Depended on by**: `window/` (`MainWindow`/`PopupWindow` inherit `WebViewPanel`), `panels/` (DUI/CUI instances inherit `WebViewPanel`), `api/` (`LibraryApi`/`QueueApi` call the cache and queue, and handlers broadcast events through `WebViewContext`), and `main.cpp` (which registers `WebViewUI`, `PreferencesPage`, and `BackgroundService`).

---

## Extension Guide

- **Add a runtime service**: prefer making it a singleton (see `LibraryCache`/`QueueManager`), use `mutex`/`shared_mutex` for thread safety, and clarify the invalidation/cleanup timing (trigger `Invalidate` in the corresponding `callbacks/`).
- **Add an overridable lifecycle hook**: add a virtual function (such as `OnXxx`) to `WebViewPanel` with an empty base implementation, and let `MainWindow`/panels override it as needed, keeping the three modes' behavior consistent.
- **Add a preference**: add a cfg_var and UI control in `PreferencesPage`, and expose accessor functions through `webview_prefs::*` / `security_config::*`, avoiding reading global variables directly in the business layer.

---

See also: the repository root [README.md](../../README.md), [../api/README.md](../api/README.md), and [../window/README.md](../window/README.md).

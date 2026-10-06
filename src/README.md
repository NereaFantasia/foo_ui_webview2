English | [中文](./README.zh-CN.md)

# src/ — C++ Backend Source Map

`src/` is the heart of the foo_ui_webview2 C++ implementation: it turns the entire foobar2000 window (along with DUI/CUI panels) into a WebView2 canvas and builds a bidirectional JSON bridge between the foobar2000 SDK and the web frontend.

This file is the overview map for `src/`. Every top-level functional module has its own README; jump into the corresponding subdirectory when you want to dig deeper into a specific module.

---

## Overview

The component's runtime path can be summarized as "frontend call → bridge dispatch → SDK execution → event return":

```
JavaScript                       C++                          foobar2000 SDK
──────────────────────────────────────────────────────────────────────────
fb2k.invoke('playback.play')
      └─ postMessage ─────> BridgeCore::HandleMessage   (src/api)
                                 └─ handlers_["playback.play"]
                                       └─ PlaybackApi   (src/api)
                                             └─ playback_control::start()
                                                   └─ on_playback_new_track
PlaybackCallback (src/callbacks) ──> api::emit::Broadcast<…> (src/api/EventEmit.h)
      └─ postMessage ─────> fb2k.on('playback:trackChanged')
```

- The window & host layer (`window/` + `core/` + `webview/`) embeds WebView2 into foobar2000 windows/panels and manages their lifecycle; `media/` serves local audio and video files for the page to play.
- The bridge & API layer (`api/`) receives frontend messages, routes them to specific handlers, and sends back responses and events.
- The event callback layer (`callbacks/` + `selection/`) listens for foobar2000 SDK changes and broadcasts them to the frontend.
- The utility & abstraction layer (`utils/` + `interfaces/`) provides cross-cutting capabilities such as encoding conversion and service injection.
- The domain layer (`domain/`) holds logic that does not depend on the bridge, windows, or UI: path security validation and the library directory-tree index and cache.

---

## Module Landscape

> Core modules each have their own README. Supporting modules are described in the "Supporting Modules" section on this page.

| Module | Responsibility | Key Classes/Files | Docs |
|------|------|------------|------|
| `api/` | C++ ↔ JS bridge + APIs in 40+ namespaces (declared in `api/schema/`) + plugin extension | `BridgeCore`, `PluginRegistry`, `*Api` | [api/README.md](api/README.md) |
| `window/` | Window system: main window/popups, Chrome (Mica/title bar), tray/taskbar | `MainWindow`, `WindowManager`, `ChromeController`, `WindowChrome*` | [window/README.md](window/README.md) |
| `callbacks/` | foobar2000 SDK events → BridgeCore event broadcasting | 9 callbacks such as `InitPlaybackCallbacks` | [callbacks/README.md](callbacks/README.md) |
| `core/` | WebView lifecycle, multi-instance routing, JIT queue | `WebViewPanel`, `WebViewContext`, `QueueManager` | [core/README.md](core/README.md) |
| `prefs/` | Preferences pages (Display → WebView2 UI and its sub-pages), template management, `webview_prefs::*` accessors | `PreferencesPage`, `PreferencesPageBase`, `PreferencesFields`, `PreferencesDraft` | See below on this page |
| `ui/` | foobar2000 UI entry, background mode, main-menu command, `app:beforeQuit` | `UserInterface` (`WebViewUI`), `BackgroundService`, `WebViewMainMenu`, `AppQuit` | See below on this page |
| `settings/` | cfg_var and Advanced-preference storage, `security_config` / `window_config` accessors | `SecurityConfig`, `AdvancedConfig`, `WindowStateConfig` | See below on this page |
| `domain/` | Domain logic independent of the bridge, windows, and UI: path security validation, library directory-tree index and cache | `PathSecurity`, `LibraryTreeIndex`, `LibraryCache` | See below on this page |
| `utils/` | Utilities: path expansion / encoding / Base64 / icons / i18n, etc. | `PathExpansion`, `Encoding`, `Base64`, `I18n` | See below on this page |
| `panels/` | DUI/CUI panel integration & config persistence | `WebViewDuiElement`, `WebViewCuiPanel`, `PanelConfig` | [panels/README.md](panels/README.md) |
| `webview/` | WebView2 host & shared environment, injected scripts | `WebViewHost`, `WebViewEnvironment` | See below on this page |
| `interfaces/` | Service abstractions (dependency injection / testability) | `IPlaybackService`, `IPlaylistService` | See below on this page |
| `selection/` | Selection tracking | `SelectionHolder`, `SelectionWatcher` | See below on this page |
| `media/` | Local audio/video files handed to the page's `<video>` / `<audio>` through the virtual host, with MP4 and Matroska container parsing | `MediaService`, `MediaFile`, `ContainerInfo` | See below on this page |

> The top level also contains `main.cpp` (component version declaration), `ComponentDescription.inc` (the text shown under Preferences → Components), `pch.h/.cpp` (precompiled header), and `version.h` (version number).

---

## Core Mechanisms at a Glance

### 1. The Three-Stage Bridge (`api/BridgeCore`)

- **Declaration and registration**: methods and events are declared in `api/schema/<ns>.ts`, which generates the C++ params structs and parsers; each `XxxApi.cpp` exposes `RegisterXxxApi()`, which registers handlers via `api::RegisterApi("namespace.method", handler)`. The validation level of path parameters is part of the declaration too.
- **Dispatch**: `HandleMessage()` parses the frontend `postMessage` and looks up the handler by `method`.
- **Response/Event**: a handler returns a result → `SendResponse()`; change notifications go through the `api::emit::` helpers in `api/EventEmit.h`, to one page or to every window as the declaration says.

### 2. Naming Conventions (Never Confuse Them)

| Scenario | Format | Example |
|------|------|------|
| invoke method | dot | `playback.play` |
| C++ → JS event | colon | `playback:trackChanged` |

### 3. Dual-Mode Bridge

- **Singleton mode**: the standalone main window uses `BridgeCore::GetInstance()` (backward compatible).
- **Per-instance mode**: DUI/CUI panels each own their own `BridgeCore`, registered and routed by HWND through `WebViewContext`.

### 4. Six Path-Security Levels

`SecurityLevel` (`None / Read / Write / MediaRead / MediaWrite / FileWrite`) maps to different validation functions in `domain/PathSecurity.h`, checked uniformly as declared before bridge dispatch.

### 5. Plugin Extension

External foobar2000 components register their own namespaced APIs via the exported `GetPluginRegistry()` → `RegisterPlugin` + `RegisterExternalApi`, protected by `RESERVED_NAMESPACES` isolation.

---

## Supporting Modules

These modules do not have their own README; their responsibilities are as follows:

- **`webview/`** — the low-level WebView2 host. `WebViewHost` wraps WebView2 creation, navigation, `ExecuteScript`/`PostMessage`, virtual host mapping, DWM transparency (Visual Hosting + DirectComposition), and cursor/mouse input forwarding; its members are split by responsibility across several `WebViewHost*.cpp` files. `WebViewEnvironment` warms up at startup and shares a single WebView2 environment to speed up subsequent creation. The scripts injected into every page live in `BridgeBootstrapScript.inl` (the `window.fb2k` bridge), `StartupProbeScript.inl` (startup readiness signals) and `SdkBridgeScript.inl` (the `window.fb` SDK). `dnd/` holds the file drop target and its origin check, and `SharedPcmBuffer` hands PCM data to the page through a SharedBuffer.
- **`media/`** — the backend for pages that play local audio and video. `MediaService` issues token URLs under `https://foo-ui-webview2.local/fb2k-media/` for files the page may read, and `WebViewHostMedia.cpp` answers Range requests with the file content; `ContainerInfo` with `Mp4Container` and `MatroskaContainer` reads container information, and `MediaIoQueue` does the reading on a background thread. It backs the `media.*` API (experimental).
- **`interfaces/`** — the service abstraction layer. `IPlaybackService` / `IPlaylistService` abstract the SDK subset that API handlers depend on into interfaces; production uses the `Fb2kPlaybackService` / `Fb2kPlaylistService` implementations in `api/adapters/`, which can be swapped for mocks in unit tests (see service injection in `api/`).
- **`selection/`** — selection tracking. `SelectionHolder` wraps `ui_selection_holder`, acquiring it when a panel gains focus and releasing it when focus is lost; `SelectionWatcher` listens for global selection changes and throttles broadcasting `selection:changed` to each panel.
- **`prefs/`** — the preferences pages. `PreferencesPage` registers Preferences → Display → WebView2 UI (the overview page) and owns the cfg_vars behind the `webview_prefs::*` accessors: the active template and the template folders under `profile/webview-ui/`, window, tray and taskbar switches, preheat, default zoom, backdrop effect, and the CDP port. The overview page's template commands are in `PreferencesTemplateActions.cpp`, and the "API and services..." inventory window is in `ApiInventoryWindow.cpp`. `PreferencesPageBase` is the shared page skeleton (container, fonts, layout, scrolling, draft and commit) behind the Window, Performance and Developer sub-pages. The header-only `PreferencesLayout`, `PreferencesLayoutBuilder`, `PreferencesDraft`, `PreferencesFields`, `PreferencesCdpPort`, `PreferencesDevServerUrl`, `PreferencesTemplateName` and `PreferencesZoom` hold the pure logic that the unit tests include directly. Configuration GUIDs must never change.
- **`ui/`** — the entry points foobar2000 calls into. `UserInterface` implements `WebViewUI : user_interface` (`init`/`shutdown`/`activate`/`hide`) and creates and holds `MainWindow`; `BackgroundService` keeps a WebView2 window running in the background when another UI is active, so the APIs stay reachable; `WebViewMainMenu.cpp` registers the View → WebView2 UI → Show/Hide Window command; `AppQuit` is the one place `app:beforeQuit` is emitted from.
- **`settings/`** — component settings storage. `WindowStateConfig` persists the main window position and the background window visibility (`window_config::*`); `AdvancedConfig` holds the Advanced-preference branch and its checkboxes, the settings moved out of Advanced into the preferences sub-pages together with their one-time migration, and the `security_config::*` accessors declared in `SecurityConfig.h`; `AdvconfigI18n.h` provides the bilingual Advanced checkbox factory. Configuration GUIDs must never change, or users lose their stored values.
- **`domain/`** — domain logic that may include only `settings/`, `utils/`, `interfaces/`, the foobar2000 SDK, and the standard library; it never includes `api/`, `core/`, `window/`, `webview/`, `ui/`, `prefs/`, or `panels/`. `PathSecurity` validates paths in dynamic trust mode (system-drive whitelist, protected-directory blacklist, trust for files in the library or a playlist) and backs the `SecurityLevel` checks in `api/BridgeCore`; the class is declared in `PathSecurity.h` and implemented in `PathSecurity.cpp`. `library/` holds the library-side services behind `api/LibraryApi`: `LibraryTreeIndex` (singleton) infers the real media roots by comparing path tails against `library_manager::get_relative_path` and serves `library.getRoots` / `library.browseTree`, with its full build in `LibraryTreeBuilder.cpp` and the path helpers it shares in `LibraryPathAlgebra`; `LibraryCache` (singleton) tells when the library changed: `Invalidate()` moves a generation counter that `api/LibraryApi` checks before reusing the results it keeps, and the cache hit/miss statistics live here. `callbacks/LibraryCallback` invalidates both when the library changes.
- **`utils/`** — the cross-cutting utility set. `PathExpansion` (path variable expansion), `Encoding` (UTF-8 / UTF-16 conversion), `Base64`, `GuidUtils`, `I18n` (the `TRU` bilingual macro), `IconLoader`, `ImageUtils`, `SubsongUtils`, `ArtworkCacheKey`, `WindowUtils`, and more. Path security validation lives in `domain/PathSecurity`; the helpers it delegates to (`PathCanonicalForm`, `PathProtocolScheme`, `PathTraversalSegments`, `NetworkShareResolver`) stay here.

---

## Dependencies

```
main.cpp / ui/UserInterface
        │
        ▼
  window/  ──┐
  panels/  ──┤── inherits ──> core/WebViewPanel ── holds ──> webview/WebViewHost
             │                      │
             │                      ├── registers ──> api/ (BridgeCore + *Api)
             │                      └── initializes ─> callbacks/ + selection/
             ▼
        core/WebViewContext (multi-instance routing)
```

- `window/` and `panels/` both inherit `core/WebViewPanel` to reuse WebView2 + bridge capabilities.
- `api/` is registered and shared by all windows/panels; `callbacks/` feeds SDK events back into the event channel of `api/`.
- `utils/` and `interfaces/` are low-level dependencies, widely referenced by `api/`, `window/`, and `core/`.
- `domain/` is called from `api/`, `callbacks/`, and `webview/`.

---

## Build

All source is compiled through the build script at the repository root; never hardcode the MSBuild path or invoke `msbuild` directly:

```powershell
.\build.ps1 -Config Release -Platform x64
```

For more conventions, see the repository root [README.md](../README.md).

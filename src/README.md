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
PlaybackCallback (src/callbacks) ──> BridgeCore::EmitEvent / WebViewContext::BroadcastEvent
      └─ postMessage ─────> fb2k.on('playback:trackChanged')
```

- The window & host layer (`window/` + `core/` + `webview/`) embeds WebView2 into foobar2000 windows/panels and manages their lifecycle.
- The bridge & API layer (`api/`) receives frontend messages, routes them to specific handlers, and sends back responses and events.
- The event callback layer (`callbacks/` + `selection/`) listens for foobar2000 SDK changes and broadcasts them to the frontend.
- The utility & abstraction layer (`utils/` + `interfaces/`) provides cross-cutting capabilities such as path security, encoding conversion, and service injection.

---

## Module Landscape

> P1: core modules, each with its own README. P2: supporting modules, whose responsibilities are described in the "Supporting Modules (P2)" section on this page.

| Module | Files | Responsibility | Key Classes/Files | Docs |
|------|-------|------|------------|------|
| `api/` | 74 | C++ ↔ JS bridge + 30+ namespaced APIs + plugin extension | `BridgeCore`, `PluginRegistry`, `*Api` | [api/README.md](api/README.md) |
| `window/` | 31 | Window system: main window/popups, Chrome (Mica/title bar), tray/taskbar | `MainWindow`, `WindowManager`, `ChromeController`, `WindowChrome*` | [window/README.md](window/README.md) |
| `callbacks/` | 18 | foobar2000 SDK events → BridgeCore event broadcasting | 9 callbacks such as `InitPlaybackCallbacks` | [callbacks/README.md](callbacks/README.md) |
| `core/` | 17 | WebView lifecycle, UI entry, library cache, JIT queue | `UserInterface`, `WebViewPanel`, `WebViewContext`, `LibraryCache` | [core/README.md](core/README.md) |
| `utils/` | 13 | Utilities: path security / Base64 / icons / i18n, etc. | `PathSecurity`, `PathExpansion`, `Base64`, `I18n` | See P2 on this page |
| `panels/` | 8 | DUI/CUI panel integration & config persistence | `WebViewDuiElement`, `WebViewCuiPanel`, `PanelConfig` | [panels/README.md](panels/README.md) |
| `webview/` | 5 | WebView2 host & shared environment, injected scripts | `WebViewHost`, `WebViewEnvironment` | See P2 on this page |
| `interfaces/` | 4 | Service abstractions (dependency injection / testability) | `IPlaybackService`, `IPlaylistService` | See P2 on this page |
| `selection/` | 4 | Selection tracking | `SelectionHolder`, `SelectionWatcher` | See P2 on this page |

> The top level also contains `main.cpp` (component entry and cfg_var configuration), `pch.h/.cpp` (precompiled header), and `version.h` (version number).

---

## Core Mechanisms at a Glance

### 1. The Three-Stage Bridge (`api/BridgeCore`)

- **Registration**: each `XxxApi.cpp` exposes `RegisterXxxApi()`, which registers handlers internally via `RegisterApi("namespace.method", handler)`; methods with path parameters attach a `PathSecuritySpec` through a decorator overload.
- **Dispatch**: `HandleMessage()` parses the frontend `postMessage` and looks up the handler by `method`.
- **Response/Event**: a handler returns JSON → `SendResponse()`; change notifications go through `EmitEvent()` (single instance) / `WebViewContext::BroadcastEvent()` (all windows).

### 2. Naming Conventions (Never Confuse Them)

| Scenario | Format | Example |
|------|------|------|
| invoke method | dot | `playback.play` |
| C++ → JS event | colon | `playback:trackChanged` |

### 3. Dual-Mode Bridge

- **Singleton mode**: the standalone main window uses `BridgeCore::GetInstance()` (backward compatible).
- **Per-instance mode**: DUI/CUI panels each own their own `BridgeCore`, registered and routed by HWND through `WebViewContext`.

### 4. Five Path-Security Levels

`SecurityLevel` (`None / Read / Write / MediaRead / MediaWrite`) maps to different validation functions in `utils/PathSecurity.h`, intercepted uniformly by the decorator before bridge dispatch.

### 5. Plugin Extension

External foobar2000 components register their own namespaced APIs via the exported `GetPluginRegistry()` → `RegisterPlugin` + `RegisterExternalApi`, protected by `RESERVED_NAMESPACES` isolation.

---

## Supporting Modules (P2)

These modules do not have their own README; their responsibilities are as follows:

- **`webview/`** — the low-level WebView2 host. `WebViewHost` wraps WebView2 creation, navigation, `ExecuteScript`/`PostMessage`, virtual host mapping, DWM transparency (Visual Hosting + DirectComposition), and cursor/mouse input forwarding; `WebViewEnvironment` warms up at startup and shares a single WebView2 environment to speed up subsequent creation; `SdkBridgeScript.inl` is the bridge script injected into the page.
- **`interfaces/`** — the service abstraction layer. `IPlaybackService` / `IPlaylistService` abstract the SDK subset that API handlers depend on into interfaces; production uses the `Fb2kPlaybackService` / `Fb2kPlaylistService` implementations, which can be swapped for mocks in unit tests (see service injection in `api/`).
- **`selection/`** — selection tracking. `SelectionHolder` wraps `ui_selection_holder`, acquiring it when a panel gains focus and releasing it when focus is lost; `SelectionWatcher` listens for global selection changes and throttles broadcasting `selection:changed` to each panel.
- **`utils/`** — the cross-cutting utility set. `PathSecurity` (path security validation, dynamic trust mode), `PathExpansion` (path variable expansion), `Base64`, `GuidUtils`, `I18n` (the `TRU` bilingual macro), `IconLoader`, `ImageUtils`, `SubsongUtils`, `ArtworkCacheKey`, `WindowUtils`, and more.

---

## Dependencies

```
main.cpp / UserInterface
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

---

## Build

All source is compiled through the build script at the repository root; never hardcode the MSBuild path or invoke `msbuild` directly:

```powershell
.\build.ps1 -Config Release -Platform x64
```

For more conventions, see the repository root [README.md](../README.md), `AGENTS.md`, and `CLAUDE.md`.

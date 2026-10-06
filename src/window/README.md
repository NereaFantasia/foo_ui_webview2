English | [中文](./README.zh-CN.md)

# src/window/ — Window System & Chrome Subsystem

`window/` is responsible for carrying WebView2 content into "real Windows windows": the standalone main window, multiple popup windows, and their borders/backgrounds/title bars (Chrome), startup presentation timing, and tray/taskbar integration. It solves every problem at the native window layer — client-area extended title bars, Mica/Acrylic backgrounds, framelessness, rounded corners, activation and always-on-top guarding, Snap Layout, and more.

---

## Overview

Both the main window and popups inherit from `core/WebViewPanel` (gaining WebView2 + bridge capabilities) and then layer their own native window logic on top. Both also implement the unified abstract interface `WindowShellBase`, offering a consistent "shell-state observation + Chrome commands" entry point; Chrome's resolve/apply is consolidated into one set of `WindowChrome*` structures, avoiding scattered direct DWM writes.

```
WebViewPanel (core/)
     ├── MainWindow  : WebViewPanel + WindowShellBase   (standalone main window, singleton semantics)
     └── PopupWindow : WebViewPanel + WindowShellBase   (multiple popups, managed by WindowManager)
```

---

## Key Files / Classes

### Window Family

| File | Responsibility |
|------|------|
| `MainWindow.h/.cpp` | Main window: creation and teardown, the message procedure (`WndProc`/`HandleMessage`), resizing, WebView callbacks, and the startup reveal state machine |
| `MainWindowCaption.cpp` | Client-area extended title bar (`WM_NCCALCSIZE`/`WM_NCHITTEST`), drag and no-drag regions, caption button geometry, system menu, DPI changes, min/max size |
| `MainWindowActivation.cpp` | `WM_ACTIVATE`/`WM_ACTIVATEAPP` handling, overlay mis-activation defense, expected always-on-top guarding |
| `MainWindowBackground.cpp` | Background suspension to save memory: occlusion detection, WebView suspend and restore, page health probe after restore, rebuild after the render process dies |
| `MainWindowPlacement.cpp` | Saving and restoring the window position, and default size constraints from the initial DPI |
| `MainWindowMaximizeButton.cpp` / `MaximizeButtonRegion.h/.cpp` | The page-drawn maximize button: answers hit tests over the rectangle the page reports with `HTMAXBUTTON`, so Windows 11 offers Snap layouts, and passes the mouse input on the button back to the page |
| `MainWindowShell.cpp` | `WindowShellBase` implementation: capabilities, shell snapshot, chrome patch commands, full-screen flag |
| `MainWindowDwm.cpp` | DWM-related implementation of the main window (a split unit for Mica/frame extension, etc.) |
| `MainWindowMenu.cpp` | Main window menu commands (open file, preferences, DevTools, main-menu lookup, etc.) |
| `MainWindowDiagnostics.cpp` | Diagnostic evidence: interactive resize, surface and WebView lifecycle logs, runtime DOM probes |
| `MainWindowInternal.h/.cpp` | Non-member helpers shared by the `MainWindow*.cpp` units: DWM call wrappers, dark mode, evidence logging, formatting |
| `PopupWindow.h/.cpp` | Popup window: `CreateParams` creation parameters, profiles (standard/miniPlayer/desktopLyrics), `beforeClose` async close, mouse pass-through + interactive hot zones, overlay custom dragging and activation-chain elimination |
| `WindowManager.h/.cpp` | Singleton: popup creation/destruction/query, cross-window directed/broadcast messaging, activation-handoff sink, panel-mode reference counting, `MAX_POPUPS=8` limit |
| `WindowShellBase.h/.cpp` | Unified window-shell abstraction: capability descriptor `WindowShellCapabilities`, observation snapshot `WindowShellSnapshot`, Chrome commands, and full-screen lifecycle interface |
| `WindowTargetPolicy.h/.cpp` | Input classification for `WindowTargetResolver`; pure logic, unit-testable |
| `WindowGeometryMath.h/.cpp` / `WindowDpiProbe.h/.cpp` | Pure DPI conversion and size-constraint logic; probes the target monitor's DPI before `CreateWindowExW` |
| `BackgroundSuspendPolicy.h` | The background-suspend decision: combinations of locked, covered, minimized and similar conditions |
| `WindowTargetResolver.h/.cpp` | Unified target resolution: `ResolveForMutation` (must fail if not found) / `ResolveForObservation` (may fall back to main), replacing scattered caller-HWND lookups |
| `StartupPresentationCoordinator.h/.cpp` | Startup presentation decision-maker: based on navigation completion / ready signal / chrome readiness / fallback timer, decides when to commit reveal (to avoid white-screen flicker) |

### Chrome Subsystem (Layered)

Chrome's "resolve" and "apply" are strictly layered; there is only one schema, and introducing a second raw/compat/resolved set is forbidden:

| File | Layer | Responsibility |
|------|------|------|
| `WindowChromeState.h` | Data | All chrome data structures: `WindowKind`, `WindowNativeFrameStrategy`, backdrop strategy, base/compat/standard/derived states, `WindowChromeResolverSnapshot`, `ResolvedWindowChromeState` |
| `WindowChromeResolver.h/.cpp` | Resolve | `Resolve()`s a snapshot into a `ResolvedWindowChromeState`; `NormalizeBackdropEffect` normalizes the backdrop-effect vocabulary |
| `WindowChromeApplier.h/.cpp` | Apply | Applies the resolved state to the native window through `WindowChromeApplyHooks` (a set of callbacks provided by the window); `RefreshNativeFrame` refreshes the native frame |
| `ChromeController.h/.cpp` | Entry | Wraps Resolver + Applier, providing the unified entry points `Resolve` / `Apply` / `ResolveAndApply` / `RefreshNativeFrame` |
| `WindowChromeTrace.h` | Diagnostics | `[WindowChromeTrace]` diagnostic logging (off by default, enabled by setting the environment variable `FOO_UI_WEBVIEW2_WINDOW_TRACE=1`) |
| `WindowBehaviorTrace.h/.cpp` | Diagnostics | Records the window, DWM and WebView side effects of this module so two builds can be compared; off by default. Unlike `FOO_UI_WEBVIEW2_WINDOW_TRACE` it only records and turns on no probes that change behavior. Enabled by `webview_behavior_trace.on` in the profile directory or `FOO_UI_WEBVIEW2_BEHAVIOR_TRACE=1` |

### Tray / Taskbar / Menu Overlay Surface

| File | Responsibility |
|------|------|
| `TrayIcon.h/.cpp` | System tray singleton: icon/balloon, three-zone context menu (top/playback/bottom), dual backends Native (`TrackPopupMenu`) and WebView (self-drawn), rich menu items (nowplaying/rating/slider/segmented), minimize/close to tray, re-registration after Explorer restarts |
| `TaskbarIntegration.h/.cpp` | Taskbar singleton (`ITaskbarList3`): thumbnail toolbar buttons, progress indication, overlay icons, flashing; updates default buttons with playback state |
| `TaskbarTrayContracts.h` | Shared contract definitions for tray/taskbar |
| `TaskbarProgressPolicy.h` | Mapping from playback state to the taskbar progress bar (pure logic) |
| `MenuOverlayHost.h/.cpp` | Self-drawn menu overlay-surface host (singleton): holds its own `PopupWindow` (not entered into `WindowManager`, not counted toward `MAX_POPUPS`), pools and reuses WebView, supports owner-mode event routing, content-sized window, and exit animation |

| `MenuActionContract.h` / `MenuTokenTable.h` / `MenuResourceLimits.h` / `MenuOverlayGeometry.h` | Header-only pieces shared by the tray and self-drawn menus: the internal routing identity of menu actions, the per-show opaque token table, resource caps and overlay geometry; unit tests include them directly |
| `menu-overlay/` and `MenuOverlayPage.inl` | HTML/CSS/JS sources of the self-drawn menu page; `MenuOverlayPage.inl` is generated from them and not edited by hand |

> `TestPageHtml.inl` is the embedded test-page HTML, shown when no template can be loaded.

---

## How It Works / Data Flow

### Chrome Resolve–Apply Pipeline

```
window state change (activation / preference change / fullscreen toggle / runtime setBackdropPolicy)
      │
      ▼
window builds snapshot  BuildChromeBaseState / StandardState / DerivedState
      │                       (+ compatibility overrides)
      ▼
ChromeController::ResolveAndApply
      ├─ WindowChromeResolver::Resolve(snapshot) ─► ResolvedWindowChromeState
      └─ WindowChromeApplier::Apply(hooks, request)
              ├─ applyFrameless()              (frameless)
              ├─ applyNativeFrameStrategy()    (Standard/Captionless/Popup/Fullscreen)
              ├─ applyCornerPreference()       (rounded corners)
              ├─ apply DWM backdrop (Mica/MicaAlt/Acrylic/none)
              └─ broadcastBackdropStateChanged() (notify frontend)
```

`WindowChromeApplyHooks` is the decoupling point between the Applier and the concrete window: the window hands "how to go frameless / how to set the frame strategy / how to apply rounded corners / what to do after the backdrop is applied" to the Applier as `std::function` callbacks, so the Resolver/Applier do not depend directly on `MainWindow`/`PopupWindow`.

### Client-Area Extended Title Bar (Mica-Compatible)

The main window is frameless by default: `WM_NCCALCSIZE` removes the non-client area so the WebView fills everything; `WM_NCHITTEST`, combined with the drag/non-drag regions reported by the frontend, returns hit codes such as `HTCAPTION`, achieving "frontend self-drawn title bar + system Snap Layout / double-click maximize". The background effect uses DWM `DWMSBT_*` system backdrop materials, combined with frame extension to let Mica/Acrylic show through.

### Startup Reveal Timing

To avoid "black/white screen first, then content", the main window starts hidden; `StartupPresentationCoordinator` aggregates three kinds of signals — navigation completion, page ready handshake, and chrome readiness — to decide when to commit reveal, and arms a fallback timer (1500ms by default) to prevent it from never showing if a signal is missing.

### Multiple Windows & Cross-Window Messaging

`WindowManager` uniformly assigns window IDs, creates/destroys popups, and provides `SendWindowMessage` (directed) / `BroadcastMessage` (broadcast). The activation-fallback problem of overlay-type popups (such as desktop lyrics) is handled by the "activation-handoff sink + overlay interaction signal" mechanism, to avoid mistakenly pushing the foobar2000 main window above external applications.

---

## Dependencies

- **Depends on**: `core/WebViewPanel` (base class), `core/WebViewContext` (event broadcasting / instance lookup), `api/` (window/tray/taskbar-related APIs are exposed to the frontend through it), `utils/`.
- **Depended on by**: `ui/UserInterface` creates and holds `MainWindow`; `window/`, `api/WindowApi`, `api/TrayApi`, `api/TaskbarApi`, and `api/MenuApi` realize native window behavior through this module; `callbacks/PlaybackCallback` updates taskbar buttons through `TaskbarIntegration`.

---

## Extension Guide

- **Add a window Chrome capability**: only extend the schema in `WindowChromeState.h`, then fill in the `Resolver` (resolution rules) and `Applier` (apply actions + corresponding hooks); the window side implements the new `WindowChromeApplyHooks` callback. Do not scatter DWM calls inside the window.
- **Add a popup form**: prefer combining the profile/behavior/backdropPolicy of `PopupWindow::CreateParams` rather than adding boolean branches; overlay types must use the existing noActivate / clickThrough / custom-drag paths.
- **Add a rich tray menu item**: extend `TrayMenuItem::type` and the corresponding rendering; note that the Native backend degrades (text only), while only the WebView backend renders rich controls.
- **Debug window behavior**: set the environment variable `FOO_UI_WEBVIEW2_WINDOW_TRACE=1` to enable diagnostic logging such as `WindowChromeTrace` / ActivationEvidence.

---

See also: the repository root [README.md](../../README.md).

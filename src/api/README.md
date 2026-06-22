English | [中文](./README.zh-CN.md)

# src/api/ — C++ ↔ JS Bridge & Namespaced APIs

`api/` is the "nerve center" of the component: every frontend `fb2k.invoke()` call is received, routed, executed, and returned here, and every event pushed to the frontend is emitted from here too. It also hosts 30+ namespaces of built-in APIs, the unified error envelope, the path-security decorator, and the plugin extension system for external foobar2000 components.

---

## Overview

There is only one physical channel between the frontend and C++: WebView2's `postMessage`. Over this channel, `BridgeCore` implements a streamlined JSON-RPC-style protocol:

- The frontend sends `{ id, method, params }`, and `BridgeCore` looks up and executes the corresponding handler by `method`.
- The handler synchronously returns a JSON result, and `BridgeCore` sends the response back keyed by `id`.
- When foobar2000 state changes, `callbacks/` proactively pushes events to the frontend via `EmitEvent` / `BroadcastEvent`.

In the architecture, `api/` sits between "windows/panels" and the "foobar2000 SDK"; it is the only layer permitted to translate frontend requests into SDK calls.

---

## Key Files / Classes

| File | Responsibility |
|------|------|
| `BridgeCore.h/.cpp` | Bridge core: API registration, message dispatch, response/event sending, path-security validation, dual singleton + per-instance modes |
| `ErrorEnvelope.h` | Unified failure envelope: `ApiErrorCode` machine-readable error codes + `ApiEnvelope::MakeError/MakeFailureEvent` + failure sampling hooks |
| `CallerContext.h/.cpp` | Extracts the caller's bridge from `_callerHwnd` and routes events back to the WebView instance that initiated the call |
| `PluginRegistry.h/.cpp` | External plugin API registry: namespace isolation, API discovery, `system.*` discovery APIs, dynamic-load notifications |
| `ApiConstants.h` | Shared constants for the API layer |
| `PlaybackApi.*` | `playback.*` + `test.*` (playback control, volume, playback order, play-by-path) |
| `PlaylistApi.*` | `playlist.*` (playlist CRUD, selection, move, undo/redo) |
| `LibraryApi.*` | `library.*` (library search, enumeration, root/directory-tree browsing, aggregate statistics) |
| `WindowApi.*` | `window.*` (window creation/control, Chrome, cross-window messaging) |
| `ConfigApi.*` | `config.*` (output devices, DSP presets, advconfig, preferences page, component info) |
| `ArtworkApi.*` | `artwork.*` (cover art retrieval, Base64) |
| `AudioApi.*` | `audio.*` (real-time spectrum subscription, whole-track waveform, BPM analysis, channel mode) |
| `FileApi.*` | `file.*` (file read/write, directory listing, copy/move, with path security) |
| `MetadataApi.*` | `metadata.*` (tag read/write) |
| `LyricsApi.*` | `lyrics.*` (lyrics) |
| `QueueApi.*` | `queue.*` + JIT queue (just-in-time streaming queue, working with `core/QueueManager`) |
| `SelectionApi.*` | `selection.*` (selection set/query, working with `selection/`) |
| `PortApi.*` / `PortHub.*` | `port.*` (named channels), `event.*` (semantic events), `state.*` (cross-window shared state) |
| `MenuApi.*` | `menu.*` (self-drawn menus, context menus) |
| `TrayApi.*` / `TaskbarApi.*` | `tray.*` / `taskbar.*` (system tray and taskbar, working with `window/`) |
| Other `*Api.*` | `dialog` `clipboard` `shell` `http` `keyboard` `ui` `cursor` `console` `dnd` `replaygain` `playcount` `titleformat` `misc` `output` `dsp` `discovery`, etc. |

> The authoritative registration list is `WebViewPanel::RegisterAllApis()` in `core/WebViewPanel.cpp`; the comment at the top of each `XxxApi.h` lists the full names of the APIs registered in that file, which is the first-hand reference for verifying whether an API exists.

---

## How It Works / Data Flow

### The Three-Stage Bridge

```
① Registration (once at startup)
   WebViewPanel::RegisterAllApis()
        └─ RegisterPlaybackApi()
              └─ bridge.RegisterApi("playback.play", PlaybackPlay);
                 bridge.RegisterApi("playback.playPath", PlaybackPlayPath,
                                    {{"path", SecurityLevel::MediaRead}});  // with path-security decorator

② Dispatch (every call)
   frontend fb2k.invoke("playback.play", {...})
        └─ postMessage → BridgeCore::HandleMessage(message, responseTarget, callerHwnd)
              ├─ parse { id, method, params }
              ├─ (decorator) validate path params against PathSecuritySpec
              ├─ handlers_["playback.play"](params)
              └─ SendResponse(id, result)   // or SendError(id, code, msg)

③ Event return (async)
   callbacks/PlaybackCallback → BridgeCore::EmitEvent("playback:trackChanged", data)
        └─ postMessage → fb2k.on("playback:trackChanged", cb)
```

### Unified Error Envelope

Failures inside a handler uniformly build a synchronous error via `ApiEnvelope::MakeError(message, code)` (`{ success:false, error, code }`); failures of asynchronous tasks push an event via `MakeFailureEvent`. Framework-level errors (method not found, invalid JSON, handler throwing an exception) are produced by `BridgeCore::SendError`, with error codes drawn from `ApiErrorCode` (such as `METHOD_NOT_FOUND`, `INVALID_PARAMS`, `PERMISSION_DENIED`).

---

## Path Security: the Five `SecurityLevel` Levels

APIs with path parameters declare a `PathSecuritySpec` validation spec (parameter key, level, whether it is an array, nested key, whether to skip invalid entries) via the decorator overload of `RegisterApi`. Before dispatch, `ValidatePathParam` intercepts uniformly; on validation failure it returns `PERMISSION_DENIED` directly and the handler is never invoked.

| SecurityLevel | Corresponding `utils/PathSecurity.h` function | Semantics |
|---------------|-------------------------------|------|
| `None` | — | No path parameter / no validation |
| `Read` | `ValidatePath` | Filesystem read-only (non-system drives allowed, system drives go through allow/deny lists) |
| `Write` | `ValidateWritePath` | Strict write allowlist (profile / temp directories only) |
| `MediaRead` | `ValidateMediaAccess` | Read + library/playlist context trust |
| `MediaWrite` | `ValidateMediaWriteAccess` | Write + media context trust, but arbitrary paths on system drives are not allowed |

Declaration example (from `FileApi.cpp`, where each parameter specifies its own level):

```cpp
bridge.RegisterApi("file.copy", FileCopy, {
    {"source",      SecurityLevel::Read},
    {"destination", SecurityLevel::MediaWrite}
});

// array parameter: the 3rd field true marks it as an array, the 5th true means skip invalid entries one by one
bridge.RegisterApi("playback.playPaths", PlaybackPlayPaths,
    {{"paths", SecurityLevel::MediaRead, true, "", true}});
```

---

## Extension Guide: How to Add a New API

Using `playback.fooBar` as an example (a built-in API):

1. **Three-point verification first** (mandatory, see `AGENTS.md` §2.5): confirm the SDK has the corresponding capability, that parameter keys align with `params.value("key", ...)` inside the handler, and that event names use the colon format.
2. **Implement the handler**: in `PlaybackApi.cpp`, write `static json PlaybackFooBar(const json& params)`, read parameters from `params`, call the SDK / service interface, and return JSON; on failure use `ApiEnvelope::MakeError`.
3. **Register**: inside that file's `RegisterPlaybackApi()`, add `bridge.RegisterApi("playback.fooBar", PlaybackFooBar);`; if it has path parameters, attach a `PathSecuritySpec` via the decorator overload.
4. **Update the header comment**: add `playback.fooBar` to the "Registered APIs" list at the top of `PlaybackApi.h`, keeping the documentation consistent with the code.
5. **(If emitting events)** push via `EmitEvent("playback:fooChanged", data)` in the corresponding `callbacks/`, and make sure invoke uses dot while events use colon.
6. **Sync the SDK and docs**: API changes must be synced to `sdk/` (bridge.js / types) and `docs/` (see the development workflow in `CLAUDE.md`).
7. **Build verification**: `\.\build.ps1 -Config Release -Platform x64`.

> If a handler needs to send an asynchronous event back to "the window that initiated the call", use `CallerContext::FromParams(params)` to obtain the caller's bridge and then `EmitEvent`, to avoid mistakenly sending it to other instances.

---

## Service Injection & Unit Testing

Playback/playlist handlers do not call the SDK directly but go through the service interfaces in `interfaces/`, which makes offline unit testing easier:

```cpp
// production uses Fb2kPlaybackService by default; inject a mock in tests
void SetPlaybackService(IPlaybackService* service);  // pass nullptr to reset to default
IPlaybackService* GetPlaybackService();
```

In tests, first call `SetPlaybackService(&mock)`, invoke the handler and assert the mock's behavior, then call `SetPlaybackService(nullptr)` to reset. `PlaylistApi` provides the same `SetPlaylistService/GetPlaylistService`.

---

## Singleton vs Per-Instance Dual Modes

`BridgeCore` supports two lifecycles simultaneously:

- **Singleton mode**: `BridgeCore::GetInstance()`, used for the standalone main window, backward compatible. `RegisterXxxApi()` registers to the singleton by default.
- **Per-instance mode**: DUI/CUI panels each `new BridgeCore`, registered by HWND via `WebViewContext::RegisterInstance(hwnd, host, bridge, windowId, panel)`. `HandleMessage` takes `responseTarget` / `callerHwnd` parameters to ensure responses and events return to the correct WebView.

Internally, `BridgeCore` protects `handlers_` / `securitySpecs_` with `mutex_`, making registration and dispatch thread-safe.

---

## PluginRegistry — External Plugin Extension

Other foobar2000 components can expose their own APIs for the frontend to call:

```cpp
#include "api/PluginRegistry.h"

void RegisterMyApis() {
    auto& registry = GetPluginRegistry();   // accessor exported across DLLs
    registry.RegisterPlugin("my_plugin", "My Plugin", "1.0.0", "Author", "Description");
    registry.RegisterExternalApi("my_plugin", "doSomething",
        [](const json& params) -> json {
            return {{"result", "done"}};
        },
        "Perform an operation"
    );
}
```

```javascript
const result = await fb2k.invoke('my_plugin.doSomething', { param1: 'value' });
```

- Namespace isolation: `RESERVED_NAMESPACES` prevents external plugins from impersonating built-in namespaces.
- API discovery: the `system.*` family (`listAvailableApis` / `getApisByNamespace` / `searchApis` / `getApiStats` / `getRegisteredPlugins` / `isPluginRegistered`) is registered by `PluginRegistry::Initialize()`, allowing the frontend to enumerate all APIs.
- Dynamic notifications: plugin registration/unregistration emits `plugin:*` / `api:*` events.

---

## Dependencies

- **Depends on**: `interfaces/` (service abstraction), `utils/PathSecurity` (path validation), `core/WebViewContext` (multi-instance routing and broadcasting), `webview/WebViewHost` (the actual `postMessage`).
- **Depended on by**: `core/WebViewPanel` uniformly calls `RegisterAllApis()`; `callbacks/` reuses this layer's event channel via `EmitEvent`/`BroadcastEvent`; `window/`, `panels/`, `selection/`, `window/TrayIcon`, `core/QueueManager`, and others all register with this layer or expose capabilities through it.

---

See also: the repository root [README.md](../../README.md) ("API Overview / Plugin Extension / Security Restrictions"), `docs/AI_API_REFERENCE.md`, and `sdk/`.

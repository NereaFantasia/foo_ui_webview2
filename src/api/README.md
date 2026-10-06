English | [中文](./README.zh-CN.md)

# src/api/ — C++ ↔ JS Bridge & Namespaced APIs

`api/` is the "nerve center" of the component: every frontend `fb2k.invoke()` call is received, routed, executed, and returned here, and every event pushed to the frontend is emitted from here too. It also hosts 40+ namespaces of built-in APIs, the unified error envelope, path-security validation, and the plugin extension system for external foobar2000 components.

---

## Overview

There is only one physical channel between the frontend and C++: WebView2's `postMessage`. Over this channel, `BridgeCore` implements a streamlined JSON-RPC-style protocol:

- The frontend sends `{ id, method, params }`, and `BridgeCore` looks up and executes the corresponding handler by `method`.
- The handler returns a result, and `BridgeCore` sends the response back keyed by `id`.
- When foobar2000 state changes, `callbacks/` proactively pushes events to the frontend through the `api::emit::` helpers in `EventEmit.h`.

In the architecture, `api/` sits between "windows/panels" and the "foobar2000 SDK"; it is the only layer permitted to translate frontend requests into SDK calls.

---

## Key Files / Classes

| File | Responsibility |
|------|------|
| `schema/*.ts` | Declarations of methods and events, the only hand-written contract; the C++ headers under `generated/` are generated from them (see [schema/README.md](schema/README.md)) |
| `TypedApi.h` | `api::RegisterApi` / `RegisterApiDeferred`: register a handler by its generated params type, checking the method name and result type at compile time |
| `EventEmit.h` | `api::emit::` helpers: emit events as declared, one helper per delivery mode |
| `BridgeCore.h/.cpp` | Bridge core: API registration, message dispatch, response/event sending, path-security validation, dual singleton + per-instance modes |
| `ErrorEnvelope.h` | Unified failure envelope: `ApiErrorCode` machine-readable error codes + `ApiEnvelope::MakeError/MakeFailureEvent` + failure sampling hooks |
| `CallerContext.h/.cpp` | Extracts the caller's bridge from `_callerHwnd` and routes events back to the WebView instance that initiated the call |
| `PluginRegistry.h/.cpp` | External plugin API registry: namespace isolation, API discovery, `system.*` discovery APIs, dynamic-load notifications |
| `ApiConstants.h` | Shared constants for the API layer |
| `PlaybackApi.*` | `playback.*` + `test.*` (playback control, volume, playback order, play-by-path) |
| `MediaApi.*` | `media.*` (hands local audio/video files to the page's `<video>` / `<audio>`, working with `media/`) |
| `PlaylistApi.*` | `playlist.*` (playlist CRUD, selection, move, undo/redo) |
| `LibraryApi.*` | `library.*` (library search, enumeration, root/directory-tree browsing, aggregate statistics) |
| `WindowApi.*` | `window.*` (window creation/control, Chrome, cross-window messaging) |
| `ConfigApi.*` | `config.*` (output devices, DSP presets, advconfig, preferences page, component info) |
| `WebviewApi.*` | `webview.*` (where the page was loaded from) |
| `ArtworkApi.*` | `artwork.*` (cover art retrieval, Base64) |
| `AudioApi.*` | `audio.*` (real-time spectrum subscription, whole-track waveform, BPM analysis, channel mode) |
| `FileApi.*` | `file.*` (file read/write, directory listing, copy/move, with path security) |
| `MetadataApi.*` | `metadata.*` + `rating.*` (tag read/write, ratings) |
| `LyricsApi.*` | `lyrics.*` (lyrics) |
| `QueueApi.*` | `queue.*` + `jitQueue.*` (just-in-time streaming queue, working with `core/QueueManager`) |
| `SelectionApi.*` | `selection.*` (selection set/query, working with `selection/`) |
| `PortApi.*` / `PortHub.*` | `port.*` (named channels), `event.*` (semantic events), `state.*` (cross-window shared state) |
| `MenuApi.*` | `menu.*` (self-drawn menus, context menus) |
| `TrayApi.*` / `TaskbarApi.*` | `tray.*` / `taskbar.*` (system tray and taskbar, working with `window/`) |
| Other `*Api.*` | `dialog` `clipboard` `shell` `http` `keyboard` `ui` `cursor` `console` (with `log.*`) `dnd` `replaygain` `playcount` `titleformat` `misc` `output` `dsp` `discovery`, etc.; `panel.*` and part of `system.*` live in `WindowApi` |

> Methods and events are defined by the declarations in `schema/<ns>.ts`; registration starts at `WebViewPanel::RegisterAllApis()` in `core/PanelBootstrap.cpp`. A commit check verifies that declarations and registrations match one to one.

---

## How It Works / Data Flow

### The Three-Stage Bridge

```
① Registration (once at startup)
   WebViewPanel::RegisterAllApis()
        └─ RegisterPlaybackApi()
              └─ api::RegisterApi("playback.play", PlaybackPlay);
                 api::RegisterApi("playback.playPath", PlaybackPlayPath);
                 // params struct, parser and path-security spec all come from the
                 // header generated from schema/playback.ts

② Dispatch (every call)
   frontend fb2k.invoke("playback.play", {...})
        └─ postMessage → BridgeCore::HandleMessage(message, responseTarget, callerHwnd)
              ├─ parse { id, method, params }
              ├─ validate path params against the declared @security
              ├─ the generated parser turns params into the params struct; an undeclared
              │  key or a wrong type returns INVALID_PARAMS
              ├─ handler(parsed) returns Result<R>
              └─ SendResponse(id, result)   // or SendError(id, code, msg)

③ Event return (async)
   callbacks/PlaybackCallback → api::emit::Broadcast<playback::events::TrackChanged>(row)
        └─ postMessage → fb2k.on("playback:trackChanged", cb)
```

A method registered with `RegisterApiDeferred` does not answer when its handler returns; the handler gets a `DeferredResponder` and answers later, for example once a worker thread finishes.

### Unified Error Envelope

A handler that fails returns `api::Fail(message, code[, extra])` (`ApiResult.h`), and the registration wrapper turns it into the failure envelope `{ success:false, error, code }` with `ApiEnvelope::MakeError`; failures of asynchronous tasks push an event via `MakeFailureEvent`. Framework-level errors (method not found, invalid JSON, handler throwing an exception) are produced by `BridgeCore::SendError`, with error codes drawn from `ApiErrorCode` (such as `METHOD_NOT_FOUND`, `INVALID_PARAMS`, `PERMISSION_DENIED`).

---

## Path Security: the Six `SecurityLevel` Levels

The validation level of a path parameter is part of its declaration: the `@security` tag on the parameter in `schema/<ns>.ts`. A path array can add `@skipInvalid`, and an array of objects names the member that holds the path with `@pathKey` (see [schema/README.md](schema/README.md)). The generator turns this into a `PathSecuritySpec` that is handed to `BridgeCore` at registration. Before dispatch, `ValidatePathParam` intercepts uniformly and the handler is never invoked on failure; a refused path returns `PERMISSION_DENIED`, a parameter of the wrong shape or type returns `INVALID_PARAMS`.

| SecurityLevel | Corresponding `domain/PathSecurity.h` function | Semantics |
|---------------|-------------------------------|------|
| `None` | — | No path parameter / no validation |
| `Read` | `ValidatePath` | Filesystem read-only (non-system drives allowed, system drives go through allow/deny lists) |
| `Write` | `ValidateWritePath` | Strict write allowlist (profile / temp directories only) |
| `MediaRead` | `ValidateMediaAccess` | Read + library/playlist context trust |
| `MediaWrite` | `ValidateMediaWriteAccess` | Write + media context trust, but arbitrary paths on system drives are not allowed |
| `FileWrite` | `ValidateFileWriteAccess` | Write whitelist -> watch folders -> non-system-drive pass-through -> library/playlist trust, in that order; carries the general `file.*` write endpoints |

Declaration example (from `schema/file.ts`, where each parameter specifies its own level):

```ts
interface CopyParams {
  /**
   * Source file path.
   * @security Read
   */
  source: string;
  /**
   * Destination file path.
   * @security FileWrite
   */
  destination: string;
}
```

---

## Extension Guide: How to Add a New API

The parameters, result, path-security levels and event payloads of a method are written only in `schema/<ns>.ts`. The C++ params structs and parsers (`generated/<Ns>Schema.h`), the SDK types and the documentation blocks are generated from it, and the generated files are never edited by hand. Using `playback.fooBar` as an example:

1. **Declare it first**: add `fooBar(params: FooBarParams): FooBarResult;` to the `Api` interface in `schema/playback.ts`, with interfaces for its parameters and result; give path parameters `@security`. See [schema/README.md](schema/README.md).
2. **Regenerate**: `node scripts/api-schema/generate.mjs --write`, then `npm --prefix sdk run gen:types`.
3. **Implement the handler**: in `PlaybackApi.cpp`, write `api::Result<playback::FooBarResult> PlaybackFooBar(const playback::FooBarParams& params)`, returning `api::Fail(message, ApiErrorCode::…)` on failure. Use the overload that also takes `const CallerContext&` when the handler needs to know the calling page.
4. **Register**: inside `RegisterPlaybackApi()`, add `api::RegisterApi("playback.fooBar", PlaybackFooBar);`. A method name that does not match the generated params type, or a handler that returns another method's result type, fails to compile.
5. **(If emitting events)** declare the event and its payload in the `Events` interface of `schema/playback.ts`, and emit it with an `api::emit::` helper from `EventEmit.h` rather than a literal event name.
6. **Build verification**: `.\build.ps1 -Config Release -Platform x64`. The build rewrites the C++ headers from the declarations first and runs the unit tests last.

> If a handler needs to send an asynchronous event back to "the window that initiated the call", take the `CallerContext` overload and send with `api::emit::EmitTo` / `ToCaller`, to avoid mistakenly sending it to other instances.

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

- **Depends on**: `interfaces/` (service abstraction), `domain/PathSecurity` (path validation), `core/WebViewContext` (multi-instance routing and broadcasting), `webview/WebViewHost` (the actual `postMessage`).
- **Depended on by**: `core/WebViewPanel` uniformly calls `RegisterAllApis()`; `callbacks/` reuses this layer's event channel through the `api::emit::` helpers; `window/`, `panels/`, `selection/`, `window/TrayIcon`, `core/QueueManager`, and others all register with this layer or expose capabilities through it.

---

See also: the repository root [README.md](../../README.md) ("API Overview / Plugin Extension / Security Restrictions"), the documentation site under `docs/vitepress/`, and `sdk/`.

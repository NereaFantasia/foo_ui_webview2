English | [中文](./README.zh-CN.md)

# foo_ui_webview2

[![Docs](https://img.shields.io/badge/docs-online-brightgreen)](https://nereafantasia.github.io/foo_ui_webview2/)
[![License](https://img.shields.io/badge/license-GPL--3.0%20%7C%20MIT-blue)](LICENSE)
[![npm: foo-webview-sdk](https://img.shields.io/npm/v/foo-webview-sdk?label=foo-webview-sdk)](https://www.npmjs.com/package/foo-webview-sdk)
[![npm: foo-ui-webview2-mcp](https://img.shields.io/npm/v/foo-ui-webview2-mcp?label=foo-ui-webview2-mcp)](https://www.npmjs.com/package/foo-ui-webview2-mcp)
[![CI](https://github.com/NereaFantasia/foo_ui_webview2/actions/workflows/ci.yml/badge.svg)](https://github.com/NereaFantasia/foo_ui_webview2/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/NereaFantasia/foo_ui_webview2)](https://github.com/NereaFantasia/foo_ui_webview2/releases)
[![CodeQL](https://github.com/NereaFantasia/foo_ui_webview2/actions/workflows/codeql.yml/badge.svg)](https://github.com/NereaFantasia/foo_ui_webview2/actions/workflows/codeql.yml)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/NereaFantasia/foo_ui_webview2)

**Documentation**: https://nereafantasia.github.io/foo_ui_webview2/ (English and Chinese)

A modern UI component for foobar2000 built on WebView2 (C++ DLL). It turns the entire foobar2000 window into a WebView2 canvas, letting you build the interface with modern web technologies while keeping native Windows 11 visual effects (Mica/Acrylic).

- License: GPL-3.0-or-later (main component) / MIT (`sdk/`, `mcp/`)

---

## Features

- Full WebView2 UI — the entire client area is rendered by WebView2; build it with plain HTML/CSS/JS or any framework (Vue, React, …)
- Runs three ways — standalone full-UI replacement, DUI panel, or CUI panel
- Native Windows 11 effects — Mica / Acrylic / Tabbed backgrounds
- Extended title bar into the client area — custom title bar content, Snap Layout support
- Bidirectional C++ / JS bridge (BridgeCore) — player state and events pushed to your UI in real time
- 400+ methods in 40+ namespaces, and 100+ events
- Multi-window system — popup windows, cross-window messaging, async close handling
- Web Components library (fb-* elements) and a typed TypeScript SDK on npm
- SMP compatibility layer — Spider Monkey Panel knowledge and much existing code carry over
- MCP Server — AI agent integration via CDP
- Plugin extension system (PluginRegistry) — other components can expose APIs to your UI

---

## Why foo_ui_webview2?

foobar2000 already has excellent customization stacks; this component occupies a different spot in the trade-off space:

| | foo_ui_webview2 | JS panel hosts (JSplitter / SMP) | WebView panel components (foo_uie_webview / foo_webview2) |
| --- | --- | --- | --- |
| UI technology | Web platform — HTML/CSS/JS, any framework, Chromium DevTools | JavaScript over GDI/GDI+ drawing callbacks | Web platform |
| Runs as | Standalone full UI, DUI/CUI panel, popup windows | Panel / splitter inside DUI or CUI | Panel inside DUI or CUI |
| Player integration | Built-in bridge — playback, playlists, media library, metadata, artwork, queue, config, and more in one component | Mature, battle-tested script API with a large existing theme ecosystem | Host-object API centered on the panel and rendering |
| Native windowing | Fullscreen, Mica/Acrylic, custom caption, multi-window (standalone mode) | Managed by the host UI | Managed by the host UI |
| Skill / code reuse | Web skills; SMP knowledge via the compatibility layer | Existing SMP / JScript scripts and skills | Web skills |
| AI tooling | MCP server included | — | — |

These stacks compose rather than exclude each other: this component itself runs as a CUI/DUI panel next to your existing layout, and other foobar2000 components can register APIs into its bridge through PluginRegistry.

---

## Quick start (30 seconds)

1. Install the `.fb2k-component` package, then pick `Webview2 UI` under `File → Preferences → Display → Default User Interface` ([installation guide](https://nereafantasia.github.io/foo_ui_webview2/how-to/install)).
2. Save this as `<profile>\webview-ui\default\index.html` (create the folders if needed):

```html
<!DOCTYPE html>
<html>
<head><meta charset="UTF-8"><title>My foobar2000 UI</title></head>
<body>
    <h1 id="track">Waiting for playback...</h1>
    <button id="play">Play / Pause</button>

    <script>
    document.getElementById('play').onclick = () => fb2k.invoke('playback.playOrPause');
    fb2k.on('playback:trackChanged', (data) => {
        document.getElementById('track').textContent = data.artist + ' - ' + data.title;
    });
    </script>
</body>
</html>
```

3. Restart foobar2000.

No SDK files, no Node.js, no build step — the `fb2k` bridge is injected natively by the component. Until you deploy a template, the component shows a built-in test page with clickable API demos, so a fresh install is never a blank window.

## Templates & themes

Default theme: <https://github.com/NereaFantasia/foo-webview-default-theme>

The component ships without a built-in frontend. Place your WebUI under your foobar2000 **profile** directory at `webview-ui\<template>\`, with `index.html` as the required entry point at the template root (e.g. `<profile>\webview-ui\default\index.html`). `<template>` defaults to `default` and can be managed / switched from the component's preferences page. The legacy location `<component dir>\foo_ui_webview2_resources\dist\` is still supported for backward compatibility. Restart foobar2000 to load it. You can build custom themes on top of `sdk/` (`foo-webview-sdk`).

## Choose your path

- **Coming from Spider Monkey Panel / JScript?** The SMP compatibility layer maps 35 SMP callbacks (`on_playback_new_track`, …) and ships `fb` / `plman` wrapper objects, so existing knowledge and much existing code carry over. → [SMP compatibility docs](https://nereafantasia.github.io/foo_ui_webview2/reference/smp-compat)
- **Web developer?** `npm install foo-webview-sdk` gives you typed `fb.*` wrappers and Web Components, and you can point the component at your Vite/webpack dev server (`window.setDevServerConfig`) for hot reload against the live player. → [Build your first theme](https://nereafantasia.github.io/foo_ui_webview2/tutorials/first-theme)
- **Rather not write code by hand?** Point an AI agent at the MCP server (`npx foo-ui-webview2-mcp`): it can control playback and playlists, read player state, and take screenshots over CDP — enough for an agent to build and iterate a theme together with you. → [MCP docs](https://nereafantasia.github.io/foo_ui_webview2/mcp/overview)

---

## npm Packages

The two JavaScript packages below are published to npm and can be installed directly (no source build required):

| Package | Install / Use | Description |
|----|------------|------|
| [`foo-webview-sdk`](https://www.npmjs.com/package/foo-webview-sdk) | `npm install foo-webview-sdk` | Theme / frontend SDK (Bridge API + Web Components + SMP compatibility layer), see [`sdk/`](sdk/) |
| [`foo-ui-webview2-mcp`](https://www.npmjs.com/package/foo-ui-webview2-mcp) | `npx foo-ui-webview2-mcp` | MCP Server for AI agents to drive foobar2000 over CDP, see [`mcp/`](mcp/) |

---

## API Overview

400+ methods in namespaces such as playback, playlist, library, window, config, file, metadata, audio, queue and tray. The [API overview](https://nereafantasia.github.io/foo_ui_webview2/api/overview) lists every namespace with its method count.

### Events (colon format)

A selection of the 100+ events; the [Events API](https://nereafantasia.github.io/foo_ui_webview2/api/events) lists them all.

```
playback:trackChanged, playback:paused, playback:stopped, playback:seeked,
playback:volumeChanged, playback:time, playlist:itemsAdded, playlist:itemsRemoved,
playlist:activated, playlist:created, playlist:removed, playlist:renamed,
playlist:selectionChanged, metadb:changed, plugin:registered, api:registered
```

### Usage Example

```javascript
// invoke uses dot format
const track = await fb2k.invoke('playback.getCurrentTrack');
const results = await fb2k.invoke('library.search', { query: 'artist:Radiohead' });

// event listeners use colon format
fb2k.on('playback:trackChanged', (track) => { /* ... */ });
```

---

## Plugin Extensions

Other foobar2000 plugins can register APIs through PluginRegistry for the frontend to call:

```cpp
#include "api/PluginRegistry.h"

void RegisterMyApis() {
    auto& registry = PluginRegistry::GetInstance();
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

---

## Security Restrictions

**Threat model.** This is a dedicated, domain-specific UI host for foobar2000 — themes come from you or trusted sources, so installing a theme carries the same trust as installing any foobar2000 component. The goal is therefore *fail-safe* (stop a buggy theme from accidentally harming the system), **not** *sandboxing untrusted code*; over-locking-down such a specialized component would only tie its own hands. The real guardrails are page-origin checks, path validation and protocol checks:

| API | Restriction |
|-----|------|
| All APIs | Page origin — only pages the host trusts can call the bridge or receive events: template folders and built-in pages, the configured development server, a panel's URL, and popup URLs the opener already trusts. Calls from any other page fail with `ORIGIN_DENIED` |
| shell.exec / shell.spawn | No command / executable allowlist — commands run as given; `cwd` and absolute paths are validated by PathSecurity (trusts the theme author; this is not a sandbox) |
| shell.openWith | Blocklist — opening 29 executable / script extensions (`.exe`, `.bat`, `.ps1`, `.dll`, `.lnk`, …) is refused |
| file.read/write | Path restrictions — system directories (`C:\Windows`, `C:\Program Files`, `C:\ProgramData`) are inaccessible. `file.*` writes are accepted in the foobar2000 config dir and `%TEMP%`, inside media-library watch folders, on any non-system drive, and on library or playlist tracks |
| http.get/post | SSRF protection — localhost / private / link-local addresses are blocked (including post-DNS-resolution checks against rebinding) |
| CDP remote debugging | Off by default — must be enabled manually, bound to localhost only |

Full details: [security notes](https://nereafantasia.github.io/foo_ui_webview2/reference/security) and [permissions reference](https://nereafantasia.github.io/foo_ui_webview2/reference/permissions).

---

## Building from Source

> You only need this section to develop the component itself. Users install the packaged `.fb2k-component` — see Quick start above.
>
> The development workflow, including how to change an API and the checks CI runs, is described in [CONTRIBUTING.md](./CONTRIBUTING.md).

### Requirements

- Visual Studio 2026 (v145 toolset), or Visual Studio 2022 with `-PlatformToolset v143`
- Windows SDK 10.0.22621+
- WebView2 Runtime
- Node.js 18+ (to build the SDK / MCP / docs site)

### C++ Component

```powershell
# Standard build
.\build.ps1 -Config Release -Platform x64

# Package .fb2k-component (x86 + x64)
.\build-package.ps1

# Package offline documentation (Windows users need no extra runtime)
.\build-docs-package.ps1
```

The documentation ZIP opens through `open-docs.cmd` and uses only Windows 10/11 built-in Windows PowerShell 5.1, .NET Framework, and the default browser. It does not require Node.js, Python, npm, administrator access, or an Internet connection.

### TypeScript SDK (build from source, for development)

> Consumers do not need to build it — just `npm install foo-webview-sdk` (see "npm Packages" above).

```powershell
cd sdk
npm ci
npm run build
```

### MCP Server (build from source, for development)

> Consumers do not need to build it — just `npx foo-ui-webview2-mcp` (see "npm Packages" above).

```powershell
cd mcp
npm install
npm run build
```

### Tech Stack

| Layer | Technology |
|----|------|
| C++ component | C++20, MSVC v145 (VS 2026) or v143 (VS 2022), Windows SDK 10.0 |
| WebView2 | Microsoft.Web.WebView2 1.0.3719.77 (NuGet) |
| WIL | Microsoft.Windows.ImplementationLibrary 1.0.240803.1 |
| JSON | nlohmann/json |
| foobar2000 | SDK v2.0+ (lib/) |
| Columns UI | SDK (not in the repo; `build.ps1` clones it into lib/columns_ui-sdk) |
| TypeScript SDK | foo-webview-sdk (sdk/, MIT, tsup) |
| MCP Server | foo-ui-webview2-mcp (mcp/, Node.js 18+) |

### Project Structure

```
foo_ui_webview2/
├── src/                    # C++ source code
│   ├── core/               # WebView core, multi-instance routing, JIT queue
│   ├── ui/                 # UserInterface entry, background mode, main menu
│   ├── prefs/              # Preferences pages
│   ├── window/             # MainWindow, PopupWindow, WindowManager
│   ├── webview/            # WebView host and environment, injected scripts
│   ├── media/              # Local audio/video files served to the page (MP4, Matroska)
│   ├── api/                # BridgeCore + API handlers
│   ├── callbacks/          # foobar2000 SDK event callbacks
│   ├── panels/             # DUI/CUI panel integration
│   ├── selection/          # Selection tracking
│   ├── interfaces/         # Service abstractions
│   ├── settings/           # Stored settings (cfg_var, Advanced preferences)
│   ├── domain/             # Path security, library tree index and cache
│   └── utils/              # Utility functions
├── sdk/                    # TypeScript SDK (foo-webview-sdk)
│   ├── src/                # Source (bridge/, components/, smp/, types/)
│   └── package.json
├── mcp/                    # MCP Server (AI agent CDP bridge)
│   ├── src/                # TypeScript source
│   └── tests/              # Unit + E2E tests
├── tests/                  # C++ unit tests (GoogleTest)
├── docs/vitepress/         # User documentation site
├── scripts/                # Generators and checks (see CONTRIBUTING.md)
├── lib/                    # Third-party libraries
│   ├── foobar2000_sdk/     # foobar2000 SDK (BSD)
│   ├── columns_ui-sdk/     # Columns UI SDK (cloned by build.ps1)
│   └── json/               # nlohmann/json (MIT)
├── build.ps1               # Standard build script
├── build-package.ps1       # .fb2k-component packaging
├── foo_ui_webview2.sln     # VS solution
├── foo_ui_webview2.vcxproj # VS project file
└── packages.config         # NuGet package config
```

---

## License

The main component of this project is licensed under the **GNU General Public License v3.0 or later** (GPL-3.0-or-later); see [LICENSE](LICENSE) for the full terms.

- Main component: GPL-3.0-or-later
- TypeScript SDK (`sdk/`): MIT License (see `sdk/LICENSE`)
- MCP server (`mcp/`): MIT License (see `mcp/LICENSE`)

Third-party libraries retain their own licenses: foobar2000 SDK (BSD), pfc / libPPUI (zlib), nlohmann/json (MIT), Columns UI SDK.

---

## Acknowledgements

- [foobar2000 SDK](https://www.foobar2000.org/SDK)
- [WebView2](https://developer.microsoft.com/microsoft-edge/webview2/)
- [nlohmann/json](https://github.com/nlohmann/json)
- [Columns UI SDK](https://github.com/reupen/columns_ui-sdk)
- [WIL](https://github.com/microsoft/wil)

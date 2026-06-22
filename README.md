English | [中文](./README.zh-CN.md)

# foo_ui_webview2

[![Docs](https://img.shields.io/badge/docs-online-brightgreen)](https://nereafantasia.github.io/foo_ui_webview2/)
[![License](https://img.shields.io/badge/license-GPL--3.0%20%7C%20MIT-blue)](LICENSE)
[![npm: foo-webview-sdk](https://img.shields.io/npm/v/foo-webview-sdk?label=foo-webview-sdk)](https://www.npmjs.com/package/foo-webview-sdk)
[![npm: foo-ui-webview2-mcp](https://img.shields.io/npm/v/foo-ui-webview2-mcp?label=foo-ui-webview2-mcp)](https://www.npmjs.com/package/foo-ui-webview2-mcp)
<!-- CI / CodeQL badges return 404 while the repository is private; enable when it becomes public:
[![CI](https://github.com/NereaFantasia/foo_ui_webview2/actions/workflows/ci.yml/badge.svg)](https://github.com/NereaFantasia/foo_ui_webview2/actions/workflows/ci.yml)
[![CodeQL](https://github.com/NereaFantasia/foo_ui_webview2/actions/workflows/codeql.yml/badge.svg)](https://github.com/NereaFantasia/foo_ui_webview2/actions/workflows/codeql.yml)
-->
<!-- DeepWiki (enable after the repo is indexed): [![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/NereaFantasia/foo_ui_webview2) -->

**Documentation**: https://nereafantasia.github.io/foo_ui_webview2/

A modern UI component for foobar2000 built on WebView2 (C++ DLL). It turns the entire foobar2000 window into a WebView2 canvas, letting you build the interface with modern web technologies while keeping native Windows 11 visual effects (Mica/Acrylic).

- Version: 1.9.0
- License: GPL-3.0-or-later (main component) / MIT (`sdk/`)

---

## Features

- Full WebView2 UI — the entire client area is rendered by WebView2
- Native Windows 11 effects — Mica / Acrylic / Tabbed backgrounds
- Extended title bar into the client area — custom title bar content, Snap Layout support
- Bidirectional C++ / JS bridge (BridgeCore)
- 200+ APIs across 20+ namespaces
- Plugin extension system (PluginRegistry)
- Web Components library (fb-* elements)
- SMP compatibility layer
- MCP Server — AI agent integration via CDP
- DUI / CUI panel support

---

## Tech Stack

| Layer | Technology |
|----|------|
| C++ component | C++20, VS 2022 (v145), Windows SDK 10.0 |
| WebView2 | Microsoft.Web.WebView2 1.0.3719.77 (NuGet) |
| WIL | Microsoft.Windows.ImplementationLibrary 1.0.240803.1 |
| JSON | nlohmann/json |
| foobar2000 | SDK v2.0+ (lib/) |
| Columns UI | SDK (git submodule: lib/columns_ui-sdk) |
| TypeScript SDK | foo-webview-sdk (sdk/, MIT, tsup) |
| MCP Server | foo-ui-webview2-mcp (mcp/, Node.js 18+) |

---

## npm Packages

The two JavaScript packages below are published to npm and can be installed directly (no source build required):

| Package | Install / Use | Description |
|----|------------|------|
| [`foo-webview-sdk`](https://www.npmjs.com/package/foo-webview-sdk) | `npm install foo-webview-sdk` | Theme / frontend SDK (Bridge API + Web Components + SMP compatibility layer), see [`sdk/`](sdk/) |
| [`foo-ui-webview2-mcp`](https://www.npmjs.com/package/foo-ui-webview2-mcp) | `npx foo-ui-webview2-mcp` | MCP Server for AI agents to drive foobar2000 over CDP, see [`mcp/`](mcp/) |

---

## Project Structure

```
foo_ui_webview2/
├── src/                    # C++ source code
│   ├── core/               # WebView core, UserInterface, etc.
│   ├── window/             # MainWindow, PopupWindow, WindowManager
│   ├── webview/            # WebView host and environment
│   ├── api/                # BridgeCore + API handlers
│   ├── callbacks/          # foobar2000 SDK event callbacks
│   ├── panels/             # DUI/CUI panel integration
│   ├── selection/          # Selection tracking
│   ├── interfaces/         # Service abstractions
│   └── utils/              # Utility functions
├── sdk/                    # TypeScript SDK (foo-webview-sdk)
│   ├── src/                # Source (bridge/, components/, smp/, types/)
│   └── package.json
├── mcp/                    # MCP Server (AI agent CDP bridge)
│   ├── src/                # TypeScript source
│   └── tests/              # Unit + E2E tests
├── tests/                  # C++ unit tests (GoogleTest)
├── docs/vitepress/         # User documentation site
├── lib/                    # Third-party libraries
│   ├── foobar2000_sdk/     # foobar2000 SDK (BSD)
│   ├── columns_ui-sdk/     # Columns UI SDK (submodule)
│   └── json/               # nlohmann/json (MIT)
├── build.ps1               # Standard build script
├── build-package.ps1       # .fb2k-component packaging
├── foo_ui_webview2.sln     # VS solution
├── foo_ui_webview2.vcxproj # VS project file
└── packages.config         # NuGet package config
```

---

## Build

### Requirements

- Visual Studio 2022 (17.8+), v145 toolset
- Windows SDK 10.0.22621+
- WebView2 Runtime
- Node.js 18+ (to build the SDK / MCP / docs site)

### C++ Component

```powershell
# Standard build
.\build.ps1 -Config Release -Platform x64

# Package .fb2k-component (x86 + x64)
.\build-package.ps1
```

### TypeScript SDK (build from source, for development)

> Consumers do not need to build it — just `npm install foo-webview-sdk` (see "npm Packages" above).

```powershell
cd sdk
npm run build
```

### MCP Server (build from source, for development)

> Consumers do not need to build it — just `npx foo-ui-webview2-mcp` (see "npm Packages" above).

```powershell
cd mcp
npm install
npm run build
```

### WebUI / Themes

The component ships without a built-in frontend. Place your WebUI (containing at least `index.html`) into
`foo_ui_webview2_resources/dist/` under the foobar2000 components directory, then restart foobar2000 to load it. You can build custom themes on top of `sdk/` (`foo-webview-sdk`).

---

## API Overview

200+ APIs span the following namespaces:

playback, playlist, library, window, config, file, dialog, clipboard, shell, http, keyboard, ui, lyrics, metadata, audio, console, dnd, queue, discovery, replaygain, playcount, titleformat, selection, port

### Events (colon format)

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

| API | Restriction |
|-----|------|
| shell.exec | Disabled — arbitrary command execution is too risky |
| shell.openWith | Blocklist — opening 30+ executable extensions is forbidden |
| file.read/write | Path restrictions — protected system directories are inaccessible |
| http.get/post | SSRF protection — access to intranet addresses is blocked |
| CSP | Frontend pages are protected by a Content-Security-Policy |
| CDP remote debugging | Off by default — must be enabled manually, bound to localhost only |

---

## License

The main component of this project is licensed under the **GNU General Public License v3.0 or later** (GPL-3.0-or-later); see [LICENSE](LICENSE) for the full terms.

- Main component: GPL-3.0-or-later
- TypeScript SDK (`sdk/`): MIT License (see `sdk/LICENSE`)

Third-party libraries retain their own licenses: foobar2000 SDK (BSD), pfc / libPPUI (zlib), nlohmann/json (MIT), Columns UI SDK.

---

## Acknowledgements

- [foobar2000 SDK](https://www.foobar2000.org/SDK)
- [WebView2](https://developer.microsoft.com/microsoft-edge/webview2/)
- [nlohmann/json](https://github.com/nlohmann/json)
- [Columns UI SDK](https://github.com/reupen/columns_ui-sdk)
- [WIL](https://github.com/microsoft/wil)

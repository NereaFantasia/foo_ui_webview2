# Installation

## System requirements

| Item | Requirement |
| --- | --- |
| OS | Windows 10/11 (64-bit) |
| foobar2000 | v2.0 or later (64-bit) |
| WebView2 Runtime | Usually preinstalled on Windows 10/11; otherwise download from Microsoft |

::: tip Note
foobar2000 v2.0 is the 64-bit cutoff. If you still run v1.x, upgrade to v2.0+ first.
:::

## Install the component package

1. **Download the package** — get `foo_ui_webview2-<version>.fb2k-component` from the release page (the filename includes the component version, for example `foo_ui_webview2-1.11.0.fb2k-component`)
2. **Install through foobar2000** — open or double-click the package so foobar2000 shows its component install confirmation
3. **Confirm** — click `Yes` and let foobar2000 install the package contents
4. **Restart foobar2000**
5. **Select the UI** — `File → Preferences → Display → Default User Interface` → choose `Webview2 UI`

::: info INFO
`.fb2k-component` is the official foobar2000 component package format. Install it with foobar2000's component installer; do not treat the package as a plain zip that you unpack by hand into the profile.
:::

## Manual binary install (not recommended)

Release packages contain architecture-specific binaries (`foo_ui_webview2.dll`, `WebView2Loader.dll`, and an `x64/` layout). Copying only a single DLL into an arbitrary `components` folder is incomplete and layout-dependent.

Prefer the official `.fb2k-component` install path above. Manual extraction is unsupported documentation for advanced recovery only and is not a recommended install method.

## Verify installation

After a successful install you should see:

- The Default User Interface list includes `Webview2 UI`
- Menu entry `View → WebView2 UI → Show/Hide Window`
- Preferences page `File → Preferences → Display → WebView2 UI`
- Advanced branch `File → Preferences → Advanced → Tools → WebView2 UI`

## Resource root and templates

User-editable frontend resources live under the profile resource root:

```text
profile/
└── webview-ui/                      # resource root
    ├── default/                     # common default template
    │   └── index.html
    └── <active-template>/           # global active template
        └── index.html
```

Resolution order used by the component:

1. Panel-level override: `panelConfig.templateName` → `profile/webview-ui/<templateName>/`
2. Global active template: `profile/webview-ui/<active-template>/`
3. Packaged component resources: `foo_ui_webview2_resources/dist`
4. Compatibility fallback: `profile/webview-ui/default/`

`default` is the usual starter template name, not a permanent or exclusive contract. Creating a template seeds a template directory and a starter `index.html`; it does **not** deploy an SDK tree.

## Obtaining the SDK

The component injects native `window.fb2k`. The higher-level `fb.*` wrappers come from the separate npm package `foo-webview-sdk` (or a versioned SDK ZIP release asset). The component does **not** automatically create `sdk/` under the profile.

### Bundler / ESM

```bash
npm install foo-webview-sdk
```

```js
import fb from 'foo-webview-sdk'
// or public subpaths such as 'foo-webview-sdk/bridge' / 'foo-webview-sdk/components'
await fb.player.play()
```

### No bundler (IIFE globals)

Copy built IIFE files from the package/SDK ZIP into your theme directory, then load the paths you actually copied:

```html
<!-- Example layout after you copy the built IIFE files into ./sdk/dist/ -->
<script src="./sdk/dist/bridge.global.js"></script>
<script src="./sdk/dist/components.global.js"></script>
```

Those `./sdk/dist/...` paths are a post-copy example layout, not a directory created by the component installer.

| Surface | Requires SDK files? |
| --- | --- |
| Native `window.fb2k.invoke` / `window.fb2k.on` | No |
| `fb.*` wrappers and Web Components helpers | Yes (`foo-webview-sdk`) |

## Background mode

Background mode lets WebView2 UI run alongside Default UI or Columns UI.

**Enable steps:**

1. `Preferences → Advanced → Tools → WebView2 UI` → `Enable Background Mode`
2. `Preferences` → `Display` → `Default User Interface` → choose another UI
3. Restart foobar2000
4. `View → WebView2 UI → Show/Hide Window`

Visibility depends on background-mode settings and the last Show/Hide state. When no independent WebView window exists (panel-only cases), the same menu entry may open Preferences instead of toggling a window.

**Typical uses:**

- Use WebView2 UI as a lyrics/artwork companion window
- Keep Default UI for playlist management while WebView2 UI shows visualizations
- Run custom scripts in a companion window while another UI remains primary

**Page visibility contract:** when the window is minimized, hidden to the tray, or the session is locked, the WebView page becomes `visibilityState=hidden` — `requestAnimationFrame` stops and timers are heavily throttled. Prefer event-driven updates (`fb2k.on(...)`) over polling loops so your theme resumes cleanly. If you automate the player over CDP/MCP, see the keep-alive option in the [MCP setup guide](/mcp/setup) to keep the page active in these states.

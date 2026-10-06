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

1. **Download the package** — get `foo_ui_webview2-<version>.fb2k-component` from the release page (the filename includes the component version, for example `foo_ui_webview2-1.12.0.fb2k-component`)
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

## Preferences page

`File → Preferences → Display → WebView2 UI` is the overview page, with three groups:

- **Web template** — the active template and its folder. `Manage...` creates, renames or deletes templates (you name a new template yourself); `Open...` opens the folder when it exists.
- **Appearance** — the component language (Auto / English / Chinese) and the default window backdrop. A language change applies to newly opened component dialogs; menus and panel descriptions update after a restart.
- **Tools** — a read-only line with the development-server state, plus buttons to the Advanced branch and the API list.

Three sub-pages sit under it in the tree on the left (their names follow the component language after a restart):

- **Window** — remember the window position and size; background mode (takes effect after a restart) and restoring the background window's last Show/Hide state on startup (editable once background mode is checked); minimize to tray and close to tray (disabled with a note while the current theme has not created a tray icon); the thumbnail toolbar playback buttons (after a restart) and playback progress on the taskbar button. Values a theme sets at run time through the `tray.*` / `taskbar.*` API override these for the current session only and are not written back.
- **Performance** — preheat the WebView2 environment at startup (off: it is created when the first window needs it; after a restart); deep-suspend the WebView while the main window is hidden (main window only; not while CDP keep-alive is active); the default content zoom, 50–200% in steps of 25 on top of the system DPI scaling. `Apply` re-zooms open windows except those whose theme has called `window.setZoom`; new windows read the value when they are created.
- **Developer** — Developer Tools (F12); the development server switch and URL, which `Apply` puts into effect at once by reloading the main window and the panels that follow the global template (no restart, and no effect on panels with their own template or URL or on popup windows); CDP remote debugging and its port (1024–65535, default 9222). Developer Tools, the CDP switch and the port take effect after a restart. If you change the port, point the MCP server at it as well (`FB2K_CDP_PORT`, see the [MCP setup guide](/mcp/setup)).

Every page holds its edits as a draft: nothing is written until `Apply` or `OK`, `Reset page` returns the draft to the defaults without writing, and `Cancel` discards it. Template create / rename / delete are file operations that run immediately and are not undone by `Cancel`; the active template and any template a loaded panel references cannot be renamed or deleted.

Security exceptions stay in `File → Preferences → Advanced → Tools → WebView2 UI`: the CDP background keep-alive, HTTP access to the local network, invalid TLS certificates for `fb.http.*`, and the reserved HSTS entry. Entry names follow the component language. Background mode, deep suspend, Developer Tools, the CDP switch and the development server switch and URL are no longer Advanced entries; a value set there by an earlier version is carried over the first time the new setting is read.

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

1. `Preferences → Display → WebView2 UI → Window` → check `Background mode: keep the WebView running while another user interface is active` and press `Apply` (the label is translated when the component language is Chinese)
2. `Preferences` → `Display` → `Default User Interface` → choose another UI
3. Restart foobar2000
4. `View → WebView2 UI → Show/Hide Window`

Visibility depends on background-mode settings and the last Show/Hide state. When no independent WebView window exists (panel-only cases), the same menu entry may open Preferences instead of toggling a window.

**Typical uses:**

- Use WebView2 UI as a lyrics/artwork companion window
- Keep Default UI for playlist management while WebView2 UI shows visualizations
- Run custom scripts in a companion window while another UI remains primary

**Page visibility contract:** when the window is minimized, hidden to the tray, or the session is locked, the WebView page becomes `visibilityState=hidden` — `requestAnimationFrame` stops and timers are heavily throttled. Prefer event-driven updates (`fb2k.on(...)`) over polling loops so your theme resumes cleanly. If you automate the player over CDP/MCP, see the keep-alive option in the [MCP setup guide](/mcp/setup) to keep the page active in these states.

# How a page is found

When a window or panel opens, the component decides which page to load. Knowing the order explains why a window shows an older theme, a built-in test page, or the "Frontend not found" page.

## Templates

A template is a folder with an `index.html` under the profile's resource root, `<profile>\webview-ui\`. A theme can be several pages, scripts and assets; only `index.html` is required.

```text
<profile>/
└── webview-ui/                 resource root
    ├── default/                default template when none is specified
    │   └── index.html
    └── my-theme/               custom theme template
        ├── index.html
        └── assets/
```

The global active template is configured under `File → Preferences → Display → WebView2 UI`; when unconfigured, it defaults to `default`. The 'New Template' action creates a bare `index.html` placeholder without SDK bindings, scripts, or build assets.

Local pages are served via the custom virtual origin `https://foo-ui-webview2.local/`, mapped to the active template root directory: `/index.html` resolves to the template entry point, and root-relative paths like `/assets/app.js` resolve within that folder.

## The order

The main window, background window, and UI panels evaluate candidates in sequence and load the first matching source:

1. **Development server**: When `Use development server (HMR hot reload)` is enabled with a valid URL, all windows and panels prioritize this URL; if unreachable, resolution falls back to subsequent candidates.
2. **Panel URL override**: Loads the dedicated URL if configured on the individual panel.
3. **Panel-specific template**: Loads the panel's explicitly configured template directory when it contains an `index.html` entry point.
4. **Global active template**: Loads the globally configured active template when its directory contains `index.html`.
5. **Component distribution directory**: Loads `foo_ui_webview2_resources\dist` in the component directory when `index.html` exists; skipped if empty or missing the entry point.
6. **Default template**: Loads `<profile>\webview-ui\default\` when it contains `index.html`.
7. **Built-in fallback page**: When no candidate matches, the main and background windows load the built-in API test page, while panels display "Frontend not found. Please install a template."

Popups opened via `window.createPopup` follow independent resolution rules: they load the specified page file (such as `<page>.html`) directly from the active template (or development server), appending `windowId` to query parameters. If the development server is unreachable, popups fall back to the local template file; when no template folder has an `index.html`, they show the "Frontend not found" page.

<a id="when-pages-reload"></a>

## Reload triggers {#reload-triggers}

Page reloading is governed by explicit configuration changes and application events across window scopes:

- **Preferences application**: Modifying the active template or development server settings and clicking `Apply` immediately reloads the main window and all panels inheriting the global template; panels with explicit template or URL overrides remain unaffected.
- **Development server precedence**: While the development server remains active, switching the global template does not trigger reloads because pages continue serving from the development server.
- **Panel-level configuration**: Modifying a panel's dedicated template or URL override within its configuration dialog reloads that specific panel only.
- **Popup window lifecycle**: Applying preferences never reloads active popup windows; updated settings apply only to popups opened after the change.

## Related

- [Install a theme](/how-to/install-theme)
- [Debug a theme with the development server](/how-to/dev-server)
- [Put a page in a panel](/how-to/panels)

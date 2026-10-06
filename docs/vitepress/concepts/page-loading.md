# How a page is found

When a window or panel opens, the component decides which page to load. Knowing the order explains why a window shows an older theme, a built-in test page, or the "Frontend not found" page.

## Templates

A template is a folder with an `index.html` under the profile's resource root, `<profile>\webview-ui\`. A theme can be several pages, scripts and assets; only `index.html` is required.

```text
<profile>/
└── webview-ui/                 resource root
    ├── default/                the template used when none is chosen
    │   └── index.html
    └── my-theme/               any other template
        ├── index.html
        └── assets/
```

The global active template is the one picked on the `File → Preferences → Display → WebView2 UI` page; when none was ever picked, it is `default`. Creating a template there writes only a placeholder `index.html`: no SDK, scripts or other files.

A local page is served from `https://foo-ui-webview2.local/`, mapped to the template folder, so `/index.html` is the template's `index.html` and root-relative paths such as `/assets/app.js` resolve inside the folder.

## The order

For the main window, the background window and each panel, the first of these that applies wins:

1. **Development server.** When `Use development server (HMR hot reload)` is on and a URL is set, every window and panel loads that URL. If it cannot be reached, loading continues with step 2.
2. **The panel's URL override.** A panel whose configuration has a URL loads it.
3. **The panel's own template**, when it names one and that folder has an `index.html`.
4. **The global active template**, when its folder has an `index.html`.
5. **`foo_ui_webview2_resources\dist`** in the component's folder, when it has an `index.html`. Older releases installed a built-in theme there; current ones do not, and the empty folder the component package creates is skipped.
6. **`<profile>\webview-ui\default\`**, when it has an `index.html`.
7. **A built-in page.** The main window and the background window show a test page with buttons that try out the API; a panel shows "Frontend not found. Please install a template."

Popup windows opened by a page are different: they load `<page>.html` from the same template (or from the development server), with their window ID in the query string. When the development server cannot be reached, a popup loads the same page from the template instead, and when no template folder has an `index.html` it shows the "Frontend not found" page.

## When pages reload

- `Apply` on the preferences page after picking another template reloads the main window and the panels that follow the global template. Panels with their own template or URL keep their page.
- `Apply` after changing the development-server settings reloads the same windows.
- Neither `Apply` reloads popup windows: an open popup keeps its page, and popups opened afterwards follow the new settings.
- While the development server is on, a template change does not reload anything: pages keep coming from the server.
- Changing a panel's template or URL in its configuration dialog reloads that panel.

## Related

- [Install a theme](/how-to/install-theme)
- [Debug a theme with the development server](/how-to/dev-server)
- [Put a page in a panel](/how-to/panels)

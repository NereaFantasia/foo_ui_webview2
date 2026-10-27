# Put a page in a panel

In addition to replacing the entire application interface, the component provides a dedicated `WebView2 Panel` element for Default UI and Columns UI layouts. Each panel instance runs its own web page in isolation. [Run modes](/concepts/run-modes) details the capability boundaries of pages embedded inside panels.

## Add a panel to Default UI

1. Ensure Default UI is the active interface (`File → Preferences → Display → Default User Interface`).
2. Enable layout editing: `View → Layout → Enable Layout Editing Mode`.
3. Right-click an empty area of the layout, choose `Add New UI Element`, and select `WebView2 Panel`.
4. Disable layout editing mode.

While layout editing is active, each WebView2 panel displays a placeholder reading "WebView2 Panel / Right-click to configure" instead of rendering its web page; the page reloads automatically once layout editing is disabled.

## Add a panel to Columns UI

1. Install the Columns UI component and set it as the active interface.
2. Open its layout configuration (`File → Preferences → Display → Columns UI → Layout`).
3. Add `WebView2 Panel` from the `Panels` category into the desired layout splitter.

<a id="give-a-panel-its-own-template-or-url"></a>

## Configure a panel template or URL override

A newly created panel defaults to the global active template. To customize its contents, open the panel's configuration dialog: in Default UI, enable layout editing mode, right-click the panel, and select `Configure...`; in Columns UI, use the panel's configuration item within layout settings.

| Field | Effect |
| --- | --- |
| `Template` | `(Follow global)`, or a dedicated template folder under `<profile>\webview-ui\` specific to this panel |
| `URL Override (optional)` | A direct URL to load instead of any local template; takes precedence over `Template` |
| `Allow this site to access foobar2000 and this computer` | Grants the override URL access to invoke host APIs (including file system access and shell execution). Loopback addresses (`localhost`, `127.x.x.x`, `[::1]`) are implicitly trusted and grayed out. External origins require manual authorization; changing the URL to another origin resets this permission |
| `Resources path` | Read-only: the absolute directory path currently loaded by the panel, with notes on fallback status |
| `Transparent Background`, `Grab Focus`, `Enable Drag & Drop`, `Enable DevTools` | Individual panel toggles; `Enable DevTools` stacks on top of global developer settings |
| `Edge Style` | Panel border style: None, Sunken, or Grey |

Configuration changes take effect immediately upon dialog confirmation; the panel automatically reloads when its template path, override URL, or security permissions change. Dedicated panel templates must contain an `index.html` entry file, or the host falls back to the global template. Note that running pages cannot dynamically reconfigure their template path, URL override, or developer-tools toggle: the `panel.setConfig` endpoint accepts only panel title, transparency, focus capture, and drag-and-drop flags.

<a id="adapt-the-page-to-the-panel"></a>

## Adapt page behavior for panel mode

A page embedded inside a panel cannot resize, move, or re-theme the top-level foobar2000 application window: top-level `window.*` methods fail with `PANEL_MODE_UNSUPPORTED`, as do `tray.*` and `taskbar.*` when WebView2 UI is not the active main interface. Controls depending on these APIs should be hidden or adjusted. Query the host mode upon page initialization:

```javascript
const info = await fb2k.invoke('window.getMode');
if (info.success === false) throw new Error(info.error);
if (info.panelMode) {
    document.body.classList.add('panel-mode', `mode-${info.mode}`);
}
```

```css
/* Window chrome only makes sense in the standalone window */
.panel-mode .title-bar,
.panel-mode .window-controls {
    display: none;
}
```

The `mode` field returns `dui` or `cui` inside embedded panels, and `standalone` in the top-level main window. While panels also emit a `panel:initialized` event once their WebView initializes, this event typically fires before page scripts register listeners; applications should inspect `window.getMode` directly during startup.

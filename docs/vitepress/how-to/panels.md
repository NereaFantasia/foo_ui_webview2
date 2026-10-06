# Put a page in a panel

Besides replacing the whole interface, the component provides a `WebView2 Panel` for Default UI and for Columns UI layouts. Each panel runs its own page. [Run modes](/concepts/run-modes) explains what changes for a page inside a panel.

## Add a panel to Default UI

1. Make sure Default UI is the active interface (`File → Preferences → Display → Default User Interface`).
2. Turn on layout editing: `View → Layout → Enable Layout Editing Mode`.
3. Right-click an empty area of the layout, choose `Add New UI Element`, and pick `WebView2 Panel`.
4. Turn layout editing off again.

While layout editing is on, every WebView2 panel shows a placeholder reading "WebView2 Panel / Right-click to configure" instead of its page; the page comes back when editing ends.

## Add a panel to Columns UI

1. Install the Columns UI component and make it the active interface.
2. Open its layout settings (`File → Preferences → Display → Columns UI → Layout`).
3. Add `WebView2 Panel` from the `Panels` category to the layout.

## Give a panel its own template or URL

A new panel shows the global template. To change that, open the panel's configuration dialog: in Default UI, turn on layout editing, right-click the panel and choose `Configure...`; in Columns UI, use the panel's configure command in the layout settings.

| Field | Effect |
| --- | --- |
| `Template` | `(Follow global)`, or a template folder under `<profile>\webview-ui\` for this panel only |
| `URL Override (optional)` | A URL to load instead of any template; it wins over `Template` |
| `Resources path` | Read-only: the folder the panel actually loads, and a note when it falls back |
| `Transparent Background`, `Grab Focus`, `Enable Drag & Drop`, `Enable DevTools` | Per-panel switches; `Enable DevTools` adds to the global developer-tools setting |
| `Edge Style` | The panel border: none, sunken or grey |

Changes apply as soon as you confirm the dialog; the panel reloads its page when the template or the URL changed. A panel's own template must contain an `index.html`, otherwise the panel uses the global template. A page cannot change its panel's template, URL or developer-tools switch itself: `panel.setConfig` accepts only the name, transparency, focus and drag-and-drop fields.

## Adapt the page to the panel

A page in a panel cannot move, resize or style the foobar2000 window: the `window.*` methods that do so fail with `PANEL_MODE_UNSUPPORTED`, and so do `tray.*` and `taskbar.*` while WebView2 UI is not the main interface. Hide the controls that need them. Ask for the mode when the page starts:

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

`mode` is `dui` or `cui` inside a panel and `standalone` in the main window. Panels also send `panel:initialized` with the same fields once their WebView is ready, but usually before the page has subscribed, so read `window.getMode` instead of waiting for the event.

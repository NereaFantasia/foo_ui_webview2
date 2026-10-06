# Run modes

The same page can run in three places, and the place decides how much of the window it controls.

| Mode | Where the page appears | What it controls |
| --- | --- | --- |
| Standalone | `Webview2 UI` is the Default User Interface; the page is the whole foobar2000 window | The window itself: size, caption, backdrop, tray icon, taskbar button |
| DUI panel | A `WebView2 Panel` element in a Default UI layout | Only its own area |
| CUI panel | A `WebView2 Panel` in a Columns UI layout | Only its own area |

[Background mode](/how-to/background-mode) adds a fourth arrangement: a separate WebView2 UI window that runs next to Default UI or Columns UI.

## What stays the same

Every mode runs the same kind of page with the same bridge. Playback, playlists, the media library, artwork, metadata, configuration and the other foobar2000 namespaces work everywhere, and the `fb.*` methods and `fb-*` components do not need to know where they run. A theme can therefore be one codebase for the window and the panels.

## What changes in a panel

A panel is a child area of a window that belongs to another interface, so it cannot act on that window:

- The `window.*` methods that change the window (size, position, minimize and maximize, fullscreen, caption and drag regions, Mica, acrylic and other backdrops, corner style, always-on-top) fail with `PANEL_MODE_UNSUPPORTED` when a panel calls them.
- `tray.*` and `taskbar.*` fail the same way while WebView2 UI is not the main interface, because there is no WebView2 UI window to own a tray icon or a taskbar button.
- Each panel has its own configuration: it may load its own template or URL, and it has its own switches for transparency, focus, drag and drop, and developer tools.

`window.getMode` tells a page where it is running; [Put a page in a panel](/how-to/panels#adapt-the-page-to-the-panel) shows how to adapt the page.

## Why there are three modes

Replacing the whole interface gives a theme full control, but many people keep the playlist and library views of Default UI or Columns UI and want only one part of the window to be a web page: a cover and lyrics pane, a visualization, a now-playing strip. Panels serve that case without asking the theme to reimplement the rest of foobar2000's interface.

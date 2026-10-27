# Run modes

The same web page can run in three host modes, and the mode determines its scope of control over the host window.

| Mode | Where the page appears | What it controls |
| --- | --- | --- |
| Standalone | `WebView2 UI` is the active user interface; the page is the whole foobar2000 window | The window itself: dimensions, caption, system backdrop, tray icon, taskbar button |
| DUI panel | A `WebView2 Panel` element in a Default UI layout | Only its own rectangular area |
| CUI panel | A `WebView2 Panel` element in a Columns UI layout | Only its own rectangular area |

[Background mode](/how-to/background-mode) adds a fourth arrangement: a separate WebView2 UI window that runs alongside Default UI or Columns UI.

## What stays the same

Every mode runs the same standard web page over the same Bridge channel. Playback, playlists, the media library, artwork, metadata, configuration, and other foobar2000 namespaces work identically everywhere; typed `fb.*` SDK methods and `fb-*` Web Components do not need to branch on host mode. A single theme codebase can therefore power both top-level windows and embedded panels.

## What changes in a panel

A panel is an embedded child element within a window managed by another interface component, so it cannot alter that top-level window:

- The `window.*` methods that manipulate the top-level window (size, position, minimize/maximize, fullscreen, caption and drag regions, Mica, Acrylic, or other system backdrops, corner style, always-on-top) fail with `PANEL_MODE_UNSUPPORTED` when called from a panel.
- When WebView2 UI is not active as the primary interface, `tray.*` and `taskbar.*` fail with `PANEL_MODE_UNSUPPORTED` because there is no top-level WebView2 UI window to host tray icons or taskbar integration.
- Each panel maintains independent configuration: it can load its own template or URL, with separate settings for transparency, focus handling, drag-and-drop, and developer tools.

`window.getMode` allows a page to query its current host environment at runtime; [Put a page in a panel](/how-to/panels#adapt-the-page-to-the-panel) shows how to adapt page behavior accordingly.

## Why there are three modes

Replacing the entire interface grants a theme complete control over the application window. However, many users prefer retaining the established playlist and library layouts of Default UI or Columns UI, embedding web content only for dedicated elements—such as album art panes, synchronized lyrics, audio visualizers, or now-playing strips. Panels fulfill this requirement without requiring developers to rebuild foobar2000's native UI capabilities.

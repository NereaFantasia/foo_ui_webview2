# Preferences

The component's settings are on the `File → Preferences → Display → WebView2 UI` page and its three sub-pages, plus a few security exceptions in the Advanced branch.

## WebView2 UI page

The overview page has three groups:

- **Web template**: the active template and its folder. `Manage...` creates, renames or deletes templates (you name a new template yourself); `Open...` opens the folder when it exists.
- **Appearance**: the component language (Auto / English / Chinese) and the default window backdrop. A language change applies to newly opened component dialogs; menus and panel descriptions update after a restart.
- **Tools**: a read-only line with the development-server state, plus buttons to the Advanced branch and the API list.

## Sub-pages

Three sub-pages sit under it in the tree on the left; their names follow the component language after a restart.

- **Window**: remember the window position and size; background mode (takes effect after a restart) and restoring the background window's last Show/Hide state on startup (editable once background mode is checked); minimize to tray and close to tray (disabled with a note while the current theme has not created a tray icon); the thumbnail toolbar playback buttons (after a restart) and playback progress on the taskbar button. Values a theme sets at run time through the `tray.*` / `taskbar.*` API override these for the current session only and are not written back.
- **Performance**: preheat the WebView2 environment at startup (off: it is created when the first window needs it; after a restart); deep-suspend the WebView while the main window is hidden (main window only; not while CDP keep-alive is active); the default content zoom, 50–200% in steps of 25 on top of the system DPI scaling. `Apply` re-zooms open windows except those whose theme has called `window.setZoom`; new windows read the value when they are created.
- **Developer**: Developer Tools (F12); the development server switch and URL, which `Apply` puts into effect at once by reloading the main window and the panels that follow the global template (no restart); CDP remote debugging and its port (1024–65535, default 9222). Developer Tools, the CDP switch and the port take effect after a restart. If you change the port, point the MCP server at it as well (`FB2K_CDP_PORT`, see the [MCP setup guide](/mcp/setup)).

## Draft and apply

Every page holds its edits as a draft: nothing is written until `Apply` or `OK`, `Reset page` returns the draft to the defaults without writing, and `Cancel` discards it. Template create / rename / delete are file operations that run immediately and are not undone by `Cancel`; the active template and any template a loaded panel references cannot be renamed or deleted.

## Advanced branch

Security exceptions stay in `File → Preferences → Advanced → Tools → WebView2 UI`: the CDP background keep-alive, HTTP access to the local network, invalid TLS certificates for `fb.http.*`, and the reserved HSTS entry. Entry names follow the component language. Background mode, deep suspend, Developer Tools, the CDP switch and the development server switch and URL are no longer Advanced entries; a value set there by an earlier version is carried over the first time the new setting is read.

## Related

- [Install a theme](/how-to/install-theme)
- [Debug a theme with the development server](/how-to/dev-server)
- [Run WebView2 UI next to another interface](/how-to/background-mode)

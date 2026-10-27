# Preferences

The component's settings are located on the `File → Preferences → Display → WebView2 UI` page and its three sub-pages, plus security exceptions in the Advanced branch.

## WebView2 UI page

The overview page contains three groups:

- **Web template**: Active template and its directory. `Manage...` creates, renames, or deletes templates (new templates are named by the user); `Open...` opens the template directory in the system file manager when present.
- **Appearance**: Component language (Auto / English / Chinese) and default window backdrop effect (e.g. Mica, Acrylic, or none). Language changes apply to newly opened component dialogs immediately; menus and panel descriptions update after restarting foobar2000.
- **Tools**: Read-only development-server status line, plus buttons linking to the Advanced branch and the public API reference.

## Sub-pages

Three sub-pages sit under it in the left navigation tree; their labels follow the component language after a restart:

- **Window**: Remember window position and size; background mode (takes effect after restart) and restoring the background window's last Show/Hide state on startup (editable once background mode is checked); minimize to tray and close to tray (disabled with an explanatory note when the current theme has not created a tray icon); thumbnail toolbar playback buttons (takes effect after restart) and playback progress on the taskbar button. Values set at runtime by themes through the `tray.*` or `taskbar.*` APIs override these settings for the current session only and are not persisted back.
- **Performance**: Pre-warm the WebView2 environment on startup (when off, created on demand when the first window requires it; takes effect after restart); deep-suspend the WebView while the main window is hidden (main window only; suppressed while CDP keep-alive is active); default content zoom (50%–200% in 25% increments on top of system DPI scaling). Clicking `Apply` immediately re-zooms open windows except those that explicitly called `window.setZoom`; newly created windows read this value at initialization.
- **Developer**: Developer Tools (F12); development server toggle and URL, which `Apply` puts into effect immediately by reloading the main window and all panels following the global template (no restart needed); CDP remote debugging and its listen port (1024–65535, default 9222). Developer Tools, the CDP switch, and the port take effect after a restart. If the port is changed, update the MCP server configuration accordingly (`FB2K_CDP_PORT`, see the [MCP setup guide](/mcp/setup)).

## Draft and Persistence {#draft-and-apply}

Each page holds edits in a temporary draft buffer: settings are only persisted to disk upon clicking `Apply` or `OK`; `Reset page` resets the draft to defaults without writing to disk; `Cancel` discards all unapplied draft changes. Template management operations (create, rename, delete) execute immediately on the filesystem and are not rolled back by `Cancel`; the active template and any template currently referenced by open panels cannot be renamed or deleted.

## Advanced Branch

Security exceptions remain under `File → Preferences → Advanced → Tools → WebView2 UI`: CDP background keep-alive, HTTP requests to the local network (SSRF exceptions), allowing invalid TLS certificates for `fb.http.*`, and reserved HSTS entries. Setting names follow the component language. Background mode, deep suspend, Developer Tools, CDP toggle, and dev server options have moved to the standard Display preferences; earlier settings are automatically migrated upon first read.

## Related

- [Install a theme](/how-to/install-theme)
- [Debug a theme with the development server](/how-to/dev-server)
- [Run WebView2 UI next to another interface](/how-to/background-mode)

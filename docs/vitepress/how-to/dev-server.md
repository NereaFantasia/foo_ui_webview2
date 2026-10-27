# Debug a theme with the development server

During theme development, foobar2000 can load web pages directly from a development server (such as Vite with HMR hot module replacement) instead of static template folders, allowing source modifications to reflect immediately upon save. [Build your first theme](/tutorials/first-theme) walks through a complete end-to-end setup.

## Point foobar2000 at the server

1. Start your local development server and copy its network address, such as `http://localhost:5173`.
2. Open `File → Preferences → Display → WebView2 UI → Developer`.
3. Check `Use development server (HMR hot reload)` and input the address into `URL`. The URL must start with `http://` or `https://` and include a host.
4. Press `Apply`.

The main window and any panels configured to follow the global template reload immediately; no application restart is required. Embedded panels configured with custom templates or URL overrides will similarly load from the dev server on their next reload. Popup windows open `<server>/<page>.html` hosted on the same dev server.

The host grants API trust to pages loaded from this exact address, enabling `fb2k.invoke()` and the SDK to operate normally. Trust is bound to the exact scheme, host, and port: pages on alternative ports, or accessing `127.0.0.1` when configured for `localhost`, cannot call host APIs. See [Which pages can call the API](/reference/security#which-pages-can-call-the-api).

Pages or build scripts can update these preferences programmatically via `fb.ui.setDevServerConfig({ useDevServer, devServerUrl })` (corresponding to host endpoint `window.setDevServerConfig`). These values persist across sessions, taking effect upon the next page reload.

## Troubleshooting fallback to local templates {#if-the-old-theme-appears-instead}

When foobar2000 fails to connect to the specified development server, it falls back to the active local template. Verify that the development server process is actively running and listening on the designated address (configuring `strictPort: true` in Vite prevents silent port drift). Once confirmed, press `Apply` again or restart foobar2000.

## Open the developer tools

- Check `Enable Developer Tools (F12)` on the same preference page. This setting is evaluated during WebView instance creation, requiring a foobar2000 restart to take effect. Once enabled, pressing F12 in any WebView opens DevTools, regardless of whether a dev server is in use.
- When the main window loads from a development server, DevTools opens automatically on launch.
- Embedded panels can toggle DevTools independently in their configuration dialog without requiring a restart: see [Put a page in a panel](./panels.md#give-a-panel-its-own-template-or-url).

The host disables the default WebView2 context menu, so there is no `Inspect Element` item; custom page right-click event handlers remain unaffected.

## Switch back to installed local templates {#go-back-to-the-installed-template}

Uncheck `Use development server (HMR hot reload)` and press `Apply`. To retain the developed theme, compile the production bundle and copy the distribution files into a template folder: see [Install a theme](./install-theme.md).

## Remote debugging and automation {#remote-debugging-for-automation}

The `CDP remote debugging` option on the same preference page exposes the WebView instance via the Chrome DevTools Protocol over a local port (default 9222), enabling automation via the MCP server. This requires an application restart; see the [MCP setup guide](/mcp/setup).

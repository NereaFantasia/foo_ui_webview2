# Debug a theme with the development server

While you work on a theme, foobar2000 can load it from a development server such as Vite instead of a template folder, so changes show up as soon as you save. [Build your first theme](/tutorials/first-theme) walks through a complete setup.

## Point foobar2000 at the server

1. Start your development server and note its address, for example `http://localhost:5173`.
2. Open `File → Preferences → Display → WebView2 UI → Developer`.
3. Tick `Use development server (HMR hot reload)` and type the address into `URL`. It must start with `http://` or `https://` and name a host.
4. Press `Apply`.

The main window and the panels that follow the global template reload at once; no restart is needed. Panels with their own template or URL load from the server too the next time they load their page. Popup windows open `<server>/<page>.html` on the same server.

The host trusts the page it loads from that address, so `fb2k.invoke()` and the SDK work from the dev-server page. Trust goes by the exact scheme, host and port: a page on another port, or on `127.0.0.1` when you entered `localhost`, cannot call the host. See [Which pages can call the API](/reference/security#which-pages-can-call-the-api).

A page or a script can change the same two settings with `fb.ui.setDevServerConfig({ useDevServer, devServerUrl })` (host method `window.setDevServerConfig`). They are stored, but take effect only the next time a page loads.

## If the old theme appears instead

foobar2000 falls back to the installed template when it cannot reach the server. Check that the server is still running and listens on the address you entered (a Vite config with `strictPort: true` refuses to move to another port), then reload: press `Apply` again or restart foobar2000.

## Open the developer tools

- Tick `Enable Developer Tools (F12)` on the same page. The setting is read when a WebView is created, so restart foobar2000 after changing it. With it on, F12 in any WebView opens the developer tools, whether or not the development server is in use.
- When the main window loads its page from the development server, it opens the developer tools by itself.
- A panel can enable the developer tools for itself in its configuration dialog, without a restart: see [Put a page in a panel](./panels.md#give-a-panel-its-own-template-or-url).

There is no `Inspect` context menu; the page's own right-click handling stays in effect.

## Go back to the installed template

Untick `Use development server (HMR hot reload)` and press `Apply`. To keep the theme you were developing, build it and copy the output into a template: see [Install a theme](./install-theme.md).

## Remote debugging for automation

`CDP remote debugging` on the same page exposes the WebView over the Chrome DevTools Protocol on a local port (default 9222), which the MCP server uses. It takes effect after a restart; see the [MCP setup guide](/mcp/setup).

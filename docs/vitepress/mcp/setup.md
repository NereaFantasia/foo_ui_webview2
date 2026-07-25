# Installation and Configuration

## Prerequisites

1. **Node.js 18+** — [download Node.js](https://nodejs.org/)
2. **foobar2000** with the `foo_ui_webview2` component installed and running
3. **CDP remote debugging enabled** in the component's advanced settings; the runtime uses port `9222`

## Installation

### Option 1: run with npx (recommended)

No global installation is required. Point the MCP client at the package:

```json
{
  "foo-ui-webview2": {
    "command": "npx",
    "args": ["-y", "foo-ui-webview2-mcp"],
    "env": {
      "FB2K_CDP_PORT": "9222"
    },
    "type": "stdio"
  }
}
```

### Option 2: install globally

```bash
npm install -g foo-ui-webview2-mcp
foo-ui-webview2-mcp
```

### Option 3: local development

```bash
cd mcp/
npm install
npm run build
npm start
```

## Client configuration

### VS Code (GitHub Copilot)

Create or edit `.vscode/mcp.json` in the workspace root:

```json
{
  "servers": {
    "foo-ui-webview2": {
      "command": "npx",
      "args": ["-y", "foo-ui-webview2-mcp"],
      "env": {
        "FB2K_CDP_PORT": "9222"
      },
      "type": "stdio"
    }
  }
}
```

### Claude Desktop

Edit the client configuration file:

- **Windows**: `%APPDATA%\\Claude\\claude_desktop_config.json`
- **macOS**: `~/Library/Application Support/Claude/claude_desktop_config.json`

```json
{
  "mcpServers": {
    "foo-ui-webview2": {
      "command": "npx",
      "args": ["-y", "foo-ui-webview2-mcp"],
      "env": {
        "FB2K_CDP_PORT": "9222"
      }
    }
  }
}
```

### Cursor

Add the same server configuration under **MCP Servers** in Cursor settings.

## Environment variables

| Variable | Default | Description |
| --- | --- | --- |
| `FB2K_CDP_PORT` | `9222` | WebView2 CDP debugging port used by the MCP client |
| `FB2K_CDP_HOST` | `localhost` | WebView2 CDP host address |
| `FB2K_CDP_TARGET_URL` | unset | URL substring that pins the CDP page target when several WebViews (popups, panels, tray menu overlay) share the port, e.g. `foo-ui-webview2.local`. When set and nothing matches, connecting fails and lists the candidates instead of guessing. |
| `FB2K_ENABLE_EVAL` | unset | Set to `1` or `true` to register `fb2k_evaluate` |

::: warning `FB2K_ENABLE_EVAL`
`fb2k_evaluate` can execute an arbitrary JavaScript expression in the WebView2 page. Keep it disabled outside a trusted development or debugging session.
:::

## CDP connection

### Enable CDP remote debugging

1. Open foobar2000.
2. Go to **Preferences → Advanced → Tools → WebView UI**.
3. Enable **Enable CDP remote debugging on port 9222 (for MCP/AI agents)**.
4. Restart foobar2000.

### Connection sequence

```text
MCP server starts
    │
    ├─ GET http://localhost:9222/json → discover the first non-DevTools page target
    ├─ connect to the target over WebSocket
    ├─ enable Runtime and Page in parallel
    ├─ attempt a 1×1 PNG warm-up capture
    └─ ready for tool calls
```

### Connection failures

| Scenario | Behavior | Action |
| --- | --- | --- |
| foobar2000 is not running | CDP discovery or connection fails | Start foobar2000. |
| CDP is disabled | No page target is available | Enable CDP and restart foobar2000. |
| Host or port does not match | Discovery or connection fails | Set `FB2K_CDP_HOST` / `FB2K_CDP_PORT` to the runtime endpoint. |
| Connection is lost | The cached client is dropped; the next call reconnects | Let the retry sequence complete. |
| A Bridge invocation exceeds the timeout | The call fails after 30 seconds | Check whether foobar2000 or the WebView2 page is responsive. |

### Background behavior & keep-alive

To save memory, the component normally suspends the WebView page (`visibilityState=hidden`, rendering paused, timers throttled) when the window is **minimized**, **hidden to the tray**, or the **session is locked**. A suspended page breaks CDP automation: screenshots stall because no frames are produced, and calls that depend on `requestAnimationFrame` or timers can run into the 30-second invoke timeout.

When **Enable CDP remote debugging** is on, the companion option **Keep WebView active in background while CDP remote debugging is on (tray/minimize/lock)** on the same Advanced page (enabled by default) skips that suspension, so CDP tools stay responsive while the window is minimized, in the tray, or the screen is locked. Disable it if you prefer background power savings over automation stability; toggling it takes effect immediately without a restart. CDP mode additionally disables Chromium's background timer throttling.

| Symptom | Likely cause | Fix |
| --- | --- | --- |
| `fb2k_screenshot` times out while the window is minimized / in the tray | Page suspended (keep-alive disabled) | Restore the window, or enable the keep-alive option |
| Calls stall after ~5 minutes in the background | Background timer throttling on an older component version | Update the component; CDP mode now disables throttling |
| Tools act on the wrong window (for example the tray menu overlay) | Several page targets share port 9222 | Set `FB2K_CDP_TARGET_URL` (for example `foo-ui-webview2.local`) |

::: tip Target pinning
Popups, DUI/CUI panels, and the tray menu overlay are separate CDP page targets on the same port. The client prefers targets with a concrete URL — the tray overlay and the built-in test page both report `about:blank` — and warns on stderr when several candidates exist. Set `FB2K_CDP_TARGET_URL` to pin one deterministically; a filter that matches nothing fails with the candidate list rather than binding to an arbitrary page.
:::

## Pairing with Chrome DevTools MCP

The server can be paired with [chrome-devtools-mcp](https://github.com/ChromeDevTools/chrome-devtools-mcp):

| MCP server | Role | Typical use |
| --- | --- | --- |
| `foo-ui-webview2-mcp` | Semantic Bridge API | Playback, playlists, library, queue, and metadata |
| `chrome-devtools-mcp` | Generic browser inspection and interaction | DOM inspection, input, and visual debugging |

Both clients target the WebView2 CDP endpoint at `localhost:9222` by default.

::: tip Connection ownership
If another CDP client prevents this server from attaching to the selected page target, disconnect that client and retry the MCP tool call.
:::

# Run WebView2 UI next to another interface

Background mode keeps a WebView2 UI window available while Default UI or Columns UI is the main interface, for example as a lyrics or artwork companion window, or to show visualizations while you manage playlists in Default UI.

## Turn it on

1. Open `File → Preferences → Display → WebView2 UI → Window`, tick `Background mode: keep the WebView running while another user interface is active` and press `Apply`. The label is translated when the component language is Chinese.
2. Choose the other interface under `File → Preferences → Display → Default User Interface`.
3. Restart foobar2000. Background mode takes effect after a restart.
4. Show the window with `View → WebView2 UI → Show/Hide Window`.

The window loads the global template, like the main window does. Tick `Restore background window visibility on startup` on the same page to bring it back as you left it when foobar2000 starts.

## When the menu entry reads "Preferences..."

Without a WebView2 UI window, for example when the other interface is active and background mode is off, the menu entry under `View → WebView2 UI` reads `Preferences...` and opens the WebView2 UI preferences page instead.

## Keep the page working while it is hidden

When the window is minimized, hidden to the tray or the session is locked, the page's `document.visibilityState` becomes `hidden`: `requestAnimationFrame` stops and timers are heavily throttled. Update the page from events (`fb2k.on(...)` or `fb.on(...)`) rather than polling loops, so it is correct again when it comes back. When you automate the player over CDP or MCP, the keep-alive option in the [MCP setup guide](/mcp/setup) keeps the page active in these states.

# Run WebView2 UI next to another interface

Background mode keeps an independent WebView2 UI companion window available while Default UI or Columns UI remains the primary interface, suitable for desktop lyrics, artwork galleries, or real-time audio visualizers alongside standard playlist navigation.

## Enable background mode {#turn-it-on}

1. Open `File → Preferences → Display → WebView2 UI → Window`, check `Background mode: keep the WebView running while another user interface is active`, and click `Apply`. (In Chinese UI environments, the option reads `后台模式：使用其他界面时仍运行 WebView`).
2. Select your primary interface under `File → Preferences → Display → Default User Interface`.
3. Restart foobar2000. Background mode takes effect upon restart.
4. Toggle window visibility via `View → WebView2 UI → Show/Hide Window`.

The companion window loads the global active template, identical to standalone main window behavior. Check `Restore background window visibility on startup` on the same page to preserve and restore window visibility across application launches.

## When the menu item displays "Preferences..." {#when-the-menu-entry-reads-preferences}

If no WebView2 UI window instance exists (for example, when an alternate interface is active and background mode is disabled), the menu item under `View → WebView2 UI` displays `Preferences...` and opens the WebView2 UI preference dialog.

## Page lifecycle and throttling while hidden {#keep-the-page-working-while-it-is-hidden}

When the window is minimized, hidden to the system tray, or the OS session is locked, the web page's `document.visibilityState` switches to `hidden`: the browser engine pauses `requestAnimationFrame` callbacks and heavily throttles `setInterval`/`setTimeout` timers. Applications should adopt event-driven designs (`fb2k.on(...)` or `fb.on(...)`) rather than polling loops, ensuring the interface reflects state immediately upon restoration. If automating player operations via CDP or MCP, consult the keep-alive options in the [MCP setup guide](/mcp/setup) to maintain background execution.

# Install the component

## System requirements

| Item | Requirement |
| --- | --- |
| OS | Windows 10/11 (64-bit) |
| foobar2000 | v2.0 or later (64-bit) |
| WebView2 Runtime | Usually preinstalled on Windows 10/11; otherwise download it from Microsoft |

::: tip Note
foobar2000 v2.0 is the 64-bit cutoff. If you still run v1.x, upgrade to v2.0+ first.
:::

## Install the component package

1. **Download the package**: get `foo_ui_webview2-<version>.fb2k-component` from the release page; `<version>` is the component version.
2. **Install it through foobar2000**: open or double-click the package so foobar2000 shows its component install confirmation.
3. **Confirm**: click `Yes` and let foobar2000 install the package contents.
4. **Restart foobar2000.**
5. **Select the UI**: `File → Preferences → Display → Default User Interface` → choose `Webview2 UI`.

::: info INFO
`.fb2k-component` is the official foobar2000 component package format. Install it with foobar2000's component installer; do not treat the package as a plain zip that you unpack by hand into the profile.
:::

To use the component as a panel inside Default UI or Columns UI instead of as the whole interface, skip step 5 and follow [Put a page in a panel](./panels.md).

## Manual binary install (not recommended)

Release packages contain architecture-specific binaries (`foo_ui_webview2.dll`, `WebView2Loader.dll`, and an `x64/` layout). Copying only a single DLL into an arbitrary `components` folder is incomplete and layout-dependent. Manual extraction is only for advanced recovery.

## Verify the installation

After a successful install you should see:

- The Default User Interface list includes `Webview2 UI`
- Menu entry `View → WebView2 UI → Show/Hide Window`
- Preferences page `File → Preferences → Display → WebView2 UI`
- Advanced branch `File → Preferences → Advanced → Tools → WebView2 UI`

The component installs no theme. Until a template holds an `index.html`, the main window shows a built-in test page whose buttons try out the API, and a panel shows "Frontend not found. Please install a template."; [Install a theme](./install-theme.md) and [Build your first theme](/tutorials/first-theme) cover getting one.

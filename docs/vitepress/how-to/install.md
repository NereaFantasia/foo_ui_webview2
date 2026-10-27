# Install the component

## System requirements

| Item | Requirement |
| --- | --- |
| OS | Windows 10/11 (64-bit / x64) |
| foobar2000 | v2.0 or later (64-bit recommended; 32-bit supported) |
| WebView2 Runtime | Usually preinstalled on Windows 10/11; otherwise download it from Microsoft |

::: tip Note
foobar2000 introduced 64-bit architecture with v2.0. The `.fb2k-component` package bundles both x64 and 32-bit binaries. If you still run foobar2000 v1.x, upgrade to v2.0+ first.
:::

## Install the component package

1. **Download the package**: get `foo_ui_webview2-<version>.fb2k-component` from the release page (`<version>` is the component version)
2. **Install through foobar2000**: open or double-click the package to open the foobar2000 component install confirmation dialog
3. **Confirm**: click `Yes` to let foobar2000 install the package files
4. **Restart foobar2000**: restart the player to load the component
5. **Select the UI**: navigate to `File → Preferences → Display → Default User Interface` and select `Webview2 UI`

::: info INFO
`.fb2k-component` is the official foobar2000 component package format. Install it with foobar2000's component installer; do not treat the package as a plain zip that you unpack by hand into the profile.
:::

To use the component as a panel inside Default UI or Columns UI instead of as the whole interface, skip step 5 and follow [Put a page in a panel](./panels.md).

## Manual binary install (not recommended)

Release packages contain architecture-specific binaries (`foo_ui_webview2.dll`, `WebView2Loader.dll`, and an `x64/` layout). Copying only a single DLL into an arbitrary `components` folder is incomplete and layout-dependent. Manual extraction is only for advanced recovery.

## Verify the installation

After a successful installation, check the following integration points:

- `Webview2 UI` appears in the Default User Interface list
- Menu item `View → WebView2 UI → Show/Hide Window` is available
- Preferences page `File → Preferences → Display → WebView2 UI` is registered
- Advanced setting branch `File → Preferences → Advanced → Tools → WebView2 UI` appears

The component does not bundle default theme files. When the theme folder has no `index.html`, the standalone main window displays a built-in diagnostic test page whose interactive controls test each Bridge API, while embedded panels display “Frontend not found. Please install a template.”. To load a theme, see [Install a theme](./install-theme.md) and [Build your first theme](/tutorials/first-theme).

# Install a theme

A theme is a folder with an `index.html`, called a template. Templates live under `<profile>\webview-ui\`, one folder per template, and the active one fills the main window. [How a page is found](/concepts/page-loading) explains the full lookup.

## Create a template and copy the theme into it

1. Open `File → Preferences → Display → WebView2 UI`.
2. Press `Manage...` and choose `Create...`. Enter a name made of letters, digits, `-` and `_`, then confirm. foobar2000 creates `<profile>\webview-ui\<name>\` with a placeholder `index.html` and selects the new template in the `Template` box.
3. Press `Open...` to open the template folder in File Explorer.
4. Copy the theme's files into it, replacing the placeholder `index.html`. For a project built with a bundler, copy the contents of its output folder (for Vite, `dist`), not the folder itself.
5. Press `Apply`. The main window, and every panel that follows the global template, reloads with the theme.

`Apply` refuses a template whose folder has no `index.html`, or an empty one.

## Switch between installed templates

Pick another entry in the `Template` box and press `Apply`. Creating, renaming and deleting templates are file operations that happen at once and are not undone by `Cancel`; the active template and any template a loaded panel uses cannot be renamed or deleted.

## Use a template in one panel only

A panel can load its own template instead of the global one: see [Put a page in a panel](./panels.md#give-a-panel-its-own-template-or-url).

## When the development server is on

While `Use development server (HMR hot reload)` is ticked on the `Developer` page, pages load from the server URL and a template change does not reload them. Untick it and press `Apply` to see the installed template. See [Debug a theme with the development server](./dev-server.md).

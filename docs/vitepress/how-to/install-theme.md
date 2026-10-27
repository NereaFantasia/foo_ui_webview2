# Install a theme

A theme is organized as a directory containing an `index.html` entry point, referred to as a template. All templates reside under `<profile>\webview-ui\`, with one folder per template, and the global active template powers the main window. [How a page is found](/concepts/page-loading) outlines the full resolution hierarchy.

## Create a template and deploy theme assets {#create-a-template-and-copy-the-theme-into-it}

1. Open `File → Preferences → Display → WebView2 UI`.
2. Click `Manage...` and choose `Create...`. Enter an identifier using letters, numbers, `-`, and `_`, then confirm. foobar2000 generates `<profile>\webview-ui\<name>\` with a starter `index.html` and selects this new entry in the `Template` dropdown.
3. Click `Open...` to reveal the template folder in File Explorer.
4. Copy your theme files into the folder, replacing the placeholder `index.html`. For projects built with a bundler, copy the contents of the distribution folder (such as `dist` in Vite), not the wrapper folder itself.
5. Click `Apply`. The main window and all panels following the global template will reload with the new theme.

The preferences page rejects templates whose directory lacks an `index.html` or contains a 0-byte empty file.

## Switch between installed templates

Select another entry from the `Template` dropdown and press `Apply`. Creating, renaming, and deleting templates are immediate file operations on disk that cannot be reverted via `Cancel`. The currently active template and any template in use by an active panel cannot be renamed or deleted.

## Use a dedicated template for a single panel {#use-a-template-in-one-panel-only}

An embedded panel can load its own template independently of the global setting: see [Put a page in a panel](./panels.md#give-a-panel-its-own-template-or-url).

## Behavior when the development server is active {#when-the-development-server-is-on}

When `Use development server (HMR hot reload)` is checked under the `Developer` page, all windows load from the configured server URL, and switching templates will not trigger a reload. Uncheck the option and press `Apply` to return to the installed local template. See [Debug a theme with the development server](./dev-server.md).

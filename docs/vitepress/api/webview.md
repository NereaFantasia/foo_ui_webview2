# WebView API

Methods of the `webview` namespace. Its events, such as `webview:processFailed`, are listed in [Events API](./events.md).

## webview

### webview.getSource

<!-- api-schema:begin webview.getSource -->
Report where the host loaded the calling WebView's page from: the development server, a URL, a template folder, or the built-in "Frontend not found" page. The host records this when it submits the navigation; a page that later navigates elsewhere by itself is still reported by that record. Fails with `NOT_FOUND` when the caller is not a WebView of this plugin or the host has not loaded a page into it.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `source` | `"devServer" \| "url" \| "panelTemplate" \| "activeTemplate" \| "componentDirectory" \| "defaultTemplate" \| "builtInPage"` | Where the page came from. `devServer`: the development server. `url`: a URL from the panel's configuration, or an absolute URL passed to `window.createPopup`. `panelTemplate`: the panel's own template. `activeTemplate`: the global active template. `componentDirectory`: `foo_ui_webview2_resources\dist` in the component's folder. `defaultTemplate`: `webview-ui\default`, reached because the folders before it had no `index.html`. `builtInPage`: the page shown when no folder had an `index.html`. |
| `directory` | `string` | Folder mapped to `https://foo-ui-webview2.local/`; present for the four folder sources. |
| `templateName` | `string` | Name of the template folder that was loaded; present for `panelTemplate`, `activeTemplate` and `defaultTemplate`. |
| `url` | `string` | Address the host navigated to; present for `devServer` and `url`. |
| `activeTemplateName` | `string` | The global active template as configured now, which can differ from the one loaded. |
| `templatesDirectory` | `string` | Folder that holds the templates, `webview-ui` under the foobar2000 profile. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

The result describes the page as the host loaded it. `activeTemplateName` is read at the time of the call: for a page loaded from the active template, comparing it with `templateName` shows whether the user has picked another template since.

```js
const source = await fb2k.invoke('webview.getSource');
if (source.success === false) throw new Error(source.error);
if (source.directory) {
	console.log(`Loaded ${source.templateName ?? 'a page'} from ${source.directory}`);
}
```

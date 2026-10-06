# fb.webview WebView

`fb.webview` reports facts about the WebView the page runs in.

## getSource()

Signature: `fb.webview.getSource(): Promise<WebviewGetSourceResponse>`

Reports where the host loaded this page from. `source` is one of `devServer`, `url`, `panelTemplate`, `activeTemplate`, `componentDirectory`, `defaultTemplate` and `builtInPage`. A page loaded from a template folder also gets the mapped `directory`, and `templateName` when the folder is a template; a page loaded from the development server or a URL gets the `url` the host navigated to. The answer is the host's record from when it submitted the navigation. Only `activeTemplateName` is read at the time of the call, so for a page loaded from the active template, a different `activeTemplateName` means the user has picked another template since. The call fails with `code: 'NOT_FOUND'` when the host has not loaded a page into this WebView.

```javascript
const res = await fb.webview.getSource();
if (res.success === false) throw new Error(res.error);
if (res.source === 'activeTemplate' && res.templateName !== res.activeTemplateName) {
	console.log(`Reload to switch from ${res.templateName} to ${res.activeTemplateName}`);
}
```

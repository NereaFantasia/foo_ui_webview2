# fb.webview WebView

`fb.webview` 报告页面所在 WebView 本身的情况。

## getSource()

签名：`fb.webview.getSource(): Promise<WebviewGetSourceResponse>`

报告宿主把这个页面从哪里加载。`source` 取 `devServer`、`url`、`panelTemplate`、`activeTemplate`、`componentDirectory`、`defaultTemplate`、`builtInPage` 之一。从文件夹加载的页面另带映射的 `directory`，文件夹是模板时还带 `templateName`；从开发服务器或某个 URL 加载的页面带宿主导航到的 `url`。结果是宿主提交导航时记下的；只有 `activeTemplateName` 在调用时读取，所以页面来自活动模板时，`activeTemplateName` 与 `templateName` 不同就说明用户之后换过模板。宿主还没给这个 WebView 加载页面时，以 `code: 'NOT_FOUND'` 失败。

```javascript
const res = await fb.webview.getSource();
if (res.success === false) throw new Error(res.error);
if (res.source === 'activeTemplate' && res.templateName !== res.activeTemplateName) {
	console.log(`重新加载后会从 ${res.templateName} 换到 ${res.activeTemplateName}`);
}
```

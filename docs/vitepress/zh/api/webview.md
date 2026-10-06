# WebView API

`webview` 命名空间的方法。它的事件（如 `webview:processFailed`）见[事件 API](./events.md)。

## webview

### webview.getSource

<!-- api-schema:begin webview.getSource -->
报告宿主把调用方 WebView 的页面从哪里加载：开发服务器、某个 URL、某个模板文件夹，或内置的「Frontend not found」页面。宿主在提交导航时记下来源；页面之后自己导航到别处，报告的仍是这份记录。调用方不是本插件的 WebView，或宿主还没给它加载页面时，以 `NOT_FOUND` 失败。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `source` | `"devServer" \| "url" \| "panelTemplate" \| "activeTemplate" \| "componentDirectory" \| "defaultTemplate" \| "builtInPage"` | 页面的来源。`devServer`：开发服务器。`url`：面板配置里的 URL，或传给 `window.createPopup` 的绝对 URL。`panelTemplate`：面板自己的模板。`activeTemplate`：全局活动模板。`componentDirectory`：组件文件夹里的 `foo_ui_webview2_resources\dist`。`defaultTemplate`：`webview-ui\default`，因为它前面的文件夹都没有 `index.html` 才落到这里。`builtInPage`：所有文件夹都没有 `index.html` 时显示的页面。 |
| `directory` | `string` | 映射到 `https://foo-ui-webview2.local/` 的文件夹；只在四种文件夹来源下出现。 |
| `templateName` | `string` | 加载的模板文件夹名；只在 `panelTemplate`、`activeTemplate`、`defaultTemplate` 下出现。 |
| `url` | `string` | 宿主导航到的地址；只在 `devServer` 与 `url` 下出现。 |
| `activeTemplateName` | `string` | 现在配置的全局活动模板，可能与已加载的那个不同。 |
| `templatesDirectory` | `string` | 存放模板的文件夹，即 foobar2000 profile 下的 `webview-ui`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

结果描述的是宿主加载这个页面时的情况。`activeTemplateName` 在调用时读取：页面来自活动模板时，拿它和 `templateName` 比较，就能知道加载之后用户有没有换过模板。

```js
const source = await fb2k.invoke('webview.getSource');
if (source.success === false) throw new Error(source.error);
if (source.directory) {
	console.log(`从 ${source.directory} 加载了 ${source.templateName ?? '页面'}`);
}
```

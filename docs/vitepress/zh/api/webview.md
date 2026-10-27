# WebView API

`webview` 命名空间的方法。它的事件（如 `webview:processFailed`）见[事件 API](./events.md)。

## webview

### webview.getSource

<!-- api-schema:begin webview.getSource -->
获取调用方 WebView 页面的加载来源。来源包括开发服务器、自定义 URL、模板文件夹或内置「Frontend not found」缺省页。宿主在发起导航时记录来源；页面后续自行重定向仍返回该记录。调用方非本插件 WebView 或尚未加载页面时返回 `NOT_FOUND`

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `source` | `"devServer" \| "url" \| "panelTemplate" \| "activeTemplate" \| "componentDirectory" \| "defaultTemplate" \| "builtInPage"` | 页面加载来源。`devServer`：开发服务器；`url`：面板配置 URL 或传给 `window.createPopup` 的绝对 URL；`panelTemplate`：面板独立模板；`activeTemplate`：全局活动模板；`componentDirectory`：组件内置资源目录（`foo_ui_webview2_resources\dist`）；`defaultTemplate`：回退默认模板（`webview-ui\default`，各前序目录均缺失 `index.html` 时使用）；`builtInPage`：各模板均未找到 `index.html` 时展示的内置缺省页 |
| `directory` | `string` | 映射到虚拟域 `https://foo-ui-webview2.local/` 的本地目录绝对路径（仅在四种目录类来源下存在） |
| `templateName` | `string` | 已加载的模板文件夹名称（仅在 `panelTemplate`、`activeTemplate` 与 `defaultTemplate` 来源下存在） |
| `url` | `string` | 宿主导航的目标地址（仅在 `devServer` 与 `url` 来源下存在） |
| `activeTemplateName` | `string` | 当前配置的全局活动模板名称（可能与实际加载的模板不一致） |
| `templatesDirectory` | `string` | 存放模板的根目录绝对路径（对应配置目录下的 `webview-ui`） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

返回结果描述宿主加载当前页面时的上下文。`activeTemplateName` 在调用时动态读取：对于来自活动模板的页面，比对该字段与 `templateName` 即可获知用户是否在页面加载后切换了全局活动模板。

```js
const source = await fb2k.invoke('webview.getSource');
if (source.success === false) throw new Error(source.error);
if (source.directory) {
	console.log(`从 ${source.directory} 加载了 ${source.templateName ?? '页面'}`);
}
```

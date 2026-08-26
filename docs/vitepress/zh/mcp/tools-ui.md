# UI Testing 工具 

默认注册 3 个直接使用 CDP、而不经过 Bridge 的 UI 测试工具。第 4 个条件工具 `fb2k_evaluate` 仅在 `FB2K_ENABLE_EVAL` 为 `1` 或 `true` 时注册。

## fb2k_screenshot 

截取 WebView2 页面截图，返回 base64 编码的 PNG 图片。

- **传输**: `Page.captureScreenshot` (CDP)

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| fullPage | boolean | 否 | 为 `true` 时，截图前会暂时把视口调整为测得的内容尺寸；省略时截取当前视口 |

**返回**: MCP `image` 类型内容（base64 PNG）

::: tip 预热行为
每次成功连接 CDP 后，都会在真实截图前尽力执行一次 1×1 PNG 截图。实际截图耗时取决于宿主、页面和渲染状态，不保证固定延迟。
:::

## fb2k_dom_snapshot 

获取页面的简化 DOM 可访问性树。

- **参数**: 无
- **传输**: `Runtime.evaluate` (CDP)

**返回**:

每行一个元素，形如 `tag#id.class "textContent"`，缩进表示层级关系。仅当元素只有一个文本子节点时才附带文本，且截断到 80 个字符。

## `fb2k_evaluate`

在 WebView2 中执行 JavaScript 表达式。

- **注册条件**: `FB2K_ENABLE_EVAL=1` 或 `FB2K_ENABLE_EVAL=true`
- **传输**: `Runtime.evaluate` (CDP)

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| expression | string | 是 | 要执行的 JavaScript 表达式 |

::: danger 安全限制
此工具默认关闭。需设置环境变量 `FB2K_ENABLE_EVAL=1` 才会注册。仅建议在开发环境使用。
:::

## fb2k_console_messages 

获取最近 100 条控制台日志。

- **参数**: 无
- **传输**: 读取 `window.__fb2kMcpConsoleLogs` (CDP)

**返回**:

```text
[info] Page loaded in 45ms
[warn] Missing artwork for track 3
[error] Failed to connect to lyrics server
```

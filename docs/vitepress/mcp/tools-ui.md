# UI Testing 工具 

页面截图、DOM 检查和控制台日志。共 4 个工具。这些工具直接使用 CDP 协议，不经过 Bridge。

## fb2k_screenshot 

截取 WebView2 页面截图，返回 base64 编码的 PNG 图片。

- **传输**: `Page.captureScreenshot` (CDP)

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| fullPage | boolean | ? | 是否截取完整页面（默认 false） |

**返回**: MCP `image` 类型内容（base64 PNG）

::: tip 性能
首次截图通过 1×1 像素预热已优化至 ~330ms（原始冷启动需 ~8s）。
:::

## fb2k_dom_snapshot 

获取页面的简化 DOM 可访问性树。

- **参数**: 无
- **传输**: `Runtime.evaluate` (CDP)

**返回**:

每行一个元素，格式: tag#id.class "textContent"
缩进表示层级关系

## fb2k_evaluate 

在 WebView2 中执行 JavaScript 表达式。

- **传输**: `Runtime.evaluate` (CDP)

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| expression | string | ? | JavaScript 表达式 |

::: danger 安全限制
此工具默认关闭。需设置环境变量 `FB2K_ENABLE_EVAL=1` 才会注册。仅建议在开发环境使用。
:::

## fb2k_console_messages 

获取最近 100 条控制台日志。

- **参数**: 无
- **传输**: 读取 `window.__fb2kMcpConsoleLogs` (CDP)

**返回**:

[info] Page loaded in 45ms
[warn] Missing artwork for track 3
[error] Failed to connect to lyrics server

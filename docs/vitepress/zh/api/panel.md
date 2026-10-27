# Panel 面板 API

提供 Default UI (DUI) 与 Columns UI (CUI) 面板容器的配置读取、运行时属性更新与生命周期事件交互。

## Panel API - 面板配置

### panel.getConfig

<!-- api-schema:begin panel.getConfig -->
获取当前调用方面板的配置信息（仅 DUI 元素与 CUI 面板拥有面板配置，在独立窗口模式下调用将失败）

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `config` | `PanelConfig` | 面板当前的完整配置对象 |
| `config.panelName` | `string` | 面板自定义显示名称 |
| `config.templateName` | `string` | 面板加载的页面模板名称 |
| `config.edgeStyle` | `integer` | 面板边框样式（0 无边框、1 凹陷立体效果、2 纯灰边框） |
| `config.urlOverride` | `string` | 覆盖模板的自定义加载 URL（未设置时为空字符串） |
| `config.transparentBackground` | `boolean` | 面板是否启用透明背景渲染 |
| `config.grabFocus` | `boolean` | 点击面板时是否主动获取键盘焦点 |
| `config.enableDragDrop` | `boolean` | 是否允许向面板拖放文件 |
| `config.enableDevTools` | `boolean` | 是否启用 WebView2 开发者工具 F12/右键审查 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### panel.setConfig

<!-- api-schema:begin panel.setConfig -->
更新当前调用方面板的配置项（仅允许修改声明的特定配置键，省略的键保持原值不变；模板或调试等其余字段仅支持通过面板配置对话框调整）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `panelName` | `string` | 否 | 面板自定义显示名称 |
| `transparentBackground` | `boolean` | 否 | 面板是否启用透明背景渲染 |
| `grabFocus` | `boolean` | 否 | 点击面板时是否主动获取键盘焦点 |
| `enableDragDrop` | `boolean` | 否 | 是否允许向面板拖放文件 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `changed` | `boolean` | 传入配置是否导致面板实际状态发生改变 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

::: warning 安全与权限约束
`enableDevTools`、`urlOverride` 与 `templateName` 涉及模板加载与调试安全，仅支持通过面板配置对话框手动修改，禁止通过 API 动态覆盖。
:::

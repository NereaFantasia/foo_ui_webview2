# Panel 面板 API

`panel` 命名空间的方法。

## Panel API - 面板配置

### panel.getConfig

<!-- api-schema:begin panel.getConfig -->
报告调用方面板的配置。只有 DUI 元素与 CUI 面板才有面板配置；独立窗口上调用失败。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `config` | `PanelConfig` | 配置。 |
| `config.panelName` | `string` | 面板显示名。 |
| `config.templateName` | `string` | 面板加载的页面模板名。 |
| `config.edgeStyle` | `integer` | 面板边框样式：`0` 无边框，`1` 凹陷，`2` 灰边。 |
| `config.urlOverride` | `string` | 代替模板加载的 URL；没有时为空。 |
| `config.transparentBackground` | `boolean` | 面板是否透明背景。 |
| `config.grabFocus` | `boolean` | 点击是否让面板获得键盘焦点。 |
| `config.enableDragDrop` | `boolean` | 是否允许拖放到面板。 |
| `config.enableDevTools` | `boolean` | 是否启用开发者工具。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### panel.setConfig

<!-- api-schema:begin panel.setConfig -->
修改调用方面板的配置。页面只能改下面这些键，省略的键保持原值；其余字段只能经面板对话框修改。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `panelName` | `string` | 否 | 面板显示名。 |
| `transparentBackground` | `boolean` | 否 | 面板是否透明背景。 |
| `grabFocus` | `boolean` | 否 | 点击是否让面板获得键盘焦点。 |
| `enableDragDrop` | `boolean` | 否 | 是否允许拖放到面板。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `changed` | `boolean` | 是否有改动。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

::: warning
`enableDevTools`、`urlOverride`、`templateName` 仅可通过配置对话框修改。
:::

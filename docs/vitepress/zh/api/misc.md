# Misc 杂项 API

（v1.2.0+）提供系统路径查询和常用 UI 命令。

## Misc API - 系统路径与命令

### misc.getFoobarPath

<!-- api-schema:begin misc.getFoobarPath -->
报告 foobar2000 的安装目录。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 该目录的原生路径。 |
| `value` | `string` | 同一个目录；保留给读 `value` 的主题。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const result = await fb2k.invoke('misc.getFoobarPath');
if (result.success === false) throw new Error(result.error);
console.log('安装目录:', result.path);
```

### misc.getProfilePath

<!-- api-schema:begin misc.getProfilePath -->
报告 foobar2000 的配置目录，配置与组件都在这里。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 该目录的原生路径。 |
| `value` | `string` | 同一个目录；保留给读 `value` 的主题。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const result = await fb2k.invoke('misc.getProfilePath');
if (result.success === false) throw new Error(result.error);
console.log('配置目录:', result.path);
```

### misc.getComponentPath

<!-- api-schema:begin misc.getComponentPath -->
报告组件 DLL 所在的目录。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 该目录的原生路径。 |
| `value` | `string` | 同一个目录；保留给读 `value` 的主题。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const result = await fb2k.invoke('misc.getComponentPath');
if (result.success === false) throw new Error(result.error);
console.log('插件目录:', result.path);
```

### misc.showConsole

<!-- api-schema:begin misc.showConsole -->
打开 foobar2000 控制台窗口。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('misc.showConsole');
```

### misc.showPreferences

<!-- api-schema:begin misc.showPreferences -->
打开 foobar2000 首选项对话框。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('misc.showPreferences');
```

### misc.showLibrarySearch

<!-- api-schema:begin misc.showLibrarySearch -->
打开媒体库搜索窗口，可预填一个查询。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `query` | `string` | 否 | 预填进搜索框的查询；为空则打开空白搜索。默认 `""`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `query` | `string` | 窗口打开时使用的查询。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### misc.showPopupMessage

<!-- api-schema:begin misc.showPopupMessage -->
显示 foobar2000 的弹出消息对话框。对话框显示出来就返回，不等它关闭。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `string` | 是 | 对话框正文。不能为空。 |
| `title` | `string` | 否 | 标题栏文本。默认 `"Message"`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 只认 `message`；带 `msg` 键的调用以 `INVALID_PARAMS`（`unknown parameter 'msg'`）拒绝。

### misc.restart

<!-- api-schema:begin misc.restart -->
让 foobar2000 重启。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### misc.exit

<!-- api-schema:begin misc.exit -->
让 foobar2000 退出。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
// 获取路径
const res = await fb2k.invoke('misc.getFoobarPath');
if (res.success === false) throw new Error(res.error);
const { path } = res;
console.log('foobar2000 path:', path);

// 显示控制台
await fb2k.invoke('misc.showConsole');

// 弹出消息
await fb2k.invoke('misc.showPopupMessage', { message: 'Hello!', title: 'Test' });
```

## 契约补充

以下补充这些方法的完整参数契约，不改变前文的已有说明。

<!-- phase3-supplement:misc.showPopupMessage -->
### Contract 补充：`misc.showPopupMessage`

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `message` | `string` | 是 | — | 对话框正文，不能为空。 |
| `title` | `string` | 否 | `Message` | 标题栏文本。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `success` | `boolean` | 否 |

语义：省略 `title` 用 handler 默认值。`message` 缺失或为空串以 `INVALID_PARAMS`（`message is required`）拒绝，带 `msg` 键的调用被当作未知参数拒绝；否则弹出对话框并立即返回 `success: true`，不等它关闭。

```js
await fb2k.invoke('misc.showPopupMessage', { message: '导出完成' });
```

# Misc 杂项 API

提供 foobar2000 系统路径查询与常用原生界面命令交互。

## Misc API - 系统路径与命令

### misc.getFoobarPath

<!-- api-schema:begin misc.getFoobarPath -->
获取 foobar2000 主程序所在的安装目录路径

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 原生目录完整文件路径 |
| `value` | `string` | 原生目录完整文件路径（保留供兼容读取 value 的主题） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const result = await fb2k.invoke('misc.getFoobarPath');
if (result.success === false) throw new Error(result.error);
console.log('安装目录:', result.path);
```

### misc.getProfilePath

<!-- api-schema:begin misc.getProfilePath -->
获取 foobar2000 用户配置目录路径（包含配置文件与已安装组件）

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 原生目录完整文件路径 |
| `value` | `string` | 原生目录完整文件路径（保留供兼容读取 value 的主题） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const result = await fb2k.invoke('misc.getProfilePath');
if (result.success === false) throw new Error(result.error);
console.log('配置目录:', result.path);
```

### misc.getComponentPath

<!-- api-schema:begin misc.getComponentPath -->
获取当前组件 DLL 文件所在的安装目录路径

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 原生目录完整文件路径 |
| `value` | `string` | 原生目录完整文件路径（保留供兼容读取 value 的主题） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const result = await fb2k.invoke('misc.getComponentPath');
if (result.success === false) throw new Error(result.error);
console.log('插件目录:', result.path);
```

### misc.showConsole

<!-- api-schema:begin misc.showConsole -->
弹出并激活 foobar2000 宿主控制台日志窗口

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('misc.showConsole');
```

### misc.showPreferences

<!-- api-schema:begin misc.showPreferences -->
弹出并激活 foobar2000 宿主首选项设置对话框

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('misc.showPreferences');
```

### misc.showLibrarySearch

<!-- api-schema:begin misc.showLibrarySearch -->
弹出并激活媒体库搜索窗口（支持预填搜索查询表达式）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `query` | `string` | 否 | 预填入搜索框的查询表达式（传入空字符串或省略时展示空白搜索框）默认 `""`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `query` | `string` | 搜索窗口打开时实际采用的查询表达式 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### misc.showPopupMessage

<!-- api-schema:begin misc.showPopupMessage -->
弹出原生提示消息对话框（对话框弹出后立即返回，不阻塞等待用户确认关闭）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `string` | 是 | 提示对话框的正文内容不能为空。 |
| `title` | `string` | 否 | 提示对话框的标题栏文本默认 `"Message"`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 参数键名严格校验：仅接受 `message`；传入未声明的 `msg` 键将以 `INVALID_PARAMS` 错误拒绝。

### misc.restart

<!-- api-schema:begin misc.restart -->
请求重启 foobar2000 应用程序

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### misc.exit

<!-- api-schema:begin misc.exit -->
请求正常退出 foobar2000 应用程序

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
| `message` | `string` | 是 | — | 对话框正文，不能为空 |
| `title` | `string` | 否 | `Message` | 标题栏文本 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `success` | `boolean` | 否 |

省略 `title` 时采用默认值 `"Message"`。`message` 缺失或为空字符串时以 `INVALID_PARAMS` 错误拒绝；传入未声明的参数键（例如 `msg`）同样以参数校验失败拒绝；参数合法时弹出系统对话框并立即返回 `success: true`，不阻塞等待对话框关闭。

```js
await fb2k.invoke('misc.showPopupMessage', { message: '导出完成' });
```

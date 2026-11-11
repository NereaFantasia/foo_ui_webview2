# fb.misc 杂项工具

`fb.misc` 汇聚 foobar2000 宿主环境控制与路径查询接口：包含安装目录、配置目录与组件目录获取，控制台、首选项与媒体库搜索原生窗口唤起，系统弹窗提示，以及宿主进程的重启与退出控制。

## exit()

签名：`fb.misc.exit(): Promise<MiscExitResponse>`

请求 foobar2000 宿主主进程正常退出。

```javascript
const res = await fb.misc.exit();
if (res.success === false) throw new Error(res.error);
```

## getComponentPath()

签名：`fb.misc.getComponentPath(): Promise<MiscGetComponentPathResponse>`

返回组件 DLL 文件所在目录的绝对路径，赋值于 `path` 字段。`value` 字段包含相同路径，作为向后兼容保留。

```javascript
const r = await fb.misc.getComponentPath();
if (r.success === false) throw new Error(r.error);
console.log(r.path); // 组件目录
```

## getFoobarPath()

签名：`fb.misc.getFoobarPath(): Promise<MiscGetFoobarPathResponse>`

返回 foobar2000 应用程序安装目录的绝对路径，赋值于 `path` 与 `value` 字段。

```javascript
const res = await fb.misc.getFoobarPath();
if (res.success === false) throw new Error(res.error);
const { path } = res;
```

## getProfilePath()

签名：`fb.misc.getProfilePath(): Promise<MiscGetProfilePathResponse>`

返回 foobar2000 用户配置目录（Profile 目录，存放配置文件与用户组件）的绝对路径，赋值于 `path` 与 `value` 字段。

```javascript
const res = await fb.misc.getProfilePath();
if (res.success === false) throw new Error(res.error);
const { path } = res;
```

## restart()

签名：`fb.misc.restart(): Promise<MiscRestartResponse>`

请求 foobar2000 宿主主进程重启。

```javascript
await fb.misc.restart();
```

## showConsole()

签名：`fb.misc.showConsole(): Promise<MiscShowConsoleResponse>`

打开 foobar2000 原生控制台窗口。若需向控制台输出日志文本，请使用 [fb.console](./console.md)。

```javascript
await fb.misc.showConsole();
```

## showLibrarySearch(query?)

签名：`fb.misc.showLibrarySearch(query?: string): Promise<MiscShowLibrarySearchResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `query` | `string` | 否 | 预填充至媒体库搜索输入框的查询表达式；空字符串或省略时显示空白搜索框 |

打开 foobar2000 媒体库搜索原生窗口。响应对象中的 `query` 字段记录窗口初始化时应用的实际查询表达式。

```javascript
await fb.misc.showLibrarySearch('artist IS Beatles');
```

## showPopupMessage(message, title?)

签名：`fb.misc.showPopupMessage(message: string, title?: string): Promise<MiscShowPopupMessageResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `string` | 是 | 对话框正文内容；不可为空字符串 |
| `title` | `string` | 否 | 对话框标题栏文本；空字符串或省略时缺省为 `"Message"` |

弹出 foobar2000 原生消息提示对话框。该方法在对话框成功创建弹出后立即返回，不阻塞等待用户关闭；若 `message` 为空字符串，操作以 `INVALID_PARAMS` 错误拒绝。

```javascript
await fb.misc.showPopupMessage('操作完成', '提示');
```

## showPreferences()

签名：`fb.misc.showPreferences(): Promise<MiscShowPreferencesResponse>`

打开 foobar2000 原生首选项设置对话框。

```javascript
await fb.misc.showPreferences();
```

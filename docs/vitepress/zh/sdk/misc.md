# fb.misc 杂项工具

`fb.misc` 提供几个 foobar2000 宿主操作：报告安装目录、配置目录与组件目录，打开控制台、首选项和媒体库搜索窗口，弹出消息对话框，以及重启或退出 foobar2000。

## exit()

签名：`fb.misc.exit(): Promise<MiscExitResponse>`

让 foobar2000 退出。

```javascript
const res = await fb.misc.exit();
if (res.success === false) throw new Error(res.error);
```

## getComponentPath()

签名：`fb.misc.getComponentPath(): Promise<MiscGetComponentPathResponse>`

报告组件 DLL 所在的目录，`path` 是该目录的原生路径。`value` 是同一个目录，保留给读这个字段的主题。

```javascript
const r = await fb.misc.getComponentPath();
if (r.success === false) throw new Error(r.error);
console.log(r.path); // 组件目录
```

## getFoobarPath()

签名：`fb.misc.getFoobarPath(): Promise<MiscGetFoobarPathResponse>`

报告 foobar2000 的安装目录，放在 `path`（`value` 是同一个目录）。

```javascript
const res = await fb.misc.getFoobarPath();
if (res.success === false) throw new Error(res.error);
const { path } = res;
```

## getProfilePath()

签名：`fb.misc.getProfilePath(): Promise<MiscGetProfilePathResponse>`

报告 foobar2000 的配置目录（配置与组件都在这里），放在 `path`（`value` 是同一个目录）。

```javascript
const res = await fb.misc.getProfilePath();
if (res.success === false) throw new Error(res.error);
const { path } = res;
```

## restart()

签名：`fb.misc.restart(): Promise<MiscRestartResponse>`

让 foobar2000 重启。

```javascript
await fb.misc.restart();
```

## showConsole()

签名：`fb.misc.showConsole(): Promise<MiscShowConsoleResponse>`

打开 foobar2000 控制台窗口。要往控制台写消息，用 [fb.console](./console.md)。

```javascript
await fb.misc.showConsole();
```

## showLibrarySearch(query?)

签名：`fb.misc.showLibrarySearch(query?: string): Promise<MiscShowLibrarySearchResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `query` | `string` | 否 | 预填进搜索框的查询。为空或省略时打开空白搜索。 |

打开媒体库搜索窗口。响应里的 `query` 是窗口打开时使用的查询。

```javascript
await fb.misc.showLibrarySearch('artist IS Beatles');
```

## showPopupMessage(message, title?)

签名：`fb.misc.showPopupMessage(message: string, title?: string): Promise<MiscShowPopupMessageResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `string` | 是 | 对话框正文，不能为空。 |
| `title` | `string` | 否 | 标题栏文本；为空或省略时宿主用 `"Message"`。 |

显示 foobar2000 的弹出消息对话框。对话框显示出来就返回，不等它关闭。`message` 为空时以 `INVALID_PARAMS` 失败。

```javascript
await fb.misc.showPopupMessage('操作完成', '提示');
```

## showPreferences()

签名：`fb.misc.showPreferences(): Promise<MiscShowPreferencesResponse>`

打开 foobar2000 首选项对话框。

```javascript
await fb.misc.showPreferences();
```

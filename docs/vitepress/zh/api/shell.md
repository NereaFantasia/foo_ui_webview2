# Shell 系统外壳 API

`shell` 命名空间的方法：在资源管理器中定位文件，用关联程序或默认程序打开文件与链接，启动外部进程。

## Shell API - 系统集成

### shell.showInExplorer

<!-- api-schema:begin shell.showInExplorer -->
在资源管理器中显示并选中文件或文件夹。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 要显示的文件或文件夹。不能为空。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('shell.showInExplorer', { path: 'C:\\Music\\song.flac' });
```

### shell.openExternal

<!-- api-schema:begin shell.openExternal -->
用默认程序打开 `http://`、`https://` 或 `mailto:` URL。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `url` | `string` | 是 | 要打开的 URL；其他协议以 `INVALID_PARAMS` 失败。不能为空。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('shell.openExternal', { url: 'https://www.foobar2000.org' });
```

### shell.exec

<!-- api-schema:begin shell.exec -->
按命令行启动进程并立即返回，不等待。不设命令白名单：主题的信任边界等同于已安装的组件。`cwd` 经路径安全校验。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `command` | `string` | 是 | 命令行，按原样执行。不能为空。 |
| `args` | `string[]` | 否 | 追加到命令行末尾的参数，逐个加引号。 |
| `cwd` | `string` | 否 | 工作目录；缺省或为空时继承 foobar2000 的。 |
| `hidden` | `boolean` | 否 | 不显示进程窗口。默认 `true`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `processId` | `integer` | 已启动进程的 Windows 进程 id。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

::: warning 安全限制
不限制可执行命令（信任主题作者，信任边界等同于安装一个 foobar2000 组件）。若提供 `cwd`，会经 PathSecurity 路径校验拒绝越界路径。破坏性文件操作请用 `fb.file.*`（受路径黑名单保护）。
:::

::: tip 行为说明
`shell.exec` 是 fire-and-forget 语义，只表示命令进程已发起，不保证目标服务已经就绪。
:::

```javascript
// 启动 Node.js 服务器
await fb2k.invoke('shell.exec', { command: 'cmd /c start /b node "E:\\server.js"' });

// 无命令白名单：任意命令均可执行（信任主题作者）
await fb2k.invoke('shell.exec', { command: 'curl http://example.com' });
```

### shell.spawn

<!-- api-schema:begin shell.spawn -->
按可执行文件与参数列表启动进程。给了 `waitForExitMs` 时等待这么久看它是否提前退出；等待内以非零码退出则以 `OPERATION_FAILED` 失败，失败信封带 `processId`、`exited` 与 `exitCode`。绝对路径的可执行文件与 `cwd` 经路径安全校验。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `executable` | `string` | 是 | 可执行文件名（经 PATH 解析）或路径。首尾空白与引号会被去掉。不能为空。 |
| `args` | `string[]` | 否 | 传给进程的参数，逐个加引号。 |
| `cwd` | `string` | 否 | 工作目录，必须存在；缺省或为空时继承 foobar2000 的。 |
| `hidden` | `boolean` | 否 | 不显示进程窗口。默认 `true`。 |
| `waitForExitMs` | `integer` | 否 | 等待进程提前退出的毫秒数；`0` 表示进程一启动就返回。不小于 `0`。默认 `0`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `processId` | `integer` | 已启动进程的 Windows 进程 id。 |
| `exited` | `boolean` | 进程是否在 `waitForExitMs` 内退出；没有要求等待时不出现。 |
| `exitCode` | `integer` | 退出码，`exited` 为 `true` 时出现。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

::: warning 安全限制
不限制可执行文件（信任主题作者）。绝对路径可执行文件与 `cwd` 会经路径安全校验，拒绝指向系统目录等越界路径。
:::

```javascript
// ✅ 推荐：直接启动 node server.js（可检测 CreateProcess 失败）
const result = await fb2k.invoke('shell.spawn', {
  executable: 'E:\\FB2K\\Runtime\\node.exe',
  args: ['E:\\FB2K\\NeteaseApi\\server.js'],
  cwd: 'E:\\FB2K\\NeteaseApi',
  hidden: true,
  waitForExitMs: 900
});

if (result.success === false) {
  console.error(result.error, 'exitCode' in result ? result.exitCode : undefined);
}
```

### shell.openWith

<!-- api-schema:begin shell.openWith -->
用 Windows 关联的程序打开文件。可执行文件、脚本、安装包、快捷方式与库一律以 `PERMISSION_DENIED` 拒绝。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 交给关联程序打开的文件。不能为空。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

::: danger 安全限制
禁止打开可执行文件（.exe/.bat/.cmd 等 29 种扩展名）。
:::

```javascript
await fb2k.invoke('shell.openWith', { path: 'C:\\Music\\notes.txt' });
```

## Shell 边界

- `shell.openExternal` 仅接受 `http://`、`https://` 或 `mailto:` URL。`shell.openWith` 会拒绝可执行文件、脚本、安装包、快捷方式、库及相关危险扩展名。
- `shell.exec` 与 `shell.spawn` 有意不设置命令白名单。它们的 `cwd` 与绝对可执行文件路径都会被校验；`shell.spawn.waitForExitMs` 可选地报告进程是否提前退出。

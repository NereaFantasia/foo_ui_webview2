# Shell 系统外壳 API

文件定位、关联打开与外部进程调度。

## Shell API - 系统集成

### shell.showInExplorer

<!-- api-schema:begin shell.showInExplorer -->
在 Windows 资源管理器中打开指定路径并自动高亮选中目标文件或文件夹

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 待在资源管理器中定位的文件或文件夹路径不能为空。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('shell.showInExplorer', { path: 'C:\\Music\\song.flac' });
```

### shell.openExternal

<!-- api-schema:begin shell.openExternal -->
调用系统默认关联程序打开外部链接，仅支持 `http://`、`https://` 与 `mailto:` 协议

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `url` | `string` | 是 | 待打开的外部链接，仅允许 `http://`、`https://` 或 `mailto:` 协议，其他协议以 `INVALID_PARAMS` 失败不能为空。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('shell.openExternal', { url: 'https://www.foobar2000.org' });
```

### shell.exec

<!-- api-schema:begin shell.exec -->
启动命令行进程并立即返回，不等待执行结束；不设命令白名单（主题信任边界等同于已安装组件）；若指定 `cwd` 则会进行路径安全校验

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `command` | `string` | 是 | 待执行的命令行字符串，按原样传入执行不能为空。 |
| `args` | `string[]` | 否 | 追加至命令行末尾的参数列表，各参数会自动添加双引号转义 |
| `cwd` | `string` | 否 | 子进程工作目录；缺省或为空时继承 foobar2000 主进程工作目录 |
| `hidden` | `boolean` | 否 | 是否以隐藏窗口模式启动子进程默认 `true`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `processId` | `integer` | 已启动子进程的 Windows 进程 ID |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

::: warning 安全限制
`shell.exec` 不设可执行命令白名单（信任主题作者，其安全边界等同于安装第三方 foobar2000 原生组件）。若传入 `cwd`，宿主将执行路径安全校验并拒绝越界路径。对于破坏性文件系统操作，请优先使用受黑名单保护的 `file.*` API。
:::

::: tip 行为说明
`shell.exec` 采用即发即弃（fire-and-forget）语义：方法调用成功仅表示底层进程创建成功，不保证目标外部服务或命令已完全就绪。
:::

```javascript
// 启动 Node.js 服务器
await fb2k.invoke('shell.exec', { command: 'cmd /c start /b node "E:\\server.js"' });

// 无命令白名单：任意命令均可执行（信任主题作者）
await fb2k.invoke('shell.exec', { command: 'curl http://example.com' });
```

### shell.spawn

<!-- api-schema:begin shell.spawn -->
根据可执行程序路径与参数列表创建子进程；若指定 `waitForExitMs` 则等待对应毫秒以检测提前退出，等待期内以非零退出码退出时以 `OPERATION_FAILED` 失败且错误信封附带 `processId`、`exited` 与 `exitCode`；绝对路径可执行文件与 `cwd` 均受路径安全校验保护

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `executable` | `string` | 是 | 待启动的可执行文件名（从系统 PATH 环境变量解析）或绝对路径，首尾空白符与引号会自动剔除不能为空。 |
| `args` | `string[]` | 否 | 传递给子进程的参数数组，各参数会自动添加双引号转义 |
| `cwd` | `string` | 否 | 子进程工作目录，目录必须已存在；缺省或为空时继承 foobar2000 主进程工作目录 |
| `hidden` | `boolean` | 否 | 是否以隐藏窗口模式启动子进程默认 `true`。 |
| `waitForExitMs` | `integer` | 否 | 等待子进程提前退出的超时毫秒数；设为 `0` 时进程启动后立即返回不小于 `0`。默认 `0`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `processId` | `integer` | 已启动子进程的 Windows 进程 ID |
| `exited` | `boolean` | 子进程是否在 `waitForExitMs` 等待时间内已退出；未请求等待时不返回此字段 |
| `exitCode` | `integer` | 子进程退出码；仅当 `exited` 为 `true` 时返回 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

::: warning 安全限制
`shell.spawn` 不限制可执行程序名称（基于对主题作者的同等信任）。当传入绝对路径可执行文件或 `cwd` 时，宿主将执行路径安全校验，拒绝指向受保护目录的越界路径。
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
使用 Windows 文件类型关联的默认程序打开文件；可执行文件、脚本、安装包、快捷方式与动态库等危险类型一律拒绝并返回 `PERMISSION_DENIED`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 待通过系统关联程序打开的文件路径不能为空。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

::: danger 安全限制
禁止通过关联程序打开可执行文件与危险脚本（包括 `.exe`、`.bat`、`.cmd` 等 29 类受限扩展名），违规调用将返回 `PERMISSION_DENIED`。
:::

```javascript
await fb2k.invoke('shell.openWith', { path: 'C:\\Music\\notes.txt' });
```

## Shell 边界

- 外部链接与关联程序：`shell.openExternal` 仅接受 `http://`、`https://` 或 `mailto:` URL，其余协议以 `INVALID_PARAMS` 失败；`shell.openWith` 会严格拦截可执行文件、脚本、安装包、快捷方式、动态库等危险扩展名并以 `PERMISSION_DENIED` 拒绝。
- 外部进程与安全校验：`shell.exec` 与 `shell.spawn` 设计上不设命令与程序白名单，但显式指定的 `cwd` 与绝对路径可执行文件均受路径安全校验保护；`shell.spawn.waitForExitMs` 支持在指定等待期内捕获子进程提前退出与非零退出码。

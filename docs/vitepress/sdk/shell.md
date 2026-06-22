# fb.shell 系统集成 

## showInExplorer(path) 

在资源管理器中显示文件。

```javascript
await fb.shell.showInExplorer('E:\\\\Music\\\\song.flac');
```

## openWith(path) 

用系统默认程序打开文件。

::: danger 安全限制
禁止打开可执行文件（.exe/.bat/.cmd 等）。
:::

```javascript
await fb.shell.openWith('E:\\\\Music\\\\cover.jpg');
```

## openExternal(url) 

用默认浏览器打开 URL。

```javascript
await fb.shell.openExternal('https://www.foobar2000.org');
```

## exec(command, options?) 

执行系统命令。

::: warning 安全限制
不限制可执行命令（主题来自用户自己或可信来源，信任边界等同于安装一个 foobar2000 组件）。若提供 `cwd`，会经路径安全校验拒绝系统目录等越界路径。破坏性文件操作请用 `fb.file.*`，其受 PathSecurity 路径黑名单保护。
:::

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| command | string | 要执行的命令 |
| options | object | 可选：{args, cwd, hidden} |

```javascript
await fb.shell.exec('notepad', { args: ['E:\\\\notes.txt'] });
```

## spawn(executable, options?) 

结构化启动进程（推荐用于启动 Node 服务）。

::: warning 安全限制
不限制可执行文件（信任主题作者）。绝对路径可执行文件与 `cwd` 会经路径安全校验，拒绝指向系统目录等越界路径。
:::

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| executable | string | 可执行文件名或绝对路径 |
| options | object | 可选：{args, cwd, hidden, waitForExitMs} |

```javascript
const result = await fb.shell.spawn('E:\\\\FB2K\\\\Runtime\\\\node.exe', {
  args: ['E:\\\\FB2K\\\\NeteaseApi\\\\server.js'],
  cwd: 'E:\\\\FB2K\\\\NeteaseApi',
  hidden: true,
  waitForExitMs: 900
});
```

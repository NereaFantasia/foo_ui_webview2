# fb.shell System Integration

## showInExplorer(path) 

Reveals a file in Windows File Explorer.

```javascript
await fb.shell.showInExplorer('E:\\Music\\song.flac');
```

## openWith(path) 

Opens a file with its system-associated application.

::: danger Security restriction
Executable file types such as `.exe`, `.bat`, and `.cmd` are rejected.
:::

```javascript
await fb.shell.openWith('E:\\Music\\cover.jpg');
```

## openExternal(url) 

Opens a URL in the default browser.

```javascript
await fb.shell.openExternal('https://www.foobar2000.org');
```

## exec(command, options?) 

Executes a system command and returns a `ShellExecResponse` containing `success` and, on success, `processId`.

::: warning Security boundary
Commands are not allowlisted because installed themes are treated as trusted code, with a trust boundary comparable to installing a foobar2000 component. When provided, `cwd` is validated and paths outside the permitted boundary, including protected system directories, are rejected. Prefer `fb.file.*` for destructive file operations so PathSecurity restrictions apply.
:::

| Parameter | Type | Description |
| --- | --- | --- |
| command | string | Command to execute |
| options.args | string[] | Command-line arguments |
| options.cwd | string | Working directory |
| options.hidden | boolean | Hide the child window. Defaults to `true` |

```javascript
await fb.shell.exec('notepad', { args: ['E:\\notes.txt'] });
```

## spawn(executable, options?) 

Starts a process through the structured spawn API, recommended for services such as Node applications.

::: warning Security boundary
Executables are not allowlisted; trust the theme author. Absolute executable paths and `cwd` values are validated, and paths outside the permitted boundary, including protected system directories, are rejected. An empty-string `cwd` is rejected by the SDK before invoking the host.
:::

| Parameter | Type | Description |
| --- | --- | --- |
| executable | string | Executable name or absolute path |
| options.args | string[] | Process arguments |
| options.cwd | string | Working directory |
| options.hidden | boolean | Hide the child window. Defaults to `true` |
| options.waitForExitMs | number | Wait up to this many milliseconds for exit. Defaults to `0` (do not wait) |

The response contains `processId` on success. If `waitForExitMs` is positive, it may also contain `exited` and, when the process exited in time, `exitCode`.

```javascript
const result = await fb.shell.spawn('E:\\FB2K\\Runtime\\node.exe', {
  args: ['E:\\FB2K\\NeteaseApi\\server.js'],
  cwd: 'E:\\FB2K\\NeteaseApi',
  hidden: true,
  waitForExitMs: 900
});
```

# 安全限制

本组件是**专为 foobar2000 设计**的 UI 宿主。主题来自用户自己或可信来源，安装一个主题的信任边界大致等同于安装一个 foobar2000 组件。

因此安全设计目标是 **fail-safe（防主题 bug 误伤系统）**，而不是完整 **sandbox（防不可信代码越权）**。

## 威胁模型

- 主要护栏是 PathSecurity 与协议限制。
- `shell.exec` / `shell.spawn` **故意**不维护可执行白名单。
- 带路径参数的 Bridge API 仍走 decorator 校验，路径被拒返回 `PERMISSION_DENIED`，参数形状或类型不对返回 `INVALID_PARAMS`。

## shell.exec

- 无命令白名单。
- 若提供 `cwd`，会经 PathSecurity 校验并在越界时拒绝。

## shell.spawn

- 无可执行白名单。
- 参数化启动，避免拼接 shell 命令字符串。
- 可选 `waitForExitMs` 可检测进程提前退出。
- 绝对可执行路径与 `cwd` 会做路径校验。

## shell.openWith

被禁止的扩展名（29 种）：

`.exe .com .cmd .bat .ps1 .vbs .vbe .js .jse .wsf .wsh .msc .scr .pif .hta .cpl .msi .msp .msu .dll .ocx .sys .drv .lnk .url .reg .inf .jar .application`

## file.read

禁止访问的系统盘目录包括：

- `C:\\Windows\\`
- `C:\\Program Files\\`
- `C:\\Program Files (x86)\\`
- `C:\\ProgramData\\`

非系统盘通常放行，以支持 NAS / 便携版布局。

## file.write

写入目标必须通过 PathSecurity 写策略；实践上为 foobar2000 配置目录与系统临时目录。

## http.get / http.post

SSRF 防护拒绝：

- `localhost` / `127.x.x.x`
- `192.168.x.x`
- `10.x.x.x`
- `172.16-31.x.x`
- `169.254.x.x`
- `::1`

::: tip 启用内网访问
**Preferences → Advanced → Tools → WebView UI → Allow local network access**
:::

## DevTools

默认禁用。启用方法：

**Preferences → Advanced → Tools → WebView UI → Enable DevTools**，然后重启 foobar2000。

## 相关 API 与错误码

- `file.read`
- `file.write`
- `http.get`
- `http.post`
- `shell.exec`
- `shell.openWith`
- `shell.spawn`
- `PERMISSION_DENIED`
- `INVALID_PARAMS`

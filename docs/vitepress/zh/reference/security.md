# 安全限制

本组件是**专为 foobar2000 设计**的 UI 宿主。主题来自用户自己或可信来源，安装一个主题的信任边界大致等同于安装一个 foobar2000 组件。

因此安全设计目标是 **fail-safe（防主题 bug 误伤系统）**，而不是完整 **sandbox（防不可信代码越权）**。

## 威胁模型

- 主要护栏是 PathSecurity 与协议限制。
- `shell.exec` / `shell.spawn` **故意**不维护可执行白名单。
- 带路径参数的 Bridge API 仍走 decorator 校验，路径被拒返回 `PERMISSION_DENIED`，参数形状或类型不对返回 `INVALID_PARAMS`。

## 哪些页面能调用 API

只有顶层页面满足下面任一条件时，页面才能调用宿主、收到宿主的事件：

- 位于 `https://foo-ui-webview2.local`，即提供本地模板的虚拟主机；
- 是宿主自己写入的页面，例如内置页面和菜单浮层；
- 位于宿主把这个窗口或面板导航过去的来源：开发服务器、面板配置的 URL，或者弹出窗口的 `http://`、`https://` 地址，前提是打开它的页面已经信任该来源。

地址先解析成协议、主机和端口再比较，从不按文本比较。带用户名或密码的地址（例如 `https://foo-ui-webview2.local:1@example.com/`）实际是 `example.com` 上的页面，不受信任。`file://` 页面一律不受信任。

不在上述范围内的页面照常显示，但每次调用都立即以 `ORIGIN_DENIED` 失败，事件和之前调用的应答也都不会送到它那里。受信任的页面一旦自己跳到别处，同样如此。

拖放里的真实文件路径只给更少的页面：上面的虚拟主机，以及开发服务器开着时、偏好设置页里填写的那个开发服务器地址。

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

`file.write`、`file.delete`、`file.mkdir`、`file.copy` 的目标与 `file.move` 的两端按 `FileWrite` 级别校验。系统目录一律拒绝；除此之外，路径位于 foobar2000 配置目录或临时目录、媒体库监视目录内、任一非系统盘上（按盘符，UNC 路径不算），或是媒体库、播放列表里的曲目时放行。这是主题能用的最宽的写入通道；各级别的完整规则见权限页。

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

**Preferences → Display → WebView2 UI → 开发者 → 启用开发者工具 (F12)**，按**应用**，然后重启 foobar2000。

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

# 安全限制

## 威胁模型 

本组件是围绕 foobar2000 设计的播放器 UI 宿主，主题来自用户自己或可信来源，安装一个主题的信任边界等同于安装一个 foobar2000 组件。因此安全设计目标是 **fail-safe（防主题 bug 误伤系统）**，而非 **sandbox（防不可信代码越权）**。`shell.exec` / `shell.spawn` 不限制可执行程序，真正的护栏是路径校验（PathSecurity）与协议限制。

## shell.exec - 无命令白名单 

不限制可执行命令。若提供 `cwd`，会经 PathSecurity 路径校验拒绝越界路径。

## shell.spawn - 无可执行白名单 

不限制可执行文件。

特性:

- 参数化调用，不需要拼接 shell 命令字符串
- 可选 `waitForExitMs` 检测进程早退（例如 Node 启动即崩溃）
- 绝对路径可执行文件与 `cwd` 会执行路径安全校验

## shell.openWith - 可执行黑名单 

被禁止的扩展名 (30+ 种):

.exe .com .cmd .bat .ps1 .vbs .vbe .js .jse .wsf .wsh .msc
.scr .pif .hta .cpl .msi .msp .msu .dll .ocx .sys .drv
.lnk .url .reg .inf .jar .application

## file.read - 路径安全 

禁止访问的目录（系统盘）: `C:\Windows\`、`C:\Program Files\`、`C:\Program Files (x86)\`、`C:\ProgramData\`

非系统盘（D:、E: 等）默认放行，支持 NAS 和便携版使用场景。

## file.write - 写入限制 

允许写入的目录: foobar2000 配置目录 (`%APPDATA%\foobar2000\`) 和系统临时目录 (`%TEMP%`)。

## http.get/post - SSRF 防护 

被禁止的地址: `localhost`/`127.x.x.x`、`192.168.x.x`、`10.x.x.x`、`172.16-31.x.x`、`169.254.x.x`、`::1`

::: tip 启用内网访问
**Preferences → Advanced → Tools → WebView UI → Allow local network access**
:::

## DevTools 

默认禁用，启用方法: Preferences → Advanced → Tools → WebView UI → Enable DevTools，重启 foobar2000。

## CSP 策略 

| 类型 | 允许的来源 |
| --- | --- |
| 脚本 | cdnjs.cloudflare.com, unpkg.com, cdn.jsdelivr.net |
| 样式 | 上述 CDN + fonts.googleapis.com |
| 字体 | fonts.gstatic.com, 上述 CDN |
| 图片 | 所有 https: 来源 |
| 连接 | https:, localhost:*, 127.0.0.1:*, WebSocket |

内联脚本 (`'unsafe-inline'`) 和 eval (`'unsafe-eval'`) 已允许，以支持 Vite/Vue 开发模式。

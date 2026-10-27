# 安全体系与信任边界 {#安全限制}

本组件是专为 foobar2000 设计的专用 UI 宿主。主题来自用户自行编写或受信任的第三方分发渠道；安装主题的信任边界与安装 foobar2000 原生组件基本一致。

因此，组件的安全目标偏向 **fail-safe（防止主题缺陷或意外操作破坏系统稳定性）**，而非对抗不可信恶意代码的完整**沙箱隔离（Sandbox）**。

## 威胁模型

- 核心防御机制依托 PathSecurity 路径安全规范与协议过滤规则
- `shell.exec` 与 `shell.spawn` 在架构设计上不设可执行文件白名单：由于 `cmd /c` 等命令行解释器可绕过白名单，白名单机制无法构成可靠安全信任边界
- 涉及路径参数的 Bridge API 均在执行前受到 PathSecurity 强制校验；路径越界时返回 `PERMISSION_DENIED`，参数类型或结构不符时返回 `INVALID_PARAMS`

## 哪些页面能调用 API

仅当顶层页面满足以下条件之一时，页面才具备调用宿主 Bridge 及接收宿主事件的权限：

- 位于 `https://foo-ui-webview2.local`（提供本地模板的虚拟主机域）
- 宿主自身生成的受保护页面（如内置缺省页、右键菜单浮层）
- 宿主主动导航加载的目标来源：本地开发服务器；面板配置的目标 URL（须位于本机或已在面板配置中明确授权）；弹窗加载的 `http://` 或 `https://` 页面（前提是打开弹窗的父页面已信任该来源）

本机地址限定为 `localhost`、`127.x.x.x` 与 `[::1]`。开发服务器来源完全受信任；受信任页面可通过 `window.setDevServerConfig` 更新开发服务器配置（下次加载页面时生效）。

页面地址按协议（scheme）、主机（host）与端口（port）结构化比对，绝不采用模糊纯文本匹配。携带凭据信息的地址（例如 `https://foo-ui-webview2.local:1@example.com/`）实际归属于 `example.com`，不属于受信来源。`file://` 协议页面一律不受信任。

不在上述范围内的页面虽可正常渲染，但所有 API 调用均以 `ORIGIN_DENIED` 立即失败，宿主不会向其派发事件或返回回调。受信任页面后续自行导航重定向至外部地址后，同样立即失去 API 调用权限。

文件拖放（Drag and Drop）产生的真实文件物理路径受更严格约束：仅投递给本地虚拟主机以及开发服务器开启状态下的指定调试服务器地址。

## `shell.exec`

- 不设可执行命令白名单
- 若提供 `cwd` 工作目录，将在执行前经过 PathSecurity 路径校验并在越界时拒绝

## `shell.spawn`

- 不设可执行文件白名单
- 采用参数化启动机制，避免拼接 shell 命令字符串注入
- 可选配置 `waitForExitMs` 参数以检测进程提前退出异常
- 绝对可执行文件路径与 `cwd` 工作目录均经 PathSecurity 严格校验

## `shell.openWith`

被禁止的可执行扩展名（共 29 种）：

`.exe .com .cmd .bat .ps1 .vbs .vbe .js .jse .wsf .wsh .msc .scr .pif .hta .cpl .msi .msp .msu .dll .ocx .sys .drv .lnk .url .reg .inf .jar .application`

## `file.read`

禁止访问的系统盘受保护目录包括：

- `C:\Windows\`
- `C:\Program Files\`
- `C:\Program Files (x86)\`
- `C:\ProgramData\`

foobar2000 的 profile 目录不受此限：便携版装在 `C:\Program Files\` 下时，仍能读写自己的 profile。

非系统盘目录默认放行，支持 NAS 网络共享与便携化安装布局。

## `file.write`

`file.write`、`file.delete`、`file.mkdir`、`file.copy` 的目标路径与 `file.move` 的两端路径均按 `FileWrite` 权限级别校验。系统受保护目录拒绝写入，foobar2000 的 profile 目录除外；除此之外，路径位于 foobar2000 配置目录、临时目录、媒体库监视目录内、任一非系统盘驱动器卷上（按盘符判定，UNC 路径不在此列），或是已建库或播放列表中的曲目时予以放行。该通道为主题可用的最大范围写入通道；各级别完整细则见[路径权限模型参考](./permissions.md)。

## `http.get` / `http.post`

SSRF 安全防护机制默认拒绝访问以下地址与私有网段：

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

默认处于禁用状态。启用步骤：

进入 **Preferences → Display → WebView2 UI → 开发者 → 启用开发者工具 (F12)**，点击**应用**，随后重启 foobar2000。

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

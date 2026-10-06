# 安装与配置 

## 先决条件 

1. **Node.js 18+** — [下载](https://nodejs.org/)
2. **foobar2000** 已安装并运行 `foo_ui_webview2` 组件
3. **CDP 远程调试已启用** — 在组件的 `Preferences → Display → WebView2 UI → 开发者` 页开启；端口也在那里设置（默认 `9222`）

## 安装方式 

### 方式一：npx 直接运行（推荐） 

无需安装，在 MCP 客户端配置中直接使用：

```json
{
  "foo-ui-webview2": {
    "command": "npx",
    "args": ["-y", "foo-ui-webview2-mcp"],
    "env": {
      "FB2K_CDP_PORT": "9222"
    },
    "type": "stdio"
  }
}
```

### 方式二：全局安装 

```bash
npm install -g foo-ui-webview2-mcp
foo-ui-webview2-mcp
```

### 方式三：本地开发 

```bash
cd mcp/
npm install
npm run build
npm start
```

## 客户端配置 

### VS Code (GitHub Copilot) 

在项目根目录创建或编辑 `.vscode/mcp.json`：

```json
{
  "servers": {
    "foo-ui-webview2": {
      "command": "npx",
      "args": ["-y", "foo-ui-webview2-mcp"],
      "env": {
        "FB2K_CDP_PORT": "9222"
      },
      "type": "stdio"
    }
  }
}
```

### Claude Desktop 

编辑配置文件：

- **Windows**: `%APPDATA%\\Claude\\claude_desktop_config.json`
- **macOS**: `~/Library/Application Support/Claude/claude_desktop_config.json`

```json
{
  "mcpServers": {
    "foo-ui-webview2": {
      "command": "npx",
      "args": ["-y", "foo-ui-webview2-mcp"],
      "env": {
        "FB2K_CDP_PORT": "9222"
      }
    }
  }
}
```

### Cursor 

在 Cursor 设置中，MCP Servers 部分添加同样的配置。

## 环境变量 

| 变量 | 默认值 | 说明 |
| --- | --- | --- |
| FB2K_CDP_PORT | 9222 | WebView2 CDP 调试端口 |
| FB2K_CDP_HOST | localhost | WebView2 CDP 主机地址 |
| FB2K_CDP_TARGET_URL | — | 固定 CDP page target 的 URL 子串（弹窗/面板/托盘 overlay 多 WebView 共享端口时用），示例 `foo-ui-webview2.local`；设置后无匹配会报错并列出全部候选，不会乱绑 |
| FB2K_READ_ONLY | — | 设为 `1` 或 `true` 时只注册只读工具 |
| FB2K_ENABLE_EVAL | — | 设为 `1` 或 `true` 时注册 `fb2k_page_evaluate`（安全风险）；只读模式下不生效 |
| FB2K_MAX_RESPONSE_CHARS | 100000 | 文本结果的最大字符数；超出部分被截断并附说明 |
| FB2K_MAX_IMAGE_BYTES | 3932160 | 以图片返回的封面最多多少字节（3.75 MiB）；更大的封面不附上，结果里会说明 |

::: warning 关于 FB2K_ENABLE_EVAL
`fb2k_page_evaluate` 允许在 WebView2 页面里执行任意 JavaScript 表达式，能调用页面可以调用的全部 Bridge 方法，仅建议在可信的开发或调试会话里使用。
:::

## CDP 连接 

### 启用 CDP 远程调试 

1. 打开 foobar2000
2. 进入 **Preferences → Display → WebView2 UI → 开发者**
3. 勾选 **启用 CDP 远程调试（供 MCP / AI 代理使用）**；端口保持 `9222` 或在 1024–65535 内另选，然后按 **应用**
4. 重启 foobar2000。改了端口的话，把 MCP 客户端配置里的 `FB2K_CDP_PORT` 设成同一个值

### 连接流程 

MCP Server 启动
    │
    ├─ GET http://localhost:9222/json → 发现 page 类型 target
    │
    ├─ WebSocket 连接到 target
    │
    ├─ 并行: Runtime.enable() + Page.enable()
    │
    ├─ 1×1 截图预热渲染管线
    │
    └─ 就绪，等待工具调用

### 连接失败处理 

| 场景 | 表现 | 解决方案 |
| --- | --- | --- |
| foobar2000 未运行 | WebView2 not available at port 9222 | 启动 foobar2000 |
| CDP 未启用 | 同上 | 在开发者子页启用并重启 |
| 端口被占用 | 连接超时 | 更换端口或关闭占用程序 |
| 连接中断 | 自动重连（最多 3 次） | 等待自动恢复 |
| 调用超时 | 30 秒后返回错误 | 检查 foobar2000 是否卡顿 |

### 后台行为与 keep-alive 

为节省内存，组件默认在窗口**最小化**、**隐藏到托盘**或**锁屏**时挂起 WebView 页面（`visibilityState=hidden`、渲染暂停、定时器节流）。挂起的页面会破坏 CDP 自动化：截图因不再出帧而停摆，依赖 `requestAnimationFrame` 或定时器的调用可能撞上 30 秒超时。

开启 **Enable CDP remote debugging** 后，**Preferences → Advanced → Tools → WebView2 UI** 里的子开关 **Keep WebView active in background while CDP remote debugging is on (tray/minimize/lock)**（默认开启）会豁免上述挂起，让窗口最小化、托盘隐藏或锁屏期间 CDP 工具保持可用。若更在意后台省电可以关闭它，切换即时生效、无需重启。CDP 模式还会自动禁用 Chromium 的后台定时器节流。

| 症状 | 可能原因 | 处理 |
| --- | --- | --- |
| 窗口最小化/托盘期间 `fb2k_page_inspect` 的 `screenshot` 超时 | 页面被挂起（keep-alive 已关闭） | 恢复窗口，或开启 keep-alive 子开关 |
| 后台约 5 分钟后调用开始卡顿 | 旧版本组件的后台定时器节流 | 更新组件；CDP 模式现已禁用节流 |
| 工具作用在错误窗口（如托盘菜单 overlay） | 多个 page target 共享 9222 端口 | 设置 `FB2K_CDP_TARGET_URL`（如 `foo-ui-webview2.local`） |

::: tip 固定 target
弹窗、DUI/CUI 面板与托盘菜单 overlay 是同端口下彼此独立的 CDP page target。客户端优先选择有具体 URL 的 target——托盘 overlay 与内置测试页都显示为 `about:blank`——多候选时向 stderr 告警。用 `FB2K_CDP_TARGET_URL` 可以锁定到唯一 target；过滤无匹配会报错并列出候选，而不是绑到任意页面。
:::

## 与 Chrome DevTools MCP 配合 

可搭配 [chrome-devtools-mcp](https://github.com/ChromeDevTools/chrome-devtools-mcp) 使用：

| MCP Server | 职责 | 适用场景 |
| --- | --- | --- |
| foo-ui-webview2-mcp | 语义化 Bridge API | 播放控制、播放列表、媒体库 |
| chrome-devtools-mcp | 通用 DOM 交互 | 点击按钮、填写输入框 |

两者均连接 `localhost:9222` 的同一个 WebView2 实例。

::: tip
WebView2 CDP 同一时刻仅支持一个客户端连接同一 target。如果 chrome-devtools-mcp 已连接，foo-ui-webview2-mcp 可能需要等待其断开。
:::

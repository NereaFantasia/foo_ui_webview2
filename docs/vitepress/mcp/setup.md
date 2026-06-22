# 安装与配置 

## 先决条件 

1. **Node.js 18+** — [下载](https://nodejs.org/)
2. **foobar2000** 已安装并运行 `foo_ui_webview2` 组件
3. **CDP 远程调试已启用** — 在 foobar2000 高级设置中开启 DevTools / CDP，默认端口 `9222`

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
| FB2K_ENABLE_EVAL | — | 设为 1 启用 fb2k_evaluate 工具（安全风险） |

::: warning 关于 FB2K_ENABLE_EVAL
`fb2k_evaluate` 允许执行任意 JavaScript 表达式，仅建议在开发环境使用。生产环境应保持关闭。
:::

## CDP 连接 

### 启用 CDP 远程调试 

1. 打开 foobar2000
2. 进入 **Preferences → Advanced → Tools → WebView UI**
3. 开启 **Enable DevTools** / **Enable CDP**
4. 设置端口（默认 `9222`）
5. 重启 foobar2000

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
| CDP 未启用 | 同上 | 在高级设置中启用 |
| 端口被占用 | 连接超时 | 更换端口或关闭占用程序 |
| 连接中断 | 自动重连（最多 3 次） | 等待自动恢复 |
| 调用超时 | 30 秒后返回错误 | 检查 foobar2000 是否卡顿 |

## 与 Chrome DevTools MCP 配合 

可搭配 [chrome-devtools-mcp](https://github.com/niclas-nicai/chrome-devtools-mcp) 使用：

| MCP Server | 职责 | 适用场景 |
| --- | --- | --- |
| foo-ui-webview2-mcp | 语义化 Bridge API | 播放控制、播放列表、媒体库 |
| chrome-devtools-mcp | 通用 DOM 交互 | 点击按钮、填写输入框 |

两者均连接 `localhost:9222` 的同一个 WebView2 实例。

::: tip TIP
WebView2 CDP 同一时刻仅支持一个客户端连接同一 target。如果 chrome-devtools-mcp 已连接，foo-ui-webview2-mcp 可能需要等待其断开。
:::

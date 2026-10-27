# 构建第一个主题

本教程引导构建一个轻量级 foobar2000 播放器页面并将其安装为独立主题。页面包含当前曲目封面、标题、艺术家、带播放进度与总时长的进度条，以及基础播放控制按钮（上一曲、播放/暂停、下一曲）。开发期间 foobar2000 直接从本地开发服务器加载页面，支持热重载（HMR），修改保存后即时呈现。

项目基于 [Vite](https://vite.dev/) 构建开发服务器，UI 组件采用 [`foo-webview-sdk`](https://www.npmjs.com/package/foo-webview-sdk) 提供的无样式 Web Components，无需引入前端框架。完成品源码可直接参考仓库中的 [`examples/starter`](https://github.com/NereaFantasia/foo_ui_webview2/tree/main/examples/starter)。

## 前置准备 {#开始之前}

- foobar2000 v2.0 或更高版本，已安装 foo_ui_webview2 且界面选择为 `Webview2 UI`（详见[安装组件](/zh/how-to/install)）
- Node.js 20.19 或更高版本（运行 `node --version` 验证）
- foobar2000 播放列表中包含可播放的曲目

本文以组件语言设置为简体中文时的界面文案为准。

## 创建项目

在 foobar2000 profile 目录外新建项目文件夹 `my-theme`（主题构建完成后再统一导入 profile 目录）。

在 `my-theme` 根目录创建 `package.json`：

```json
{
  "name": "my-foobar2000-theme",
  "private": true,
  "version": "0.0.0",
  "type": "module",
  "scripts": {
    "dev": "vite",
    "build": "vite build",
    "preview": "vite preview"
  },
  "dependencies": {
    "foo-webview-sdk": "2.0.0"
  },
  "devDependencies": {
    "vite": "8.3.1"
  },
  "engines": {
    "node": "^20.19.0 || >=22.12.0"
  }
}
```

在 `my-theme` 目录打开终端安装依赖：

```bash
npm install
```

依赖安装完成后，项目根目录下将生成 `node_modules` 目录与 `package-lock.json` 锁文件。

## 编写页面

项目包含四个核心源文件。首先创建 `vite.config.js`，锁定开发服务器监听端口：

```js
import { defineConfig } from 'vite';

export default defineConfig({
  server: {
    port: 5173,
    // foobar2000 loads the address typed into its preferences; if the port is
    // taken, fail instead of moving to another one.
    strictPort: true,
  },
});
```

接下来创建页面入口 `index.html`。所有 `fb-*` 标签均为 SDK 提供的标准化 Web Components，挂载后自动从宿主订阅数据，并在播放状态或进度变更时自主响应更新：

```html
<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>My foobar2000 theme</title>
  </head>
  <body>
    <main class="player">
      <fb-cover-art class="cover"></fb-cover-art>

      <div class="info">
        <fb-track-text class="title" field="title" placeholder="Nothing playing"></fb-track-text>
        <fb-track-text class="artist" field="artist"></fb-track-text>
      </div>

      <div class="time">
        <fb-time-current></fb-time-current>
        <fb-seek-bar class="seek"></fb-seek-bar>
        <fb-time-total></fb-time-total>
      </div>

      <div class="controls">
        <fb-prev-button></fb-prev-button>
        <fb-play-button></fb-play-button>
        <fb-next-button></fb-next-button>
      </div>
    </main>

    <script type="module" src="/src/main.js"></script>
  </body>
</html>
```

创建 `src` 目录，并在其中编写入口脚本 `src/main.js`，负责注册组件并引入样式表：

```js
import { registerComponents } from 'foo-webview-sdk/components';
import './style.css';

registerComponents();
```

编写样式表 `src/style.css`。SDK 组件遵循零样式规范，不附带视觉规则；通过 `::part(...)` 选择器直接为内部部件配置样式（例如播放按钮主体、进度条填充轨道）：

```css
:root {
  --accent: #4cc2ff;
  color-scheme: dark;
  color: #f3f3f3;
  background: #202020;
  font-family: 'Segoe UI', system-ui, sans-serif;
}

body {
  margin: 0;
  min-height: 100vh;
  display: grid;
  place-items: center;
}

.player {
  width: 320px;
  display: grid;
  gap: 16px;
}

.cover {
  width: 320px;
  height: 320px;
  border-radius: 8px;
  overflow: hidden;
  background: #2b2b2b;
}

.info {
  display: grid;
  gap: 4px;
}

.title {
  font-size: 20px;
  font-weight: 600;
}

.artist {
  color: #bdbdbd;
}

.time {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 12px;
  font-variant-numeric: tabular-nums;
}

.seek {
  flex: 1;
}

/* The seek bar draws nothing by itself: give the track a line, the fill a colour and the thumb a dot. */
.seek::part(track) {
  background: linear-gradient(#ffffff33, #ffffff33) center / 100% 4px no-repeat;
}

.seek::part(fill) {
  height: 4px;
  border-radius: 2px;
  background: var(--accent);
}

.seek::part(thumb) {
  width: 12px;
  height: 12px;
  margin-left: -6px;
  border-radius: 50%;
  background: #ffffff;
}

.controls {
  display: flex;
  justify-content: center;
  gap: 12px;
}

.controls ::part(button) {
  width: 48px;
  height: 48px;
  border: none;
  border-radius: 50%;
  background: #2b2b2b;
  color: inherit;
  font-size: 18px;
  cursor: pointer;
}

.controls fb-play-button::part(button) {
  background: var(--accent);
  color: #000000;
}
```

当前项目目录结构如下：

```text
my-theme/
├── node_modules/
├── src/
│   ├── main.js
│   └── style.css
├── index.html
├── package-lock.json
├── package.json
└── vite.config.js
```

## 启动开发服务器

在项目根目录运行开发服务器：

```bash
npm run dev
```

启动成功后控制台输出本地访问地址：

```text
  VITE v8.3.1  ready in 144 ms

  ➜  Local:   http://localhost:5173/
```

保持终端运行。在普通浏览器中打开 `http://localhost:5173`：页面显示封面占位深灰方块、“Nothing playing” 占位文本，进度条两端均为 `0:00`，下方包含三个控制按钮。由于外部浏览器缺乏宿主 Bridge 环境，此时点击按钮不会触发播放控制。

## 在 foobar2000 里加载页面

在 foobar2000 中打开 `File → Preferences → Display → WebView2 UI` 下的 `开发者` 页：

1. 勾选 `使用开发服务器（HMR 热重载）`
2. 在 `URL` 框中填写 `http://localhost:5173`
3. 点击 `应用`

foobar2000 窗口将重新加载并呈现该页面。播放任意曲目：封面、曲目标题与艺术家即时呈现，播放进度随之推进，播放按钮自动切换为暂停状态。点击进度条任意位置可直接定位，同时支持上一曲与下一曲控制。

若窗口仍显示原有主题，说明未能成功连接开发服务器（宿主回退至本地模板）。请确认终端中 `npm run dev` 正常运行且端口一致，必要时重启 foobar2000。

## 体验热重载（修改强调色） {#改个颜色}

在编辑器中打开 `src/style.css`，将顶部的强调色变量修改为目标颜色（例如橙色）：

```css
:root {
  --accent: #4cc2ff; /* [!code --] */
  --accent: #ff8c42; /* [!code ++] */
  color-scheme: dark;
  color: #f3f3f3;
  background: #202020;
  font-family: 'Segoe UI', system-ui, sans-serif;
}
```

保存文件后，播放按钮背景与进度条填充轨道即时切换颜色，页面无需整页刷新。

## 编写业务逻辑与状态监听 {#显示播放状态}

此前的数据获取与界面响应均由 Web Components 自主完成。接下来直接调用 SDK 编写业务逻辑：添加状态文本展示当前播放状态，并新增一个随机播放按钮。

在 `index.html` 的 `<main>` 容器内追加状态栏结构（紧跟 `.controls` 元素之后）：

```html
      <div class="extra">
        <span id="status">Not connected to foobar2000</span>
        <button id="random" type="button">Random track</button>
      </div>
```

保存后页面将重新加载（HTML 结构变更触发整页刷新）。控制按钮下方将呈现未应用样式的文本与按钮。

在 `src/style.css` 末尾追加样式定义：

```css
.extra {
  display: flex;
  align-items: center;
  justify-content: space-between;
  font-size: 12px;
  color: #bdbdbd;
}

#random {
  padding: 6px 12px;
  border: 1px solid #ffffff33;
  border-radius: 4px;
  background: transparent;
  color: inherit;
  font: inherit;
  cursor: pointer;
}
```

在 `src/main.js` 中引入 `fb` 核心实例并注册事件监听：

```js
import fb from 'foo-webview-sdk'; // [!code ++]
import { registerComponents } from 'foo-webview-sdk/components';
import './style.css';

registerComponents();

const status = document.querySelector('#status'); // [!code ++:15]

function showState(state) {
  status.textContent = state === 'playing' ? 'Playing' : state === 'paused' ? 'Paused' : 'Stopped';
}

fb.player.getState().then((res) => {
  if (res.success) showState(res.state);
});

fb.on('playback:stateChanged', ({ state }) => showState(state));

document.querySelector('#random').addEventListener('click', () => {
  fb.player.random();
});
```

`fb.player.getState()` 用于初始化时拉取当前播放状态；`fb.on('playback:stateChanged', ...)` 持续监听状态流转并在变更时回调。SDK 异步调用统一返回结果信封，`success` 字段指示调用是否成功；宿主桥接环境缺失时 `success` 为 `false`，因此外部浏览器中将常驻显示缺省文本。

保存修改后，文本即时更新为 “Playing”。在 foobar2000 中暂停，文本随之变为 “Paused”；点击 `Random track` 按钮，播放器将随机切播下一首曲目。

## 安装主题至 Profile {#安装主题}

本地开发服务器适合调试。若需脱离终端长期使用主题，需执行生产构建并将产物作为静态模板导入 foobar2000 profile 目录。

在 foobar2000 `开发者` 首选项页中取消勾选 `使用开发服务器（HMR 热重载）` 并点击 `应用`，使窗口切回本地模板模式。

在终端终止开发服务器进程（`Ctrl+C`），执行生产构建：

```bash
npm run build
```

构建完成后，生产包产物将输出至 `dist` 目录（包含 `index.html` 与 `assets` 静态资源目录）。

在 foobar2000 中回到 `WebView2 UI` 主首选项页（`File → Preferences → Display → WebView2 UI`）：

1. 点击 `管理...` 弹出模板菜单，选择 `新建...`，输入名称 `my-theme` 并确认。宿主将在 profile 中创建模板目录并写入占位文件，同时自动选中该模板
2. 点击 `打开...` 在文件资源管理器中定位当前模板目录
3. 将项目 `dist` 目录下的所有文件及子目录复制到模板目录中，覆盖原有的占位 `index.html`
4. 点击 `应用`

窗口重新加载后将直接呈现由模板目录驱动的全新主题。该配置在重启 foobar2000 后持续有效，且无需依赖外部开发服务器。

## 后续指引 {#接下来}

完成入门主题开发后，可参考以下专题进一步扩展界面功能：

- [命名空间](/zh/sdk/namespaces)：浏览 SDK 完整 API 能力（播放列表、媒体库、窗口控制、系统托盘等）
- [组件](/zh/components/)：探索现成 Web Components（播放列表视图、音量滑块等）
- [把页面放进面板](/zh/how-to/panels)：将 Web 页面嵌入 Default UI 或 Columns UI 布局面板
- [结果与失败](/zh/sdk/overview#结果与失败)：掌握统一响应信封与错误处理机制
- [首选项参考](/zh/reference/preferences)：查阅外观材质、预热及开发者高级设置

完整的示例项目源码可直接参考仓库中的 [`examples/starter`](https://github.com/NereaFantasia/foo_ui_webview2/tree/main/examples/starter)。

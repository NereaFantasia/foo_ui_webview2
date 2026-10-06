# 构建第一个主题

这篇教程里，我们给 foobar2000 做一个小巧的播放器页面，最后把它装成主题。页面显示当前曲目的封面、标题和艺术家，一条带已播放时间和总时长的进度条，以及上一首、播放 / 暂停、下一首三个按钮。开发期间 foobar2000 从开发服务器加载页面，文件一保存，改动就出现在窗口里。

开发服务器与构建用 [Vite](https://vite.dev/)，现成的播放器元素来自 [`foo-webview-sdk`](https://www.npmjs.com/package/foo-webview-sdk) 包，不需要框架。做完的项目在仓库的 [`examples/starter`](https://github.com/NereaFantasia/foo_ui_webview2/tree/main/examples/starter)。

## 开始之前

需要准备：

- foobar2000 2.x，已装好 foo_ui_webview2，并把用户界面选为 `Webview2 UI`。两步都见[安装组件](/zh/how-to/install)。
- Node.js 20.19 或更高版本。运行 `node --version` 查看当前版本。
- foobar2000 播放列表里有几首曲目。

下文的选项和按钮名按组件语言为中文时的文字书写。

## 创建项目

新建一个空文件夹 `my-theme`，放在 foobar2000 的 profile 文件夹之外；主题做完后，我们再把它复制进 profile。

在 `my-theme` 里创建 `package.json`：

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

在 `my-theme` 打开终端，安装依赖：

```bash
npm install
```

npm 装完后，`my-theme` 里会多出 `node_modules` 文件夹和 `package-lock.json` 文件。

## 编写页面

接下来添加四个文件。先是 `vite.config.js`，它把开发服务器的端口固定下来：

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

然后是 `index.html`。每个 `fb-*` 标签都是 SDK 提供的播放器元素：它自己向 foobar2000 取数据，曲目或播放位置变了就自己更新。

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

新建 `src` 文件夹。里面的 `src/main.js` 注册 `fb-*` 元素并加载样式表：

```js
import { registerComponents } from 'foo-webview-sdk/components';
import './style.css';

registerComponents();
```

最后是 `src/style.css`。这些元素本身不带任何外观，以 `::part(...)` 结尾的规则给元素内部的部件上样式，比如播放按钮里的按钮、进度条的填充段。

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

现在项目结构如下：

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

在终端运行：

```bash
npm run dev
```

Vite 打印出页面的地址：

```text
  VITE v8.3.1  ready in 144 ms

  ➜  Local:   http://localhost:5173/
```

让它一直运行到教程结束。用浏览器打开 `http://localhost:5173`：封面位置是一块深灰色方块，下面写着 “Nothing playing”，进度条两端都是 `0:00`，再下面是三个按钮。普通浏览器背后没有 foobar2000，所以按钮在这里点了没反应。

## 在 foobar2000 里加载页面

在 foobar2000 中打开 `File → Preferences → Display → WebView2 UI` 下的 `开发者` 页，然后：

1. 勾选 `使用开发服务器（HMR 热重载）`。
2. 在 `URL` 框里填 `http://localhost:5173`。
3. 按 `应用`。

foobar2000 窗口重新加载，显示我们的页面。播放一首曲目：封面、标题和艺术家出现，时间开始走，播放按钮变成暂停按钮。点进度条上的任意位置可以跳过去，上一首、下一首按钮也可以试试。

如果窗口显示的还是原来的主题，说明 foobar2000 没连上开发服务器，改为从 profile 加载了主题。确认终端里的 `npm run dev` 还在运行，然后重启 foobar2000。

## 改个颜色

让 foobar2000 和编辑器并排开着。把 `src/style.css` 开头的强调色改成你喜欢的颜色，例如：

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

保存文件。播放按钮和进度条的填充段立刻换了颜色，页面没有重新加载。

## 显示播放状态

到目前为止，活都是元素自己干的。现在我们亲自调用 SDK：加一行文字显示 foobar2000 是否在播放，再加一个随机播放一首的按钮。

在 `index.html` 的 `<main>` 里、`controls` 那个 div 之后加入：

```html
      <div class="extra">
        <span id="status">Not connected to foobar2000</span>
        <button id="random" type="button">Random track</button>
      </div>
```

保存后整个页面会重新加载：Vite 能原地替换样式表，页面的 HTML 却换不了。按钮下方出现新的一行，暂时还没有样式。

在 `src/style.css` 末尾加上这些规则：

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

再给 `src/main.js` 加上标出的几行。第一行导入 `fb`，也就是那些元素一直在用的 SDK 对象：

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

`fb.player.getState()` 向 foobar2000 查一次当前状态；`fb.on('playback:stateChanged', ...)` 在状态每次变化时再调用我们的函数。SDK 的每个调用都 resolve 一个结果对象，其中 `success` 字段表示调用是否成功；在 foobar2000 之外它是 `false`，所以普通浏览器里这一行始终显示 “Not connected to foobar2000”。

保存。这一行现在显示 “Playing”。在 foobar2000 里暂停，它变成 “Paused”；按 `Random track`，foobar2000 随机跳到一首曲目。

## 安装主题

开发服务器只在终端开着时运行。要长期使用这个主题，就得构建它，再作为模板复制进 foobar2000 的 profile。

先让 foobar2000 切回来：在 `开发者` 页取消勾选 `使用开发服务器（HMR 热重载）`，按 `应用`。窗口显示原来的主题。

在终端按 `Ctrl+C` 停掉开发服务器，然后构建：

```bash
npm run build
```

Vite 把做好的主题写进项目的 `dist` 文件夹：一个 `index.html` 和一个 `assets` 文件夹。

在 foobar2000 中回到 `WebView2 UI` 页本身（`File → Preferences → Display → WebView2 UI`）：

1. 按 `管理...`，选 `新建...`。名称填 `my-theme` 并确认。foobar2000 创建模板文件夹，放入一个占位用的 `index.html`，并在 `模板` 框里选中 `my-theme`。
2. 按 `打开...`，在文件资源管理器里打开模板文件夹。
3. 把项目 `dist` 文件夹里的全部内容复制进模板文件夹，覆盖原有的 `index.html`。
4. 按 `应用`。

窗口重新加载，显示我们的主题，这次是从模板文件夹加载的。重启 foobar2000 后它依然在，也不再需要开发服务器。

## 接下来

现在我们有了一个能用的主题，也有了实时修改它的办法。接下来可以看：

- [命名空间](/zh/sdk/namespaces)：SDK 能做的全部事情，从播放列表、媒体库到窗口和托盘图标。
- [组件](/zh/components/)：其他 `fb-*` 元素，比如播放列表视图和音量控制。
- [把页面放进面板](/zh/how-to/panels)：怎样把页面作为面板放进 Default UI 或 Columns UI。
- [结果与失败](/zh/sdk/overview#结果与失败)：SDK 调用 resolve 的结果对象是什么样的。
- [偏好设置](/zh/reference/preferences)：这一路上略过的其他设置。

想从做好的代码重新开始，就从仓库复制 [`examples/starter`](https://github.com/NereaFantasia/foo_ui_webview2/tree/main/examples/starter)。

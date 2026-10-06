# 把页面放进面板

除了接管整个界面，组件还为 Default UI 和 Columns UI 的布局提供 `WebView2 Panel`，每个面板运行自己的页面。页面放进面板后有哪些不同，见[运行模式](/zh/concepts/run-modes)。

## 往 Default UI 里加面板

1. 确认当前界面是 Default UI（`File → Preferences → Display → Default User Interface`）。
2. 打开布局编辑：`View → Layout → Enable Layout Editing Mode`。
3. 右键布局里的空白处，选 `Add New UI Element`，再选 `WebView2 Panel`。
4. 关闭布局编辑。

布局编辑开着时，每个 WebView2 面板都不显示页面，而是显示 “WebView2 Panel / 右键单击进行配置” 的占位；结束编辑后页面恢复。

## 往 Columns UI 里加面板

1. 安装 Columns UI 组件，并把它设为当前界面。
2. 打开它的布局设置（`File → Preferences → Display → Columns UI → Layout`）。
3. 从 `Panels` 分类里把 `WebView2 Panel` 加进布局。

## 给面板指定自己的模板或 URL

新面板显示的是全局模板。要改，就打开面板的配置对话框：Default UI 下打开布局编辑，右键面板选 `配置...`；Columns UI 下在布局设置里用该面板的配置命令。

| 字段 | 作用 |
| --- | --- |
| `Template` | `(Follow global)`，或 `<profile>\webview-ui\` 下只给这个面板用的模板文件夹 |
| `URL Override (optional)` | 不用任何模板、直接加载的 URL；优先于 `Template` |
| `Resources path` | 只读：面板实际加载的文件夹，发生回退时附带说明 |
| `Transparent Background`、`Grab Focus`、`Enable Drag & Drop`、`Enable DevTools` | 面板自己的开关；`Enable DevTools` 与全局的开发者工具设置叠加生效 |
| `Edge Style` | 面板边框：无、凹陷或灰色 |

确认对话框后改动立即生效，模板或 URL 变了时面板会重新加载页面。面板自己的模板里必须有 `index.html`，否则面板改用全局模板。页面不能自己改所在面板的模板、URL 或开发者工具开关：`panel.setConfig` 只接受名称、透明、焦点与拖放这几个字段。

## 让页面适应面板

面板里的页面不能移动、缩放 foobar2000 的窗口，也不能改它的样式：做这些事的 `window.*` 方法会以 `PANEL_MODE_UNSUPPORTED` 失败；WebView2 UI 不是主界面时，`tray.*` 与 `taskbar.*` 也一样。把依赖它们的控件藏起来。页面启动时查询模式：

```javascript
const info = await fb2k.invoke('window.getMode');
if (info.success === false) throw new Error(info.error);
if (info.panelMode) {
    document.body.classList.add('panel-mode', `mode-${info.mode}`);
}
```

```css
/* 窗口控件只在独立窗口里有意义 */
.panel-mode .title-bar,
.panel-mode .window-controls {
    display: none;
}
```

面板里 `mode` 是 `dui` 或 `cui`，主窗口里是 `standalone`。面板在 WebView 就绪时也会发一次带同样字段的 `panel:initialized`，但通常早于页面订阅，所以应读取 `window.getMode`，不要等这个事件。

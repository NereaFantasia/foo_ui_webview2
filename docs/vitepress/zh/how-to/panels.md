# 把页面放进面板

除了作为顶层用户界面接管整个窗口，组件还为 Default UI 和 Columns UI 界面布局提供专用的 `WebView2 Panel` 面板元素。每个面板实例独立运行各自的前端页面。面板环境下的具体能力差异见[运行模式](/zh/concepts/run-modes)。

## 在 Default UI 中添加面板 {#往-default-ui-里加面板}

1. 确认当前界面是 Default UI（`File → Preferences → Display → Default User Interface`）。
2. 打开布局编辑：`View → Layout → Enable Layout Editing Mode`。
3. 右键单击布局中的空白区域，选择 `Add New UI Element`，再选择 `WebView2 Panel`。
4. 退出布局编辑模式。

在布局编辑模式下，各个 WebView2 面板不渲染实际网页，而是显示 “WebView2 Panel / 右键单击进行配置” 的占位提示；退出编辑模式后页面自动恢复。

## 在 Columns UI 中添加面板 {#往-columns-ui-里加面板}

1. 安装 Columns UI 组件并将其设为当前活动用户界面。
2. 打开布局配置页（`File → Preferences → Display → Columns UI → Layout`）。
3. 从 `Panels` 分类中将 `WebView2 Panel` 添加至对应分栏布局中。

<a id="给面板指定自己的模板或-url"></a>

## 为面板配置专属模板或 URL 覆盖

新建面板默认跟随全局活动模板。如需指定专属内容，可进入面板属性配置对话框：Default UI 环境下启用布局编辑模式后右键单击面板并选择 `配置...`；Columns UI 环境下直接在布局树中选择该面板的配置命令。

| 字段 | 作用 |
| --- | --- |
| `Template` | `(Follow global)`，或 `<profile>\webview-ui\` 下只给该面板使用的专属模板文件夹 |
| `URL Override (optional)` | 不加载本地模板、直接加载的 URL 地址；优先级高于 `Template` |
| `Allow this site to access foobar2000 and this computer` | 授权该 URL 覆盖页面调用宿主 API（包括文件读写与系统命令执行）。本机环回地址（`localhost`、`127.x.x.x`、`[::1]`）默认受信任且该复选框置灰。外网域名须手动勾选授权；URL 改为其他来源时该授权自动重置 |
| `Resources path` | 只读：当前面板实际加载的物理目录绝对路径，发生回退时附带说明 |
| `Transparent Background`、`Grab Focus`、`Enable Drag & Drop`、`Enable DevTools` | 面板独立开关；`Enable DevTools` 与全局开发者设置叠加生效 |
| `Edge Style` | 面板边框样式：无边框（None）、凹陷（Sunken）或灰边（Grey） |

确认配置对话框后修改即时生效；当模板路径、URL 覆盖或安全权限发生变更时，面板将自动重新加载页面。面板自定义模板必须包含有效的 `index.html` 入口文件，否则宿主将回退使用全局模板。注意：运行中的页面无法通过 API 动态修改模板、URL 覆盖或 DevTools 开关，`panel.setConfig` 端点仅支持调整面板名称、背景透明、焦点捕获与拖放开关。

<a id="让页面适应面板"></a>

## 页面针对面板模式的自适应处理

面板内承载的页面无权控制 foobar2000 宿主顶层窗口：涉及窗口尺寸、位置、材质或状态的 `window.*` 方法均以 `PANEL_MODE_UNSUPPORTED` 失败；非独立主界面环境下 `tray.*` 与 `taskbar.*` 同样受限。建议在页面初始化阶段查询当前模式，并隐藏仅在独立窗口中生效的窗口控制按钮：

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

在面板环境中，`mode` 返回 `dui` 或 `cui`；在独立主窗口中返回 `standalone`。虽然面板在 WebView 初始化就绪时亦会派发一次包含相同字段的 `panel:initialized` 事件，但该事件通常早于页面脚本完成事件订阅，因此推荐在启动时直接调用 `window.getMode` 进行探测。

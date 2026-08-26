# 运行模式

> foo_ui_webview2 支持三种运行模式，适应不同的使用场景。

## 概述

| 模式 | 类名 | 接口 | 说明 |
| --- | --- | --- | --- |
| 独立窗口 | MainWindow | - | 替换默认 UI，完整窗口控制 |
| DUI 面板 | WebViewDuiElement | ui_element | 嵌入 Default UI 布局 |
| CUI 面板 | WebViewCuiPanel | uie::window | 嵌入 Columns UI 布局 |

所有模式共享相同的 API 和前端代码，但**面板模式下部分窗口控制 API 不可用**。

## 独立窗口模式 (Standalone)

当在 Default User Interface 中选择 `Webview2 UI` 时，组件会完全替换 foobar2000 的默认界面。

- 完整的窗口控制权（最大化、最小化、关闭、拖拽）
- 支持 DWM 效果（`window.setMica`、`window.setAcrylic`）
- 支持自定义标题栏和全屏模式
- 所有 Window API 均可用

## DUI 面板模式 (Default UI)

将 WebView2 作为 Default UI 布局中的一个 UI Element。

**使用方法：**

1. 确保当前使用 Default UI
2. `View` → `Layout` → `Enable Layout Editing Mode`
3. 右键空白区域 → `Add new UI element` → `WebView2 Panel`

**注意事项：**

- 在布局编辑模式下修改面板配置后，WebView 会自动隐藏直到退出编辑模式
- 配置对话框独立于面板宿主窗口，布局变更不影响对话框生命周期
- 面板可通过 `templateName` 加载 `profile/webview-ui/<templateName>/`，而不使用全局活动模板

## CUI 面板模式 (Columns UI)

将 WebView2 作为 Columns UI 布局中的一个 Panel。

**使用方法：**

1. 确保已安装 Columns UI 组件
2. `View` → `Columns UI` → `Layout`
3. 从面板列表中添加 `WebView2 Panel`

## 模式检测

启动时以及任何需要确认宿主模式的场景，都应主动调用 `window.getMode`，并读取返回的 `mode`、`panelMode`、`windowId`。

DUI/CUI 面板还可额外监听 `panel:initialized`，其 payload 同样包含上述三个字段，作为异步通知。独立窗口模式**不会**等待该事件——两者互补，不能互相替代。

```javascript
async function detectMode() {
    const info = await fb2k.invoke('window.getMode');
    // info.mode: 'standalone' | 'dui' | 'cui' | ...
    // info.panelMode: boolean
    // info.windowId: string
    return info;
}

// DUI/CUI 面板可选的异步通知
fb2k.on('panel:initialized', (data) => {
    // data.mode, data.panelMode, data.windowId
    if (data.panelMode) {
        document.body.classList.add('panel-mode');
        document.body.classList.add(`mode-${data.mode}`);
    }
});

const current = await detectMode();
console.log(current.mode, current.panelMode, current.windowId);
```

## API 兼容性矩阵

| API | 独立窗口 | DUI 面板 | CUI 面板 |
| --- | --- | --- | --- |
| window.minimize | ✓ | ✗ | ✗ |
| window.maximize | ✓ | ✗ | ✗ |
| window.setMica | ✓ | ✗ | ✗ |
| window.setAcrylic | ✓ | ✗ | ✗ |
| window.setCornerPreference | ✓ | ✗ | ✗ |
| window.startDrag | ✓ | ✗ | ✗ |
| window.getMode | ✓ | ✓ | ✓ |
| playback.* | ✓ | ✓ | ✓ |
| playlist.* | ✓ | ✓ | ✓ |
| library.* | ✓ | ✓ | ✓ |
| artwork.* | ✓ | ✓ | ✓ |
| config.* | ✓ | ✓ | ✓ |
| metadata.* | ✓ | ✓ | ✓ |

不存在合并的 DWM effect 设置 API。请使用辅助接口 window.setMica 和 window.setAcrylic。

## 最佳实践

```javascript
const modeInfo = await fb2k.invoke('window.getMode');
if (modeInfo.panelMode) {
    document.body.classList.add('panel-mode');
    document.body.classList.add(`mode-${modeInfo.mode}`);
}

fb2k.on('panel:initialized', (data) => {
    if (data.panelMode) {
        document.body.classList.add('panel-mode');
        document.body.classList.add(`mode-${data.mode}`);
    }
});
```

```css
/* 面板模式隐藏仅适用于独立窗口的控件 */
.panel-mode .title-bar,
.panel-mode .window-controls {
    display: none !important;
}
.panel-mode .main-content {
    padding-top: 0;
}
```

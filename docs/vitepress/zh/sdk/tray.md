# fb.tray 系统托盘

本页是 `fb.tray` 的 SDK 视角文档入口。

`create()` / `setIcon()` 的顶层 `icon` 只接受裸 Base64 编码的 `.ico`
文件字节，不带 `data:` 或 `base64:` 前缀。PNG、JPEG、SVG、Data URL 或
无效 Base64 会回退到 foobar2000 主图标。`TrayMenuItem.icon` 是当前不渲染
的保留字段；WebView 菜单项图标请使用 `iconSvg`。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## 其余方法

### appendMenuItems()

封装 `tray.appendMenuItems`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.tray.appendMenuItems(/* 参数见 TypeScript 声明 */);
```

### clearMenuItems()

封装 `tray.clearMenuItems`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.tray.clearMenuItems(/* 参数见 TypeScript 声明 */);
```

### create()

封装 `tray.create`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.tray.create(/* 参数见 TypeScript 声明 */);
```

### destroy()

封装 `tray.destroy`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.tray.destroy(/* 参数见 TypeScript 声明 */);
```

### getMenuItems()

封装 `tray.getMenuItems`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.tray.getMenuItems(/* 参数见 TypeScript 声明 */);
```

### isVisible()

封装 `tray.isVisible`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.tray.isVisible(/* 参数见 TypeScript 声明 */);
```

### removeMenuItems()

封装 `tray.removeMenuItems`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.tray.removeMenuItems(/* 参数见 TypeScript 声明 */);
```

### setCloseToTray()

封装 `tray.setCloseToTray`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.tray.setCloseToTray(/* 参数见 TypeScript 声明 */);
```

### setContextMenu()

封装 `tray.setContextMenu`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.tray.setContextMenu(/* 参数见 TypeScript 声明 */);
```

### setIcon()

封装 `tray.setIcon`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.tray.setIcon(/* 参数见 TypeScript 声明 */);
```

### setMenuItemState()

封装 `tray.setMenuItemState`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.tray.setMenuItemState(/* 参数见 TypeScript 声明 */);
```

### setMinimizeToTray()

封装 `tray.setMinimizeToTray`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.tray.setMinimizeToTray(/* 参数见 TypeScript 声明 */);
```

### setTooltip()

封装 `tray.setTooltip`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.tray.setTooltip(/* 参数见 TypeScript 声明 */);
```

### showBalloon()

签名：`fb.tray.showBalloon(opts: { title: string; message: string; icon?: string }): Promise<BaseResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `opts.title` | `string` | 是 | 通知标题 |
| `opts.message` | `string` | 是 | 通知正文 |
| `opts.icon` | `string` | 否 | `'info'`（默认）、`'warning'` 或 `'error'` |

```javascript
await fb.tray.showBalloon({
  title: '播放',
  message: '播放列表已经结束。',
  icon: 'info',
});
```

<!-- END AUTO-GENERATED SDK STUBS -->

## 布局模式（`layoutMode`）

`TrayMenuConfig.layoutMode` **默认为 `'flat'`**。不做任何配置时，WebView 托盘菜单保持原有的直接子元素 DOM（`#menu > .fb-item` / `.fb-sep`），已有的结构选择器不用改。

### flat（默认）——无需迁移 DOM

```javascript
await fb.tray.setContextMenu(items, {
  render: 'webview',
  // 省略 layoutMode 即为 'flat'
  css: `
    #menu { display: flex; flex-direction: column; gap: 2px; }
    .fb-item[data-zone="playback"] { opacity: 0.95; }
  `,
});
```

### zones（按需启用）——top / playback / bottom 三个容器

```javascript
const ver = await fb.config.getVersionInfo();
const plugin = ver?.plugin?.version; // 启用 zones 前先探测版本
// zones 自 1.10.0 起提供；需兼容旧版时先探测运行时版本。
await fb.tray.setContextMenu(items, {
  render: 'webview',
  layoutMode: 'zones',
  css: `
    .fb-zone[data-zone="top"] { display: flex; flex-direction: column; }
    .fb-zone[data-zone="playback"] { display: grid; gap: 4px; }
    .fb-zone[data-zone="bottom"] { display: flex; flex-direction: column; }
    .fb-item[data-item-id="volume"] { padding-inline: 12px; }
  `,
});
```

### 兼容性说明

- **原生菜单**（`render: 'native'`）忽略 `layoutMode`。
- **旧版运行时**会忽略这个不认识的键（不会崩溃），但也不会生成 `.fb-zone` 包装层。
- **`menu.show` / `fb.menu.popup`** 始终用原有的直接子元素 DOM，不会继承托盘菜单的 zones。
- 稳定的样式挂载点：`.fb-menu[data-depth]`、`.fb-zone[data-zone]`、`.fb-item[data-item-id|data-kind|data-depth|data-zone]`。优先用它们，不要用 `:nth-child()`。
- `data-item-token` 是单次弹出内部使用的身份标识，**不是**对外的 CSS 契约。
- 这是一项按需启用的能力，不等于承诺“完全兼容”历史主题用过的每一个选择器。

## 滑块方向与可访问性

`TrayMenuItem.orientation` **只对滑块生效**（`'horizontal' | 'vertical'`），省略时按横向处理。SDK 原样透传这个字段，**不会**替你补默认值。

### 横向 / 纵向示例

```javascript
// 横向（默认）——旧版运行时和原生菜单遇到不认识的 orientation 也不会出错。
await fb.tray.setContextMenu([
  { id: 'vol', type: 'slider', label: 'Volume', min: 0, max: 100, value: 40 },
], { render: 'webview' });

// 纵向——先探测插件版本，不要硬编码一个假的最低版本号。
const { plugin } = await fb.config.getVersionInfo();
await fb.tray.setContextMenu([
  {
    id: 'vol',
    type: 'slider',
    label: 'Volume',
    min: 0,
    max: 100,
    value: 40,
    orientation: 'vertical',
  },
], { render: 'webview' });
```

### 键盘 / ARIA / 减弱动效

- 导航态：roving `tabindex` + 真实行焦点；Up/Down/Home/End 移动；Enter/Space 激活；子菜单 Right/Enter 展开并聚焦，Left 关闭并还原焦点，Escape 逐层退出。
- 编辑态（评分 / 滑块 / 分段控件）：Enter 或 Right 进入，焦点落到内部控件；Escape/Enter 退回该行。
- 纵向滑块：最小值在下、最大值在上；Up/Right 增大，Down/Left 减小；Home 取最小值，End 取最大值。
- `checked: false` 仍表示这是一个可勾选项（`menuitemcheckbox`）；普通菜单项直接不写 `checked`。
- 默认入退场动画遵循 `prefers-reduced-motion: reduce`（禁用 transform/transition），自定义 CSS 也应照做。这部分**不属于**受保护 CSS，也不改变 hide protocol 与 `closeAnimationMs`。
- 原生后端忽略 `orientation`（退化为分步子菜单）。旧版运行时会忽略这个键，保持横向交互。

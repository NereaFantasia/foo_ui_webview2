# fb.taskbar 任务栏

本页是 `fb.taskbar` 的 SDK 视角文档入口。

所有 `icon` 字段只接受裸 Base64 编码的 `.ico` 文件字节，不带 `data:` 或
`base64:` 前缀。PNG、JPEG、SVG 和 Data URL 不是有效的 taskbar 图标表示；
无效值可能回退到默认图标。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## 其余方法

### flash()

封装 `taskbar.flash`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.taskbar.flash(/* 参数见 TypeScript 声明 */);
```

### setOverlayIcon()

封装 `taskbar.setOverlayIcon`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.taskbar.setOverlayIcon(/* 参数见 TypeScript 声明 */);
```

### setThumbnailButtons()

封装 `taskbar.setThumbnailButtons`、`taskbar.updateButton`、`taskbar.setProgress`。缩略图工具栏在每个窗口中只能安装一次，之后应通过 `updateButton(options)` 更新已有按钮，不能增删按钮。单击按钮会发出 `taskbar:buttonClicked`，payload 为 `{ id }`。`setProgress({ state, value? })` 的状态可为 `none`、`indeterminate`、`normal`、`error` 或 `paused`；确定进度状态下的 `value` 是 0 到 1 的比例。

```javascript
const result = await fb.taskbar.setThumbnailButtons();
```

无需重装工具栏即可更新已有缩略图按钮：

```javascript
await fb.taskbar.updateButton({ id: 'play', tooltip: '暂停', enabled: true });
```

<!-- END AUTO-GENERATED SDK STUBS -->

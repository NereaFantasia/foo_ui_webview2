# fb.taskbar 任务栏

`fb.taskbar` 控制主窗口的任务栏按钮：缩略图工具栏按钮、进度、叠加图标与闪烁。在 Default UI 或 Columns UI 的面板里调用时一律以 `PANEL_MODE_UNSUPPORTED` 失败。

所有 `icon` 字段只接受裸 Base64 编码的 `.ico` 文件字节，不带 `data:` 或
`base64:` 前缀。PNG、JPEG、SVG 和 Data URL 不是有效的 taskbar 图标表示；
无效值可能回退到默认图标。

## flash(options?)

签名：`fb.taskbar.flash(options?: TaskbarFlashParams): Promise<TaskbarFlashResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| options.count | number | 否 | 闪烁次数，默认 3 |
| options.interval | number | 否 | 两次闪烁的间隔，单位毫秒；省略或为 `0` 时用系统光标闪烁频率 |

闪烁主窗口的任务栏按钮。主窗口的任务栏按钮尚未创建或主窗口已不存在时以 `OPERATION_FAILED` 失败。

```javascript
const result = await fb.taskbar.flash({ count: 3 });
```

## setOverlayIcon(options?)

签名：`fb.taskbar.setOverlayIcon(options?: TaskbarSetOverlayIconParams): Promise<TaskbarSetOverlayIconResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| options.icon | string \| null | 否 | 裸 Base64 编码的 `.ico` 文件字节，不带 `data:` 或 `base64:` 前缀；省略或传 `null` 时清除叠加图标 |
| options.description | string | 否 | 供辅助技术读出的图标说明 |

在任务栏按钮上叠加一个小图标，常用来表示播放状态；不带参数调用即清除。任务栏按钮尚未创建或系统拒绝时以 `OPERATION_FAILED` 失败。

```javascript
// 主题事先存下的 .ico 文件字节，裸 Base64；没有时为 null，即清除叠加图标
const icon = localStorage.getItem('pausedOverlayIcon');
const result = await fb.taskbar.setOverlayIcon({ icon, description: '已暂停' });
```

## setThumbnailButtons(buttons)

签名：`fb.taskbar.setThumbnailButtons(buttons: ThumbnailButton[]): Promise<TaskbarSetThumbnailButtonsResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| buttons | ThumbnailButton[] | 是 | 最多七个按钮，每个有 `id`、`tooltip`，可选 `icon` 与状态标志 |

封装 `taskbar.setThumbnailButtons`、`taskbar.updateButton`、`taskbar.setProgress`。缩略图工具栏在每个窗口中只能安装一次，之后应通过 `updateButton(options)` 更新已有按钮，不能增删按钮。单击按钮会发出 `taskbar:buttonClicked`，payload 为 `{ id }`。`setProgress({ state, value? })` 的状态可为 `none`、`indeterminate`、`normal`、`error` 或 `paused`；确定进度状态下的 `value` 是 0 到 1 的比例。

```javascript
const result = await fb.taskbar.setThumbnailButtons([
    { id: 'play-pause', tooltip: '播放或暂停' }
]);
```

无需重装工具栏即可更新已有缩略图按钮：

```javascript
await fb.taskbar.updateButton({ id: 'play', tooltip: '暂停', enabled: true });
```

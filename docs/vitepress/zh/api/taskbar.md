# Taskbar 任务栏 API

Windows 任务栏按钮的缩略图工具栏、进度条、叠加图标与闪烁。

> **前提**：仅在 standalone 窗口模式下可用。DUI/CUI 面板里调用 `taskbar.*` 与 `tray.*` 都会失败，`code` 为 `PANEL_MODE_UNSUPPORTED`。

---

## Taskbar API — 任务栏缩略图（5 个 API）

鼠标悬停任务栏图标时出现的预览缩略图工具栏，最多 7 个按钮。

### taskbar.setThumbnailButtons

<!-- api-schema:begin taskbar.setThumbnailButtons -->
安装任务栏预览缩略图上的工具栏。Windows 只允许每个窗口安装一次，之后用 `taskbar.updateButton` 改按钮状态。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `buttons` | `ThumbnailButton[]` | 是 | 按显示顺序排列的按钮，最多七个。超过时整个调用失败，不会截断。 |
| `buttons[].id` | `string` | 是 | 按钮标识；`taskbar:buttonClicked` 事件与 `taskbar.updateButton` 都用它指认按钮。不能为空。 |
| `buttons[].icon` | `string \| null` | 否 | 按钮图标，裸 Base64 编码的 `.ico` 文件字节，不带前缀。空、`null` 或省略时用 foobar2000 主图标。 |
| `buttons[].tooltip` | `string` | 否 | 悬停提示文本。默认 `""`。 |
| `buttons[].enabled` | `boolean` | 否 | 是否可点击。默认 `true`。 |
| `buttons[].visible` | `boolean` | 否 | 是否显示。默认 `true`。 |
| `buttons[].dismissOnClick` | `boolean` | 否 | 点击后是否关闭缩略图预览。默认 `false`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

超过七个按钮时整个调用以 `too many thumbnail buttons; Windows allows at most 7` 失败，不会截断；缺少 `buttons`、按钮没有 `id` 或带未声明的键时返回 `INVALID_PARAMS`。

```javascript
await fb2k.invoke('taskbar.setThumbnailButtons', {
    buttons: [
        { id: 'prev',      tooltip: '上一首' },
        { id: 'playPause', tooltip: '播放' },
        { id: 'next',      tooltip: '下一首' },
    ]
});

fb2k.on('taskbar:buttonClicked', ({ id }) => {
    if (id === 'prev')      fb2k.invoke('playback.previous');
    if (id === 'playPause') fb2k.invoke('playback.playOrPause');
    if (id === 'next')      fb2k.invoke('playback.next');
});
```

---

### taskbar.updateButton

<!-- api-schema:begin taskbar.updateButton -->
原地更新一个已安装的缩略图按钮，不能新增或删除按钮。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `id` | `string` | 是 | 按钮标识，即传给 `taskbar.setThumbnailButtons` 的 `id`。不能为空。 |
| `icon` | `string \| null` | 否 | 新图标，裸 Base64 编码的 `.ico` 文件字节，不带前缀。空、`null` 或省略时保持当前图标。 |
| `tooltip` | `string` | 否 | 新的悬停提示文本。空或省略时保持当前文本。默认 `""`。 |
| `enabled` | `boolean` | 否 | 是否可点击。省略时保持当前状态。 |
| `visible` | `boolean` | 否 | 是否显示。省略时保持当前状态。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

`id` 缺失或为空时返回 `INVALID_PARAMS`。

```javascript
// 播放状态变化时切换按钮提示
fb2k.on('playback:stateChanged', ({ state }) => {
    fb2k.invoke('taskbar.updateButton', {
        id: 'playPause',
        tooltip: state === 'playing' ? '暂停' : '播放',
    });
});
```

---

### taskbar.setProgress

<!-- api-schema:begin taskbar.setProgress -->
设置任务栏按钮上的进度条。主题写过进度后，插件到下一次播放状态变化前不再用播放进度覆盖它。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `state` | `"none" \| "indeterminate" \| "normal" \| "error" \| "paused"` | 否 | 进度条状态，`none` 表示不显示进度条。默认 `"none"`。 |
| `value` | `number` | 否 | 填充比例，对 `normal`、`error`、`paused` 三种状态有意义。取值 `0` 到 `1`（含端点）。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

`state` 不在枚举内、`value` 不在 0 到 1 之间时返回 `INVALID_PARAMS`。

```javascript
// 缓存最近一次已知时长；playback:time 只携带 position。
let currentDuration = 0;

fb2k.on('playback:stateChanged', ({ state, duration }) => {
    currentDuration = duration || 0;
    if (state === 'paused') {
        fb2k.invoke('taskbar.setProgress', { state: 'paused' });
    } else if (state === 'stopped') {
        fb2k.invoke('taskbar.setProgress', { state: 'none' });
    }
    // playing 时由下面 playback:time 在 position 推进时设置 normal
});

fb2k.on('playback:time', ({ position }) => {
    if (currentDuration > 0) {
        fb2k.invoke('taskbar.setProgress', {
            state: 'normal',
            value: position / currentDuration,
        });
    }
});
```

---

### taskbar.setOverlayIcon

<!-- api-schema:begin taskbar.setOverlayIcon -->
在任务栏按钮上叠加一个小徽标，或清除它。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `icon` | `string \| null` | 否 | 叠加图标，裸 Base64 编码的 `.ico` 文件字节，不带前缀。空、`null` 或省略时清除叠加图标。 |
| `description` | `string` | 否 | 叠加图标的无障碍说明文本。默认 `""`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
fb2k.on('playback:stateChanged', ({ state }) => {
    if (state === 'playing') {
        fb2k.invoke('taskbar.setOverlayIcon', { description: '正在播放' });
    } else {
        fb2k.invoke('taskbar.setOverlayIcon', {});  // 清除
    }
});
```

---

### taskbar.flash

<!-- api-schema:begin taskbar.flash -->
闪烁任务栏按钮以吸引注意。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `count` | `integer` | 否 | 闪烁次数。不小于 `0`。默认 `3`。 |
| `interval` | `integer` | 否 | 两次闪烁的间隔毫秒数，`0` 用系统光标闪烁速率。不小于 `0`。默认 `0`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
// 新曲目开始时闪烁
fb2k.on('playback:trackChanged', () => {
    fb2k.invoke('taskbar.flash', { count: 2 });
});
```

---

### 事件（Taskbar）

| 事件 | 数据 | 描述 |
| --- | --- | --- |
| `taskbar:buttonClicked` | `{ id: string }` | 缩略图按钮被点击 |

---

## 契约补充

<!-- phase3-supplement:taskbar.setProgress -->
### Contract 补充：`taskbar.setProgress`

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `state` | `string` | 否 | `none` | 取 `none`、`indeterminate`、`normal`、`error`、`paused` 之一；其他值以 `INVALID_PARAMS` 拒绝。 |
| `value` | `number` | 否 | — | 填充比例，0 到 1（含端点）；其他值以 `INVALID_PARAMS` 拒绝。 |

#### 返回值

只有信封：任务栏接受了本次变更时为 `{ success: true }`。即使参数合法，任务栏 COM 集成尚未初始化时也会失败，`code` 为 `OPERATION_FAILED`；在面板里调用同样失败，`code` 为 `PANEL_MODE_UNSUPPORTED`。

```js
// indeterminate 下 value 不生效
await fb2k.invoke('taskbar.setProgress', { state: 'indeterminate' });
```

## 运行时生命周期、菜单数据与事件

`taskbar.setProgress` 接受 `none`、`indeterminate`、`normal`、`error` 和 `paused`；
其他 `state` 或 0–1 以外的 `value` 都以 `INVALID_PARAMS` 拒绝。缩略图按钮激活时广播
`taskbar:buttonClicked`，payload 为 `{ id }`。

与 `tray` 共用的说明在 Tray 页：standalone 主窗口要求、缩略图按钮数量上限与事件监听示例见[运行时生命周期、菜单数据与事件](./tray.md#运行时生命周期、菜单数据与事件)，顶层 `icon` 字段的格式见 [tray.setContextMenu](./tray.md#tray-setcontextmenu) 下的「顶层图标格式」提示，任务栏与托盘一起初始化的写法见[完整示例](./tray.md#完整示例)。

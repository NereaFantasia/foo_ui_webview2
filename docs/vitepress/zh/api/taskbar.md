# Taskbar 任务栏 API

提供 Windows 任务栏集成能力，支持缩略图预览工具栏、播放进度条、状态徽标与窗口闪烁提醒。

> **前提**：仅在 standalone 窗口模式下可用。DUI/CUI 面板里调用 `taskbar.*` 与 `tray.*` 都会失败，`code` 为 `PANEL_MODE_UNSUPPORTED`。

---

## Taskbar API — 任务栏缩略图（5 个 API）

鼠标悬停任务栏图标时出现的预览缩略图工具栏，最多 7 个按钮。

### taskbar.setThumbnailButtons

<!-- api-schema:begin taskbar.setThumbnailButtons -->
配置任务栏缩略图预览工具栏按钮（Windows 系统限制每个窗口仅可初始化安装一次；后续状态变更须调用 taskbar.updateButton）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `buttons` | `ThumbnailButton[]` | 是 | 缩略图按钮配置列表（按显示顺序排列，最多允许 7 个；超出限制将直接报错拒绝而不做截断） |
| `buttons[].id` | `string` | 是 | 按钮唯一标识符（用于 taskbar:buttonClicked 事件派发与 taskbar.updateButton 状态更新）不能为空。 |
| `buttons[].icon` | `string \| null` | 否 | 按钮图标（.ico 文件未经前缀修饰的原始 Base64 编码字符串；为空、null 或省略时使用 foobar2000 主程序图标） |
| `buttons[].tooltip` | `string` | 否 | 鼠标悬停时的提示文本默认 `""`。 |
| `buttons[].enabled` | `boolean` | 否 | 按钮是否处于启用可点击状态默认 `true`。 |
| `buttons[].visible` | `boolean` | 否 | 按钮是否可见默认 `true`。 |
| `buttons[].dismissOnClick` | `boolean` | 否 | 点击按钮后是否自动收起任务栏缩略图预览窗口默认 `false`。 |

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
更新已安装缩略图按钮的状态与属性（工具栏不支持动态增删按钮）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `id` | `string` | 是 | 待更新按钮的标识符（对应 taskbar.setThumbnailButtons 中配置的 id）不能为空。 |
| `icon` | `string \| null` | 否 | 新按钮图标（.ico 文件原始 Base64 编码；为空、null 或省略时保持原图标不变） |
| `tooltip` | `string` | 否 | 新悬停提示文本（省略时保持原提示不变）默认 `""`。 |
| `enabled` | `boolean` | 否 | 按钮是否可点击（省略时保持原状态不变） |
| `visible` | `boolean` | 否 | 按钮是否显示（省略时保持原状态不变） |

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
设置任务栏图标上的进度条状态与进度值（主题主动写入进度后，宿主暂停自动镜像播放进度，直至下一次播放状态发生变更）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `state` | `"none" \| "indeterminate" \| "normal" \| "error" \| "paused"` | 否 | 进度条显示状态（none 清除进度条、indeterminate 不确定循环、normal 正常绿色、error 红色错误、paused 黄色暂停）默认 `"none"`。 |
| `value` | `number` | 否 | 进度条填充比例（取值范围 0.0 至 1.0，仅在 normal、error、paused 状态下生效）取值 `0` 到 `1`（含端点）。 |

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
设置或清除任务栏图标上的状态徽标图标

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `icon` | `string \| null` | 否 | 徽标图标（.ico 文件的原始 Base64 编码；为空、null 或省略时清除徽标） |
| `description` | `string` | 否 | 徽标图标的无障碍辅助说明文本默认 `""`。 |

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
触发任务栏窗口按钮闪烁以提示用户关注

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `count` | `integer` | 否 | 任务栏按钮闪烁总次数不小于 `0`。默认 `3`。 |
| `interval` | `integer` | 否 | 闪烁间隔时间（单位毫秒；传入 0 表示采用系统默认光标闪烁速率）不小于 `0`。默认 `0`。 |

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
| `taskbar:buttonClicked` | `{ id: string }` | 缩略图工具栏自定义按钮被点击 |

---

## 契约补充

<!-- phase3-supplement:taskbar.setProgress -->
### Contract 补充：`taskbar.setProgress`

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `state` | `string` | 否 | `none` | 取 `none`、`indeterminate`、`normal`、`error`、`paused` 之一；其他值以 `INVALID_PARAMS` 拒绝 |
| `value` | `number` | 否 | — | 填充比例（0 到 1，含端点）；其他值以 `INVALID_PARAMS` 拒绝 |

#### 返回值

只有信封：任务栏接受了本次变更时为 `{ success: true }`。即使参数合法，任务栏 COM 集成尚未初始化时也会失败，`code` 为 `OPERATION_FAILED`；在面板里调用同样失败，`code` 为 `PANEL_MODE_UNSUPPORTED`。

```js
// indeterminate 下 value 不生效
await fb2k.invoke('taskbar.setProgress', { state: 'indeterminate' });
```

## 运行时生命周期、菜单数据与事件

`taskbar.setProgress` 接受 `none`、`indeterminate`、`normal`、`error` 和 `paused`；其他 `state` 或 0–1 范围之外的 `value` 均以 `INVALID_PARAMS` 拒绝。缩略图工具栏按钮被点击时广播 `taskbar:buttonClicked` 事件，其数据负载为 `{ id }`。

与 `tray` 共用的机制说明请参阅托盘文档：standalone 独立主窗口运行要求、缩略图按钮上限与事件监听机制见[运行时生命周期、菜单数据与事件](./tray.md#运行时生命周期、菜单数据与事件)；顶层 `icon` 字段格式见 [tray.setContextMenu](./tray.md#tray-setcontextmenu) 下的「顶层图标格式」说明；任务栏与托盘统一初始化的代码模版见[完整示例](./tray.md#完整示例)。

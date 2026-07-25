# fb.taskbar 任务栏

本页是 `fb.taskbar` 的 SDK 视角文档入口。

所有 `icon` 字段只接受裸 Base64 编码的 `.ico` 文件字节，不带 `data:` 或
`base64:` 前缀。PNG、JPEG、SVG 和 Data URL 不是有效的 taskbar 图标表示；
无效值可能回退到默认图标。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## SDK 方法 stub

> 该区块用于补齐 SDK 视角方法覆盖，后续可人工扩展为完整示例与最佳实践。

### flash()

签名：`fb.taskbar.flash(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `taskbar.flash` 调用结果。

```javascript
const result = await fb.taskbar.flash();
```

### setOverlayIcon()

签名：`fb.taskbar.setOverlayIcon(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `taskbar.setOverlayIcon` 调用结果。

```javascript
const result = await fb.taskbar.setOverlayIcon();
```

### setThumbnailButtons()

签名：`fb.taskbar.setThumbnailButtons(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `taskbar.setThumbnailButtons`、`taskbar.updateButton`、`taskbar.setProgress` 调用结果。缩略图工具栏在每个窗口中只能安装一次，之后应通过 `updateButton(options)` 更新已有按钮，不能增删按钮。单击按钮会发出 `taskbar:buttonClicked`，payload 为 `{ id }`。`setProgress({ state, value? })` 的状态可为 `none`、`indeterminate`、`normal`、`error` 或 `paused`；确定进度状态下的 `value` 是 0 到 1 的比例。

```javascript
const result = await fb.taskbar.setThumbnailButtons();
```

无需重装工具栏即可更新已有缩略图按钮：

```javascript
await fb.taskbar.updateButton({ id: 'play', tooltip: '暂停', enabled: true });
```

<!-- END AUTO-GENERATED SDK STUBS -->

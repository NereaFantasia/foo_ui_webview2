# fb.taskbar 任务栏

本页是 `fb.taskbar` 的 SDK 视角文档入口。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## SDK 方法 stub

> 由 `scripts/gen_vitepress_sdk_doc.mjs` 生成。该区块用于补齐 SDK 视角方法覆盖，后续可人工扩展为完整示例与最佳实践。

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

返回值：底层 `taskbar.setThumbnailButtons`, `taskbar.updateButton`, `taskbar.setProgress` 调用结果。

```javascript
const result = await fb.taskbar.setThumbnailButtons();
```

<!-- END AUTO-GENERATED SDK STUBS -->

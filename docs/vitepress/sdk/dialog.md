# fb.dialog 对话框

本页是 `fb.dialog` 的 SDK 视角文档入口。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## SDK 方法 stub

> 由 `scripts/gen_vitepress_sdk_doc.mjs` 生成。该区块用于补齐 SDK 视角方法覆盖，后续可人工扩展为完整示例与最佳实践。

### confirm()

签名：`fb.dialog.confirm(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `dialog.confirm` 调用结果。

```javascript
const result = await fb.dialog.confirm();
```

### openFile()

签名：`fb.dialog.openFile(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `dialog.openFile` 调用结果。

```javascript
const result = await fb.dialog.openFile();
```

### openFolder()

签名：`fb.dialog.openFolder(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `dialog.openFolder` 调用结果。

```javascript
const result = await fb.dialog.openFolder();
```

### saveFile()

签名：`fb.dialog.saveFile(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `dialog.saveFile` 调用结果。

```javascript
const result = await fb.dialog.saveFile();
```

<!-- END AUTO-GENERATED SDK STUBS -->

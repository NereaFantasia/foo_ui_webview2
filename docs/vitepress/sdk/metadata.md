# fb.metadata metadata

本页是 `fb.metadata` 的 SDK 视角文档入口。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## SDK 方法 stub

> 由 `scripts/gen_vitepress_sdk_doc.mjs` 生成。该区块用于补齐 SDK 视角方法覆盖，后续可人工扩展为完整示例与最佳实践。

### read()

签名：`fb.metadata.read(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `metadata.read` 调用结果。

```javascript
const result = await fb.metadata.read();
```

### readBatch()

签名：`fb.metadata.readBatch(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `metadata.readBatch` 调用结果。

```javascript
const result = await fb.metadata.readBatch();
```

### readByPath()

签名：`fb.metadata.readByPath(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `metadata.readByPath`, `metadata.readRaw` 调用结果。

```javascript
const result = await fb.metadata.readByPath();
```

### removeField()

签名：`fb.metadata.removeField(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `metadata.removeField` 调用结果。

```javascript
const result = await fb.metadata.removeField();
```

### removeTag()

签名：`fb.metadata.removeTag(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `metadata.removeTag` 调用结果。

```javascript
const result = await fb.metadata.removeTag();
```

### write()

签名：`fb.metadata.write(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `metadata.write` 调用结果。

```javascript
const result = await fb.metadata.write();
```

### writeBatch()

签名：`fb.metadata.writeBatch(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `metadata.embedArtwork`, `metadata.removeEmbeddedArt` 调用结果。

```javascript
const result = await fb.metadata.writeBatch();
```

<!-- END AUTO-GENERATED SDK STUBS -->

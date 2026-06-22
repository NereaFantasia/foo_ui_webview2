# fb.queue 播放队列

本页是 `fb.queue` 的 SDK 视角文档入口。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## SDK 方法 stub

> 由 `scripts/gen_vitepress_sdk_doc.mjs` 生成。该区块用于补齐 SDK 视角方法覆盖，后续可人工扩展为完整示例与最佳实践。

### flush()

签名：`fb.queue.flush(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `queue.flush` 调用结果。

```javascript
const result = await fb.queue.flush();
```

### getCount()

签名：`fb.queue.getCount(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `queue.getCount` 调用结果。

```javascript
const result = await fb.queue.getCount();
```

<!-- END AUTO-GENERATED SDK STUBS -->

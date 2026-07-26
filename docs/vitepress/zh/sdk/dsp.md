# fb.dsp dsp

本页是 `fb.dsp` 的 SDK 视角文档入口。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## SDK 方法 stub

> 该区块用于补齐 SDK 视角方法覆盖，后续可人工扩展为完整示例与最佳实践。

### addDsp()

签名：`fb.dsp.addDsp(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `dsp.addDsp` 调用结果。

```javascript
const result = await fb.dsp.addDsp('{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}');
```

### applyPreset()

签名：`fb.dsp.applyPreset(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `dsp.applyPreset` 调用结果。

```javascript
const result = await fb.dsp.applyPreset(0);           // 按索引
const byName = await fb.dsp.applyPreset('My Preset'); // 按名称
```

### getAvailable()

签名：`fb.dsp.getAvailable(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `dsp.getAvailable` 调用结果。

```javascript
const result = await fb.dsp.getAvailable();
```

### getChain()

签名：`fb.dsp.getChain(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `dsp.getChain` 调用结果。

```javascript
const result = await fb.dsp.getChain();
```

### getPresets()

签名：`fb.dsp.getPresets(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `dsp.getPresets` 调用结果。

```javascript
const result = await fb.dsp.getPresets();
```

### moveDsp()

签名：`fb.dsp.moveDsp(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `dsp.moveDsp` 调用结果。

```javascript
const result = await fb.dsp.moveDsp(0, 2);
```

### removeDsp()

签名：`fb.dsp.removeDsp(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `dsp.removeDsp` 调用结果。

```javascript
const result = await fb.dsp.removeDsp(2);
```

### setChain()

签名：`fb.dsp.setChain(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `dsp.setChain` 调用结果。

```javascript
const result = await fb.dsp.setChain([{ guid: '{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}' }]);
```

<!-- END AUTO-GENERATED SDK STUBS -->

# fb.system 系统 

## listApis() 

列出所有可用 API。

```javascript
const apis = await fb.system.listApis(true, true);
// ["playback.play", "playback.pause", ...]
```

## getApiStats() 

获取 API 统计信息。

```javascript
const stats = await fb.system.getApiStats();
// {total: 368, ...}
```

## getApisByNamespace(namespace) 

获取指定命名空间下的所有 API。

```javascript
const apis = await fb.system.getApisByNamespace('playback');
```

## searchApis(query) 

搜索 API（支持模糊匹配）。

```javascript
const results = await fb.system.searchApis('volume');
```

## getRegisteredPlugins() 

获取所有已注册的外部插件列表。

## isPluginRegistered(namespace) 

检查指定插件是否已注册。

```javascript
const r = await fb.system.isPluginRegistered('my_plugin');
if (r.registered) { /* ... */ }
```

获取系统主题信息。返回 `{isDark, accentColor, ...}`。

```javascript
const theme = await fb.system.getTheme();
if (theme.isDark) document.body.classList.add('dark');

const dpi = await fb.system.getDPI();
console.log(`DPI: ${dpi.dpi}, Scale: ${dpi.scale}`);
```

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## SDK 方法 stub

> 由 `scripts/gen_vitepress_sdk_doc.mjs` 生成。该区块用于补齐 SDK 视角方法覆盖，后续可人工扩展为完整示例与最佳实践。

### getLocale()

签名：`fb.system.getLocale(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `system.getLocale` 调用结果。

```javascript
const result = await fb.system.getLocale();
```

### getRegisteredPlugins()

签名：`fb.system.getRegisteredPlugins(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `system.getRegisteredPlugins` 调用结果。

```javascript
const result = await fb.system.getRegisteredPlugins();
```

<!-- END AUTO-GENERATED SDK STUBS -->

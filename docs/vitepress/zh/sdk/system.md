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

## getTheme() / getDPI()

获取系统主题信息。返回 `{isDark, accentColor, ...}`。

```javascript
const theme = await fb.system.getTheme();
if (theme.isDark) document.body.classList.add('dark');

const dpi = await fb.system.getDPI();
console.log(`DPI: ${dpi.dpi}, Scale: ${dpi.scale}`);
```

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## 其余方法

### getLocale()

封装 `system.getLocale`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.system.getLocale(/* 参数见 TypeScript 声明 */);
```

### getRegisteredPlugins()

封装 `system.getRegisteredPlugins`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.system.getRegisteredPlugins(/* 参数见 TypeScript 声明 */);
```

<!-- END AUTO-GENERATED SDK STUBS -->

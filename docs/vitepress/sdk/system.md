# `fb.system` API and plugin discovery

## listApis() 

Lists available APIs. Optional booleans control inclusion of internal and external registrations. The result is `SystemApiInfo[]`, not a string array.

```javascript
const apis = await fb.system.listApis(true, true);
// [{ fullName, namespace, method, description, version, isExternal, ... }, ...]
```

## getApiStats() 

Returns API registration statistics as `SystemApiStatsResponse`.

```javascript
const stats = await fb.system.getApiStats();
console.log(stats.total);
```

## getApisByNamespace(namespace) 

Returns `SystemApiInfo[]` for a namespace.

```javascript
const apis = await fb.system.getApisByNamespace('playback');
```

## searchApis(query) 

Searches registered API metadata and returns `SystemApiInfo[]`.

```javascript
const results = await fb.system.searchApis('volume');
```

## getRegisteredPlugins() 

Returns all registered external plugins as `SystemPluginInfo[]`.

## isPluginRegistered(namespace) 

Checks whether an external plugin namespace is registered.

```javascript
const r = await fb.system.isPluginRegistered('my_plugin');
if (r.registered) { /* ... */ }
```

## getTheme() / getDPI() / getLocale()

`getTheme()` returns `{ darkMode, isDark, accentColor, transparency }`, `getDPI()` returns `{ dpi, scale }`, and `getLocale()` returns `{ locale: string }`.

```javascript
const { isDark } = await fb.system.getTheme();
document.documentElement.dataset.theme = isDark ? 'dark' : 'light';

const { dpi } = await fb.system.getDPI();
const { locale } = await fb.system.getLocale();
console.log(`DPI: ${dpi}; locale: ${locale}`);
```

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## Additional methods

> This block completes SDK method coverage and may later be expanded with richer examples and guidance.

### getLocale()

Signature: `fb.system.getLocale(): Promise<{ locale: string }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns the `system.getLocale` result.

```javascript
const result = await fb.system.getLocale();
```

### getRegisteredPlugins()

Signature: `fb.system.getRegisteredPlugins(): Promise<SystemPluginInfo[]>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns the `system.getRegisteredPlugins` result.

```javascript
const result = await fb.system.getRegisteredPlugins();
```

<!-- END AUTO-GENERATED SDK STUBS -->

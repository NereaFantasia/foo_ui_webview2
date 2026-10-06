# `fb.system` API and plugin discovery

## listApis() 

Lists available APIs. Optional booleans control inclusion of internal and external registrations. Resolves with `{ apis }`, a `SystemApiInfo[]` rather than a string array, or with a failure envelope.

```javascript
const res = await fb.system.listApis(true, true);
if (res.success === false) throw new Error(res.error);
// res.apis: [{ fullName, namespace, method, description, version, isExternal, ... }, ...]
```

## getApiStats() 

Returns API registration statistics as `SystemGetApiStatsResponse` (`SystemApiStatsResponse` remains as an alias).

```javascript
const stats = await fb.system.getApiStats();
if (stats.success === false) throw new Error(stats.error);
console.log(stats.totalApis, stats.byNamespace.playback);
```

## getApisByNamespace(namespace) 

Resolves with `{ apis }`, the `SystemApiInfo[]` of a namespace; empty for an unknown namespace.

```javascript
const res = await fb.system.getApisByNamespace('playback');
```

## searchApis(query) 

Searches registered API metadata and resolves with `{ apis }`, a `SystemApiInfo[]`.

```javascript
const res = await fb.system.searchApis('volume');
```

## getRegisteredPlugins() 

Lists the external plugins that registered methods with the bridge. Resolves with `{ plugins }`, a `SystemPluginInfo[]`; each entry has `name`, `namespace` (unique among plugins), `version`, `author`, `description`, `apis` (full method names) and `apiCount` (the length of `apis`).

```javascript
const res = await fb.system.getRegisteredPlugins();
if (res.success === false) throw new Error(res.error);
for (const p of res.plugins) console.log(`${p.namespace} ${p.version}: ${p.apiCount} methods`);
```

## isPluginRegistered(namespace) 

Checks whether an external plugin namespace is registered.

```javascript
const r = await fb.system.isPluginRegistered('my_plugin');
if (r.success === false) throw new Error(r.error);
if (r.registered) { /* ... */ }
```

## getTheme() / getDPI() / getLocale()

`getTheme()` returns the system theme as `{ darkMode, isDark, accentColor, transparency }` and `getDPI()` returns `{ dpi, scale }`. `getLocale()` returns the Windows user locale as `{ locale, language, country }`: `locale` is a tag such as `en-US`, and `en-US` when Windows does not report one; `language` and `country` are the localized language and country or region names, empty when unavailable.

```javascript
const res = await fb.system.getTheme();
if (res.success === false) throw new Error(res.error);
const { isDark } = res;
document.documentElement.dataset.theme = isDark ? 'dark' : 'light';

const res2 = await fb.system.getDPI();
if (res2.success === false) throw new Error(res2.error);
const { dpi } = res2;
const res3 = await fb.system.getLocale();
if (res3.success === false) throw new Error(res3.error);
const { locale } = res3;
console.log(`DPI: ${dpi}; locale: ${locale}`);
```


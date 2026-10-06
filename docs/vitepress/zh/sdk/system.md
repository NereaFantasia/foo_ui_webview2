# fb.system 系统 

## listApis() 

列出所有可用 API，返回 `{ apis }`，`apis` 是 `SystemApiInfo[]`；失败时返回失败信封。

```javascript
const res = await fb.system.listApis(true, true);
if (res.success === false) throw new Error(res.error);
// res.apis: [{ fullName: "playback.play", namespace: "playback", method: "play", ... }, ...]
```

## getApiStats() 

获取 API 统计信息，返回 `SystemGetApiStatsResponse`。

```javascript
const stats = await fb.system.getApiStats();
// { success: true, totalApis: 368, internalApis: 368, externalApis: 0, pluginCount: 0, byNamespace: { playback: 27, ... } }
```

## getApisByNamespace(namespace) 

获取指定命名空间下的所有 API，返回 `{ apis }`；未知的命名空间得到空列表。

```javascript
const res = await fb.system.getApisByNamespace('playback');
```

## searchApis(query) 

搜索名字或描述包含 `query` 的 API，返回 `{ apis }`。

```javascript
const res = await fb.system.searchApis('volume');
```

## getRegisteredPlugins() 

列出向 bridge 注册了方法的外部插件，返回 `{ plugins }`，即 `SystemPluginInfo[]`。每项有 `name`、`namespace`（插件之间唯一）、`version`、`author`、`description`、`apis`（完整方法名列表）与 `apiCount`（`apis` 的长度）。

```javascript
const res = await fb.system.getRegisteredPlugins();
if (res.success === false) throw new Error(res.error);
for (const p of res.plugins) console.log(`${p.namespace} ${p.version}: ${p.apiCount} 个方法`);
```

## isPluginRegistered(namespace) 

检查指定插件是否已注册。

```javascript
const r = await fb.system.isPluginRegistered('my_plugin');
if (r.success === false) throw new Error(r.error);
if (r.registered) { /* ... */ }
```

## getTheme() / getDPI() / getLocale()

`getTheme()` 返回系统主题 `{ darkMode, isDark, accentColor, transparency }`，`getDPI()` 返回 `{ dpi, scale }`。`getLocale()` 返回 Windows 用户区域设置 `{ locale, language, country }`：`locale` 形如 `zh-CN`，取不到时为 `en-US`；`language` 与 `country` 是本地化的语言名和国家或地区名，取不到时为空字符串。

```javascript
const theme = await fb.system.getTheme();
if (theme.success === false) throw new Error(theme.error);
if (theme.isDark) document.body.classList.add('dark');

const dpi = await fb.system.getDPI();
if (dpi.success === false) throw new Error(dpi.error);
console.log(`DPI: ${dpi.dpi}, Scale: ${dpi.scale}`);

const loc = await fb.system.getLocale();
if (loc.success === false) throw new Error(loc.error);
document.documentElement.lang = loc.locale;
```


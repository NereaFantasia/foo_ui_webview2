# System API

Methods of the `system` namespace.

## system

### system.getApiStats

<!-- api-schema:begin system.getApiStats -->
Count the registered methods, overall and per namespace, and the registered plugins.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `totalApis` | `integer` | Number of registered methods. |
| `internalApis` | `integer` | Number of built-in methods. |
| `externalApis` | `integer` | Number of methods registered by external plugins. |
| `pluginCount` | `integer` | Number of registered external plugins. |
| `byNamespace` | `Record<string, integer>` | Method count per namespace. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('system.getApiStats');
```

### system.getApisByNamespace

<!-- api-schema:begin system.getApisByNamespace -->
List the registered methods of one namespace; an unknown namespace lists nothing.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `namespace` | `string` | Yes | Namespace to list, such as `playback`. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `apis` | [SystemApiInfo[]](../reference/types.md#systemapiinfo) | The namespace's methods, in the order of `system.listAvailableApis`; empty for an unknown namespace. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const apis = await fb2k.invoke('system.getApisByNamespace', { namespace: 'playback' });
```

### system.getDPI

<!-- api-schema:begin system.getDPI -->
Report the DPI of the display showing the calling window; the main window's when the caller cannot be resolved.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `dpi` | `integer` | DPI of the display; `96` when no window can be resolved. |
| `scale` | `number` | `dpi / 96`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('system.getDPI');
```

### system.getLocale

<!-- api-schema:begin system.getLocale -->
Report the Windows user locale with its localized language and country names.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `locale` | `string` | Locale tag such as `en-US`, the default when Windows reports none. |
| `language` | `string` | Localized language name; empty when unavailable. |
| `country` | `string` | Localized country or region name; empty when unavailable. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('system.getLocale');
```

### system.getRegisteredPlugins

<!-- api-schema:begin system.getRegisteredPlugins -->
List the external plugins that registered methods with the bridge.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `plugins` | [SystemPluginInfo[]](../reference/types.md#systemplugininfo) | The plugins. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('system.getRegisteredPlugins');
```

### system.getTheme

<!-- api-schema:begin system.getTheme -->
Report the Windows personalization settings: app theme, accent color and transparency.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `darkMode` | `boolean` | `true` when Windows uses the dark app theme. |
| `isDark` | `boolean` | Same as `darkMode`; kept for callers that read this name. |
| `accentColor` | `string` | Accent color as `#RRGGBB`; `#0078D4` when DWM cannot report one. |
| `transparency` | `boolean` | Whether transparency effects are enabled. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('system.getTheme');
```

### system.isPluginRegistered

<!-- api-schema:begin system.isPluginRegistered -->
Report whether an external plugin owns the namespace.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `namespace` | `string` | Yes | Namespace to look up. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `registered` | `boolean` | Whether an external plugin owns the namespace. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('system.isPluginRegistered', { namespace: 'myplugin' });
if (res.success === false) throw new Error(res.error);
const { registered } = res;
```

### system.listAvailableApis

<!-- api-schema:begin system.listAvailableApis -->
List the methods the bridge currently accepts, built-in ones and those registered by external plugins.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `includeInternal` | `boolean` | No | Include built-in methods. Default: `true`. |
| `includeExternal` | `boolean` | No | Include methods registered by external plugins. Default: `true`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `apis` | [SystemApiInfo[]](../reference/types.md#systemapiinfo) | The methods, sorted by `fullName` in ascending byte order. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const apis = await fb2k.invoke('system.listAvailableApis', { includeExternal: false });
```

### system.searchApis

<!-- api-schema:begin system.searchApis -->
Find methods whose full name or description contains `query`, ignoring case.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `query` | `string` | Yes | Text to look for in full names and descriptions. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `apis` | [SystemApiInfo[]](../reference/types.md#systemapiinfo) | The matching methods, in the order of `system.listAvailableApis`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const apis = await fb2k.invoke('system.searchApis', { query: 'playlist' });
```

## Owner-family behavior and limits

- `system.*` reports registered runtime and plugin information.

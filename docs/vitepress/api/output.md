# Output API

Methods of the `output` namespace.

## output

### output.getDevices

<!-- api-schema:begin output.getDevices -->
List the devices of every output module.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `devices` | `OutputDevice[]` | Devices of all output modules, module by module. |
| `devices[].guid` | `string` | Device GUID rendered as `{...}`. Not unique on its own: a module reports its default device with the all-zero GUID, so key devices by `(entryGuid, guid)`. |
| `devices[].name` | `string` | Display name of the device. |
| `devices[].entry` | `string` | Display name of the output module that provides the device. |
| `devices[].entryGuid` | `string` | GUID of that output module, rendered as `{...}`. |
| `count` | `integer` | Number of entries in `devices`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('output.getDevices');
```

**`guid` is not unique within this response.** foobar2000 reports an output
module's "default device" using the all-zero GUID
`{00000000-0000-0000-0000-000000000000}`, so it appears once per module. Key
devices by the `(entryGuid, guid)` pair rather than by `guid` alone.

### output.getEntries

<!-- api-schema:begin output.getEntries -->
List the output modules (foobar2000's output entries) with their capability flags.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `entries` | `OutputEntry[]` | Every output module. |
| `entries[].guid` | `string` | Module GUID rendered as `{...}`. |
| `entries[].name` | `string` | Display name of the module. Several modules may share one, and some report an empty name. |
| `entries[].needsBitdepthConfig` | `boolean` | The module wants an output bit depth configured. |
| `entries[].needsDitherConfig` | `boolean` | The module wants dithering configured. |
| `entries[].supportsMultipleStreams` | `boolean` | The module can play several streams at once. |
| `entries[].isHighLatency` | `boolean` | The module declares itself high latency. |
| `entries[].isLowLatency` | `boolean` | The module declares itself low latency. |
| `count` | `integer` | Number of entries in `entries`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('output.getEntries');
```

### output.getSettings

<!-- api-schema:begin output.getSettings -->
Report the display names of the output modules. Informational only: the output settings are edited in foobar2000 Preferences, and `config.setOutputDevice` switches the device.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `note` | `string` | Fixed English sentence saying where output settings are edited. |
| `availableOutputs` | `string[]` | Display names of the output modules. Names can repeat or be empty, and the order is not stable between calls; prefer `output.getEntries`, which pairs each name with its GUID. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('output.getSettings');
```

Informational only — output configuration is owned by foobar2000 Preferences, and
`config.setOutputDevice` is the way to switch devices.

**Avoid `availableOutputs` in new code.** It is a bare list of display names with
two observed problems: modules that share a display name are indistinguishable,
and some modules report an empty name. Its order comes from service enumeration
and is **not stable between calls**, so array indices are not usable as
identifiers. Use `output.getEntries`, which pairs each name with its GUID.

## Runtime behavior notes

- `output.getSettings` is read-only discovery information. Output configuration is managed by foobar2000 Preferences rather than this API.

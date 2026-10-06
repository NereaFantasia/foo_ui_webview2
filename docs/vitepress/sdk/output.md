# fb.output Audio Output Discovery

`fb.output` exposes audio-output devices, output modules, and output settings.

## getDevices()

Signature: `fb.output.getDevices(): Promise<OutputGetDevicesResponse>`

Lists the devices of every output module, module by module, in `devices`, together with their `count`. A device `guid` is not unique on its own; see [Device List](#device-list) for the device fields and how to key them.

```javascript
const res = await fb.output.getDevices();
if (res.success === false) throw new Error(res.error);
console.log(`${res.count} output devices`);
```

## getEntries()

Signature: `fb.output.getEntries(): Promise<OutputGetEntriesResponse>`

Returns available output-module descriptors in `entries`, together with their `count`. Each `OutputEntryInfo` includes `guid`, `name`, and capability flags such as `needsBitdepthConfig`, `needsDitherConfig`, `supportsMultipleStreams`, `isHighLatency`, and `isLowLatency`. Several modules may share one `name`, and some report an empty name, so identify a module by its `guid`.

```javascript
const res = await fb.output.getEntries();
if (res.success === false) throw new Error(res.error);
const { entries = [] } = res;
```

## getSettings()

Signature: `fb.output.getSettings(): Promise<OutputGetSettingsResponse>`

Returns the host's output settings summary: `availableOutputs`, the display names of the output modules, and `note`, a fixed English sentence saying where output settings are edited. It does not return the active device configuration. Output settings are edited in foobar2000 Preferences, and [`fb.config.setOutputDevice`](./config.md) switches the device.

Names in `availableOutputs` can repeat or be empty, and their order is not stable between calls. Use `getEntries()` when you need each name paired with its GUID.

```javascript
const settings = await fb.output.getSettings();
```

## Device List

`fb.output.getDevices(): Promise<OutputGetDevicesResponse>` invokes `output.getDevices` and resolves with `{ devices, count }`, or with a failure envelope. A device carries `guid`, `name`, `entry` (owning output backend display name), and `entryGuid` (owning backend GUID).

The device `guid` is all-zero (`{00000000-0000-0000-0000-000000000000}`) for a backend's "default device" row and may repeat across backends — key rows by the `(entryGuid, guid)` pair, never by `guid` alone.

```javascript
const res = await fb.output.getDevices();
if (res.success === false) throw new Error(res.error);
// res.devices: [{ guid, name, entry, entryGuid }, ...]
const byKey = new Map(res.devices.map((d) => [`${d.entryGuid}|${d.guid}`, d]));
```

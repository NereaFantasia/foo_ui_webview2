# fb.output Audio Output Discovery

`fb.output` exposes audio-output devices, output modules, and output settings.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## SDK Method Stubs

> This block maintains SDK-facing method coverage and may be expanded with complete examples and best practices.

### getEntries()

Signature: `fb.output.getEntries(): Promise<OutputGetEntriesResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| None | — | — | This method takes no arguments. |

Returns available output-module descriptors in `entries`. Each `OutputEntryInfo` includes `guid`, `name`, and capability flags such as `needsBitdepthConfig`, `supportsMultipleStreams`, `isHighLatency`, and `isLowLatency`.

```javascript
const { entries = [] } = await fb.output.getEntries();
```

### getSettings()

Signature: `fb.output.getSettings(): Promise<OutputGetSettingsResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| None | — | — | This method takes no arguments. |

Returns the host's output settings summary, including optional `availableOutputs` and `note` fields.

```javascript
const settings = await fb.output.getSettings();
```

<!-- END AUTO-GENERATED SDK STUBS -->

## Device List

`fb.output.getDevices(): Promise<OutputDeviceInfo[]>` invokes `output.getDevices` and unwraps the host's `{ devices, count }` envelope. A device carries `guid`, `name`, `entry` (owning output backend display name), and `entryGuid` (owning backend GUID). When the host reports a failure the wrapper throws an `Error` with the host message instead of returning an empty array.

The device `guid` is all-zero (`{00000000-0000-0000-0000-000000000000}`) for a backend's "default device" row and may repeat across backends — key rows by the `(entryGuid, guid)` pair, never by `guid` alone.

```javascript
const devices = await fb.output.getDevices();
// [{ guid, name, entry, entryGuid }, ...]
const byKey = new Map(devices.map((d) => [`${d.entryGuid}|${d.guid}`, d]));
```

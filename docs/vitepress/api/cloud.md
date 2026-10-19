# Cloud API

Methods of the `cloud` namespace.

## cloud

### cloud.getStates

<!-- api-schema:begin cloud.getStates -->
Report the local/cloud state of media paths from file attributes alone. No file is opened and no content is read, so an online-only OneDrive placeholder is reported with `placeholder: true` instead of being hydrated; this is a metadata read, not a content read. Give exactly one of `paths` or `directory`: `paths` reports one entry per path in the given order, while `directory` reports the immediate children of one directory and does not recurse. A path that does not exist is reported with `exists: false` rather than failing the call; in `directory` mode a missing or non-directory path fails the call.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | No | Media paths to inspect, at most 256. A path that does not exist is a success with `exists: false`; `file://`, `file-relative://` and a `\|subsong:N` suffix are accepted and resolve to the underlying file. Must not be empty. |
| `directory` | `string` | No | Directory whose immediate children are inspected, in native path form. A directory that does not exist fails with `NOT_FOUND`; a path that is not a directory fails with `INVALID_PATH`. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `states` | `CloudPathState[]` | One entry per requested path, or one entry per immediate child of `directory`; directory entries are sorted by `path`. |
| `states[].path` | `string` | The path as given for `paths`, or the native child path for `directory`. |
| `states[].exists` | `boolean` | Whether the path exists on disk. A cloud placeholder does exist. |
| `states[].local` | `boolean` | Whether the path exists and its content is local, i.e. reading it would not need the cloud to supply data. |
| `states[].placeholder` | `boolean` | Whether the path exists and is a cloud placeholder that would be hydrated by a read (`FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS` set). Directory entries are the same shape: the directory itself being a placeholder is reported here too. |
| `states[].attrs` | `integer` | Raw Win32 file attribute bits (`GetFileAttributesW`), present when `exists` is `true`. `FILE_ATTRIBUTE_DIRECTORY` (`0x10`) tells a directory entry from a file entry. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('cloud.getStates', {
	paths: ['C:\\Music\\a.flac', 'C:\\Music\\b.flac'],
});
if (res.success === false) throw new Error(res.error);
for (const state of res.states) {
	console.log(state.path, state.exists, state.local, state.placeholder);
}
```

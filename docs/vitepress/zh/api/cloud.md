# Cloud 云状态 API

`cloud` 命名空间的方法：只凭文件属性读取媒体路径的本地/云端状态，不读文件内容、不触发水合。

## cloud

### cloud.getStates

<!-- api-schema:begin cloud.getStates -->
仅凭文件属性报告媒体路径的本地/云端状态。不打开文件、不读内容，因此仅在线可用的 OneDrive 占位符只会被标为 `placeholder: true` 而不会被水合；这是一次元数据读取，不是内容读。 `paths` 与 `directory` 必须恰好给一个：`paths` 按给定顺序每个路径一行，`directory` 只报该目录的直接子项、不递归。不存在的路径记 `exists: false`，不让整次调用失败；`directory` 模式下目录不存在或不是目录时整次调用失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 否 | 要检查的媒体路径，最多 256 个。路径不存在时成功返回 `exists: false`；接受 `file://`、 `file-relative://` 与 `\|subsong:N` 后缀，并解析到其底层文件。不能为空。 |
| `directory` | `string` | 否 | 要检查其直接子项的目录（原生路径形式）。目录不存在时以 `NOT_FOUND` 失败；路径不是目录时以 `INVALID_PATH` 失败。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `states` | `CloudPathState[]` | 每个请求路径一行，或 `directory` 的每个直接子项一行；目录项按 `path` 排序。 |
| `states[].path` | `string` | `paths` 模式下原样给出的路径；`directory` 模式下子项的原生路径。 |
| `states[].exists` | `boolean` | 路径在盘上是否存在。云占位符算存在。 |
| `states[].local` | `boolean` | 路径存在且内容在本地，即读取它不需要云端再提供数据。 |
| `states[].placeholder` | `boolean` | 路径存在且是读取时会水合的云占位符（`FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS` 置位）。目录项同样按此形状报告：目录本身是占位符也记在这里。 |
| `states[].attrs` | `integer` | 原始 Win32 文件属性位（`GetFileAttributesW`），`exists` 为 `true` 时出现。 `FILE_ATTRIBUTE_DIRECTORY`（`0x10`）用于区分目录项与文件项。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
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

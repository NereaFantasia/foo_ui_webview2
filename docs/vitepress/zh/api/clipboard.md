# Clipboard 剪贴板 API

提供 Windows 系统剪贴板的文本、富文本（HTML Format）与文件列表（CF_HDROP）读写能力。

## Clipboard API - 剪贴板 (4 个 API)

### clipboard.read

<!-- api-schema:begin clipboard.read -->
读取 Windows 系统剪贴板当前承载的数据状态（检测文本、文件列表及位图图片）

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `hasText` | `boolean` | 剪贴板当前是否包含文本数据（`CF_UNICODETEXT` 或 `CF_TEXT`） |
| `hasImage` | `boolean` | 剪贴板当前是否包含位图数据（`CF_DIB` 或 `CF_BITMAP`；仅报告存在状态，不返回图像二进制数据） |
| `hasFiles` | `boolean` | 剪贴板当前是否包含至少一个文件的文件列表数据（`CF_HDROP`） |
| `text` | `string` | 提取的纯文本内容（剪贴板无文本时为空字符串） |
| `files` | `string[]` | 提取的文件绝对路径列表（仅在 `hasFiles` 为 `true` 时提供） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const clip = await fb2k.invoke('clipboard.read');
if (clip.success === false) throw new Error(clip.error);
if (clip.hasText) console.log('文本:', clip.text);
if (clip.hasFiles) console.log('文件:', clip.files);
```

### clipboard.write

<!-- api-schema:begin clipboard.write -->
以 Unicode 纯文本替换 Windows 剪贴板内容

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `text` | `string` | 是 | 待写入剪贴板的纯文本内容不能为空。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('clipboard.write', { text: '复制的文本' });
```

### clipboard.writeHTML

<!-- api-schema:begin clipboard.writeHTML -->
以 `HTML Format` 富文本替换剪贴板内容，并同步写入纯文本回退内容

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `html` | `string` | 是 | 待以 `HTML Format` 写入剪贴板的 HTML 片段不能为空。 |
| `plainText` | `string` | 否 | 待以 `CF_UNICODETEXT` 格式写入的纯文本回退内容（缺省或为空时自动使用 HTML 文本内容） |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `htmlWritten` | `boolean` | `HTML Format` 富文本数据是否成功写入剪贴板 |
| `textWritten` | `boolean` | 纯文本回退内容是否成功写入剪贴板 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('clipboard.writeHTML', {
    html: '<b>艺术家</b> - <i>专辑</i>',
    plainText: '艺术家 - 专辑'
});
```

### clipboard.writeFiles

<!-- api-schema:begin clipboard.writeFiles -->
将文件绝对路径列表写入剪贴板（以 `CF_HDROP` 格式供文件资源管理器等 Shell 目标粘贴）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 待放入剪贴板的文件绝对路径列表（保持传入顺序）不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `fileCount` | `integer` | 实际成功写入剪贴板的文件路径数量 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('clipboard.writeFiles', {
    paths: ['C:\\Music\\song1.flac', 'C:\\Music\\song2.flac']
});
```

## 交互行为与限制

- `clipboard.writeFiles` 接受受媒体读取权限（MediaRead）保护的物理路径。`clipboard.writeHTML` 同时写入 HTML 格式与纯文本回退；`clipboard.read` 仅报告 Windows 系统剪贴板当前已就绪的格式与内容。

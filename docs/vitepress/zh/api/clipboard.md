# Clipboard 剪贴板 API

`clipboard` 命名空间的方法。

## Clipboard API - 剪贴板 (4 个 API)

### clipboard.read

<!-- api-schema:begin clipboard.read -->
报告 Windows 剪贴板当前持有的内容：文本、文件列表、图片。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `hasText` | `boolean` | 剪贴板有文本（`CF_UNICODETEXT` 或 `CF_TEXT`）。 |
| `hasImage` | `boolean` | 剪贴板有位图（`CF_DIB` 或 `CF_BITMAP`）。图片本身不返回。 |
| `hasFiles` | `boolean` | 剪贴板有文件列表（`CF_HDROP`）且至少有一个文件。 |
| `text` | `string` | 文本内容；剪贴板没有文本时为空。 |
| `files` | `string[]` | 文件路径列表，只在 `hasFiles` 为 `true` 时出现。 |

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
用 Unicode 文本替换剪贴板内容。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `text` | `string` | 是 | 要放到剪贴板的文本。不能为空。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('clipboard.write', { text: '复制的文本' });
```

### clipboard.writeHTML

<!-- api-schema:begin clipboard.writeHTML -->
用 `HTML Format` 富文本替换剪贴板内容，并同时放入纯文本回退。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `html` | `string` | 是 | 以 `HTML Format` 写入的 HTML 片段。不能为空。 |
| `plainText` | `string` | 否 | 以 `CF_UNICODETEXT` 写入的纯文本回退；缺省或为空时用 HTML 文本本身。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `htmlWritten` | `boolean` | `HTML Format` 数据已放到剪贴板。 |
| `textWritten` | `boolean` | 纯文本回退已放到剪贴板。 |

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
把文件列表放到剪贴板（`CF_HDROP`），供资源管理器等粘贴。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 绝对文件路径，按放入剪贴板的顺序。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `fileCount` | `integer` | 放到剪贴板的路径数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('clipboard.writeFiles', {
    paths: ['C:\\Music\\song1.flac', 'C:\\Music\\song2.flac']
});
```

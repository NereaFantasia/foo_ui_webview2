# Dialog 对话框 API

`dialog` 命名空间的方法：打开文件、保存文件、选择文件夹与确认对话框。

## Dialog API - 对话框

系统原生对话框。

### dialog.openFile

<!-- api-schema:begin dialog.openFile -->
打开文件选择对话框。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `title` | `string` | 否 | 窗口标题。默认“打开文件”，随界面语言本地化。 |
| `multiple` | `boolean` | 否 | 允许多选。默认 `false`。 |
| `defaultPath` | `string` | 否 | 对话框打开时定位到的目录，每次打开都定位；支持 `%music%`。路径解析不到文件夹时忽略。默认 `""`。 |
| `filters` | `FileFilter[]` | 否 | 文件类型过滤器。缺省或为空时只显示“所有文件”一项。 |
| `filters[].name` | `string` | 否 | 文件类型列表里显示的名称。默认“文件”，随界面语言本地化。 |
| `filters[].extensions` | `string[]` | 否 | 不带点的扩展名，如 `flac`；`*` 匹配所有文件。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `canceled` | `boolean` | 用户未选择就关闭对话框时为 `true`。 |
| `filePaths` | `string[]` | 选中的文件；取消时为空数组。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const result = await fb2k.invoke('dialog.openFile', {
    title: '选择音频文件',
    filters: [{ name: 'Audio', extensions: ['mp3', 'flac', 'wav'] }],
    multiple: true,
    defaultPath: '%music%'
});
if (result.success === false) throw new Error(result.error);
if (!result.canceled) {
    console.log('选中文件:', result.filePaths);
}
```

### dialog.saveFile

<!-- api-schema:begin dialog.saveFile -->
打开文件保存对话框。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `title` | `string` | 否 | 窗口标题。默认“保存文件”，随界面语言本地化。 |
| `defaultName` | `string` | 否 | 预填的文件名。默认 `""`。 |
| `filters` | `FileFilter[]` | 否 | 文件类型过滤器。缺省或为空时只显示“所有文件”一项。 |
| `filters[].name` | `string` | 否 | 文件类型列表里显示的名称。默认“文件”，随界面语言本地化。 |
| `filters[].extensions` | `string[]` | 否 | 不带点的扩展名，如 `flac`；`*` 匹配所有文件。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `canceled` | `boolean` | 用户未选择就关闭对话框时为 `true`。 |
| `filePath` | `string` | 选定的文件；取消时为空字符串。覆盖已有文件前系统会先询问。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const result = await fb2k.invoke('dialog.saveFile', {
    title: '导出播放列表',
    defaultName: 'playlist.json',
    filters: [{ name: 'JSON', extensions: ['json'] }]
});
if (result.success === false) throw new Error(result.error);
if (!result.canceled) {
    await fb2k.invoke('file.write', { path: result.filePath, content: data });
}
```

### dialog.openFolder

<!-- api-schema:begin dialog.openFolder -->
打开文件夹选择对话框。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `title` | `string` | 否 | 窗口标题。默认“选择文件夹”，随界面语言本地化。 |
| `defaultPath` | `string` | 否 | 对话框打开时定位到的目录，每次打开都定位；支持 `%music%`。路径解析不到文件夹时忽略。默认 `""`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `canceled` | `boolean` | 用户未选择就关闭对话框时为 `true`。 |
| `folderPath` | `string` | 用户确认的目录，不一定等于 `defaultPath`；取消时为空字符串。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const result = await fb2k.invoke('dialog.openFolder', {
    title: '选择音乐文件夹',
    defaultPath: 'D:\\Music'
});
if (result.success === false) throw new Error(result.error);
if (!result.canceled) {
    console.log('选中文件夹:', result.folderPath);
}
```

### dialog.confirm

<!-- api-schema:begin dialog.confirm -->
显示模态确认对话框（Windows TaskDialog），支持自定义按钮；Esc 与关闭按钮不能关闭它。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `title` | `string` | 否 | 窗口标题。默认“确认”，随界面语言本地化。 |
| `message` | `string` | 否 | 正文文本。默认 `""`。 |
| `type` | `"info" \| "warning" \| "error" \| "question"` | 否 | 图标。TaskDialog 没有问号图标，`question` 显示为信息图标。默认 `"question"`。 |
| `buttons` | `string[]` | 否 | 按钮文本，按顺序排列。默认“确定”“取消”，随界面语言本地化。 |
| `defaultButton` | `integer` | 否 | 默认聚焦的按钮索引。默认 `0`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `response` | `integer` | 被点按钮在 `buttons` 中的索引（从 0 开始）。只有连备用消息框都无法显示时才为 `-1`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const res = await fb2k.invoke('dialog.confirm', {
    title: '确认删除',
    message: '确定要删除选中的曲目吗？',
    type: 'warning',
    buttons: ['删除', '取消']
});
if (res.success === false) throw new Error(res.error);
const { response } = res;
if (response === 0) {
    // 用户点击了"删除"
}
```

## 对话框边界

- 原生对话框取消时返回 `canceled: true`，并提供空的结果路径或列表。对话框初始化失败会添加 `error`，并将 `canceled` 设为 `false`。

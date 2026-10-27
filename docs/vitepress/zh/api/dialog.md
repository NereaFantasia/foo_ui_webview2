# Dialog 对话框 API

提供 Windows 系统原生文件选择、文件保存、文件夹选择与模态任务对话框调用能力。

## Dialog API - 对话框

Windows 系统原生对话框接口。

### dialog.openFile

<!-- api-schema:begin dialog.openFile -->
弹出系统原生文件打开对话框

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `title` | `string` | 否 | 对话框窗口标题（默认“打开文件”，自动随宿主界面语言本地化） |
| `multiple` | `boolean` | 否 | 是否允许选择多个文件默认 `false`。 |
| `defaultPath` | `string` | 否 | 对话框打开时的初始定位目录（支持展开 %music% 变量；若路径无法解析为有效目录则自动忽略）默认 `""`。 |
| `filters` | `FileFilter[]` | 否 | 文件类型过滤器列表（缺省或为空时默认仅显示“所有文件”选项） |
| `filters[].name` | `string` | 否 | 文件类型列表中显示的类型说明名称（默认“文件”，自动随宿主界面语言本地化） |
| `filters[].extensions` | `string[]` | 否 | 匹配的文件扩展名列表（不包含前导句点，例如 flac；传入 * 匹配全部文件） |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `canceled` | `boolean` | 用户是否取消操作或直接关闭对话框 |
| `filePaths` | `string[]` | 用户最终选中的文件绝对路径列表（取消或未选择时为空数组） |

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
弹出系统原生文件保存对话框

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `title` | `string` | 否 | 对话框窗口标题（默认“保存文件”，自动随宿主界面语言本地化） |
| `defaultName` | `string` | 否 | 对话框打开时预填的默认文件名默认 `""`。 |
| `filters` | `FileFilter[]` | 否 | 文件类型过滤器列表（缺省或为空时默认仅显示“所有文件”选项） |
| `filters[].name` | `string` | 否 | 文件类型列表中显示的类型说明名称（默认“文件”，自动随宿主界面语言本地化） |
| `filters[].extensions` | `string[]` | 否 | 匹配的文件扩展名列表（不包含前导句点，例如 flac；传入 * 匹配全部文件） |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `canceled` | `boolean` | 用户是否取消操作或直接关闭对话框 |
| `filePath` | `string` | 用户确认保存的目标文件绝对路径（取消时为空字符串；覆盖已有文件时系统对话框将弹出覆盖确认） |

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
弹出系统原生文件夹选择对话框

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `title` | `string` | 否 | 对话框窗口标题（默认“选择文件夹”，自动随宿主界面语言本地化） |
| `defaultPath` | `string` | 否 | 对话框打开时的初始定位目录（支持展开 %music% 变量；若路径无法解析为有效目录则自动忽略）默认 `""`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `canceled` | `boolean` | 用户是否取消操作或直接关闭对话框 |
| `folderPath` | `string` | 用户确认选择的文件夹绝对路径（不一定等于 defaultPath；取消时为空字符串） |

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
弹出模态系统任务对话框（Windows TaskDialog；支持自定义按钮；Esc 键与窗口关闭按钮均不可直接关闭）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `title` | `string` | 否 | 确认对话框窗口标题（默认“确认”，自动随宿主界面语言本地化） |
| `message` | `string` | 否 | 对话框提示正文内容默认 `""`。 |
| `type` | `"info" \| "warning" \| "error" \| "question"` | 否 | 对话框图标类型（TaskDialog 原生无问号图标，question 将显示为信息图标）默认 `"question"`。 |
| `buttons` | `string[]` | 否 | 自定义按钮标签文本列表（按顺序排列；默认“确定”“取消”，自动随宿主界面语言本地化） |
| `defaultButton` | `integer` | 否 | 对话框打开时默认获得焦点的按钮零基索引默认 `0`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `response` | `integer` | 用户点击的按钮在 buttons 中的零基索引（仅在连备用消息框都无法弹出显示时返回 -1） |

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

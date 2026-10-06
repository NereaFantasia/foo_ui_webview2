# fb.dialog 对话框

`fb.dialog` 打开宿主原生的文件、文件夹、保存与确认对话框。

## confirm(options?)

签名：`fb.dialog.confirm(options?: DialogConfirmParams): Promise<DialogConfirmResponse>`

显示模态确认对话框，按钮可以自定义。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `options.title` | `string` | 否 | 窗口标题；默认“确认”，随界面语言本地化 |
| `options.message` | `string` | 否 | 确认文本 |
| `options.type` | `string` | 否 | 图标：`info`、`warning`、`error` 或 `question`；默认 `question`，TaskDialog 没有问号图标，显示为信息图标 |
| `options.defaultButton` | `number` | 否 | 默认聚焦的按钮索引；默认 `0` |
| `options.buttons` | `string[]` | 否 | 自定义按钮文本，按顺序排列 |

`response` 是被点按钮的索引，从 0 开始。默认按钮组是“确定”“取消”，`0` 表示确认、`1` 表示取消；没有 `confirmed` 字段。Esc 与关闭按钮不能关闭对话框，结果总是来自按钮点击；只有连宿主的备用消息框都无法显示时，`response` 才为 `-1`。

```javascript
const res = await fb.dialog.confirm({
	title: '移除曲目',
	message: '移除选中的曲目？'
});
if (res.success === false) throw new Error(res.error);
const { response } = res;
if (response === 0) {
	// 已确认
}
```

## openFile(options?)

签名：`fb.dialog.openFile(options?: DialogOpenFileParams): Promise<DialogOpenFileResponse>`

打开文件选择对话框。返回 `{ success: true, canceled, filePaths }`；对话框无法显示时返回失败信封 `{ success: false, error, code }`。`multiple` 为 `true` 时允许多选；其他选项有 `title`、`defaultPath`（每次打开都生效）和 `filters`。`filters` 是 `{ name, extensions }` 数组，`extensions` 只写扩展名本身，不带点，如 `flac`；`*` 匹配所有文件。

```javascript
const result = await fb.dialog.openFile({
	title: '选择音频文件',
	multiple: true,
	filters: [{ name: '音频', extensions: ['flac', 'mp3'] }]
});
```

## openFolder(options?)

签名：`fb.dialog.openFolder(options?: DialogOpenFolderParams): Promise<DialogOpenFolderResponse>`

打开文件夹选择对话框。可选参数 `title` 与 `defaultPath`：后者是对话框打开时定位到的目录，每次打开都定位；路径解析不到文件夹时静默忽略，`%music%` 展开为当前用户的音乐文件夹。返回 `{ success: true, canceled, folderPath }`（对话框无法显示时返回失败信封 `{ success: false, error, code }`），`folderPath` 是用户实际确认的目录，不一定等于 `defaultPath`。

```javascript
const result = await fb.dialog.openFolder({
	title: '选择音乐目录',
	defaultPath: 'D:\\Music'
});
```

## saveFile(options?)

签名：`fb.dialog.saveFile(options?: DialogSaveFileParams): Promise<DialogSaveFileResponse>`

打开文件保存对话框。返回 `{ success: true, canceled, filePath }`；对话框无法显示时返回失败信封 `{ success: false, error, code }`。选项有 `title`、`defaultName` 和 `filters`，`filters` 是 `{ name, extensions }` 数组。

```javascript
const result = await fb.dialog.saveFile({ defaultName: 'playlist.m3u8' });
```

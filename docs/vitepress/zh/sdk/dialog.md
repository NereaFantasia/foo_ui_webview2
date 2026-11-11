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

`response` 返回所点击按钮的从 0 开始的数字索引。默认按钮组为“确定”与“取消”，其中 `0` 表示确认、`1` 表示取消；响应中不包含 `confirmed` 字段。Esc 键与窗口关闭按钮均无法直接关闭对话框，返回值始终来源于按钮点击；仅当连宿主的后备消息框也无法弹出时，`response` 返回 `-1`。

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

弹出原生文件选择对话框。成功时返回 `{ success: true, canceled, filePaths }`；对话框无法显示时返回失败信封。设置 `multiple: true` 支持多选；可选参数包含 `title`、`defaultPath`（每次调用时定位初始目录）与 `filters`。`filters` 为 `{ name, extensions }` 对象数组，`extensions` 仅填写不含前导点的扩展名（如 `flac`）；传入 `*` 匹配所有文件。

```javascript
const result = await fb.dialog.openFile({
	title: '选择音频文件',
	multiple: true,
	filters: [{ name: '音频', extensions: ['flac', 'mp3'] }]
});
```

## openFolder(options?)

签名：`fb.dialog.openFolder(options?: DialogOpenFolderParams): Promise<DialogOpenFolderResponse>`

弹出原生文件夹选择对话框。支持可选参数 `title` 与 `defaultPath`：`defaultPath` 指定对话框打开时的初始定位目录，若路径无法解析为有效文件夹则自动忽略；支持使用 `%music%` 等内置变量展开为当前用户的音乐目录。成功时返回 `{ success: true, canceled, folderPath }`；对话框无法显示时返回失败信封。`folderPath` 为用户实际选择并确认的目录路径。

```javascript
const result = await fb.dialog.openFolder({
	title: '选择音乐目录',
	defaultPath: 'D:\\Music'
});
```

## saveFile(options?)

签名：`fb.dialog.saveFile(options?: DialogSaveFileParams): Promise<DialogSaveFileResponse>`

弹出原生文件保存对话框。成功时返回 `{ success: true, canceled, filePath }`；对话框无法显示时返回失败信封。选项包含 `title`、`defaultName`（建议文件名）与 `filters`（`{ name, extensions }` 结构数组）。

```javascript
const result = await fb.dialog.saveFile({ defaultName: 'playlist.m3u8' });
```

# fb.dialog 对话框

本页是 `fb.dialog` 的 SDK 视角文档入口。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## 其余方法

### confirm()

封装 `dialog.confirm`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.dialog.confirm(/* 参数见 TypeScript 声明 */);
```

### openFile()

封装 `dialog.openFile`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。`defaultPath` 每次打开都生效。

```javascript
await fb.dialog.openFile(/* 参数见 TypeScript 声明 */);
```

### openFolder()

封装 `dialog.openFolder`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。可选参数 `title` 与 `defaultPath`：后者是对话框打开时定位到的目录，每次打开都定位；路径解析不到文件夹时静默忽略，`%music%` 展开为当前用户的音乐文件夹。返回 `{ canceled, folderPath, error? }`，`folderPath` 是用户实际确认的目录，不一定等于 `defaultPath`。

```javascript
const result = await fb.dialog.openFolder({
	title: '选择音乐目录',
	defaultPath: 'D:\\Music'
});
```

### saveFile()

封装 `dialog.saveFile`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.dialog.saveFile(/* 参数见 TypeScript 声明 */);
```

<!-- END AUTO-GENERATED SDK STUBS -->

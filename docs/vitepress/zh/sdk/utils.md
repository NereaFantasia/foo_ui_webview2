# fb.utils 工具函数 

## ping() 

测试连接。

```javascript
const r = await fb.utils.ping(); // {mock: true} 或实际响应
```

## formatTitle(pattern, path?) 

使用 foobar2000 Title Formatting 语法格式化字符串。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| pattern | string | Title Format 模式串 |
| path | string | 可选，指定曲目路径（默认当前播放） |

```javascript
const r = await fb.utils.formatTitle('%artist% - %title%');
console.log(r); // "The Beatles - Let It Be"

// 指定曲目
const r2 = await fb.utils.formatTitle('%codec% %bitrate%kbps', 'E:\\Music\\song.flac');
```

## getFileInfo(path) 

读取文件元数据（结构化格式）。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| path | string | 音频文件路径 |

```javascript
const info = await fb.utils.getFileInfo('E:\\Music\\song.flac');
// {success, path, tags: {TITLE, ARTIST, ...}, info: {duration, bitrate, sampleRate, channels, codec}}
```

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## 其余方法

### echo()

封装 `test.echo`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.utils.echo(/* 参数见 TypeScript 声明 */);
```

<!-- END AUTO-GENERATED SDK STUBS -->

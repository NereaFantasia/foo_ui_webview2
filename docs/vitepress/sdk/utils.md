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
const r2 = await fb.utils.formatTitle('%codec% %bitrate%kbps', 'E:\\\\Music\\\\song.flac');
```

## getFileInfo(path) 

读取文件元数据（结构化格式）。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| path | string | 音频文件路径 |

```javascript
const info = await fb.utils.getFileInfo('E:\\\\Music\\\\song.flac');
// {success, path, tags: {TITLE, ARTIST, ...}, info: {duration, bitrate, sampleRate, channels, codec}}
```

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## SDK 方法 stub

> 由 `scripts/gen_vitepress_sdk_doc.mjs` 生成。该区块用于补齐 SDK 视角方法覆盖，后续可人工扩展为完整示例与最佳实践。

### echo()

签名：`fb.utils.echo(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `test.echo` 调用结果。

```javascript
const result = await fb.utils.echo();
```

<!-- END AUTO-GENERATED SDK STUBS -->

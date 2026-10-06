# fb.utils 工具函数

## ping()

测试连接。经 `test.ping` 返回 `{ pong, timestamp }`，`timestamp` 是宿主的 Unix 时间（秒）。

```javascript
const { pong } = await fb.utils.ping(); // pong === true
```

## echo(message)

经 `test.echo` 回显一条消息：消息原样放在 `echo` 里返回，`input` 是宿主收到的参数。

签名：`fb.utils.echo(message: string): Promise<TestEchoResponse>`

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| message | string | 以 `{ message }` 发给 `test.echo` 的消息 |

```javascript
const { echo } = await fb.utils.echo('Hello'); // echo === 'Hello'
```

## formatTitle(pattern, path?)

经 `titleformat.eval` 求值一个 foobar2000 Title Formatting 表达式。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| pattern | string | Title Formatting 模式串 |
| path | string | 可选，曲目路径；省略时对当前曲目求值 |

SDK 原样返回宿主的响应 `{ result: string }`，不会拆出字符串。

```javascript
const res = await fb.utils.formatTitle('%artist% - %title%');
if (res.success === false) throw new Error(res.error);
const { result } = res;
console.log(result); // "The Beatles - Let It Be"

// 对指定曲目求值
const r2 = await fb.utils.formatTitle('%codec% %bitrate%kbps', 'E:\\Music\\song.flac');
```

## getFileInfo(path)

经 `metadata.read` 读取文件元数据，返回它的结构化响应。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| path | string | 音频文件路径 |

```javascript
const info = await fb.utils.getFileInfo('E:\\Music\\song.flac');
// {success, path, tags: {TITLE, ARTIST, ...}, info: {duration, bitrate, sampleRate, channels, codec}}
```

# fb.titleformat 标题格式化

`fb.titleformat` 对一首或多首曲目求值 foobar2000 的 Title Formatting 表达式，并通过 `getBuiltinFields()` 列出常用字段。

## eval(pattern, path?)

签名：`fb.titleformat.eval(pattern: string, path?: string): Promise<TitleformatEvalResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| pattern | string | 是 | Title Formatting 表达式，如 `%artist% - %title%` |
| path | string | 否 | 曲目路径；带 `\|subsong:N` 后缀时选子曲目。省略时对正在播放的曲目求值 |

对单首曲目求值单个表达式，返回 `{ path, pattern, result, infoAvailable }`，`result` 是格式化后的文本。不传 `path` 时对正在播放的曲目求值，见下文「对正在播放的曲目求值」。表达式无效时以 `INVALID_PARAMS` 失败，路径打不开时以 `INVALID_PATH` 失败。

```javascript
const r = await fb.titleformat.eval('%artist% - %title%', 'E:\\Music\\song.flac');
if (r.success === false) throw new Error(r.error);
console.log(r.result); // 'Artist - Title'
```

## evalBatch(pattern, paths)

签名：`fb.titleformat.evalBatch(pattern: string, paths: string[]): Promise<TitleformatEvalBatchResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| pattern | string | 是 | 对每个路径应用的 Title Formatting 表达式 |
| paths | string[] | 是 | 曲目路径；带 `\|subsong:N` 后缀时选子曲目 |

对多首曲目求值同一个表达式，表达式只编译一次。返回 `pattern`、计数 `total`、`successCount` 和 `errorCount`，以及 `results`：每个路径一行，顺序与请求一致，含 `path`、`success`，成功时另有 `result` 和 `infoAvailable`，失败时另有 `error`。打不开的路径那一行失败，不会让整次调用失败；表达式无效时整次调用以 `INVALID_PARAMS` 失败。

```javascript
const batch = await fb.titleformat.evalBatch('%artist% - %title%', [
	'E:\\Music\\one.flac',
	'E:\\Music\\two.flac',
]);
if (batch.success === false) throw new Error(batch.error);
for (const row of batch.results) {
	console.log(row.path, row.success ? row.result : row.error);
}
```

## evalFields(path, fields)

签名：`fb.titleformat.evalFields(path: string, fields: TitleformatFieldMap): Promise<TitleformatEvalFieldsResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| path | string | 是 | 曲目路径；带 `\|subsong:N` 后缀时选子曲目 |
| fields | Record<string, string> | 是 | 输出键到 Title Formatting 表达式的映射 |

对单首曲目一次求值多个具名表达式。`fields` 的每个键都作为响应的顶层字段返回，值是格式化后的文本，与 `path`、`infoAvailable` 并列；没有嵌套的 `values` 对象。哪些键名不要用，见下文「这个标志覆盖不到什么」。路径打不开时以 `INVALID_PATH` 失败。

```javascript
const tags = await fb.titleformat.evalFields('E:\\Music\\song.flac', {
	artist: '%artist%',
	year: '$year(%date%)',
});
if (tags.success === false) throw new Error(tags.error);
console.log(tags.artist, tags.year);
```

## evalFieldsBatch(paths, fields)

签名：`fb.titleformat.evalFieldsBatch(paths: string[], fields: TitleformatFieldMap): Promise<TitleformatEvalFieldsBatchResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| paths | string[] | 是 | 要求值的曲目路径 |
| fields | Record<string, string> | 是 | 输出键到 Title Formatting 表达式的映射 |

返回汇总计数和每个路径一行结果。宿主把各表达式合并后只编译一次，再应用到整批路径。每行含 `path`、`success`；成功时另有 `fields` 每个键对应的字段和 `infoAvailable`，路径打不开时另有 `error`。合并后的表达式编译失败时，整次调用以 `INVALID_PARAMS` 失败。

```javascript
const result = await fb.titleformat.evalFieldsBatch(
	['E:\\Music\\one.flac', 'E:\\Music\\two.flac'],
	{ artist: '%artist%', title: '%title%' }
);
```

## getBuiltinFields()

签名：`fb.titleformat.getBuiltinFields(): Promise<TitleformatGetBuiltinFieldsResponse>`

返回 `fields`：易读名称到 Title Formatting 表达式的映射，如 `year` → `$year(%date%)`、`displayTitle` → `$if(%title%,%title%,%filename%)`。这是一份固定的常用字段表，不是 foobar2000 认识的全部字段。播放次数、评分和播放日期这几项需要 foo_playcount 组件。

```javascript
const builtin = await fb.titleformat.getBuiltinFields();
if (builtin.success === false) throw new Error(builtin.error);
const year = await fb.titleformat.eval(builtin.fields.year, 'E:\\Music\\song.flac');
```

## 对正在播放的曲目求值

`eval()` 不给路径（或给空串）时，经 foobar2000 的播放格式化器对正在播放的曲目求值。`%playback_time%`、`%isplaying%`、网络流的动态标题等字段也会填上，按路径求值时它们是空的。此时应答里的 `path` 是正在播放曲目的路径；没有曲目在播放时返回 `success: false` 与 `code: 'NO_ACTIVE_ITEM'`。另外三个方法始终要传路径。

```javascript
const now = await fb.titleformat.eval('%artist% - %title% [%playback_time%]');
if (now.success) {
	console.log(now.result);
}
```

## Info 可用性信号

每次成功求值都会返回 `infoAvailable`。foobar2000 按 metadb 缓存格式化曲目，而它没载入过的本地文件（不在任何播放列表或媒体库里）在缓存里只有占位信息，所以宿主会先从磁盘读这类文件。读盘在 foobar2000 主线程上同步进行，每个没载入的文件读一次，一大批这样的文件会让界面停顿到读完为止。

| 取值 | 含义 |
| --- | --- |
| `true` | 宿主拿到了该曲目的信息（来自 foobar2000 缓存或读自文件），标签类取值可信。 |
| `false` | 宿主拿不到该曲目的信息：远程路径且没有缓存，或文件读不了。渲染时用的是占位 `file_info`，因此 `%bitrate%`、`%codec%` 等标签类取值不可信。 |
| 缺省 | 该字段未被写入。具体情形逐方法不同 —— 见下方「什么时候没有这个字段」。 |

```javascript
const one = await fb.titleformat.eval('%bitrate%', 'E:\\Music\\one.flac');
if (one.success === false) throw new Error(one.error);
if (one.infoAvailable === false) {
	// 文件读不了，或是没有缓存的远程路径 —— 此时应显示「未知」而不是空值
}

const many = await fb.titleformat.evalFieldsBatch(
	['E:\\Music\\one.flac', 'E:\\Music\\two.flac'],
	{ bitrate: '%bitrate%', rating: '%rating%' }
);
if (many.success === false) throw new Error(many.error);
for (const row of many.results) {
	console.log(row.path, row.bitrate, row.infoAvailable);
}
```

### 什么时候没有这个字段

两个单曲方法与两个批量方法的行为不同，不要假设「缺字段」在各处含义一致。

- `eval()` 与 `evalFields()`：所有失败信封（`success: false`）都不带该字段。`evalFields()` **另外**在 `success: true` 的信封里也可能不带 —— 当 `fields` 里没有任何字符串类型的模式、或合并脚本编译失败时，因为根本没有求值发生。
- `evalBatch()` 与 `evalFieldsBatch()`：只有 `success: false` 的条目不带，成功条目必然带。这两个方法**绝不会**返回「成功但没有该字段」的条目 —— 模式编译失败时整个调用以顶层 `success: false` 失败、连 `results` 都没有。`evalFieldsBatch()` 还多一种情形：`fields` 里没有字符串类型的模式时返回 `results: []`，根本没有条目可以携带该字段。（`evalBatch()` 收的是单个 `pattern`、没有 `fields` 参数，所以第二种情形对它不成立。）

### 这个标志覆盖不到什么

`evalFields()` 与 `evalFieldsBatch()` 会把所有请求的模式合并成一个脚本、一次求值，所以一个布尔值覆盖整条记录，无法区分「`%bitrate%` 不可信」和「`%rating%` 可信」。

foo_playcount 虚拟字段 —— `%rating%`、`%play_count%`、`%added%`、`%first_played%`、`%last_played%` —— 由 display-field provider 解析，不来自曲目自身标签，因此该标志为 `false` 时它们依然有效。`infoAvailable: false` 只能读作「标签类字段不可信」，绝不能读作「整条记录都是错的」。

`fields` 的键不要取名 `success`、`path` 或 `infoAvailable`：响应里的同名字段优先，这样的键会被丢弃。取名 `error` 或 `code` 的键会保留，所以判断是否失败要看 `success`，不要看 `error`。

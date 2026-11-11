# fb.lyrics 歌词

`fb.lyrics` 提供曲目歌词的检索、存在性检测与持久化存储操作。当 `get()` 省略 `path` 参数时，宿主自动获取当前活动播放曲目。

## exists(path)

签名：`fb.lyrics.exists(path: string, options?: Omit<LyricsExistsParams, 'path'>): Promise<LyricsExistsResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 目标音频文件路径，支持添加 `\|subsong:N` 后缀指定子曲目索引 |
| `options.filename` | `string` | 否 | 同级外挂歌词文件的精确文件名（包含扩展名）；空字符串或省略时按预设规则自动检索 |

检测并返回曲目已存在的已知歌词元数据标签及同级外挂歌词文件（包含内容为空的歌词）。外挂文件候选匹配逻辑与文件系统访问策略与 `get()` 完全一致；若显式指定文件名，仅约束外挂文件的候选查找范围，内嵌标签检测依然正常执行。

返回 `{ exists, sources }`；`sources` 数组列出全部已命中的来源标识，例如 `embedded` 或 `file:song.lrc`。

```javascript
const res = await fb.lyrics.exists('E:\\Music\\song.flac');
if (res.success === false) throw new Error(res.error);
const { exists } = res;
```

## get(path?, options?)

签名：`fb.lyrics.get(path?: string, options?: Omit<LyricsGetParams, 'path'>): Promise<LyricsGetResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 否 | 目标曲目路径；省略时默认定位当前播放曲目。此时若没有正在播放的曲目，调用以 `NO_ACTIVE_ITEM` 错误拒绝 |
| `options.source` | `string` | 否 | 歌词来源过滤，可选值为 `'embedded'`、`'file'` 或 `'any'`；缺省为 `'any'`（优先检索内嵌标签，未命中时回退检索外挂文件） |
| `options.type` | `string` | 否 | 歌词类型过滤，可选值为 `'synced'`（时间轴同步）、`'unsynced'`（纯文本）或 `'any'`；缺省为 `'any'` |
| `options.format` | `string` | 否 | 外挂文件格式过滤，可选值为 `'lrc'`、`'txt'` 或 `'any'`；缺省为 `'any'`（优先匹配 `.lrc`，未命中时尝试 `.txt`）。此项仅对外挂文件检索生效 |
| `options.filename` | `string` | 否 | 音频同级外挂文件的精确文件名；传入非空字符串时覆盖自动候选生成规则并忽略 `format` 设置，`source` 与 `type` 过滤规则依然生效 |

返回 `LyricsGetResponse` 结构。调用端须先校验 `success`，再根据 `available` 字段判断歌词可用性；成功命中时包含 `source`、`sourcePath`、`tagName`、`lyrics` 以及 `synced` 字段。自动检索规则：在确认单曲文件未分轨后优先尝试与音频同名的外挂文件，未命中时尝试 `<第一艺术家> - <标题>` 命名组合；对于子曲目（subsong）仅匹配艺术家与标题组合，详见[外挂文件查找与命名规则](/zh/api/lyrics#sidecar-lookup)。

```javascript
const path = 'E:\\Music\\song.flac';
const current = await fb.lyrics.get();                              // 当前曲目
const embedded = await fb.lyrics.get(path, { source: 'embedded' }); // 仅内嵌歌词
const result = await fb.lyrics.get(undefined, { type: 'synced' });  // 仅同步歌词
```

## save(path, lyricsText, options?) {#保存歌词}

签名：`fb.lyrics.save(path: string, lyricsText: string, options?: LyricsSaveOptions): Promise<LyricsSaveResponse>`

调用底层的 `lyrics.save` 端点保存歌词文本。`options` 参数支持指定 `target`、`filename`、`tagName` 与 `format`；若 `target` 传入单个字符串，SDK 内部自动封装为单元素数组提交给宿主处理。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 歌词所属的曲目路径 |
| `lyricsText` | `string` | 是 | 要保存的歌词文本 |
| `options.target` | `LyricsSaveTarget \| LyricsSaveTarget[]` | 否 | 保存目标位置，可选值为 `'file'`（音频同级外挂文件，缺省值）、`'embedded'`（音频文件内嵌元数据标签）或 `'all'`（两处同时保存） |
| `options.filename` | `string` | 否 | `file` 目标外挂文件的精确文件名（包含扩展名）。指定非空值时覆盖默认生成规则并忽略 `format` 设置 |
| `options.tagName` | `string` | 否 | `embedded` 目标写入的元数据标签字段名，缺省为 `'LYRICS'` |
| `options.format` | `string` | 否 | 外挂文件扩展名，可选值为 `'lrc'` 或 `'txt'`；缺省为 `'lrc'`。显式指定非空 `filename` 时此项被忽略 |

响应结构为 `LyricsSaveResponse`。当指定单一保存目标时，响应直接描述该次写入结果（`file` 目标包含 `savedTo` 绝对路径，`embedded` 目标返回异步标签写入回执）；当指定多个保存目标时，各目标的执行结果按 key 汇聚于 `results` 映射中，仅当全部目标均写入失败时，整个调用才以 `OPERATION_FAILED` 错误拒绝。注意：`embedded` 目标仅执行异步标签写入排队，回执中的 `dispatched: true` 表示任务已成功提交至宿主写入队列，最终写入结果由 `metadata:writeComplete` 事件异步通告。

```javascript
const result = await fb.lyrics.save(
	'E:\\Music\\song.flac',
	'[00:00.00]歌词……',
	{
		target: ['file', 'embedded'],
		format: 'lrc',
		tagName: 'SYNCEDLYRICS',
	},
);
```

```javascript
const path = 'E:\\Music\\song.flac';
const text = '[00:00.00]歌词……';
await fb.lyrics.save(path, text); // 仅写入外挂文件
await fb.lyrics.save(path, text, { filename: 'chosen.lrc' });
const chosen = await fb.lyrics.get(path, { source: 'file', filename: 'chosen.lrc' });
const found = await fb.lyrics.exists(path, { filename: 'chosen.lrc' });
const all = await fb.lyrics.save(path, text, { target: 'all' }); // 同时写入外挂文件与内嵌元数据
if (all.success === false) throw new Error(all.error);
```

上述三个方法采用统一的 Windows 文件系统命名合法性校验规则。若曲目缺少必要的元数据标签，宿主无法推导子曲目的默认外挂文件名，此时必须显式传入 `filename`。外挂文件保存操作在成功返回前主动使读取缓存失效；内嵌标签写入操作则在宿主元数据变更落地后使缓存失效。关于已废弃的 `config` 目标与旧版子曲目命名兼容细节，请参阅[迁移指南](/zh/api/lyrics#从旧版本迁移)。

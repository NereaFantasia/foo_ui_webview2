# fb.lyrics 歌词

`fb.lyrics` 查找、检查和保存曲目的歌词。`get()` 不传路径时，宿主取正在播放的曲目。

## exists(path)

签名：`fb.lyrics.exists(path: string, options?: Omit<LyricsExistsParams, 'path'>): Promise<LyricsExistsResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 要检查的曲目路径，带 `\|subsong:N` 时选子曲目。 |
| `options.filename` | `string` | 否 | 音频旁文件的精确名称，含扩展名；空字符串或省略表示自动查找。 |

列出存在的已知歌词标签与外挂文件，空内容也算存在。文件候选与访问检查和 `get()` 一致；显式文件名只限定文件候选，仍检查内嵌标签。

返回 `{ exists, sources }`；`sources` 列出找到的全部来源，例如 `embedded` 或 `file:song.lrc`。

```javascript
const res = await fb.lyrics.exists('E:\\Music\\song.flac');
if (res.success === false) throw new Error(res.error);
const { exists } = res;
```

## get(path?, options?)

签名：`fb.lyrics.get(path?: string, options?: Omit<LyricsGetParams, 'path'>): Promise<LyricsGetResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 否 | 曲目路径；省略则取当前曲目，此时没在播放会以 `NO_ACTIVE_ITEM` 失败。 |
| `options.source` | `string` | 否 | `'embedded'`、`'file'` 或 `'any'`；默认 `'any'`，先查内嵌标签，再查外挂文件。 |
| `options.type` | `string` | 否 | `'synced'`、`'unsynced'` 或 `'any'`；默认 `'any'`。 |
| `options.format` | `string` | 否 | `'lrc'`、`'txt'` 或 `'any'`；默认 `'any'`，先试 `.lrc` 再试 `.txt`。只影响外挂文件查找。 |
| `options.filename` | `string` | 否 | 音频旁文件的精确名称；非空时替代自动候选并忽略 `format`，`source` 与 `type` 仍生效。 |

返回 `LyricsGetResponse`。先判 `success` 再看 `available`；找到时可能带 `source`、`sourcePath`、`tagName`、`lyrics` 与 `synced`。自动查找仅在确认整文件单曲后使用音频同名文件，再查首个艺人值与曲名的组合；子曲目只使用后者，见[文件查找与命名](/zh/api/lyrics#sidecar-lookup)。

```javascript
const path = 'E:\\Music\\song.flac';
const current = await fb.lyrics.get();                              // 当前曲目
const embedded = await fb.lyrics.get(path, { source: 'embedded' }); // 仅内嵌歌词
const result = await fb.lyrics.get(undefined, { type: 'synced' });  // 仅同步歌词
```

## 保存歌词

`fb.lyrics.save(path, lyricsText, options?)` 调用 `lyrics.save`。`options` 的类型为 `LyricsSaveOptions`，可包含 `filename`、`tagName`、`format` 与 `target`；`target` 传单个字符串时，SDK 以单元素数组发给宿主。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 歌词所属的曲目路径。 |
| `lyricsText` | `string` | 是 | 要保存的歌词文本。 |
| `options.target` | `LyricsSaveTarget \| LyricsSaveTarget[]` | 否 | `'file'`（音频旁的外挂文件，默认）、`'embedded'`（文件里的标签），或 `'all'` 表示两处。 |
| `options.filename` | `string` | 否 | `file` 目标的精确文件名，含扩展名。非空时替代默认名称并忽略 `format`。 |
| `options.tagName` | `string` | 否 | `embedded` 目标写的标签，默认 `'LYRICS'`。 |
| `options.format` | `string` | 否 | `'lrc'` 或 `'txt'`，默认外挂文件名的扩展名；默认 `'lrc'`，非空 `filename` 时忽略。 |

SDK 返回类型为 `LyricsSaveResponse`。单个目标时描述这次写入（文件目标有 `savedTo`，`embedded` 目标是标签写入回执）；多个目标时各目标的结果在 `results` 下，只有全部失败才以 `OPERATION_FAILED` 失败。`embedded` 目标只把标签写入排入队列：回执里的 `dispatched: true` 表示已排入，最终结果由 `metadata:writeComplete` 事件报告。

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
await fb.lyrics.save(path, text); // 只写外挂文件
await fb.lyrics.save(path, text, { filename: 'chosen.lrc' });
const chosen = await fb.lyrics.get(path, { source: 'file', filename: 'chosen.lrc' });
const found = await fb.lyrics.exists(path, { filename: 'chosen.lrc' });
const all = await fb.lyrics.save(path, text, { target: 'all' }); // 文件与标签
if (all.success === false) throw new Error(all.error);
```

三个方法使用相同的 Windows 文件名校验。标签缺失时不能推导子曲目的默认文件名，需要指定 `filename`。文件保存返回前使读取缓存失效，内嵌写入则在元数据实际变更后使缓存失效。被移除的 `config` 目标和旧子曲目命名见[迁移说明](/zh/api/lyrics#从旧版本迁移)。

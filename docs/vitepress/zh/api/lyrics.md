# Lyrics 歌词 API

歌词 API 提供歌词读取、存在性检查和保存功能。支持内嵌标签（LYRICS/UNSYNCED LYRICS/SYNCEDLYRICS 等）和外部 `.lrc`/`.txt` 文件两种来源。

::: tip SDK 封装
推荐使用 `fb.lyrics.*` SDK 封装，无需手动构造 JSON 参数。
:::

## lyrics.get

获取歌词文本。支持内嵌标签和外部 `.lrc`/`.txt` 文件，带 1200ms TTL 内存缓存。支持按歌词类型（同步/非同步）和文件格式过滤。

<!-- api-schema:begin lyrics.get -->
先从五种已知歌词标签读取非空歌词，再查音频旁的文件。自动查找仅在确认整文件单曲时尝试音频同名文件，然后尝试 `<首个艺人值> - <曲名>`；子曲目只尝试后者。无法确认输入结构时跳过音频同名候选，不查编号文件或整张镜像共享文件。按曲目、文件名与过滤条件缓存 1200 ms，文件保存成功和元数据变更会使缓存失效。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 否 | 曲目路径，带 `\|subsong:N` 时选子曲目。省略则取正在播放的曲目并保留其子曲目标识；没在播放时以 `NO_ACTIVE_ITEM` 失败。不能为空。 |
| `filename` | `string` | 否 | 音频旁文件的精确名称，含扩展名。非空时替代全部自动文件候选，并忽略 `format`；`source` 与 `type` 仍生效。空字符串表示自动查找。必须是合法的单个 Windows 文件名，不含路径语法、设备保留名或尾随句点与空格；非法名称以 `INVALID_PARAMS` 失败。 |
| `source` | `"embedded" \| "file" \| "any"` | 否 | 查找范围：内嵌标签、外挂文件，或按此顺序两者都查。默认 `"any"`。 |
| `type` | `"synced" \| "unsynced" \| "any"` | 否 | 只要同步歌词（带 LRC 时间戳）或只要非同步歌词。默认 `"any"`。 |
| `format` | `"lrc" \| "txt" \| "any"` | 否 | 自动查找的外挂文件扩展名；`any` 时先试遍 `.lrc` 再试 `.txt`。只影响自动查找，非空 `filename` 优先。默认 `"any"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `available` | `boolean` | 是否找到歌词。 |
| `path` | `string` | 查的路径：原样给出的，或正在播放曲目的路径（保留子曲目后缀）。 |
| `source` | `"embedded" \| "file"` | 歌词的来源；`available` 为 `true` 时出现。 |
| `sourcePath` | `string` | 外挂文件的完整路径；`source` 为 `file` 时出现。 |
| `tagName` | `string` | 命中 `type` 过滤的标签，`LYRICS`、`UNSYNCED LYRICS`、`UNSYNCEDLYRICS`、`SYNCEDLYRICS`、`SYNCED LYRICS` 之一；`source` 为 `embedded`、`type` 不是 `any` 且命中已知标签时出现。 |
| `lyrics` | `string` | 歌词文本；`available` 为 `true` 时出现。 |
| `synced` | `boolean` | 文本是否带 LRC 时间戳；`available` 为 `true` 时出现。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### 示例

```javascript
// 获取当前播放曲目的歌词
const result = await fb2k.invoke('lyrics.get');
if (result.success === false) throw new Error(result.error);
if (result.available) {
    console.log(`来源: ${result.source}, 同步: ${result.synced}`);
    console.log(result.lyrics);
}

// 仅查外部文件
const ext = await fb2k.invoke('lyrics.get', { source: 'file' });

// 仅获取同步歌词
const synced = await fb2k.invoke('lyrics.get', { type: 'synced' });

// 读取 .txt 格式歌词
const txt = await fb2k.invoke('lyrics.get', { format: 'txt' });

// SDK 封装
const lyrics = await fb.lyrics.get();
const syncedOnly = await fb.lyrics.get(undefined, { type: 'synced' });
```

## lyrics.exists

检查指定曲目的歌词标签与外挂文件是否存在，空内容也算存在。

<!-- api-schema:begin lyrics.exists -->
列出曲目存在的已知歌词标签与外挂文件。文件候选和访问检查与 `lyrics.get` 一致，但不读取歌词内容：空标签与空文件仍算存在。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 曲目路径，带 `\|subsong:N` 时选子曲目。不能为空。 |
| `filename` | `string` | 否 | 音频旁文件的精确名称，含扩展名，替代自动文件候选；仍检查内嵌标签。空字符串表示自动查找。文件名校验与 `lyrics.get` 相同，非法名称以 `INVALID_PARAMS` 失败。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `exists` | `boolean` | 是否有任何来源带歌词。 |
| `sources` | `string[]` | 找到的全部来源：有已知歌词标签为 `embedded`，每个外挂文件为 `file:<文件名>`；候选顺序与 `lyrics.get` 的 `format=any` 一致，指定文件名时只检查该文件。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### 示例

```javascript
const result = await fb2k.invoke('lyrics.exists', { path: 'C:\\Music\\song.flac' });
if (result.success === false) throw new Error(result.error);
if (result.exists) {
    console.log('歌词来源:', result.sources);
}

// SDK 封装
const check = await fb.lyrics.exists('C:\\Music\\song.flac');
```

## lyrics.save

保存歌词到音频旁的文件、内嵌标签或两处。

<!-- api-schema:begin lyrics.save -->
把歌词保存到音频旁的外挂文件、内嵌标签，或两处。文件保存使读取缓存失效；内嵌保存异步派发写入，元数据变更后使缓存失效。不指定 `filename` 时，确认整文件单曲后使用音频同名文件，其余使用 `<首个艺人值> - <曲名>`；后者缺少标签时，该目标以 `INVALID_PARAMS` 失败。已有文件会被覆盖，艺人与曲名相同的不同曲目应指定不同文件名。单个目标时结果描述这次写入；多个目标时各结果在 `results` 下，全部失败才以 `OPERATION_FAILED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 歌词所属的曲目路径；带 `\|subsong:N` 时选子曲目。不能为空。 |
| `lyrics` | `string` | 是 | 要保存的歌词文本。不能为空。 |
| `target` | `string[]` | 否 | 写到哪里：`file`（音频旁的外挂文件）、`embedded`（文件里的标签），或 `all` 表示两处；给多项就各写一处。其他值以 `INVALID_PARAMS` 失败，省略表示 `file`。不能为空。 |
| `filename` | `string` | 否 | `file` 目标的精确文件名，含扩展名。非空时替代推导名称并忽略 `format`，空字符串表示默认名称。文件名校验与 `lyrics.get` 相同，非法名称以 `INVALID_PARAMS` 失败。 |
| `tagName` | `string` | 否 | `embedded` 目标写的标签。默认 `"LYRICS"`。 |
| `format` | `"lrc" \| "txt"` | 否 | 默认外挂文件名的扩展名；非空 `filename` 时忽略。默认 `"lrc"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `savedTo` | `string` | 写出的文件的完整路径；单个 `file` 目标时出现。 |
| `dispatched` | `boolean` | 单个 `embedded` 目标已派发标签写入时为 `true`；最终结果由 `metadata:writeComplete` 事件报告。 |
| `path` | `string` | 原样的路径；单个 `embedded` 目标时出现。 |
| `handlePath` | `string` | 标签写入所针对句柄的路径；单个 `embedded` 目标时出现。 |
| `subsong` | `integer` | 标签写入所针对的子曲目；单个 `embedded` 目标时出现。 |
| `tagsApplied` | `Record<string, string>` | 写入的标签，键是大写标签名；单个 `embedded` 目标时出现。 |
| `tagsSet` | `integer` | 设置的标签数；单个 `embedded` 目标时出现。 |
| `tagsRemoved` | `integer` | 删除的标签数；单个 `embedded` 目标时出现。 |
| `note` | `string` | 关于派发的说明；单个 `embedded` 目标时出现。 |
| `results` | `Record<string, any>` | 多个目标时：每个目标自己的信封（`{ success, savedTo }`、标签写入回执，或 `{ success: false, error, code }`），键为 `file` 或 `embedded`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### Target 说明

| Target | 行为 |
| --- | --- |
| `file` | 写入音频所在目录，默认命名见下节 |
| `embedded` | 异步写入指定曲目的标签；通过 `metadata:writeComplete` 获取最终结果 |
| `all` | 同时尝试文件与内嵌标签，分别报告结果 |

Bridge 的 `target` 是数组，默认 `['file']`；SDK 也接受单个字符串。标签写入是否支持该容器及曲目，由 foobar2000 的输入插件决定。派发成功不表示已经写盘。

### 示例

```javascript
const path = 'C:\\Music\\song.flac';
const lyricsText = '[00:01.00]First line\n[00:05.00]Second line';

const saved = await fb.lyrics.save(path, lyricsText, { target: 'all' });
if (saved.success === false) throw new Error(saved.error);
```

## 外部歌词文件的查找 {#sidecar-lookup}

只查音频所在目录，不搜索配置目录或子目录，也不按曲名相似度猜配。

| 顺序 | 文件命名 | 适用范围 |
| --- | --- | --- |
| 1 | 音频同名文件，如 `song.wav` 对应 `song.lrc` | 读取器确认整文件只有 subsong 0 这一首曲目 |
| 2 | `<首个艺人值> - <曲名>` | 两个标签都存在，文件名非法字符替换为 `_` |

默认先尝试全部 `.lrc` 候选，再尝试全部 `.txt` 候选。无法确认整文件单曲时跳过第 1 项。缺少艺人或曲名时跳过第 2 项；合作艺人名单不会另行拼接，也不会把混音、现场版等名称视为同一首。

`lyrics.get` 返回首个可读取、非空且符合 `type` 的文件。`lyrics.exists` 使用相同候选与权限检查，返回全部存在的来源，例如 `file:song.lrc`；空文件仍可能存在，因此 `exists: true` 不保证 `get.available: true`。空歌词标签也保留这个区别。

便携安装的 `file-relative://` 路径同样支持。压缩包条目（`archive://`、`unpack://`）不作为音频旁文件的定位依据。

## 容器格式（CUE / ISO / 多子轨文件）

subsong 是输入插件给出的曲目标识，不是可以统一加一的曲目序号。多子曲目容器中的 subsong 0 也不会使用镜像同名歌词，避免一首歌词套到整张专辑。

子曲目默认按艺人与曲名查找和保存；标签缺失时，文件保存以 `INVALID_PARAMS` 失败，调用方可以指定 `filename`。相同艺人和曲名可能对应同一个文件，保存会覆盖已有内容；需要区分时给不同文件名。

### 显式文件名

三个端点均可指定带扩展名的 `filename`。非空值将文件范围限定为该文件，缺失时不再回退；允许其他扩展名，此时忽略 `format`，但仍应用 `source` 与 `type`。空字符串等同省略。

`source: 'any'` 仍先读标签；要确保读取指定文件，请使用 `source: 'file'`。`exists` 仍同时检查内嵌标签。文件名必须是合法的单个 Windows 文件名，不含路径分隔符、设备保留名、非法字符或尾随句点与空格。

```javascript
const path = 'D:\\album.flac|subsong:2';
const filename = 'chosen-track.lrc';

const saved = await fb.lyrics.save(path, '[00:01.00]Track lyrics', {
    target: 'file',
    filename,
});
if (saved.success === false) throw new Error(saved.error);

const found = await fb.lyrics.exists(path, { filename });
if (found.success === false) throw new Error(found.error);

const result = await fb.lyrics.get(path, { source: 'file', filename });
if (result.success === false) throw new Error(result.error);
if (result.available) console.log(result.lyrics);
```

### 从旧版本迁移

`config` 保存目标已移除，`all` 只表示 `file` 与 `embedded`。已有配置目录与歌词不会被删除，也不会自动读取。

旧 `.NN.lrc` 编号文件及镜像同名文件不再自动作为子曲目歌词。知道文件对应哪首曲目时，用 `filename` 精确指定即可，不必改名。

## 相关

- [`artwork.getLyrics`](/zh/api/artwork#artwork-getlyrics) — 只从五种已知标签读取歌词
- [`<fb-lyrics-panel>`](/zh/components/media) — 歌词面板 Web Component

## 使用说明

文件保存成功及元数据变更会使 1200 ms 读取缓存失效。内嵌保存是异步操作，先订阅 `metadata:writeComplete` 再保存，等待完成后读取；事件订阅方式见 [Metadata API](/zh/api/metadata)。

SDK 的 `fb.lyrics.*` 对应公开的三个 `lyrics.*` 方法，`<fb-lyrics-panel>` 使用这些方法读取歌词。

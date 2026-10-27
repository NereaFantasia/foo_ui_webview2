# Lyrics 歌词 API

歌词 API 提供歌词读取、存在性检查与本地持久化保存能力；支持音频内嵌标签（`LYRICS`、`UNSYNCED LYRICS`、`SYNCEDLYRICS` 等）与同目录外挂 `.lrc` / `.txt` 文本文件。

::: tip SDK 封装
推荐使用 `fb.lyrics.*` SDK 封装，无需手动构造 JSON 参数。
:::

## lyrics.get

获取曲目歌词文本；支持内嵌标签与同目录外挂文件，带 1200 毫秒 TTL 内存缓存；支持按同步状态（带 LRC 时间戳）与文件格式过滤。

<!-- api-schema:begin lyrics.get -->
读取曲目的非空歌词文本（依次检索五种已知内嵌标签及音频同目录下的外挂歌词文件）；自动查找时，整文件单曲优先匹配音频同名文件并回退至 `<首个艺人值> - <曲名>`（子曲目仅匹配后者，未知容器结构自动跳过同名候选且不匹配序号分轨或镜像共享文件）；基于曲目路径、文件名与过滤参数缓存 1200 毫秒（文件写入成功或元数据变更自动失效缓存）；返回歌词中的非法 UTF-8 替换为 U+FFFD（不猜测字符编码且不改写源文件）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 否 | 目标曲目路径（支持使用 `\|subsong:N` 后缀指定子曲目）；省略时默认查询当前正在播放的曲目（保留其子曲目标识），未处于播放状态时以 `NO_ACTIVE_ITEM` 失败不能为空。 |
| `filename` | `string` | 否 | 音频同目录下的精确外挂文件名（包含扩展名）；非空时覆盖全部自动候选并忽略 `format`（`source` 与 `type` 过滤规则仍生效），空字符串代表启用自动查找；必须为合法的单个 Windows 文件名（禁止包含路径分隔符、设备保留名或尾随句点空格），非法格式以 `INVALID_PARAMS` 失败 |
| `source` | `"embedded" \| "file" \| "any"` | 否 | 歌词检索来源范围：`embedded`（仅内嵌标签）、`file`（仅外挂文件）或 `any`（按先内嵌后外挂顺序均检索）默认 `"any"`。 |
| `type` | `"synced" \| "unsynced" \| "any"` | 否 | 歌词时间戳类型过滤：`synced`（仅带 LRC 时间戳的同步歌词）、`unsynced`（仅无时间戳歌词）或 `any`（不作过滤）默认 `"any"`。 |
| `format` | `"lrc" \| "txt" \| "any"` | 否 | 自动查找外挂文件时匹配的扩展名（`any` 时优先遍历 `.lrc` 候选再匹配 `.txt` 候选；仅在自动查找生效，显式指定非空 `filename` 时忽略本参数）默认 `"any"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `available` | `boolean` | 是否成功查得有效歌词 |
| `path` | `string` | 实际查询的曲目路径（传入的原样路径或当前播放曲目的路径，包含子曲目后缀） |
| `source` | `"embedded" \| "file"` | 胜出的歌词来源类型（`embedded` 或 `file`；仅在 `available` 为 `true` 时返回） |
| `sourcePath` | `string` | 匹配的外挂歌词文件绝对路径（仅在 `source` 为 `file` 时返回） |
| `tagName` | `string` | 命中过滤条件的具体内嵌标签名称（`LYRICS`、`UNSYNCED LYRICS`、`UNSYNCEDLYRICS`、`SYNCEDLYRICS` 或 `SYNCED LYRICS`；仅在 `source` 为 `embedded`、指定了 `type` 过滤且命中已知标签时返回） |
| `lyrics` | `string` | 读取到的完整歌词文本（非法 UTF-8 字符已替换为 U+FFFD；仅在 `available` 为 `true` 时返回） |
| `synced` | `boolean` | 歌词文本是否包含标准 LRC 时间戳（仅在 `available` 为 `true` 时返回） |

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

检查指定曲目的内嵌歌词标签与同目录外挂文件是否存在（包含空标签与空文件）。

<!-- api-schema:begin lyrics.exists -->
检测曲目关联的已知内嵌歌词标签与同目录下外挂歌词文件是否存在；文件探测规则与路径安全检查同 `lyrics.get`（不读取歌词实际文本内容，空标签或空文件仍计入存在）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 目标曲目路径（支持使用 `\|subsong:N` 后缀指定子曲目）不能为空。 |
| `filename` | `string` | 否 | 音频同目录下的精确外挂文件名（包含扩展名，非空时覆盖自动文件候选，仍检查内嵌标签，空字符串代表自动查找；文件名校验规则同 `lyrics.get`，非法格式以 `INVALID_PARAMS` 失败） |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `exists` | `boolean` | 是否在任一检查来源中检测到歌词存在（含空标签与空文件） |
| `sources` | `string[]` | 检测到存在歌词的所有来源标识列表（内嵌标签标记为 `embedded`，各外挂文件标记为 `file:<文件名>`；候选遍历顺序与 `lyrics.get` 的 `format="any"` 一致，显式指定文件名时仅检测该文件） |

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

将歌词文本保存至音频同目录下的外挂文件、音频内嵌元数据标签或同时写入两处。

<!-- api-schema:begin lyrics.save -->
将歌词保存至音频同目录下的外挂文件、音频内嵌元数据标签或同时写入两处；文件写入成功使读取缓存失效，内嵌标签写入采用异步派发并在元数据落盘后使缓存失效；省略 `filename` 时，整文件单曲使用音频同名文件，其余曲目使用 `<首个艺人值> - <曲名>`（缺少对应标签时该目标以 `INVALID_PARAMS` 失败）；目标文件存在时直接覆盖（同艺人同曲名的不同曲目应显式指定文件名以防覆盖冲突）；指定单个目标时直接返回该目标的写入状态，指定多个目标时在 `results` 下分别记录各自结果（仅当所有目标均失败时整次调用以 `OPERATION_FAILED` 失败）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 歌词所归属的目标曲目路径（支持使用 `\|subsong:N` 后缀指定子曲目）不能为空。 |
| `lyrics` | `string` | 是 | 待保存的完整歌词文本内容不能为空。 |
| `target` | `string[]` | 否 | 保存目标位置数组：`file`（音频同目录下的外挂文件）、`embedded`（音频文件内嵌标签）或 `all`（两处均写）；指定多项时分别写入各处（省略时默认 `["file"]`，传入非法值以 `INVALID_PARAMS` 失败）不能为空。 |
| `filename` | `string` | 否 | `file` 目标写入的精确文件名（包含扩展名；非空时覆盖自动推导名称并忽略 `format`，空字符串代表采用默认命名；文件名校验规则同 `lyrics.get`，非法格式以 `INVALID_PARAMS` 失败） |
| `tagName` | `string` | 否 | `embedded` 目标写入的元数据标签名称（写入时自动转为大写键名）默认 `"LYRICS"`。 |
| `format` | `"lrc" \| "txt"` | 否 | 自动推导外挂文件名时使用的文件扩展名（显式指定非空 `filename` 时忽略本参数）默认 `"lrc"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `savedTo` | `string` | 写入的外挂歌词文件绝对路径（仅在单个 `file` 目标成功写入时返回） |
| `dispatched` | `boolean` | 单个 `embedded` 目标是否成功派发后台标签写入（为 `true` 时最终写入结果通过 `metadata:writeComplete` 事件广播） |
| `path` | `string` | 请求传入的原样曲目路径（仅在单个 `embedded` 目标时返回） |
| `handlePath` | `string` | 标签写入目标曲目句柄规范化路径（仅在单个 `embedded` 目标时返回） |
| `subsong` | `integer` | 标签写入的目标子曲目序号（仅在单个 `embedded` 目标时返回） |
| `tagsApplied` | `Record<string, string>` | 实际写入的元数据标签键值映射（键名为大写标签名；仅在单个 `embedded` 目标时返回） |
| `tagsSet` | `integer` | 本次操作设置的标签总数（仅在单个 `embedded` 目标时返回） |
| `tagsRemoved` | `integer` | 本次操作清除的标签总数（仅在单个 `embedded` 目标时返回） |
| `note` | `string` | 异步派发状态提示说明（仅在单个 `embedded` 目标时返回） |
| `results` | `Record<string, any>` | 多目标写入时各目标的独立响应结果映射（以 `file` 或 `embedded` 为键名，对应包含各自的成功回执或错误信封） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### Target 说明

| Target | 行为 |
| --- | --- |
| `file` | 写入音频所在目录，默认命名见下节 |
| `embedded` | 异步写入指定曲目的标签；通过 `metadata:writeComplete` 获取最终结果 |
| `all` | 同时尝试文件与内嵌标签，分别报告结果 |

Bridge 接口的 `target` 为数组（默认 `["file"]`，SDK 同时支持传入单个字符串）；容器与曲目是否支持内嵌标签写入由 foobar2000 输入解码插件决定；派发成功仅表示任务已入队，不代表已完成落盘。

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
| 2 | `<首个艺人值> - <曲名>` | 两个标签均存在，文件名非法字符自动替换为 `_` |

自动查找时，优先遍历全部 `.lrc` 候选，再遍历全部 `.txt` 候选；无法确认整文件单曲时跳过第 1 项；缺少艺人或曲名标签时跳过第 2 项（合作艺人名单不另行拼接，混音版或现场版等名称差异不视为同一首）。

`lyrics.get` 返回首个可读取、非空且符合 `type` 过滤的文件；`lyrics.exists` 采用相同候选顺序与权限检查，返回所有已存在的来源标识（如 `file:song.lrc`）；由于空文件仍视作存在，因此 `exists: true` 不保证 `get.available: true`（空歌词标签同样遵循该判定规则）。

支持便携安装路径 `file-relative://`；压缩包内音频（`archive://`、`unpack://`）不支持定位同目录外挂文件。

## 容器格式（CUE / ISO / 多子轨文件）

subsong 为输入插件分配的曲目标识，不可直接按自增整数推算；多子曲目容器中的 subsong 0 亦不会匹配镜像同名歌词文件，避免单曲歌词误套用至整张专辑。

子曲目默认按艺人与曲名元数据查找和保存；当标签缺失时，外挂文件保存将以 `INVALID_PARAMS` 失败（调用方可显式传入 `filename` 避免该限制）；相同艺人与曲名的不同曲目会指向同名文件并覆盖已有内容，需要隔离时应指定不同文件名。

### 显式文件名

三个端点均支持传入包含扩展名的 `filename` 参数；非空值将外挂文件检索范围严格限定为该指定文件（不存在时不执行自动回退）；可指定自定义扩展名（此时忽略 `format`，但 `source` 与 `type` 过滤依然生效），传入空字符串等同省略。

当 `source: "any"` 时仍优先读取内嵌标签；若需强制读取指定外挂文件，请显式传入 `source: "file"`（`lyrics.exists` 仍会同时检查内嵌标签）；文件名必须为合法的单个 Windows 文件名（禁止包含路径分隔符、设备保留名、非法字符或尾随句点与空格）。

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

`config` 保存目标已废弃移除，`all` 仅代表 `file` 与 `embedded` 两个目标；历史配置目录中遗留的歌词文件不会被删除，亦不会被自动读取。

旧版 `.NN.lrc` 编号文件与镜像同名文件不再自动关联为子曲目歌词；若已知文件对应的曲目，直接通过 `filename` 精确指定即可，无需重命名物理文件。

## 相关

- [`artwork.getLyrics`](/zh/api/artwork#artwork-getlyrics) — 只从五种已知标签读取歌词
- [`<fb-lyrics-panel>`](/zh/components/media) — 歌词面板 Web Component

## 使用说明

外挂文件保存成功与元数据变更会自动使 1200 毫秒读取缓存失效；内嵌标签保存属于异步操作，建议先订阅 `metadata:writeComplete` 事件再发起保存，待接收到完成广播后再行读取（事件规范见 [Metadata API](/zh/api/metadata)）。

SDK 的 `fb.lyrics.*` 封装了底层三个 `lyrics.*` 端点，Web Component `<fb-lyrics-panel>` 内部亦使用该接口驱动歌词渲染。

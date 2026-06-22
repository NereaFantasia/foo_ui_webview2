# Lyrics 歌词 API 

歌词 API 提供歌词读取、存在性检查和保存功能。支持内嵌标签（LYRICS/UNSYNCED LYRICS/SYNCEDLYRICS 等）和外部 `.lrc`/`.txt` 文件两种来源。

::: tip SDK 封装
推荐使用 `fb.lyrics.*` SDK 封装，无需手动构造 JSON 参数。
:::

## lyrics.get 

获取歌词文本。支持内嵌标签和外部 `.lrc`/`.txt` 文件，带 1200ms TTL 内存缓存。支持按歌词类型（同步/非同步）和文件格式过滤。

### 参数 

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| path | string | 否 | "" | 音频文件路径，空则取当前播放曲目 |
| source | string | 否 | "any" | 歌词来源筛选："embedded" / "file" / "any" |
| type | string | 否 | "any" | 歌词类型筛选："synced" / "unsynced" / "any" |
| format | string | 否 | "any" | 文件格式筛选（仅 source=file 时生效）："lrc" / "txt" / "any" |

### 返回值 

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| success | boolean | 请求是否成功 |
| available | boolean | 是否找到歌词 |
| path | string | 解析后的音频文件路径 |
| source | `"embedded" \| "file"` | 实际来源 |
| sourcePath | string | 仅 source=file 时存在，外部歌词文件完整路径 |
| tagName | string | 仅 source=embedded 且使用 type 过滤时存在，匹配的标签名 |
| lyrics | string | 歌词文本 |
| synced | boolean | 是否为 LRC 同步歌词（通过时间戳格式检测） |

### 示例 

```javascript
// 获取当前播放曲目的歌词
const result = await fb2k.invoke('lyrics.get');
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

检查指定曲目是否有可用歌词（同时检查内嵌标签和外部 `.lrc` / `.txt` 文件）。

### 参数 

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| path | string | 是 | "" | 音频文件路径 |

### 返回值 

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| exists | boolean | 是否存在任何歌词来源 |
| sources | string[] | 所有可用来源，如 ["embedded", "file:song.lrc", "file:song.txt"] |

### 示例 

```javascript
const result = await fb2k.invoke('lyrics.exists', { path: 'C:\\\\Music\\\\song.flac' });
if (result.exists) {
    console.log('歌词来源:', result.sources);
}

// SDK 封装
const check = await fb.lyrics.exists('C:\\\\Music\\\\song.flac');
```

## lyrics.save 

保存歌词到外部文件（.lrc/.txt）、音频文件内嵌标签和/或配置文件夹。支持一次调用同时写入多个目标。

### 参数 

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| path | string | 是 | "" | 音频文件路径 |
| lyrics | string | 是 | "" | 歌词文本内容 |
| target | string \| string[] | 否 | "file" | 保存目标："file" / "embedded" / "config" / "all"，或数组 ["file","config"] |
| filename | string | 否 | "" | 自定义文件名（相对于输出目录），空则自动使用同名文件 |
| tagName | string | 否 | "LYRICS" | 嵌入标签名（仅 target 含 embedded 时生效） |
| format | string | 否 | "lrc" | 文件格式（仅 target 含 file/config 时生效）："lrc" / "txt" |

### Target 说明 

| Target | 行为 |
| --- | --- |
| "file" | 保存到音轨同目录（如 C:\\Music\\song.lrc） |
| "embedded" | 写入音频文件的嵌入标签（如 LYRICS） |
| "config" | 保存到 %profile%\\lyrics\\.lrc（foobar2000 配置文件夹） |
| "all" | 同时写入以上三种目标 |
| ["file","embedded"] | 数组形式，自由组合任意目标 |

### 返回值 

单目标时（向后兼容扁平格式）：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| success | boolean | 保存是否成功 |
| savedTo | string | 实际保存的文件完整路径（target=file/config） |
| error | string | 失败时的错误信息 |

多目标时（结构化多目标结果）：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| success | boolean | 任一目标成功即为 true |
| results.file | object | 文件写入结果（含 success/savedTo/error） |
| results.embedded | object | 标签写入结果（含 success/path/tagsApplied/error） |
| results.config | object | 配置文件夹写入结果（含 success/savedTo/error） |

### 示例 

```javascript
// 保存到 .lrc 文件（向后兼容）
const result = await fb2k.invoke('lyrics.save', {
    path: 'C:\\\\Music\\\\song.flac',
    lyrics: '[00:01.00]Hello world\\n[00:05.00]Second line',
    target: 'file'
});

// 嵌入到音频标签
await fb2k.invoke('lyrics.save', {
    path: 'C:\\\\Music\\\\song.flac',
    lyrics: '[00:01.00]Hello world',
    target: 'embedded',
    tagName: 'SYNCEDLYRICS'
});

// 保存到配置文件夹 (%profile%\\lyrics\\)
await fb2k.invoke('lyrics.save', {
    path: 'C:\\\\Music\\\\song.flac',
    lyrics: '[00:01.00]Hello world',
    target: 'config'
});

// 三合一：同时写入文件、标签和配置文件夹
const all = await fb2k.invoke('lyrics.save', {
    path: 'C:\\\\Music\\\\song.flac',
    lyrics: '[00:01.00]Hello world',
    target: 'all'
});

// 数组形式：只写文件和配置文件夹
await fb2k.invoke('lyrics.save', {
    path: 'C:\\\\Music\\\\song.flac',
    lyrics: '[00:01.00]Hello world',
    target: ['file', 'config']
});

// SDK 封装
await fb.lyrics.save('C:\\\\Music\\\\song.flac', lyricsText, { target: 'all' });
```

## 容器格式（CUE / ISO / 多子轨文件） 

当音频路径包含 `|subsong:N` 后缀时（如 CUE sheet、SACD ISO 等容器格式），歌词 API 会自动采用 **per-track 命名**，避免不同子轨之间的歌词互相覆盖。

### 命名规则 

| 条件 | 文件命名 | 示例 |
| --- | --- | --- |
| subsong == 0（单轨或首轨） | .lrc | album.lrc |
| subsong >= 1 | .NN.lrc（NN = subsong+1，最少两位零填充） | album.02.lrc（subsong=1）、album.10.lrc（subsong=9） |

### 各 target 行为 

| Target | 行为 |
| --- | --- |
| "file" | 在音轨同目录生成 per-track sidecar：如 D:\\album.03.lrc（subsong=2） |
| "embedded" | 委托给 foobar2000 input plugin 写入正确子轨的标签。CUE 格式写入共享标签块；SACD-ISO 由 foo_input_sacd 处理 |
| "config" | 在 %profile%\\lyrics\\ 下同样采用 per-track 命名：如 album.03.lrc |

### 读取回退 

`lyrics.get` 在查找外部歌词文件时，优先查找 per-track 文件（如 `album.03.lrc`），若不存在则回退到共享文件（如 `album.lrc`），兼容旧版数据。

### 示例 

```javascript
// CUE 第 3 轨（subsong=2）保存歌词到文件
await fb2k.invoke('lyrics.save', {
    path: 'D:\\\\album.flac|subsong:2',
    lyrics: '[00:01.00]Track 3 lyrics',
    target: 'file'
});
// → 生成 D:\\album.03.lrc

// SACD ISO 第 2 轨保存歌词
await fb2k.invoke('lyrics.save', {
    path: 'D:\\\\SACD.iso|subsong:1',
    lyrics: '[00:01.00]Track 2 lyrics',
    target: 'file'
});
// → 生成 D:\\SACD.02.lrc

// 读取时优先 per-track，回退到共享
const result = await fb2k.invoke('lyrics.get', {
    path: 'D:\\\\album.flac|subsong:2'
});
// sourcePath 为 "D:\\album.03.lrc"（若存在），否则回退到 "D:\\album.lrc"
```

## 相关 

- [`artwork.getLyrics`](/api/artwork#artwork-getlyrics) — 仅从内嵌标签读取歌词（不查外部文件）
- [`<fb-lyrics-panel>`](/components/media) — 歌词面板 Web Component，自动加载和同步高亮

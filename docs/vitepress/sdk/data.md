# 数据与元信息

涵盖 `fb.metadata`、`fb.titleformat`、`fb.playcount`、`fb.selection`、`fb.rating` 五个命名空间。

## fb.metadata 元数据读写 

### read(path) 

读取文件的结构化元数据，返回 `{ success, path, tags, info }`。底层会先 canonicalize 路径，再尝试 cached info；若关键元数据缺失，则自动退回 direct file read。

```javascript
const meta = await fb.metadata.read('E:\\\\Music\\\\song.flac');
console.log(meta.tags, meta.info.sampleRate);
```

### readBatch(paths) 

批量读取多个文件的元数据。

```javascript
const results = await fb.metadata.readBatch([
    'E:\\\\Music\\\\song1.flac',
    'E:\\\\Music\\\\song2.flac'
]);
```

### readByPath(path) 

通过路径读取扁平元数据。与 `read` 共享同一 fallback 语义，但会把标签和技术字段统一展开为大写键名。

```javascript
const meta = await fb.metadata.readByPath('E:\\\\Music\\\\song.flac');
console.log(meta.TITLE, meta.DURATION, meta.SAMPLERATE);
```

### write(path, tags) 

写入元数据标签。`tags` 是一个 field→value 的对象。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| path | string | 文件路径 |
| tags | object | 标签键值对，如 { artist: 'New Artist' } |

```javascript
await fb.metadata.write('E:\\\\Music\\\\song.flac', {
    artist: 'New Artist',
    title: 'New Title'
});
```

### writeBatch(items) 

批量写入元数据。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| items | array | [{ path, tags }, ...] |

```javascript
await fb.metadata.writeBatch([
    { path: 'song1.flac', tags: { artist: 'A' } },
    { path: 'song2.flac', tags: { artist: 'B' } }
]);
```

### embedArtwork(path, options?) 

嵌入封面到文件。

```javascript
await fb.metadata.embedArtwork('E:\\\\Music\\\\song.flac', { imageData: base64Data, type: 'front' });
```

### removeEmbeddedArt(path, options?) 

移除文件中嵌入的封面。

```javascript
await fb.metadata.removeEmbeddedArt('E:\\\\Music\\\\song.flac');
```

### removeField(path, field) 

移除文件中的指定标签字段。内部传递 `tags: [field]` 数组。

```javascript
await fb.metadata.removeField('E:\\\\Music\\\\song.flac', 'COMMENT');
```

### readRaw(path, options?) 

以 raw 格式读取元数据（绕过 Titleformat 解析），返回未处理的标签键值。

```javascript
const raw = await fb.metadata.readRaw('E:\\\\Music\\\\song.flac');
// raw.tags: { ARTIST: ['...'], TITLE: ['...'], ... }
```

### removeTag(path, tags) 

移除文件中的多个标签字段。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| path | string | 文件路径 |
| tags | array | 要移除的标签名数组 |

```javascript
await fb.metadata.removeTag('E:\\\\Music\\\\song.flac', ['COMMENT', 'LYRICS']);
```

## fb.titleformat Titleformat 格式化 

### eval(pattern, path?) 

计算 Titleformat 表达式。不传 `path` 时对当前播放曲目求值。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| pattern | string | Titleformat 表达式 |
| path | string | 可选，指定文件路径 |

```javascript
const r = await fb.titleformat.eval('%artist% - %title%');
console.log(r.result); // 'Artist - Title'
```

### evalBatch(pattern, paths) 

对多个文件批量求值同一个表达式。

```javascript
const results = await fb.titleformat.evalBatch(
    '%artist% - %title%',
    ['song1.flac', 'song2.flac']
);
```

### evalFields(path, fields) 

获取文件的字段值。`fields` 是字段名到 Titleformat 表达式的映射对象。

```javascript
const result = await fb.titleformat.evalFields('E:\\\\Music\\\\song.flac', {
    artist: '%artist%',
    title: '%title%',
    album: '%album%'
});
// result.values.artist, result.values.title, result.values.album
```

### evalFieldsBatch(paths, fields) 

批量获取多个文件的字段值。`fields` 是字段名到 Titleformat 表达式的映射对象。

### getBuiltinFields() 

获取所有内置 Titleformat 字段列表。

```javascript
const r = await fb.titleformat.getBuiltinFields();
// r.fields: ['artist', 'title', 'album', ...]
```

## fb.playcount 播放统计 

### get(path) 

获取指定文件的播放次数。

```javascript
const r = await fb.playcount.get('E:\\\\Music\\\\song.flac');
console.log(r.count);
```

### getBatch(paths) 

批量获取多个文件的播放次数。

```javascript
const results = await fb.playcount.getBatch(['song1.flac', 'song2.flac']);
```

### set(path, count) 

设置文件的播放次数。

```javascript
await fb.playcount.set('E:\\\\Music\\\\song.flac', 42);
```

### getStats() 

获取全局播放统计信息。

```javascript
const stats = await fb.playcount.getStats();
```

## fb.selection 选择同步 

### get(options?) 

获取当前选择的曲目。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| options.offset | number | 可选，偏移量 |
| options.limit | number | 可选，数量限制 |
| options.includeTrackInfo | boolean | 可选，是否包含曲目详情 |

```javascript
const sel = await fb.selection.get({ includeTrackInfo: true });
```

### getType() 

获取当前选择类型。

```javascript
const r = await fb.selection.getType();
// r.type: 'playlist' | 'now_playing' | ...
```

### set(handles) 

设置当前选择。

```javascript
await fb.selection.set([handle1, handle2]);
```

### setPlaylistTracking(mode?) 

设置播放列表跟踪模式。默认 `'selection'`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| mode | string | 'selection' 或 'playlist' |

```javascript
await fb.selection.setPlaylistTracking('playlist');
```

### getViewerMode() 

获取当前查看器模式。

### getViewingTrack(options?) 

获取当前正在查看的曲目。

```javascript
const track = await fb.selection.getViewingTrack({ includeTrackInfo: true });
```

## fb.rating 评分 

### get(path) 

获取文件的评分。

```javascript
const r = await fb.rating.get('E:\\\\Music\\\\song.flac');
console.log(r.rating); // 0-5
```

### set(path, rating) 

设置文件的评分。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| path | string | 文件路径 |
| rating | number | 评分值 (0-5)，0 表示清除 |

```javascript
await fb.rating.set('E:\\\\Music\\\\song.flac', 5);
await fb.rating.set('E:\\\\Music\\\\song.flac', 0); // 清除评分
```

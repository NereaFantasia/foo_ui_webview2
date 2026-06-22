# Titleformat API 

foobar2000 Titleformat 表达式求值。共 5 个 API。

## 单文件求值 

### titleformat.eval 

对单个文件求值单个 titleformat 表达式。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 音频文件路径 |
| pattern | string | ✓ | Titleformat 表达式 |

**返回值**:

```json
{
    "success": true,
    "path": "C:\\\\Music\\\\song.flac",
    "pattern": "%artist%",
    "result": "Artist Name"
}
```

```javascript
const r = await fb2k.invoke('titleformat.eval', {
    path: 'C:\\\\Music\\\\song.flac',
    pattern: '%artist% - %title%'
});
console.log(r.result);
```

### titleformat.evalFields 

对单个文件求值多个字段。内部合并为单次 `format_title()` 调用，比多次 `eval` 更高效。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 音频文件路径 |
| fields | object | ✓ | { fieldName: pattern } 映射 |

**返回值**:

```json
{
    "success": true,
    "path": "C:\\\\Music\\\\song.flac",
    "artist": "Artist Name",
    "album": "Album Name"
}
```

```javascript
const r = await fb2k.invoke('titleformat.evalFields', {
    path: track.absolutePath,
    fields: {
        artist: '%artist%',
        album: '%album%',
        year: '$year(%date%)'
    }
});
```

## 批量求值 

### titleformat.evalBatch 

对多个文件求值同一个 titleformat 表达式。模式只编译一次。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| paths | string[] | ✓ | 文件路径数组 |
| pattern | string | ✓ | Titleformat 表达式 |

**返回值**:

```json
{
    "success": true,
    "pattern": "%artist%",
    "total": 100,
    "successCount": 98,
    "errorCount": 2,
    "results": [
        { "path": "...", "success": true, "result": "Artist" }
    ]
}
```

### titleformat.evalFieldsBatch 

对多个文件求值多个字段。合并所有字段为单次调用，100 路径 × 10 字段仅需 100 次 `format_title()`。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| paths | string[] | ✓ | 文件路径数组 |
| fields | object | ✓ | { fieldName: pattern } 映射 |

**返回值**:

```json
{
    "success": true,
    "total": 100,
    "successCount": 100,
    "errorCount": 0,
    "results": [
        { "path": "...", "success": true, "artist": "...", "album": "..." }
    ]
}
```

## 参考 

### titleformat.getBuiltinFields 

获取内置 titleformat 字段参考表。

- **参数**: 无

**返回值**:

```json
{
    "success": true,
    "fields": {
        "artist": "%artist%",
        "album": "%album%",
        "title": "%title%",
        "duration": "%length%",
        "playCount": "%play_count%",
        "rating": "%rating%",
        "added": "%added%"
    }
}
```

::: tip TIP
完整字段列表包含标准标签、技术信息、文件信息、foo_playcount 字段和常用组合模式。
:::

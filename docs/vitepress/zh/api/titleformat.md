# Titleformat API

foobar2000 Titleformat 表达式求值。共 5 个 API。

## 单文件求值

### titleformat.eval

对单个文件求值单个 titleformat 表达式。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 否 |  |
| `pattern` | `string` | 否 | titleformat 表达式，如 `%artist% - %title%`。 |

**返回值**: `{"error":"...","path":"...","pattern":"...","result":{},"success":true}`


```javascript
const r = await fb2k.invoke('titleformat.eval', {
    path: 'C:\\Music\\song.flac',
    pattern: '%artist% - %title%'
});
console.log(r.result);
```

### titleformat.evalFields

对单个文件求值多个字段。内部合并为单次 `format_title()` 调用，比多次 `eval` 更高效。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `fields` | `object` | 是 | 字段名 → 表达式的映射对象。 |
| `path` | `string` | 否 |  |

**返回值**: `{"error":"...","path":"...","success":true}`


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

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `array` | 是 | 文件路径数组。 |
| `pattern` | `string` | 否 | titleformat 表达式，只编译一次。 |

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

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `fields` | `object` | 是 | 字段名 → 表达式的映射对象。 |
| `paths` | `array` | 是 | 文件路径数组。 |

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

::: tip
完整字段列表包含标准标签、技术信息、文件信息、foo_playcount 字段和常用组合模式。
:::

## 求值语义

- `titleformat.eval` 要求非空 `path` 和 `pattern`。表达式无效或文件不可用时，会返回带 `error` 的 `success: false`。
- `titleformat.evalBatch` 要求数组 `paths` 和非空 `pattern`；单项失败会保留在 `results` 中，`successCount` 与 `errorCount` 汇总批次结果。
- `titleformat.evalFields` 与 `titleformat.evalFieldsBatch` 要求对象 `fields`。每个键会成为响应属性；非字符串字段表达式会为对应键返回 `null`。
- `titleformat.getBuiltinFields` 是 runtime 生成的便捷参考。它不保证每个安装或曲目都填充播放次数等可选组件字段。

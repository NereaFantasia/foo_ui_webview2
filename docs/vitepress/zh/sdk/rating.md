# fb.rating rating

本页是 `fb.rating` 的 SDK 视角文档入口。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## 其余方法

### get()

返回值为 `{ rating }`。使用 `set(path, rating, { cueIndex? })` 写入评分；`rating` 必须是 0 到 5 的整数，其中 0 表示清除评分。显式 `cueIndex` 的优先级高于路径中的 `\|subsong:N` 后缀。写入响应可能说明使用的 `menuPath`、存储后端或回退提示。

```javascript
const result = await fb.rating.get('E:\\Music\\song.flac');
await fb.rating.set('E:\\Music\\song.flac', 4);
```

<!-- END AUTO-GENERATED SDK STUBS -->

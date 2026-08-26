# fb.titleformat Titleformat

本页是 `fb.titleformat` 的 SDK 视角文档入口。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## 其余方法

### evalFieldsBatch()

封装 `titleformat.evalFieldsBatch`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.titleformat.evalFieldsBatch(/* 参数见 TypeScript 声明 */);
```

<!-- END AUTO-GENERATED SDK STUBS -->

## Info 可用性信号

每次成功求值都会返回 `infoAvailable`，取自宿主 `format_title` 的返回值。

| 取值 | 含义 |
| --- | --- |
| `true` | 该曲目的 metadb info 已就绪，标签类取值可信。 |
| `false` | 该曲目的 metadb info 未就绪，渲染时用的是占位 `file_info`，因此 `%bitrate%`、`%codec%` 等标签类取值不可信。 |
| 缺省 | 该字段未被写入。具体情形逐方法不同 —— 见下方「什么时候没有这个字段」。 |

```javascript
const one = await fb.titleformat.eval('%bitrate%', 'E:\\Music\\one.flac');
if (one.infoAvailable === false) {
	// 尚未入库 —— 此时应显示「未知」而不是空值
}

const many = await fb.titleformat.evalFieldsBatch(
	['E:\\Music\\one.flac', 'E:\\Music\\two.flac'],
	{ bitrate: '%bitrate%', rating: '%rating%' }
);
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

`fields` 里名为 `infoAvailable` 的键会用你自己的模式输出覆盖该标志，与既有 `path`、`success` 键的行为一致。

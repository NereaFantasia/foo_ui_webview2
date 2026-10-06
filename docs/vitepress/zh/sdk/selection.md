# fb.selection 选择同步

`fb.selection` 读取和更新 foobar2000 当前的选择，并解析当前正在查看的曲目。

## get(opts?)

签名：`fb.selection.get(opts?: SelectionGetParams): Promise<SelectionGetResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `opts.offset` | `number` | 否 | 返回的第一个 handle 的下标，默认 `0` |
| `opts.limit` | `number` | 否 | 最多返回的 handle 数；`0` 表示从 `offset` 起的全部。省略时按 100 封顶 |

按页读取当前的全局选择。返回 `count`（整个选择的条数，不是本页的条数）、`type`（选择的来源，如 `active_playlist_selection`、`media_library_viewer`）、`handles`、实际生效的 `offset` 和 `hasMore`。handle 是原生路径，子曲目不为 `0` 时附加 `|subsong:N`。只有省略 `limit` 且选择超过 100 条时才出现 `truncated: true`。该调用只读状态，不做修改。

```javascript
const sel = await fb.selection.get({ offset: 0, limit: 50 });
if (sel.success === false) throw new Error(sel.error);
console.log(sel.count, sel.type, sel.handles, sel.hasMore);
```

## getType()

签名：`fb.selection.getType(): Promise<SelectionGetTypeResponse>`

报告当前全局选择的类型，同时给出数字 `type` 和名称 `typeName`：`0` `now_playing`、`1` `active_playlist_selection`、`2` `active_playlist`、`3` `playlist_manager`、`5` `media_library_viewer`。无法识别的类型报 `type: 0`、`typeName: 'unknown'`，所以判断时比较 `typeName`，不要比较 `type`。

```javascript
const result = await fb.selection.getType();
if (result.success === false) throw new Error(result.error);
console.log(result.type, result.typeName);
```

## getViewerMode()

签名：`fb.selection.getViewerMode(): Promise<SelectionGetViewerModeResponse>`

以 `{ mode }` 返回当前的查看器模式：选择类型是正在播放的曲目时为 `prefer_playing`，否则为 `prefer_selection`。该模式由当前的选择类型推导，不是读取某项设置。

```javascript
const result = await fb.selection.getViewerMode();
if (result.success === false) throw new Error(result.error);
console.log(result.mode);
```

## getViewingTrack(opts?)

签名：`fb.selection.getViewingTrack(opts?: SelectionGetViewingTrackParams): Promise<SelectionGetViewingTrackResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `opts.includeTrackInfo` | `boolean` | 否 | 是否把完整的曲目行作为 `track` 一并返回，默认 `false` |

解析查看器应当显示的曲目：先按查看器模式偏好的来源，那里没有曲目时回退到另一来源。该调用恒成功。`found` 表示两个来源里是否有曲目，`mode` 是本次解析依据的偏好。找到曲目时，响应另带 `source`（`now_playing` 或 `selection`）、`handle`，曲目在播放列表中的位置已知时还带 `playlistIndex` / `itemIndex`。

```javascript
const view = await fb.selection.getViewingTrack({ includeTrackInfo: true });
if (view.success === false) throw new Error(view.error);
if (view.found) console.log(view.source, view.handle, view.track?.title);
```

## set(handles)

签名：`fb.selection.set(handles: string[]): Promise<SelectionSetResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `handles` | `string[]` | 是 | 要选中的曲目，原生路径加可选的 `\|subsong:N` 后缀；至少一条 |

用给定曲目替换全局选择，返回 `count`，即交给选择的曲目数。handle 按字符串直接生成，不做查找，不存在的路径同样会被选中；后缀不是数字时按子曲目 `0` 处理。没有任何一条字符串能生成 handle 时，调用以 `INVALID_PARAMS` 失败。

宿主只在调用期间持有这份选择，而 foobar2000 在没有任何持有者时清空选择。所以新选择要在调用结束后留下，得有别的持有者在，比如开着 `grabFocus` 且有焦点的调用方面板自己持有的那个。

```javascript
const result = await fb.selection.set(['E:\\Music\\one.flac', 'E:\\Music\\two.flac']);
if (result.success === false) throw new Error(result.error);
console.log(result.count);
```

## setPlaylistTracking(mode?)

签名：`fb.selection.setPlaylistTracking(mode?: NonNullable<SelectionSetPlaylistTrackingParams['mode']>): Promise<SelectionSetPlaylistTrackingResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `mode` | `'selection' \| 'playlist'` | 否 | `'selection'`（默认）取活动播放列表的选中行；`'playlist'` 取整个活动播放列表 |

从活动播放列表设置全局选择，返回实际生效的 `mode`。选择只取一次：宿主在调用返回时释放持有者，foobar2000 的跟踪随之结束，之后播放列表的变化不会跟进。选择本身能留多久同 `set()`。

```javascript
const result = await fb.selection.setPlaylistTracking('playlist');
if (result.success === false) throw new Error(result.error);
```

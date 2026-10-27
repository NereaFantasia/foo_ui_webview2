# Selection 选择 API

提供全局选区读取、设置与查看器曲目解析能力。

## Selection API - 选择同步

### selection.getViewerMode

<!-- api-schema:begin selection.getViewerMode -->
获取当前 Selection Viewer 优先展示的来源模式（根据当前选区类型实时推导，而非读取持久化配置）

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `mode` | `"prefer_playing" \| "prefer_selection"` | 选区来源为正在播放的曲目时返回 prefer_playing，其余情况返回 prefer_selection |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### selection.getViewingTrack

<!-- api-schema:begin selection.getViewingTrack -->
解析查看器当前应当展示的目标曲目（优先读取偏好来源，无曲目时自动回退至备用来源；调用恒返回成功，通过 found 标识是否存在可展示曲目）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `includeTrackInfo` | `boolean` | 否 | 是否在 track 字段中一并返回完整的曲目元数据行默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `found` | `boolean` | 任一候选来源中是否成功解析到可用曲目 |
| `mode` | `"prefer_playing" \| "prefer_selection"` | 本次解析所采用的来源偏好模式 |
| `source` | `"now_playing" \| "selection"` | 实际提供该曲目的数据来源（found 为 false 时缺省） |
| `handle` | `string` | 曲目句柄（原生文件路径，子曲目序号大于 0 时附加 \|subsong:N；found 为 false 时缺省） |
| `playlistIndex` | `integer` | 曲目所在的播放列表从零开始的索引（若无法定位行号则缺省；正在播放的曲目取自启动播放时的来源位置，选区曲目仅检索当前活动播放列表，媒体库或非活动列表产生的选区无行号映射；与 itemIndex 同步存在） |
| `playlistGuid` | `string` | 目标播放列表的 GUID（格式与 playlistGuid 接收格式一致；与 playlistIndex 同步存在） |
| `itemIndex` | `integer` | 曲目在目标播放列表中首次出现的行号索引（playlistIndex 缺省时此字段亦缺省） |
| `track` | [Track](../reference/types.md#track) | 完整的曲目元数据行（仅在 includeTrackInfo 为 true 时返回） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

**回退逻辑**：

- `prefer_playing`：优先返回正在播放曲目，无正在播放曲目时回退至当前选区
- `prefer_selection`：优先返回当前选区曲目，无选区曲目时回退至正在播放曲目
- 两个来源均无曲目：返回 `found: false`

```javascript
const r = await fb2k.invoke('selection.getViewingTrack', { includeTrackInfo: true });
if (r.success === false) throw new Error(r.error);
if (r.found) {
    console.log(`显示: ${r.handle} (来源: ${r.source})`);
}
```

### selection.get

<!-- api-schema:begin selection.get -->
分页获取当前全局选中的曲目句柄列表（纯只读查询，不修改选区状态）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `offset` | `integer` | 否 | 首个返回句柄在选区中的起始偏移索引（从 0 开始）不小于 `0`。默认 `0`。 |
| `limit` | `integer` | 否 | 最大返回句柄数量（传入 0 表示返回从 offset 开始的全部句柄；缺省时默认限制返回前 100 条并通过 truncated 指示是否触发自动截断）不小于 `0`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | 当前全局选区的曲目总数（非单页返回数量） |
| `type` | `"now_playing" \| "active_playlist_selection" \| "active_playlist" \| "playlist_manager" \| "media_library_viewer" \| "unknown"` | 全局选区的数据来源类型 |
| `handles` | `string[]` | 当前分页包含的曲目句柄列表（原生文件路径，子曲目序号大于 0 时附加 \|subsong:N） |
| `offset` | `integer` | 实际生效的起始偏移量 |
| `hasMore` | `boolean` | 当前页之后是否仍有后续选区条目 |
| `truncated` | `boolean` | 是否触发了未指定 limit 时的 100 条自动截断限制（仅在省略 limit 且总选区超过 100 条时存在且固定为 true） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

::: warning 性能提示
未指定 `limit` 时，若选区超过 100 首曲目将自动截断为前 100 条。显式传入的 `limit` **不会**被强制收窄，如需获取全部数据请传入 `limit: 0`。
:::

`truncated: true` 仅在自动截断上限**真正生效**时出现（即选区超过 100 条**且**未传 `limit`）。在 250 条选区中显式传入 `limit: 10` 不属于自动截断，返回结果不会附带该字段。该字段从不以 `false` 出现，业务代码应判断字段是否存在而非检查布尔真假；是否需要继续分页请依据 `hasMore` 字段。`count` 表示当前选区的曲目总数，而非单次返回条数。

```js
const res = await fb2k.invoke('selection.get', { limit: 0 });
if (res.success === false) throw new Error(res.error);
const { handles, count, hasMore } = res;
```

### selection.getType

<!-- api-schema:begin selection.getType -->
获取当前全局选区的来源类型（同时返回数字索引与类型名称）

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `type` | `integer` | 选区来源类型的数值索引（0 now_playing、1 active_playlist_selection、2 active_playlist、3 playlist_manager、5 media_library_viewer；未知类型统一回退为 0） |
| `typeName` | `"now_playing" \| "active_playlist_selection" \| "active_playlist" \| "playlist_manager" \| "media_library_viewer" \| "unknown"` | 选区来源类型字符串标识 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

| type | typeName | 说明 |
| --- | --- | --- |
| 0 | now_playing | 正在播放 |
| 1 | active_playlist_selection | 活动播放列表的选择 |
| 2 | active_playlist | 活动播放列表 |
| 3 | playlist_manager | 播放列表管理器 |
| 5 | media_library_viewer | 媒体库查看器 |

### selection.set

<!-- api-schema:begin selection.set -->
将全局选区替换为指定曲目列表（曲目句柄直接依据路径字符串构造而不执行存在性检查，未存在的路径也会被加入选区；宿主仅在调用期间持有选区持有者，foobar2000 在所有持有者释放后会自动清空全局选区，若需在调用结束后维持选区，须由其他持有者继续保持，例如处于聚焦且开启 grabFocus 的调用方面板所持有的实例）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `handles` | `string[]` | 是 | 待设置到选区的曲目路径列表（原生文件路径，可带 \|subsong:N 后缀；若后缀非有效数字则回退为子曲目 0）不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | 成功提交至选区持有者的有效曲目句柄总数（该数值为实际解析提交的条数而非重读校验结果；选区维持生命周期见 set 方法说明） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

`|subsong:` 后缀格式错误时自动回退为子曲目 `0`。`handles` 缺失、包含非字符串元素、传入空数组，或无一条能成功构造句柄时，均以 `INVALID_PARAMS` 拒绝；若宿主无法获取选区持有者，则以 `OPERATION_FAILED` 失败。

### selection.setPlaylistTracking

<!-- api-schema:begin selection.setPlaylistTracking -->
将全局选区同步为当前活动播放列表的完整曲目或其选中项（foobar2000 在初始持有者释放时自动终止跟踪，宿主在调用返回时即释放该持有者，后续播放列表状态变更将不再跟随同步；选区有效生命周期机制与 set 一致）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `mode` | `"selection" \| "playlist"` | 否 | 播放列表跟踪模式（playlist 跟踪整个当前活动列表；selection 跟踪列表中当前选中的行项）默认 `"selection"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `mode` | `"selection" \| "playlist"` | 实际生效的播放列表跟踪模式 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

| mode 值 | 说明 |
| --- | --- |
| `selection` | 跟踪播放列表中用户选择的曲目 |
| `playlist` | 跟踪整个播放列表 |

`mode` 仅接受上述两个枚举值，传入其他值将以 `INVALID_PARAMS` 拒绝。

## 选区行为与生命周期 {#选择行为}

`selection.getViewerMode` 返回 `prefer_playing` 或 `prefer_selection`，该值由当前活动选区类型动态推导，而非读取静态配置项。`selection.getViewingTrack` 依据此偏好选择数据来源；若首选来源无曲目，则自动回退至备选来源。该方法恒返回 `success: true`，是否存在有效曲目请判断 `found` 字段。传入 `includeTrackInfo` 时，返回的 `track` 为统一的 [Track](../reference/types.md#track) 结构。`selection:changed` 在选区更新后广播至所有 WebView 实例，并在底层施加 50 ms 节流；其负载数据结构见[事件参考](events.md#selection-changed)。

播放队列与选区共用相同的句柄字符串格式：原生文件路径，且仅在子曲目序号大于 `0` 时附加 `|subsong:N`。传给 `queue.addPaths` 或 JIT 队列操作的路径同样支持此后缀格式。单条路径或 URL 最大长度上限为 2048 字符。

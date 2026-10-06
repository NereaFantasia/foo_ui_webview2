# fb.dnd 拖放

`fb.dnd` 暴露宿主视角的外部文件拖放会话，让页面能取到 HTML5 `File` 对象刻意
隐藏的真实文件系统路径；也让页面能把曲目作为真实文件**拖出**窗口，见
[拖出文件](#拖出文件)。

## 工作原理

Windows 把拖入的文件列表交给原生窗口而非页面，浏览器引擎中 `File.path` 恒为
`null`。本命名空间**不替代** HTML5 拖放：标准的 `dragenter` / `dragover` /
`drop` 事件照原样触发，`fb.dnd` 与它们并行运行，只回答 HTML5 无法回答的那个
问题——文件在磁盘上的真实位置。

宿主把每次拖放记为一个带 id 的**会话**，并在任何 `dnd:*` 监听器运行之前将其
发布到顶层文档的 `window.__fbDndSession`。

## 读取路径

三种方式，可靠性递减。

### getPathsAsync(sessionId?)

签名：`fb.dnd.getPathsAsync(sessionId?: string): Promise<DndGetPathsAsyncResponse>`

直接查询宿主。这是可靠选择，也是在 HTML5 `drop` 处理函数中应当使用的方式：它
读取宿主状态而非推送到页面的快照，因此不依赖消息投递顺序。在 `await` 之后调用
也安全，因为它从不访问 `event.dataTransfer`。

路径顺序与 `DataTransfer.files` 一致，页面可按下标配对。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `sessionId` | `string` | 否 | 要查询的会话，取自 `dnd:*` 载荷。省略则查询本窗口当前活动或最近结束的会话。 |

返回 `{ sessionId, paths, resolvedPaths }`。`paths` 与 `resolvedPaths` 长度恒
相等；会话已过期、未携带文件列表，或 origin 不被信任时两者同为空数组。
`resolvedPaths` 见下文「快捷方式目标」一节。

只读宿主内存。快捷方式目标在拖放抵达时已解析一次，因此反复调用不产生任何
文件系统访问。

```javascript
document.addEventListener('drop', async (event) => {
    event.preventDefault();
    const { paths } = await fb.dnd.getPathsAsync();
    if (paths.length) {
        await fb.playlist.addPaths(paths);
    }
});
```

### `dnd:drop` 事件

具权威性，因为 drop 载荷携带最终列表——拖动源可能在 `dnd:enter` 之后修改它。
该事件的到达时机相对页面自身的 `drop` 处理函数是独立的，因此应视为通知，而非
处理函数的替代品。

```javascript
fb.on('dnd:drop', (data) => {
    console.log(data.sessionId, data.paths, data.x, data.y);
});
```

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `sessionId` | `string` | 关联同一次拖放手势的 `dnd:enter`、`dnd:leave` 与 `dnd:drop`。 |
| `paths` | `string[]` | 绝对文件系统路径，顺序与 `DataTransfer.files` 一致；被扣留时为空数组。 |
| `resolvedPaths` | `(string \| null)[]` | 对应下标的快捷方式目标，或 `null`。长度与 `paths` 恒相等，详见「快捷方式目标」一节。 |
| `x`、`y` | `number` | 光标位置，客户区物理像素——除以 `devicePixelRatio` 得 CSS 像素。 |
| `keyState` | `number` | drop 时刻的 Win32 `MK_*` 修饰键 / 鼠标键掩码。 |

`dnd:enter` 携带 `sessionId`、`paths`、`resolvedPaths`、`hasFiles` 与同样的光标
字段；`dnd:leave` 仅携带 `sessionId`。

### getPaths()

签名：`fb.dnd.getPaths(): string[]`

同步读取快照，因此仅为尽力而为。以下情况返回空数组：无活动会话、拖放未携带
文件列表、origin 不被信任、宿主尚未发布会话——快速拖放可能在发布完成前就抵达
页面的 `drop` 处理函数。请把它留给乐观 UI，真实数据源用 `getPathsAsync`。

```javascript
const optimistic = fb.dnd.getPaths();
```

### hasFiles()

签名：`fb.dnd.hasFiles(): boolean`

适用于 `dragover`：此时浏览器不提供 `dataTransfer.files`，只暴露
`items.length`，页面无法区分文件拖放与其他载荷。它与 `getPaths` 有同样的时序
注意事项；权威值是 `dnd:enter` 载荷中的 `hasFiles` 字段。

```javascript
element.addEventListener('dragover', (event) => {
    if (fb.dnd.hasFiles()) {
        event.preventDefault();
    }
});
```

## 快捷方式目标

拖入快捷方式时，列表里给出的是 `.lnk` 文件本身，foobar2000 不认识它。因此上面
每一条路径来源都配了一个平行数组：`dnd:enter` / `dnd:drop` 载荷与
`getPathsAsync` 上的 `resolvedPaths`，以及读取快照的 `getResolvedPaths()`。

`paths` **不做任何改写**。它与 `DataTransfer.files` 的下标对应是固定契约，所以
目标是**并列**在原条目旁边，而非取代它——用哪一个由页面自己决定。

### getResolvedPaths()

签名：`fb.dnd.getResolvedPaths(): (string | null)[]`

长度与 `getPaths()` 恒相等。以下任一情况该项为 `null`：

- 该路径不是 `.lnk` 快捷方式
- 快捷方式指向 shell 命名空间对象（如「回收站」）而非文件
- 记录的目标路径过长、无法完整读回。Windows 交出的快捷方式目标以 `MAX_PATH`
  为上限，而被截断的路径会指向**另一个**文件，因此宁可拒绝也不上报
- 宿主读取快捷方式的那个线程上 COM 不可用
- 为保证拖放响应速度，该项的解析被跳过

以上每一种情况都用 `null`，**绝不会是空字符串**，因此判断真值即可区分「已解析」
与「未解析」。

::: warning 目标只是快捷方式的指向，不代表那个文件还在
**断链**的快捷方式**不会**报 `null`。无论目标是否还存在，Windows 都会把 `.lnk`
里记录的目标路径交回来，所以非 `null` 的条目只意味着「快捷方式指向这里」，仅此
而已。

宿主刻意不做检查：读取快捷方式发生在拖动源应用被阻塞等待的那个线程上，逐条做
文件系统探测正是这里绝不能做的事。请在页面侧处理「目标已不存在」——最省事的做法
是让接收该路径的调用自己报告失败（`playlist.addPaths` 会返回 `addedCount` 与
`invalidCount`，目标消失表现为数量差，而不是抛错）。
:::

```javascript
const paths = fb.dnd.getPaths();
const targets = fb.dnd.getResolvedPaths();

// 快捷方式播放其目标，其他条目播放自身。
const playable = paths.map((path, i) => targets[i] ?? path);
await fb.playlist.addPaths(playable);
```

同步读取快照，因此与 `getPaths()` 有同样的时序注意事项。可靠等价物是
`getPathsAsync()` 的 `resolvedPaths` 字段。

只解析 `.lnk`。`.url` 网络快捷方式、`.library-ms` 库定义、虚拟搜索结果一律报
`null`。

::: tip 为什么解析会被跳过
宿主在自己的 UI 线程上读取快捷方式目标，而此时拖动源应用正在等待拖放被接受——
在那里阻塞会冻结整个系统的拖放。指向不可达网络共享的快捷方式可能在 Windows
内部阻塞数秒，且无法中断。因此宿主对整次拖放设定时间预算，超出后剩余项直接报
`null`，而不是让用户干等。普通文件不消耗这份预算。
:::

## 能力查询

### getCapabilities()

签名：`fb.dnd.getCapabilities(): Promise<DndCapabilities>`

本窗口的拖放集成当前能提供什么。它在窗口生命周期内并非恒定：导航到不同 origin
会收回路径访问权，同时保留 HTML5 拖放事件。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `html5` | `boolean` | 页面仍能收到标准 HTML5 拖放事件。 |
| `paths` | `boolean` | 可取到真实文件系统路径。 |
| `hosting` | `'visual' \| 'standard'` | 窗口承载 WebView 的方式。 |
| `pathsUnavailableReason` | `string` | 仅当 `paths` 为 `false` 时出现。 |
| `dragOut` | `boolean` | 可通过 [`prepareDrag`](#preparedrag-paths) 把文件拖出。 |
| `dragOutUnavailableReason` | `string` | 仅当 `dragOut` 为 `false` 时出现。 |

```javascript
const caps = await fb.dnd.getCapabilities();
if (!caps.paths) {
    console.warn('paths unavailable:', caps.pathsUnavailableReason);
}
```

`pathsUnavailableReason` 取值为 `origin-untrusted`、`inner-target-not-found`、
`forward-unavailable`、`chain-failed`、`displaced`、`register-failed` 之一。

`dragOutUnavailableReason` 取值为 `not-visual-hosting`（DUI / CUI 面板，拖动源由
Chromium 持有）、`runtime-too-old`（WebView2 运行时早于本功能依赖的拖放开始事件，
需要 Edge 144 或更新）、`register-failed` 之一。`paths` 与 `dragOut` 相互独立，
用哪个就查哪个。

订阅 `dnd:capabilitiesChanged` 以响应变化。其载荷携带同样的 `html5`、`paths`、
`hosting`、`pathsUnavailableReason`、`dragOut` 与 `dragOutUnavailableReason` 字段。

```javascript
fb.on('dnd:capabilitiesChanged', (caps) => {
    dropZoneEl.hidden = !caps.paths;
});
```

## 路径在哪些宿主可用

| 宿主 | HTML5 拖放事件 | 真实路径 |
| --- | --- | --- |
| 主窗口、弹出窗口（`hosting: 'visual'`） | 可用 | 可用 |
| DUI / CUI 面板（`hosting: 'standard'`） | 可用 | 不可用——报 `inner-target-not-found` |

面板的 WebView 在独立进程中持有自己的 drop target，宿主无法接管。请在展示依赖
路径的 UI 之前调用 `getCapabilities()`，不要根据窗口类型推断结果。

对不受信任的 origin 同样不提供路径。此时 HTML5 拖放事件仍然工作，因此在
`paths` 为 `false` 时隐藏全部拖放提示的页面会损失可用功能——应对两个标志分别
判断。

### iframe

路径只发布给顶层文档。在 `<iframe>` 内，`window.__fbDndSession` 恒为 `null`，
`getPaths()` / `getResolvedPaths()` 返回空数组而**不抛错**。被嵌入的页面若需要
路径，须由主 frame 经 `postMessage` 转交——这把「是否共享路径」的决定权放在了
它该在的地方。

## 拖出文件

反方向：页面上一个可拖动的元素，用户把它拖进资源管理器或其他应用时落下的是真实
文件。页面在拖放时刻**从不经手路径**：它事先把路径换成一枚一次性 **token**，拖放
开始时把 token 写进拖放数据，宿主再用 token 换回已经校验过的文件列表。此后这次
拖放就是一次普通的网页拖放——浏览器绘制拖影，光标随目标变化，结局由页面自己的
`dragend` 反映。

提供这个手势之前，先查 [`getCapabilities()`](#getcapabilities) 的 `dragOut`。

### prepareDrag(paths)

签名：`fb.dnd.prepareDrag(paths: string[]): Promise<DndDragToken>`

校验路径，返回供本窗口下一次拖出使用的 token。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 要拖出的位置，至少一条。接受原生路径、`file://` URL、`file-relative://` URL（便携安装下与程序同卷媒体的路径形态，交由 foobar2000 解析）、`archive://` / `unpack://` 项，以及 `路径\|subsong:N` 形式（后缀会被去掉，拖走容器）。曲目对象的 `path` 与 `absolutePath` 都可以传。 |

返回 `{ success: true, token }`。token 是不透明的，唯一用途是交给
[`applyDragToken`](#applydragtoken-datatransfer-token)。

::: warning 在拖放开始之前调用，不要在 `dragstart` 里调用
拖放数据存储只在 `dragstart` 处理函数同步执行期间可写。在该处理函数里 `await`
之后拿到的 token 已经写不进去了——`setData` 静默失败，拖放空着出去。请在
`pointerdown`（或 `mousedown`）时请求 token 并保存，在 `dragstart` 里同步使用。
按下后立刻拖动仍可能抢在请求回包之前触发 `dragstart`；此时页面手里没有 token，
就让这次拖放按普通拖放进行。
:::

token 规则：

- 存活 **30 秒**
- 被第一次使用它的拖放**消费**
- 绑定请求它的**窗口**
- 同一窗口下一次 `prepareDrag` 成功后，旧 token **失效**，即使尚未过期；只有最近一枚有效

违反任一规则的 token 在拖放开始时被拒绝，`dnd:dragEnded` 报 `PERMISSION_DENIED`。

::: tip 落到目标的永远是物理文件
cue 或多曲目容器里的一条曲目拖走的是整个容器文件；`archive://` / `unpack://` 项
拖走的是整个压缩包。因此传入的多项可能折叠成一个文件，容器内的定位信息不会随
之传递。
:::

路径先归一化为原生路径，再按与 `queue.addPaths` 相同的方式校验。处理函数返回的
错误信封会使 Promise resolve 而非 reject，应判断 `success`；通信失败仍可能 reject。

::: warning 不检查文件是否存在
路径校验不保证文件存在。已删除或改名的文件仍可能换到 token，但目标接受放置并
不证明文件已复制。列表可能过期时，请在调用前检查文件，或在 `dragend` 之外核对
实际复制结果。
:::

| `code` | 含义 |
| --- | --- |
| `NOT_FOUND` | 调用窗口没有拖放注册。 |
| `ORIGIN_DENIED` | 文档 origin 不受信任，不允许拖出文件。此判定先于任何路径检查。 |
| `NOT_SUPPORTED` | 本窗口 `dragOut` 为 `false`。 |
| `INVALID_PARAMS` | `paths` 缺失、为空，或元素不全是字符串。 |
| `INVALID_PATH` | 某项背后没有本地文件（流媒体、`cdda://` 曲目）。`paths[i]` 是**你传入数组**的下标。 |
| `PERMISSION_DENIED` | 某项被路径安全策略拒绝。`paths[i]` 是**归一化去重后**列表的下标，可能短于你的数组。 |
| `OPERATION_FAILED` | 无法创建 token。 |

错误信息不含路径本体。

### applyDragToken(dataTransfer, token)

签名：`fb.dnd.applyDragToken(dataTransfer: DataTransfer, token: string): void`

把 token 写进 `dragstart` 事件，让宿主挂上文件。必须在 `dragstart` 处理函数内
**同步**调用。它做两件事，两件都不可少：

1. 把 `text/plain` 设为 token 载体（`fb2k-dnd-token/1:` 后接 token），覆盖页面
   放进去的任何内容——宿主凭这个标记识别「这是一次拖出」
2. 把 `effectAllowed` 设为 `'copy'`

::: danger `effectAllowed` 必须恰为 `'copy'`
文件列表挂上之后，**移动**是由放置目标执行的，不是页面或宿主：当目的地与源在同一
分区时，资源管理器默认就是移动。因此宿主拒绝一切允许效果比 copy 更宽的拖放
（`dnd:dragEnded` 报 `INVALID_PARAMS`），这次拖放什么都不会发生。不设
`effectAllowed` 等于 `copy | move | link`，同样被拒。`applyDragToken` 之后不要再
改它。
:::

拖出期间 `text/plain` 槽位不能再放页面文本。宿主在拖放离开之前把它置空，因此以
文本编辑器为放置目标时收到的是空串，而不是 token。

```javascript
let token = null;

trackEl.addEventListener('pointerdown', async () => {
    token = null;
    const r = await fb.dnd.prepareDrag([trackPath]);
    if (r.success) token = r.token;
});

trackEl.addEventListener('dragstart', (e) => {
    if (token) fb.dnd.applyDragToken(e.dataTransfer, token);
    // 还没有 token：按页面自己设置的内容普通拖放
});

trackEl.addEventListener('dragend', (e) => {
    // 'copy'——目标接收了文件；'none'——取消或被拒
    console.log(e.dataTransfer.dropEffect);
});
```

### onDragEnded(handler)

签名：`fb.dnd.onDragEnded(handler: (payload: DndDragEndedPayload) => void): () => void`

订阅 `dnd:dragEnded`。该事件**只在宿主拒绝**一次拖出时触发，时机是拖放开始的
那一刻。返回取消订阅函数。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `result` | `'failed'` | 恒为 `'failed'`；宿主只报告拒绝。 |
| `code` | `string` | `PERMISSION_DENIED`（token 未知、过期、已消费、被顶掉或来自其他窗口）、`INVALID_PARAMS`（`effectAllowed` 不是 `'copy'`）、`OPERATION_FAILED`（文件列表挂不上去）。 |
| `error` | `string` | 可读原因，不含路径。 |

有效 token 在检查 `effectAllowed` 和附加文件列表之前就已消费。因此收到 `INVALID_PARAMS` 或 `OPERATION_FAILED` 后，重试前也必须申请新 token。

::: warning 这不是拖放结束通知
宿主接受 token 之后**没有事件**。拖放像任何网页拖放一样进行，结局在元素自己的
`dragend` 里：`dataTransfer.dropEffect` 为 `'copy'` 表示目标接收了文件，为
`'none'` 表示取消或目标拒收。收尾逻辑放在 `dragend`；`dnd:dragEnded` 用来告诉
用户（或你自己）为什么一次拖动什么都没发生。
:::

```javascript
const off = fb.dnd.onDragEnded(({ code, error }) => {
    if (code === 'PERMISSION_DENIED') token = null; // 下次重新申请
    console.warn('drag-out refused:', code, error);
});
```

### 拖出期间会发生什么

- 带 token 的拖放要么被接手（文件已挂上），要么被拒绝并吞掉（无拖影，目标处什么
  都不发生）。它**绝不会**退化成把 token 文本拖出去。
- **不带** token 的拖放——页面拖自己的文字、图片、链接——不受任何影响，也不会产生
  `dnd:dragEnded`。
- 拖放进行期间，宿主窗口等待放置目标响应，与从 WebView 拖出的任何 HTML5 拖放完全
  一样。目标接受得慢，窗口就等得久；这是 WebView2 拖放的工作方式，页面无法缩短。
- 在 `<iframe>` 里，token 交换走与其他调用相同的 `invoke` 通道。不要把嵌框当作
  安全边界；宿主的 origin 检查才是边界。

## 不支持的能力

### startDrag(type?)

签名：`fb.dnd.startDrag(type?: string): Promise<DndStartDragResponse>`

本方法不启动拖放，传入参数会被忽略。宿主处理函数返回
`{ success: false, code: 'NOT_SUPPORTED' }`，使 Promise **resolve** 而非 reject。
应判断 `success`；通信失败仍可能 reject。请改用
[`prepareDrag`](#preparedrag-paths) 与
[`applyDragToken`](#applydragtoken-datatransfer-token)。

```javascript
const r = await fb.dnd.startDrag('files');
console.log(r.success, r.code); // false 'NOT_SUPPORTED'
```

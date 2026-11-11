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

## 接受放下

页面按 HTML5 的方式接受放下：在接收放下的元素上，`dragover` 里调
`preventDefault()`。没有处理函数这样做的地方，宿主替页面拒收：光标显示禁止，松手时拒绝放置操作，不派发 `dnd:drop` 事件，并阻止浏览器默认打开拖入的文件。文本框保留默认
的文本放下。

拖动进入窗口的前 500 毫秒内，在页面响应前光标默认显示复制样式以避免瞬间闪烁为禁止图标；此时间窗内触发的放置操作仍会正常派发。随后光标样式将根据页面的事件响应实时更新。

```javascript
document.getElementById('playlist')?.addEventListener('dragover', (event) => {
    if (fb.dnd.hasFiles()) {
        event.preventDefault(); // 这里接受；其他地方都拒收
        if (event.dataTransfer) event.dataTransfer.dropEffect = 'copy';
    }
});
```

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
| `sessionId` | `string` | 否 | 要查询的会话，取自 `dnd:*` 载荷。省略则查询本窗口当前活动或最近结束的会话 |

返回 `{ sessionId, paths, resolvedPaths }`。`paths` 与 `resolvedPaths` 长度恒
相等；会话已过期、未携带文件列表，或 origin 不被信任时两者同为空数组。
`resolvedPaths` 见下文「快捷方式目标」一节。

只读宿主内存。快捷方式目标在拖放抵达时已解析一次，因此反复调用不产生任何
文件系统访问。

```javascript
document.addEventListener('drop', async (event) => {
    event.preventDefault();
    const res = await fb.dnd.getPathsAsync();
    if (res.success === false) throw new Error(res.error);
    const { paths } = res;
    if (paths.length) {
        await fb2k.invoke('playlist.addPaths', { paths });
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
| `sessionId` | `string` | 关联同一次拖放手势的 `dnd:enter`、`dnd:leave` 与 `dnd:drop` |
| `paths` | `string[]` | 绝对文件系统路径，顺序与 `DataTransfer.files` 一致；被扣留时为空数组 |
| `resolvedPaths` | `(string \| null)[]` | 对应下标的快捷方式目标，或 `null`。长度与 `paths` 恒相等，详见「快捷方式目标」一节 |
| `x`、`y` | `number` | 光标位置，客户区物理像素——除以 `devicePixelRatio` 得 CSS 像素 |
| `keyState` | `number` | drop 时刻的 Win32 `MK_*` 修饰键 / 鼠标键掩码 |
| `source` | `'self' \| 'other-window' \| 'external'` | 拖动从哪里来，见「拖动的来源」一节 |

`dnd:enter` 携带 `sessionId`、`paths`、`resolvedPaths`、`hasFiles`、`source` 与同样
的光标字段；`dnd:leave` 仅携带 `sessionId`。

### 拖动的来源

`dnd:enter`、`dnd:drop` 及 `getPathsAsync` 返回的 `source` 区分当前窗口拖出项与外部拖入文件。若页面已通过 HTML5 原生事件处理了内部拖放，可据此忽略宿主派发的重复文件路径：

| 值 | 拖动从哪里开始 |
| --- | --- |
| `'self'` | 本窗口的页面，带不带 drag token 都算 |
| `'other-window'` | 同一个 foobar2000 的另一个窗口：主窗口或 popup |
| `'external'` | 其余地方：资源管理器、别的程序、另一个 foobar2000 进程，或 Default UI / Columns UI 面板 |

宿主为所承载窗口发起的拖放操作附加内部标记，并在进入目标窗口时读取校验。由于外部程序可构造类似标记，`source` 仅用于辅助判定放置逻辑，不可作为安全鉴权边界。

```javascript
fb.on('dnd:drop', ({ source, paths }) => {
    if (source === 'self') return; // 页面自己的 drop 处理函数已经处理了
    void fb.queue.addPaths(paths);
});
```

### getPaths()

签名：`fb.dnd.getPaths(): string[]`

同步读取快照，因此仅为尽力而为。以下情况返回空数组：无活动会话、拖放未携带
文件列表、origin 不被信任、宿主尚未发布会话——快速拖放可能在发布完成前就抵达
页面的 `drop` 处理函数。该方法建议仅用于乐观 UI 预显，实际业务数据请使用 `getPathsAsync`。

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

拖入快捷方式时，列表里给出的是 `.lnk` 文件本身，foobar2000 无法直接播放该文件。因此上述
每个路径来源均提供对应的解析目标数组：`dnd:enter` / `dnd:drop` 载荷与
`getPathsAsync` 上的 `resolvedPaths`，以及读取快照的 `getResolvedPaths()`。

`paths` **不做任何改写**。其与 `DataTransfer.files` 的下标保持严格一一对应，解析目标与原路径并行提供而非就地替换，由前端业务逻辑按需选用。

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

::: warning 解析目标仅表示快捷方式指向的路径，不保证目标文件依然存在
目标失效（断链）的快捷方式不会返回 `null`。无论目标文件是否存在，Windows API 均直接返回 `.lnk` 中记录的静态路径，因此非 `null` 仅表示该项记录了目标路径。

宿主未在此处进行文件存在性校验，以避免在拖放源线程阻塞等待期间进行高延迟的文件系统探测。页面应在消费路径时处理文件不存在的情况，例如调用 `playlist.addPaths` 时根据返回的 `addedCount` 与 `invalidCount` 确认实际导入数量。
:::

```javascript
const paths = fb.dnd.getPaths();
const targets = fb.dnd.getResolvedPaths();

// 快捷方式播放其目标，其他条目播放自身
const playable = paths.map((path, i) => targets[i] ?? path);
await fb2k.invoke('playlist.addPaths', { paths: playable });
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

签名：`fb.dnd.getCapabilities(): Promise<DndGetCapabilitiesResponse>`

本窗口的拖放集成当前能提供什么。它在窗口生命周期内并非恒定：导航到不同 origin
会收回路径访问权，同时保留 HTML5 拖放事件。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `html5` | `boolean` | 页面仍能收到标准 HTML5 拖放事件 |
| `paths` | `boolean` | 可取到真实文件系统路径 |
| `hosting` | `'visual' \| 'standard'` | 窗口承载 WebView 的方式 |
| `pathsUnavailableReason` | `string` | 仅当 `paths` 为 `false` 时出现 |
| `dragOut` | `boolean` | 可通过 [`prepareDrag`](#preparedrag-paths) 把文件拖出 |
| `dragOutUnavailableReason` | `string` | 仅当 `dragOut` 为 `false` 时出现 |

```javascript
const caps = await fb.dnd.getCapabilities();
if (caps.success === false) throw new Error(caps.error);
if (!caps.paths) {
    console.warn('paths unavailable:', caps.pathsUnavailableReason);
}
```

`pathsUnavailableReason` 取值为 `origin-untrusted`、`inner-target-not-found`、
`forward-unavailable`、`chain-failed`、`register-failed` 之一。

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

反向拖放：允许用户将页面内的可拖拽元素直接拖出至资源管理器或其他外部应用程序，并在放置时提供真实物理文件。页面在拖放过程中不直接暴露或传递文件系统路径，而是预先申请一次性安全凭证（token）；拖放开始时将 token 写入拖放数据，宿主据此关联并注入已通过校验的文件列表。随后拖放行为由浏览器标准拖放流程接管，最终完成状态由页面的 `dragend` 事件接收。

提供这个手势之前，先查 [`getCapabilities()`](#getcapabilities) 的 `dragOut`。

### prepareDrag(paths)

签名：`fb.dnd.prepareDrag(paths: string[]): Promise<DndPrepareDragResponse>`

校验路径，返回供本窗口下一次拖出使用的 token。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 要拖出的位置，至少一条。接受原生路径、`file://` URL、`file-relative://` URL（便携模式下同卷相对路径，由 foobar2000 解析）、`archive://` 与 `unpack://` 虚拟路径，以及 `路径\|subsong:N` 形式（后缀将被剥除并拖出容器文件）；支持传入曲目对象的 `path` 或 `absolutePath` 属性 |

返回 `{ success: true, token }`。token 是不透明的，唯一用途是交给
[`applyDragToken`](#applydragtoken-datatransfer-token)。

::: warning 在拖放开始之前调用，不要在 `dragstart` 里调用
拖放数据载荷仅在 `dragstart` 事件处理函数的同步执行阶段允许写入。在 `dragstart` 中执行 `await` 会导致后续 `setData` 无法写入数据，导致拖放操作丢失文件附件。建议在 `pointerdown`（或 `mousedown`）阶段提前请求并缓存 token，并在 `dragstart` 中同步注入。若在 token 响应返回前即触发拖拽，未持有 token 的操作将回退为普通网页拖放。
:::

token 规则：

- 存活 **30 秒**
- 被第一次使用它的拖放**消费**
- 绑定请求它的**窗口**
- 同一窗口下一次 `prepareDrag` 成功后，旧 token **失效**，即使尚未过期；只有最近一枚有效

违反任一规则的 token 在拖放开始时被拒绝，`dnd:dragEnded` 报 `PERMISSION_DENIED`，
拖放本身作为不带文件的普通网页拖放继续进行。

::: tip 落到目标的永远是物理文件
CUE 表或多曲目音频容器中的单首曲目将导出其宿主容器文件；`archive://` 与 `unpack://` 虚拟路径导出对应的压缩包物理文件。因此多个虚拟子项可能合并为同一物理文件，且容器内的分轨定位信息不会随外部文件传递。
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
| `NOT_FOUND` | 调用窗口没有拖放注册 |
| `ORIGIN_DENIED` | 文档 origin 不受信任，不允许拖出文件。此判定先于任何路径检查 |
| `NOT_SUPPORTED` | 本窗口 `dragOut` 为 `false` |
| `INVALID_PARAMS` | `paths` 缺失、为空，或元素不全是字符串 |
| `INVALID_PATH` | 指定项未对应本地物理文件（如网络流媒体或 `cdda://` 音轨）；`paths[i]` 对应输入数组中的索引 |
| `PERMISSION_DENIED` | 指定项未通过路径安全策略校验；`paths[i]` 对应规范化与去重后路径列表中的索引 |
| `OPERATION_FAILED` | 无法创建 token |

错误信息不含路径本体。

### applyDragToken(dataTransfer, token)

签名：`fb.dnd.applyDragToken(dataTransfer: DataTransfer, token: string): void`

把 token 写进 `dragstart` 事件，让宿主挂上文件。必须在 `dragstart` 处理函数内
**同步**调用。它做两件事，两件都不可少：

1. 将 `text/plain` 设置为 token 载荷（`fb2k-dnd-token/1:` 后接 token），覆盖页面此前设置的内容，宿主据此标识当前拖放操作关联外部文件
2. 把 `effectAllowed` 设为 `'copy'`

::: danger `effectAllowed` 必须恰为 `'copy'`
附加文件列表后，放置操作由目标程序直接执行。当源文件与目标目录位于同一磁盘分区时，Windows 资源管理器默认执行移动操作。为避免源文件被外部程序直接移除，宿主强制要求拖放操作仅允许复制：若 `effectAllowed` 包含比 copy 更宽的权限，宿主将拒绝附加文件列表（触发 `dnd:dragEnded` 并报告 `INVALID_PARAMS`），拖放操作回退为常规网页拖放。未显式设置 `effectAllowed` 相当于声明 `copy | move | link`，同样会导致文件附加失败。在调用 `applyDragToken` 后请勿修改 `effectAllowed`。
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

订阅 `dnd:dragEnded`。该事件**只在宿主拒绝**给一次拖出挂文件时触发，时机是拖放
开始的那一刻。拖放本身不会因此取消，而是作为不带文件的普通网页拖放继续进行，页面内
的放下照常有效；只有 token 文本抹不掉的极少数情况才会取消。返回取消订阅函数。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `result` | `'failed'` | 恒为 `'failed'`；宿主只报告拒绝 |
| `code` | `string` | `PERMISSION_DENIED`（token 未知、过期、已消费、被顶掉或来自其他窗口）、`INVALID_PARAMS`（`effectAllowed` 不是 `'copy'`）、`OPERATION_FAILED`（文件列表挂不上去） |
| `error` | `string` | 可读原因，不含路径 |

有效 token 在检查 `effectAllowed` 和附加文件列表之前就已消费。因此收到 `INVALID_PARAMS` 或 `OPERATION_FAILED` 后，重试前也必须申请新 token。

::: warning 这不是拖放结束通知
宿主成功接受 token 后不发送专门的完成事件。拖放交互遵循标准网页拖放生命周期，最终状态由元素的原生 `dragend` 事件确定：`dataTransfer.dropEffect` 为 `'copy'` 表示目标已接受文件，为 `'none'` 表示操作取消或被目标拒绝。后续清理逻辑应在 `dragend` 中执行；`dnd:dragEnded` 仅用于在拖拽初始化失败时获取未附加文件的原因。
:::

```javascript
const off = fb.dnd.onDragEnded(({ code, error }) => {
    if (code === 'PERMISSION_DENIED') token = null; // 下次重新申请
    console.warn('drag-out refused:', code, error);
});
```

### 拖出期间会发生什么

- 携带有效 token 的拖放若被宿主接管则附加物理文件；若未成功附加，则降级为不含外部文件的常规网页拖放，页面内部放置逻辑仍可正常响应。拖出到资源管理器既不会生成文件，也不会泄露 token 原始文本。若 token 无法清除，该次拖放将被取消。
- 未携带 token 的常规拖放（文本、图片、超链接）不受此机制影响，不会触发 `dnd:dragEnded` 事件。
- 拖放过程中，宿主窗口同步等待放置目标响应，交互行为与 WebView2 原生拖拽完全一致。目标程序响应延迟会导致交互等待，页面无法缩短该系统调用耗时。
- 在 `<iframe>` 里，token 交互复用通用的 `invoke` 通道。安全边界是宿主的 origin 校验，不能依赖 iframe 嵌入关系。

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
if (r.success === false) console.log(r.code); // 'NOT_SUPPORTED'
```

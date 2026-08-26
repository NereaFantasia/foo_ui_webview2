# fb.dnd 拖放

`fb.dnd` 暴露宿主视角的外部文件拖放会话，让页面能取到 HTML5 `File` 对象刻意
隐藏的真实文件系统路径。

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

```javascript
const caps = await fb.dnd.getCapabilities();
if (!caps.paths) {
    console.warn('paths unavailable:', caps.pathsUnavailableReason);
}
```

`pathsUnavailableReason` 取值为 `origin-untrusted`、`inner-target-not-found`、
`forward-unavailable`、`chain-failed`、`displaced`、`register-failed` 之一。

订阅 `dnd:capabilitiesChanged` 以响应变化。其载荷携带同样的 `html5`、`paths`、
`hosting` 与 `pathsUnavailableReason` 字段。

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

## 不支持的能力

### startDrag(type?)

签名：`fb.dnd.startDrag(type?: string): Promise<DndStartDragResponse>`

把曲目从窗口**拖出**到其他应用。未实现：它需要原生 `IDropSource`，本组件不提供。
Promise **会 resolve** 一个 `{ success: false, code: 'NOT_SUPPORTED' }` 信封，而不是
reject——宿主把 handler 返回的错误信封当作正常结果投递，因此应判断 `success`，
不要依赖 `catch`。参数被接受并忽略，以便旧调用点仍能编译。

```javascript
const r = await fb.dnd.startDrag('files');
console.log(r.success, r.code); // false 'NOT_SUPPORTED'
```

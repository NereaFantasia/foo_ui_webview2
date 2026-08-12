# fb.dnd 拖放

`fb.dnd` 暴露宿主视角的外部文件拖放会话，让页面能取到 HTML5 `File` 对象刻意
隐藏的真实文件系统路径。

## 工作原理

Windows 把拖入的文件列表交给原生窗口而非页面，浏览器引擎中 `File.path` 恒为
`null`。本命名空间**不替代** HTML5 拖放：标准的 `dragenter` / `dragover` /
`drop` 事件照原样触发，`fb.dnd` 与它们并行运行，只回答 HTML5 无法回答的那个
问题——文件在磁盘上的真实位置。

宿主把每次拖放记为一个带 id 的**会话**，并在任何 `dnd:*` 监听器运行之前将其
发布到页面的 `window.__fbDndSession`。

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

返回 `{ sessionId, paths }`。会话已过期、未携带文件列表，或 origin 不被信任
时，`paths` 为空。

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
| `x`、`y` | `number` | 光标位置，客户区物理像素——除以 `devicePixelRatio` 得 CSS 像素。 |
| `keyState` | `number` | drop 时刻的 Win32 `MK_*` 修饰键 / 鼠标键掩码。 |

`dnd:enter` 携带 `sessionId`、`paths`、`hasFiles` 与同样的光标字段；
`dnd:leave` 仅携带 `sessionId`。

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

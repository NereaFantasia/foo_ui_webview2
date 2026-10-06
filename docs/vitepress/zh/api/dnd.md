# DnD 拖放 API

外部文件拖入与曲目拖出。

## DnD API - 文件拖入与拖出 (4 个 API)

Windows 把拖入的文件列表交给原生窗口而非页面，且 HTML5 `File` 对象隐藏文件系统
路径，因此本命名空间作为**旁路通道**提供宿主视角的真实路径。它不替代 HTML5 拖放：
`dragenter` / `dragover` / `drop` 照原样触发。

事件：`dnd:enter`、`dnd:leave`、`dnd:drop`、`dnd:capabilitiesChanged`，以及拖出
方向的 `dnd:dragEnded`（见 `dnd.prepareDrag`）。**没有** `dnd:over` 事件——一次拖放
会产生上百次 `DragOver`，逐次发射会淹没 bridge；需要跟踪光标的页面用 HTML5
`dragover`。

**页面按 HTML5 的方式接受放下**：在接收放下的元素上，`dragover` 里调
`preventDefault()`。没有处理函数这样做的地方，宿主替页面拒收：光标显示禁止，松手
什么也不放下，也不发 `dnd:drop`，浏览器也不会自己打开拖进来的文件。文本框保留默认
的文本放下。拖动进入窗口后的头半秒，页面还没答复，光标先显示复制，免得刚进来时闪
一下禁止；这段时间里松手的放下照常送达。

`dnd:enter`、`dnd:drop` 与 `dnd.getPathsAsync` 报告拖动的 `source`：从本窗口的页面
拖出为 `self`，从同一个 foobar2000 的另一个窗口拖出为 `other-window`，其余为
`external`。已经用 HTML5 事件处理了自己拖动的页面，可以据此忽略宿主送来的同一批
文件。宿主给它承载的窗口里开始的拖动加一个标记，据此判断；任何程序都能仿造这个
标记，所以它只是提示，不是安全检查。

反方向——把曲目作为真实文件**拖出**窗口——走 `dnd.prepareDrag`。

### dnd.getCapabilities

<!-- api-schema:begin dnd.getCapabilities -->
报告本窗口的拖放集成当前能提供什么。调用窗口没有拖放注册时以 `NOT_FOUND` 失败。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `html5` | `boolean` | 页面收得到标准的 HTML5 拖放事件；`false` 表示窗口完全没有拖放支持。 |
| `paths` | `boolean` | 能经 `dnd.getPathsAsync` 与 `dnd:*` 事件拿到真实路径。在窗口生命周期内并非恒定：导航或 Chromium 重新注册自己的放置目标都会收回它，由 `dnd:capabilitiesChanged` 通知。 |
| `hosting` | `"visual" \| "standard"` | 窗口承载 WebView 的方式：主窗口与弹出窗口为 `visual`，DUI / CUI 面板为 `standard`。 |
| `pathsUnavailableReason` | `"register-failed" \| "forward-unavailable" \| "inner-target-not-found" \| "chain-failed" \| "displaced" \| "origin-untrusted"` | `paths` 为 `false` 的原因；出现即表示 `paths` 为 `false`。 |
| `dragOut` | `boolean` | 能经 `dnd.prepareDrag` 把文件拖出窗口。与 `paths` 无关。 |
| `dragOutUnavailableReason` | `"not-visual-hosting" \| "runtime-too-old" \| "register-failed"` | `dragOut` 为 `false` 的原因；出现即表示 `dragOut` 为 `false`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

`html5`、`paths`、`dragOut` 三者相互独立：面板模式宿主会失去路径旁路通道，而 HTML5
拖放事件仍然工作（Chromium 自行处理）；`dragOut` 另外依赖 WebView2 运行时。`hosting`
为 `"visual"`（主窗口 / 弹出窗口）或 `"standard"`（DUI / CUI 面板，此时路径不可用）。
`pathsUnavailableReason` 仅当 `paths` 为 `false` 时出现，取值为 `origin-untrusted`、
`inner-target-not-found`、`forward-unavailable`、`chain-failed`、`displaced`、
`register-failed` 之一。

`dragOut` 为 `true` 表示页面可用 `dnd.prepareDrag` 把文件拖出。
`dragOutUnavailableReason` 仅当它为 `false` 时出现：`not-visual-hosting`（面板模式，
拖动源由 Chromium 持有）、`runtime-too-old`（WebView2 运行时早于拖放开始事件，需要
Edge 144 或更新）、`register-failed`。

能力在窗口生命周期内并非恒定：导航到不同 origin 会收回路径访问权，并发射
`dnd:capabilitiesChanged`，载荷携带同样这些字段。

```javascript
const caps = await fb2k.invoke('dnd.getCapabilities');

fb2k.on('dnd:capabilitiesChanged', (data) => {
    dropZoneEl.hidden = !data.paths;
});
```

### dnd.getPathsAsync

<!-- api-schema:begin dnd.getPathsAsync -->
从宿主内存读取一次拖放会话的真实路径，不依赖 `dnd:*` 消息的到达顺序，`await` 之后照样可用。调用窗口没有拖放注册时以 `NOT_FOUND` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `sessionId` | `string` | 否 | 要查询的会话，取自 `dnd:*` 载荷。省略或传空串则查本窗口当前活动或最近结束的会话。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `sessionId` | `string` | 路径所属的会话；找不到会话时为空串，调用方由此区分「没有可报的」与「有会话但没有文件」。 |
| `paths` | `string[]` | 真实路径，顺序同 `DataTransfer.files`；会话过期、没有文件列表或文档 origin 不受信时为空。 |
| `resolvedPaths` | `(string \| null)[]` | `paths` 同下标处 `.lnk` 快捷方式的目标；不是快捷方式、指向 shell 对象而非文件、目标太长读不完整或解析不了时为 `null`，从不为空串。失效的快捷方式仍报它记录的路径。长度恒等于 `paths`。 |
| `source` | `"self" \| "other-window" \| "external"` | 该会话的拖动从哪里来，同 `dnd:enter`；找不到会话时没有这个字段。路径被隐去时也照常报告，因为它不泄露任何路径。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

在 HTML5 `drop` 处理函数中取路径的可靠方式：读宿主会话状态而非推送到页面的快照，
不依赖消息投递顺序。路径顺序与 `DataTransfer.files` 一致，可按下标配对。

`resolvedPaths` 携带快捷方式（`.lnk`）的目标路径，**长度恒等于 `paths`**，因此对一个
数组有效的下标对另一个同样有效；非快捷方式的项为 `null`。它与 `paths` 同时被清空，
不会单独为空。

会话不存在、已过期、未携带文件列表，或 origin 不被信任时，`paths` 为空数组。会话
存储是 per-window 的，仅凭 id 无法读取其他窗口的路径。

```javascript
document.addEventListener('drop', async (event) => {
    event.preventDefault();
    const res = await fb2k.invoke('dnd.getPathsAsync');
    if (res.success === false) throw new Error(res.error);
    const { paths } = res;
    if (paths.length) {
        await fb2k.invoke('playlist.addPaths', { paths });
    }
});
```

### dnd.prepareDrag

<!-- api-schema:begin dnd.prepareDrag -->
用一批路径换一枚一次性 token，供本窗口下一次拖出携带这些文件。参数形状错误最先报告；然后在解析或校验任何路径之前先判定文档 origin（`ORIGIN_DENIED`），再看窗口的 `dragOut` 能力（`NOT_SUPPORTED`），最后每条路径都要能解析成本地文件（`INVALID_PATH`）并通过路径安全检查（`PERMISSION_DENIED`）。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 要拖出的位置：原生路径、`file://` 与 `file-relative://` URL、`archive://` / `unpack://` 项，以及 `路径\|subsong:N`。不检查文件是否存在。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `token` | `string` | 不透明的一次性 token，32 个小写十六进制字符，30 秒内有效，绑定请求它的窗口，下一次成功调用会顶掉它。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

宿主先校验参数形状，再判定文档 origin，然后把路径归一化为原生路径并按与 `queue.addPaths` 相同的方式
校验，通过后才铸 token。拖放真正开始时，宿主凭 token 换回这份已校验的列表——
**拖放时刻不从页面接受任何路径**。

**token 必须在 `dragstart` 之前取得。** 拖放数据存储只在 `dragstart` 处理函数同步
执行期间可写，在里面 `await` 本方法会让随后的 `setData` 静默失败，拖放空着出去。
请在 `pointerdown` 时请求并保存，在 `dragstart` 里同步使用。按下后立刻拖动仍可能
抢在回包之前触发 `dragstart`；此时没有 token，这次拖放就是普通的网页拖放。

token 存活 30 秒、由第一次使用它的拖放消费，并绑定请求它的窗口。同一窗口的下一次
调用成功后，旧 token 失效，即使尚未过期。

**`dragstart` 里必须同时写两样东西：**

1. `e.dataTransfer.setData('text/plain', 'fb2k-dnd-token/1:' + token)`——宿主凭
   这个前缀识别拖出。`text/plain` 槽位因此被占用；宿主在拖放离开前把它置空，文本类
   目标收到的是空串而不是 token。
2. `e.dataTransfer.effectAllowed = 'copy'`——**必设**。文件挂上之后，**移动**由放置
   目标执行（目的地与源同一分区时资源管理器默认就是移动），因此允许效果比 copy
   更宽的拖放，宿主一律不挂文件。不设等于 `copy | move | link`，同样挂不上文件。

**拖放开始时的拒绝通过 `dnd:dragEnded` 通知**，载荷 `{ result: 'failed', code, error }`：
`PERMISSION_DENIED`（token 未知、过期、已消费、被顶掉或来自其他窗口）、
`INVALID_PARAMS`（`effectAllowed` 不是 `'copy'`）、`OPERATION_FAILED`（文件列表
挂不上去）。被拒不会取消拖放：宿主抹掉 token 文本，让它作为不带文件的普通网页拖放
继续进行，所以在页面内放下（比如把专辑拖到播放列表上）照常有效。只有 token 文本
抹不掉时才取消这次拖放。这个事件**不是**拖放结束通知：宿主接受 token 后不再发事件，拖放像普通网页
拖放一样进行（有浏览器拖影），结局在元素自己的 `dragend` 里——
`dataTransfer.dropEffect` 为 `'copy'` 表示目标接收了文件，`'none'` 表示取消或被拒。
拖放进行期间宿主窗口等待放置目标，与普通 HTML5 拖放完全一样。不带 token 前缀的
拖放不受影响，也不产生 `dnd:dragEnded`。

落到目标的永远是物理文件：cue / 多曲目容器里的一条曲目拖走整个容器，`archive://` /
`unpack://` 项拖走整个压缩包，传入的多项可能折叠为一个文件。

错误码：`NOT_FOUND`（调用窗口无 dnd 注册）、`ORIGIN_DENIED`（origin 不受信；在参数形状
错误之后、一切路径检查之前判定）、`NOT_SUPPORTED`（`dragOut` 为 `false`）、`INVALID_PARAMS`（`paths`
缺失、为空或元素非字符串）、`INVALID_PATH`（某项背后无本地文件；`paths[i]` 是传入
数组下标）、`PERMISSION_DENIED`（某项被路径安全策略拒绝；`paths[i]` 是归一化去重后
列表的下标）、`OPERATION_FAILED`（无法创建 token）。错误信息不含路径。
处理函数返回的错误信封使 Promise resolve；通信失败仍可能 reject。
有效 token 在检查允许效果或附加文件之前就已消费，因此拖放开始时失败后，
重试前也必须申请新 token。

```javascript
let token = null;
el.addEventListener('pointerdown', async () => {
    token = null;
    const r = await fb2k.invoke('dnd.prepareDrag', { paths: [trackPath] });
    if (r.success) token = r.token;
});
el.addEventListener('dragstart', (e) => {
    if (!token) return;
    e.dataTransfer.setData('text/plain', 'fb2k-dnd-token/1:' + token);
    e.dataTransfer.effectAllowed = 'copy';
});
el.addEventListener('dragend', (e) => console.log(e.dataTransfer.dropEffect));
fb2k.on('dnd:dragEnded', (p) => console.warn('drag-out refused:', p.code, p.error));
```

### dnd.startDrag

<!-- api-schema:begin dnd.startDrag -->
未实现：总是以 `NOT_SUPPORTED` 失败。请改用 `dnd.prepareDrag`。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

::: tip 注意
拖出功能由 `dnd.prepareDrag` 提供。本方法返回 `{ success: false, code: 'NOT_SUPPORTED' }`。

该 Promise 会 **resolve** 这个信封而不是 reject——宿主把 handler 返回的错误信封当作
正常结果投递，因此应判断 `success`；通信失败仍可能 reject。
:::

## 交互投递与限制

`dnd.getPathsAsync` 与 `dnd.getCapabilities` 都从消息自带的 HWND 解析调用窗口，
因此页面无法读取其他窗口的拖放会话。不受信任的 origin 拿不到路径，而 HTML5 拖放
事件仍然工作，因此应对 `paths` 与 `html5` 分别判断，而不是一并隐藏全部拖放提示。
`dnd.startDrag` 会报告拖出目前的 native 限制，而不是实现 OLE drag source。

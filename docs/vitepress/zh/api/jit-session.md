# JIT Session 会话 API

提供显式 JIT 播放会话与租约控制中枢，支持混合队列调度、原生上下文无缝接管（Adoption）与动态条目预取机制。

::: warning 实验性
本命名空间的方法仍可能变化。`jitQueue` 保持不变；两者共用宿主的执行层，会话期间调用 `jitQueue` 会接管会话。
:::

## 会话如何运行

1. 先订阅 `jitSession:*` 事件，再调用 `jitSession.create`。保管好 `controlToken`：写命令携带它、`sessionId`、所依据的 `revision`，以及自选的 `requestId`。
2. `jitSession.play` 把一个条目作为新尝试开始播放。应答只表示命令已受理：条目是否加入并开始播放由 `jitSession:operationCompleted` 告知，尝试如何结束由 `jitSession:attemptFinished` 告知。
3. 一次尝试开始时没有下一条，`jitSession:needNext` 会请求下一条，用 `jitSession.setNext` 作答。同一方法也用来替换下一条、撤销下一条（不传 `entryId`）或声明队列结束。
4. 页面重载后调用 `jitSession.discover`，`canResume` 为 true 时以 `mode: 'resume'` 调用 `jitSession.claimControl`，播放不会重启。其他页面以 `mode: 'handover'` 加当前的 `ownerEpoch` 取得控制。
5. `jitSession.release` 结束会话：清空缓冲，正在播放的曲目继续解码。

条目以调用方的 `entryId` 标识；条目每开始一次（包括重播），宿主都给一个新的 `attemptId`。事件只发给持有控制的页面，漏掉事件的页面读取 `jitSession.getState`。

## jitSession

### jitSession.getCapabilities

<!-- api-schema:begin jitSession.getCapabilities -->
实验性 API，后续版本可能发生变化。

报告本宿主的 JIT 会话能做什么、不能做什么，让协调方事先避开限制，而不是在播放中才发现

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `protocolVersion` | `integer` | 会话协议的版本 |
| `adoption` | `boolean` | 会话能否经 `jitSession.prepareAdoption` 与 `jitSession.adoptNative` 不打断地接管原生播放。本宿主支持；当前这次播放是否符合条件由 `jitSession.prepareAdoption` 判定 |
| `preciseFailure` | `boolean` | 是否对尝试失败分类（网络、解码）。不支持：foobar2000 不报告结构化的播放错误。尽力而为的替代见 `failureRecheck` |
| `failureRecheck` | `boolean` | 以 `unknown` 结束且未播满 1 秒的尝试之后，是否会发出 `jitSession:attemptRechecked` |
| `backgroundExecution` | `boolean` | 窗口最小化、隐藏到托盘或锁屏期间，控制页面能否继续回答 `jitSession:needNext`。本宿主支持：会话活动期间，它的页面只隐藏、不挂起。关闭或重载页面仍会中断供给，直到有页面重新取得控制 |
| `externalTakeoverDetection` | `"afterTrackStart"` | 何时识别出别处开始的播放：只在那首曲目开始之后，此前完成的操作仍可能被加入 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### jitSession.create

<!-- api-schema:begin jitSession.create -->
实验性 API，后续版本可能发生变化。

创建会话并取得控制。创建本身不改变播放。已有会话（已释放的除外）时以 `CONFLICT` 失败，`details` 里给出它的 `sessionId`，应改用 `jitSession.claimControl` 接管。同一页面以同一 `requestId` 重试时返回第一次的应答

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `requestId` | `string` | 是 | 调用方选定的键，重试时返回第一次的应答不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `sessionId` | `string` | 新会话 |
| `controlToken` | `string` | 写命令必须携带的令牌。由页面保管，不会出现在事件或状态中 |
| `ownerEpoch` | `integer` | 控制代次，控制每转移一次就变化 |
| `revision` | `integer` | 当前状态版本 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### jitSession.discover

<!-- api-schema:begin jitSession.discover -->
实验性 API，后续版本可能发生变化。

查找页面可以恢复或接管的会话，不改变播放，也不授予控制。在原控制页面所在的窗口里重载后的页面，`canResume` 为 true

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `session` | `JitSessionDiscovery \| null` | 会话；没有时为 null |
| `session.sessionId` | `string` | 会话 |
| `session.ownerState` | `"connected" \| "disconnected"` | 持有控制的页面仍在时为 `connected`；它导航离开或关闭后为 `disconnected` |
| `session.ownerEpoch` | `integer` | 当前控制代次，供 `jitSession.claimControl` 使用 |
| `session.canResume` | `boolean` | 调用页面能否以 `mode: 'resume'` 认领 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### jitSession.getState

<!-- api-schema:begin jitSession.getState -->
实验性 API，后续版本可能发生变化。

读取会话的一致快照。快照不含控制令牌

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `sessionId` | `string` | 是 | 要读取的会话不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `state` | `JitSessionSnapshot` | 会话快照 |
| `state.sessionId` | `string` | 会话 |
| `state.revision` | `integer` | 状态版本；会话报告的每次变化都会使它增大 |
| `state.lastEventSeq` | `integer` | 本快照已反映的最后一个事件的 `eventSeq` |
| `state.ownerEpoch` | `integer` | 当前控制代次 |
| `state.ownerState` | `"connected" \| "disconnected" \| "none"` | 控制页面是否仍在 |
| `state.phase` | `"active" \| "stopped" \| "released"` | 会话执行播放时为 `active`，停止后为 `stopped`，`jitSession.release` 或被 `jitQueue` 接管后为 `released`；已接纳的原生上下文尚未结束时被接管，会话保持 `stopped` |
| `state.phaseReason` | `"user" \| "command" \| "external" \| "legacyTakeover" \| null` | 会话最近一次停止或释放的原因；没有时为 null |
| `state.current` | `JitSessionAttempt \| null` | 当前尝试；没有时为 null |
| `state.current.entryId` | `string` | 调用方给的条目标识 |
| `state.current.attemptId` | `string` | 宿主给这次尝试的标识；同一条目重播会得到新的标识 |
| `state.current.title` | `string` | 显示标题 |
| `state.current.status` | `"preparing" \| "playing" \| "ended"` | 开始播放之前为 `preparing`，之后为 `playing`；尝试结束后为 `ended` |
| `state.current.nativeRef` | `string` | 条目播放的原生引用，用于接纳的当前曲目或原生供给；资源地址条目不含此字段 |
| `state.next` | `JitSessionNextEntry \| null` | 下一条；没有时为 null |
| `state.next.entryId` | `string` | 调用方给的条目标识 |
| `state.next.title` | `string` | 显示标题 |
| `state.next.operationId` | `string` | 把它加入缓冲的操作 |
| `state.next.nativeRef` | `string` | 条目播放的原生引用，用于原生供给；资源地址条目不含此字段 |
| `state.supplyState` | `"needed" \| "preparing" \| "ready" \| "exhausted"` | `needed`：没有下一条且未声明结束；`preparing`：下一条的操作未结束；`ready`：下一条已缓冲；`exhausted`：已声明队列结束 |
| `state.neededAfterAttemptId` | `string \| null` | 尚未答复的 `jitSession:needNext` 所针对的尝试；没有时为 null |
| `state.neededRequestKey` | `string \| null` | 尚未答复的供给请求标识；没有时为 null |
| `state.pendingOperations` | `string[]` | 未结束的操作，按受理顺序 |
| `state.nativeContext` | `JitNativeContextSummary` | 用 `jitSession.prepareAdoption` 记录的原生上下文；没有时不含此字段 |
| `state.nativeContext.contextId` | `string` | 上下文 |
| `state.nativeContext.contextRevision` | `integer` | 出现、绑定与上下文状态的版本；每次变化都与会话的 `revision` 一起增大 |
| `state.nativeContext.status` | `"prepared" \| "managed" \| "restoring" \| "restored" \| "conflicted" \| "detached"` | 只有快照、原生播放照常进行时为 `prepared` |
| `state.nativeContext.reason` | `"snapshotExpired" \| "nativeChanged" \| null` | 上下文不再适用的原因；没有时为 null。未接纳超过 30 秒为 `snapshotExpired`，播放、队列、设置或记录的播放列表发生变化为 `nativeChanged` |
| `state.nativeContext.controlMode` | `"playback" \| "recovery"` | 会话可以调度播放时为 `playback`；只允许释放时为 `recovery` |
| `state.nativeContext.sourceNextOffset` | `integer` | 交还上下文时原生播放接续的来源行；来源已耗尽时等于 `sourceCount` |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### jitSession.claimControl

<!-- api-schema:begin jitSession.claimControl -->
实验性 API，后续版本可能发生变化。

不重启正在播放的内容，取得会话的控制。`resume` 用于原控制页面所在窗口里重载后的页面；`handover` 用于其他页面，必须给出当前的 `ownerEpoch`。原控制者的令牌随即失效，它已受理的操作保留。不允许恢复时以 `NOT_OWNER` 失败，`expectedOwnerEpoch` 过期时以 `CONFLICT` 失败

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `sessionId` | `string` | 是 | 要认领的会话不能为空。 |
| `mode` | `"resume" \| "handover"` | 是 | 在控制窗口里重载后用 `resume`，其他页面用 `handover` |
| `expectedOwnerEpoch` | `integer` | 否 | 要接管的 `ownerEpoch`；`handover` 时必填不小于 `0`。 |
| `requestId` | `string` | 是 | 调用方选定的键，重试时返回第一次的应答不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `controlToken` | `string` | 新的控制令牌；原控制者的令牌不再有效 |
| `ownerEpoch` | `integer` | 新的控制代次 |
| `revision` | `integer` | 当前状态版本 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### jitSession.play

<!-- api-schema:begin jitSession.play -->
实验性 API，后续版本可能发生变化。

立即把一个条目作为新尝试播放。应答表示命令已受理，不表示资源能播：是否加入并开始播放由 `jitSession:operationCompleted` 报告，尝试如何结束由 `jitSession:attemptFinished` 报告。原生播放队列里的曲目保留，这次尝试结束后先播它们。写命令在令牌不属于当前控制者时以 `NOT_OWNER` 失败，在 `expectedRevision` 过期或 `requestId` 已用于别的命令时以 `CONFLICT` 失败

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `sessionId` | `string` | 是 | 要操作的会话不能为空。 |
| `controlToken` | `string` | 是 | 来自 `jitSession.create` 或 `jitSession.claimControl` 的控制令牌不能为空。 |
| `expectedRevision` | `integer` | 是 | 命令所依据的 `revision`不小于 `0`。 |
| `requestId` | `string` | 是 | 调用方选定的键，重试时返回第一次的应答而不是再执行一次不能为空。 |
| `entryId` | `string` | 是 | 调用方给这次出现的标识；同一资源重复入队时使用不同的标识不能为空。 |
| `title` | `string` | 否 | 显示标题默认 `""`。 |
| `location` | `string` | 是 | 要播放的资源：文件路径或 `file://` URL、表示单个子曲目的 `path\|subsong:N`，或 `http(s)://` 流。展开出多于一首曲目的播放列表文件会使操作失败，CUE 地址必须明确指定子曲目。最多 2048 字符不能为空。 |
| `expiresAt` | `number` | 否 | 资源地址的失效时刻，Unix 毫秒；只做记录 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operationId` | `string` | 加入并开始播放该条目的操作 |
| `attemptId` | `string` | 本命令开始的尝试 |
| `revision` | `integer` | 受理命令之后的状态版本 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### jitSession.setNext

<!-- api-schema:begin jitSession.setNext -->
实验性 API，后续版本可能发生变化。

设置、替换或撤销当前尝试之后的条目。只在 `afterAttemptId` 仍是当前尝试时受理。被替换或撤销的下一条从缓冲中移除，它未结束的操作以 `superseded` 结束；当前尝试继续播放。不传 `entryId` 即撤销下一条；`endOfQueue: true` 同时声明之后没有更多条目。当前尝试已经结束、会话正在等下一条时，新条目立即开始播放。当前曲目距结尾不到输出缓冲长度加 1 秒时，foobar2000 可能已经选定已缓冲的下一条接着播放，此时撤销或替换它以 `CONFLICT` 失败，`details.reason` 为 `nextPreselected`：状态不变，已缓冲的下一条照常接着播放。等它开始后，再以它的尝试为准设置之后的条目。没有已缓冲的下一条时，设置下一条照常受理

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `sessionId` | `string` | 是 | 要操作的会话不能为空。 |
| `controlToken` | `string` | 是 | 来自 `jitSession.create` 或 `jitSession.claimControl` 的控制令牌不能为空。 |
| `expectedRevision` | `integer` | 是 | 命令所依据的 `revision`不小于 `0`。 |
| `requestId` | `string` | 是 | 调用方选定的键，重试时返回第一次的应答而不是再执行一次不能为空。 |
| `afterAttemptId` | `string` | 是 | 命令所针对的当前尝试不能为空。 |
| `entryId` | `string` | 否 | 下一条的标识；不传即撤销下一条 |
| `title` | `string` | 否 | 显示标题 |
| `location` | `string` | 否 | 下一条的资源，写法同 `jitSession.play`；传 `entryId` 时必填 |
| `expiresAt` | `number` | 否 | 资源地址的失效时刻，Unix 毫秒；只做记录 |
| `endOfQueue` | `boolean` | 否 | 声明之后没有更多条目。不能与 `entryId` 同时使用默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operationId` | `string` | 加入下一条的操作；撤销下一条时不含 |
| `revision` | `integer` | 受理命令之后的状态版本 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### jitSession.skip

<!-- api-schema:begin jitSession.skip -->
实验性 API，后续版本可能发生变化。

结束当前尝试，播放已缓冲的下一条，不动原生播放队列。只在 `attemptId` 仍是当前尝试时受理。下一条尚未就绪时以 `NO_ACTIVE_ITEM` 拒绝，当前尝试继续播放，版本号不变，也不创建操作

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `sessionId` | `string` | 是 | 要操作的会话不能为空。 |
| `controlToken` | `string` | 是 | 来自 `jitSession.create` 或 `jitSession.claimControl` 的控制令牌不能为空。 |
| `expectedRevision` | `integer` | 是 | 命令所依据的 `revision`不小于 `0`。 |
| `requestId` | `string` | 是 | 调用方选定的键，重试时返回第一次的应答而不是再执行一次不能为空。 |
| `attemptId` | `string` | 是 | 要结束的当前尝试不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operationId` | `string` | 起播已缓冲下一条的操作 |
| `revision` | `integer` | 受理命令之后的状态版本 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### jitSession.stop

<!-- api-schema:begin jitSession.stop -->
实验性 API，后续版本可能发生变化。

停止播放并取代未结束的操作。会话保留为 `stopped`，可以读取状态与结果；之后的 `jitSession.play` 开始新的尝试。`jitSession.adoptNative` 之后，剩余的原生队列出现会交还，但不开始播放；交还失败记录在 `nativeContext` 中

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `sessionId` | `string` | 是 | 要操作的会话不能为空。 |
| `controlToken` | `string` | 是 | 来自 `jitSession.create` 或 `jitSession.claimControl` 的控制令牌不能为空。 |
| `expectedRevision` | `integer` | 是 | 命令所依据的 `revision`不小于 `0`。 |
| `requestId` | `string` | 是 | 调用方选定的键，重试时返回第一次的应答而不是再执行一次不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `revision` | `integer` | 受理命令之后的状态版本 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### jitSession.getOperation

<!-- api-schema:begin jitSession.getOperation -->
实验性 API，后续版本可能发生变化。

读取一个操作的状态。可以读到会话的全部未结束操作与最近 64 个已结束的操作；会话释放后保留到下一次 `jitSession.create` 或十分钟，以先到者为准。记录过期后以 `NOT_FOUND` 失败

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `sessionId` | `string` | 是 | 操作所属的会话不能为空。 |
| `operationId` | `string` | 是 | 要读取的操作不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operation` | `JitSessionOperation` | 操作 |
| `operation.operationId` | `string` | 操作 |
| `operation.kind` | `"play" \| "setNext" \| "skip"` | 创建它的命令 |
| `operation.entryId` | `string` | 它加入的条目 |
| `operation.attemptId` | `string \| null` | play 或 skip 开始的尝试；准备下一条时为 null |
| `operation.status` | `"pending" \| "completed" \| "failed" \| "superseded"` | `pending`，或终态：`completed`（已加入，`play` 还已开始播放）、`failed`，或被之后的命令 `superseded` |
| `operation.reason` | `string \| null` | 失败或被取代的原因；没有时为 null |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### jitSession.release

<!-- api-schema:begin jitSession.release -->
实验性 API，后续版本可能发生变化。

结束会话并归还原生上下文：清空缓冲，正在播放的曲目继续解码，恢复会话首次播放时保存的播放顺序与活动播放列表。`jitSession.adoptNative` 之后，先交还剩余的原生队列出现（见 `nativeDisposition`）；交还失败时会话保留，错误带有 `details.contextId` 与 `details.recoveryId`，以新的 `requestId` 再次释放会从中断处继续。foobar2000 可能已经选定已缓冲的下一条时（见 `jitSession.setNext`），交还上下文以 `CONFLICT` 失败，`details.reason` 为 `nextPreselected`，状态不变；`nativeDisposition: "detach"` 照常受理。没有接管上下文时，这样选定的下一条在释放后仍可能播放

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `sessionId` | `string` | 是 | 要操作的会话不能为空。 |
| `controlToken` | `string` | 是 | 来自 `jitSession.create` 或 `jitSession.claimControl` 的控制令牌不能为空。 |
| `expectedRevision` | `integer` | 是 | 命令所依据的 `revision`不小于 `0`。 |
| `requestId` | `string` | 是 | 调用方选定的键，重试时返回第一次的应答而不是再执行一次不能为空。 |
| `nativeDisposition` | `"restore" \| "detach"` | 否 | 如何处理已接纳的原生上下文：`restore`（默认）把剩余的出现交还原生队列并接着播放来源；`detach` 不动原生队列与播放列表，出现仍可读取。只用于接纳了原生播放的会话 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### jitSession.prepareAdoption

<!-- api-schema:begin jitSession.prepareAdoption -->
实验性 API，后续版本可能发生变化。

为接管记录原生播放：正在播放的曲目、原生播放队列的逐次出现（重复曲目分开记录），以及播放所在的播放列表。播放、队列、播放列表与设置都不改变；出现与来源行用 `jitSession.getNativeContext` 读取。会话必须是新的：已播放过、`jitQueue` 缓冲正在使用或已接管的上下文尚未结束时以 `CONFLICT` 失败。再次调用会替换尚未接纳的快照。除非播放正在进行、未暂停、来自普通播放列表（不是自动播放列表或其他被锁定的列表）的一行，播放顺序为 Default，光标跟随、播放跟随与“播完当前停止”均关闭，且队列不超过 1024 首、各项位置与所在列表一致，否则以 `NOT_SUPPORTED` 失败，`details.reason` 指出第一个不满足的条件。快照由 `jitSession.adoptNative` 接纳

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `sessionId` | `string` | 是 | 要操作的会话不能为空。 |
| `controlToken` | `string` | 是 | 来自 `jitSession.create` 或 `jitSession.claimControl` 的控制令牌不能为空。 |
| `expectedRevision` | `integer` | 是 | 命令所依据的 `revision`不小于 `0`。 |
| `requestId` | `string` | 是 | 调用方选定的键，重试时返回第一次的应答而不是再执行一次不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `contextId` | `string` | 原生上下文 |
| `snapshotToken` | `string` | 授权接纳这次快照，只能用一次。由页面保管，不会出现在事件或状态中 |
| `expiresAt` | `number` | 快照未被接纳时的过期时刻，Unix 毫秒；宿主按自己的单调时钟计时 |
| `revision` | `integer` | 受理命令之后的状态版本 |
| `contextRevision` | `integer` | 上下文的版本 |
| `current` | `JitNativeTrack` | 正在播放的曲目；它的 `nativeRef` 与所在来源行的相同 |
| `current.nativeRef` | `string` | 宿主给这个出现或来源行的引用，只在所属上下文内有效 |
| `current.path` | `string` | 曲目的 foobar2000 路径 |
| `current.subsong` | `integer` | 文件内的子曲目序号 |
| `current.title` | `string` | 标题标签；没有时为文件名 |
| `current.playlistGuid` | `string \| null` | 曲目所在的播放列表；没有播放列表位置的队列项为 null |
| `current.row` | `integer \| null` | 在该播放列表中的行；没有时为 null |
| `manualCount` | `integer` | 从原生播放队列记录的出现数 |
| `sourceCount` | `integer` | 正在播放的播放列表的行数 |
| `sourceNextOffset` | `integer` | 正在播放的曲目之后的来源行 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### jitSession.getNativeContext

<!-- api-schema:begin jitSession.getNativeContext -->
实验性 API，后续版本可能发生变化。

读取原生上下文的一页：从原生队列记录的 `manual` 出现（按原顺序），或正在播放的播放列表的 `source` 行。读取不改变两个版本号；同一行再次读取保持同一个 `nativeRef`。`expectedContextRevision` 过期时以 `CONFLICT` 失败；读 `source` 页时，播放列表内容自快照以来变过也以 `CONFLICT` 失败。超出末尾的页为空

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `sessionId` | `string` | 是 | 上下文所属的会话不能为空。 |
| `contextId` | `string` | 是 | 要读取的上下文不能为空。 |
| `expectedContextRevision` | `integer` | 是 | 页面所依据的 `contextRevision`不小于 `0`。 |
| `collection` | `"manual" \| "source"` | 是 | `manual` 读取队列出现，`source` 读取播放列表的行 |
| `offset` | `integer` | 是 | 第一项：`manual` 为出现序号，`source` 为播放列表的行不小于 `0`。 |
| `limit` | `integer` | 是 | 每页项数取值 `1` 到 `128`（含端点）。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `context` | `JitNativeContextDetail` | 上下文，附带记录时的当前曲目与各项数量 |
| `context.contextId` | `string` | 上下文 |
| `context.contextRevision` | `integer` | 上下文的版本 |
| `context.status` | `"prepared" \| "managed" \| "restoring" \| "restored" \| "conflicted" \| "detached"` | 状态，同状态快照 |
| `context.reason` | `"snapshotExpired" \| "nativeChanged" \| null` | 上下文不再适用的原因，同状态快照；没有时为 null |
| `context.controlMode` | `"playback" \| "recovery"` | 控制模式，同状态快照 |
| `context.sourceNextOffset` | `integer` | 交还上下文时原生播放接续的来源行 |
| `context.current` | `JitNativeTrack` | 快照时正在播放的曲目 |
| `context.current.nativeRef` | `string` | 宿主给这个出现或来源行的引用，只在所属上下文内有效 |
| `context.current.path` | `string` | 曲目的 foobar2000 路径 |
| `context.current.subsong` | `integer` | 文件内的子曲目序号 |
| `context.current.title` | `string` | 标题标签；没有时为文件名 |
| `context.current.playlistGuid` | `string \| null` | 曲目所在的播放列表；没有播放列表位置的队列项为 null |
| `context.current.row` | `integer \| null` | 在该播放列表中的行；没有时为 null |
| `context.manualCount` | `integer` | 从原生播放队列记录的出现数 |
| `context.sourceCount` | `integer` | 快照时正在播放的播放列表的行数 |
| `total` | `integer` | 该集合的总项数 |
| `items` | `JitNativeEntry[]` | 本页内容 |
| `items[].nativeRef` | `string` | 宿主给的引用，只在本上下文内有效 |
| `items[].entryId` | `string \| null` | 绑定到这个引用的调用方条目；供给之前为 null |
| `items[].path` | `string` | 曲目的 foobar2000 路径 |
| `items[].subsong` | `integer` | 文件内的子曲目序号 |
| `items[].title` | `string` | 标题标签；没有时为文件名 |
| `items[].playlistGuid` | `string \| null` | 曲目所在的播放列表；没有播放列表位置的队列项为 null |
| `items[].row` | `integer \| null` | 在该播放列表中的行；没有时为 null |
| `items[].occurrenceId` | `string \| null` | `manual` 项的出现标识；来源行为 null |
| `items[].disposition` | `"pending" \| "queued" \| "submitted" \| "consumed" \| "discarded" \| "indeterminate" \| "restored" \| null` | `manual` 出现的去向：`pending` 尚未交给播放，`queued` 作为下一首在原生队列中，`submitted` 已被播放取走、起播未确认，`consumed` 已开始播放，`discarded` 被调用方删除，`indeterminate` 已交出但结局未知，`restored` 已交还原生队列。来源行为 null |
| `items[].restorationRank` | `integer \| null` | 在待交还出现中的位置，从 0 起；不交还时为 null |
| `nextOffset` | `integer \| null` | 下一页的起点；最后一页为 null |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### jitSession.adoptNative

<!-- api-schema:begin jitSession.adoptNative -->
实验性 API，后续版本可能发生变化。

不打断地接纳 `jitSession.prepareAdoption` 记录的原生播放。正在播放的曲目成为当前尝试，绑定 `currentEntryId`，继续解码；原生播放队列被清空，其中的出现由宿主保存，之后播什么由会话调度（`jitSession:needNext` 会来索取）。停止或释放会话时，剩余的出现交还原生队列，并从来源中最后播放的一行之后接着播放。来源在那一行之后已无内容时，最后一个出现放进宿主为此保留的单曲列表（`__webview_restore__`），播完即停，不沿它原来所在的列表继续。上下文未结束时调用 `jitQueue` 方法，它照常执行；会话随后保持 `stopped`，原因为 `legacyTakeover`，保留控制令牌，只接受 `jitSession.release`。快照已被替换、过期或已用过，或者原生播放自快照以来有变化时以 `CONFLICT` 失败；设置已不允许接管时以 `NOT_SUPPORTED` 失败；失败时不改变任何状态

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `sessionId` | `string` | 是 | 要操作的会话不能为空。 |
| `controlToken` | `string` | 是 | 来自 `jitSession.create` 或 `jitSession.claimControl` 的控制令牌不能为空。 |
| `expectedRevision` | `integer` | 是 | 命令所依据的 `revision`不小于 `0`。 |
| `requestId` | `string` | 是 | 调用方选定的键，重试时返回第一次的应答而不是再执行一次不能为空。 |
| `contextId` | `string` | 是 | `jitSession.prepareAdoption` 返回的上下文不能为空。 |
| `snapshotToken` | `string` | 是 | 与之一同返回的快照令牌不能为空。 |
| `currentEntryId` | `string` | 是 | 调用方给正在播放的曲目的条目标识不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `contextId` | `string` | 上下文，此后由会话管理 |
| `attemptId` | `string` | 给正在播放的曲目的尝试 |
| `revision` | `integer` | 受理命令之后的状态版本 |
| `contextRevision` | `integer` | 上下文的版本 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### jitSession.playNative

<!-- api-schema:begin jitSession.playNative -->
实验性 API，后续版本可能发生变化。

立即播放已接纳上下文中的一个来源行或队列出现，与 `jitSession.play` 播放资源地址相同：应答表示已受理，起播结果由 `jitSession:operationCompleted` 报告。某个 `nativeRef` 第一次供给时绑定到 `entryId`；重播沿用该 `entryId`，换用别的会以 `CONFLICT` 失败。队列出现一旦开始播放即被消费，之后不再交还；重播已消费或已删除的出现不会让它重新回到待交还之列。播放来源行会改变交还上下文时来源接续的位置

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `sessionId` | `string` | 是 | 要操作的会话不能为空。 |
| `controlToken` | `string` | 是 | 来自 `jitSession.create` 或 `jitSession.claimControl` 的控制令牌不能为空。 |
| `expectedRevision` | `integer` | 是 | 命令所依据的 `revision`不小于 `0`。 |
| `requestId` | `string` | 是 | 调用方选定的键，重试时返回第一次的应答而不是再执行一次不能为空。 |
| `contextId` | `string` | 是 | 已接纳的上下文不能为空。 |
| `nativeRef` | `string` | 是 | 从 `jitSession.getNativeContext` 读到的 `nativeRef`不能为空。 |
| `entryId` | `string` | 是 | 调用方给这个条目的标识不能为空。 |
| `title` | `string` | 否 | 显示标题；不传时用曲目的标题标签 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operationId` | `string` | 加入并开始播放该条目的操作 |
| `attemptId` | `string` | 本命令开始的尝试 |
| `revision` | `integer` | 受理命令之后的状态版本 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### jitSession.setNativeNext

<!-- api-schema:begin jitSession.setNativeNext -->
实验性 API，后续版本可能发生变化。

把已接纳上下文中的一个来源行或队列出现设为当前尝试之后的条目，与 `jitSession.setNext` 设置资源地址相同。绑定规则同 `jitSession.playNative`；撤销时使用不带 `entryId` 的 `jitSession.setNext`。替换已缓冲的下一条时，与 `jitSession.setNext` 在同样条件下以 `nextPreselected` 失败

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `sessionId` | `string` | 是 | 要操作的会话不能为空。 |
| `controlToken` | `string` | 是 | 来自 `jitSession.create` 或 `jitSession.claimControl` 的控制令牌不能为空。 |
| `expectedRevision` | `integer` | 是 | 命令所依据的 `revision`不小于 `0`。 |
| `requestId` | `string` | 是 | 调用方选定的键，重试时返回第一次的应答而不是再执行一次不能为空。 |
| `contextId` | `string` | 是 | 已接纳的上下文不能为空。 |
| `afterAttemptId` | `string` | 是 | 命令所针对的当前尝试不能为空。 |
| `nativeRef` | `string` | 是 | 从 `jitSession.getNativeContext` 读到的 `nativeRef`不能为空。 |
| `entryId` | `string` | 是 | 调用方给这个条目的标识不能为空。 |
| `title` | `string` | 否 | 显示标题；不传时用曲目的标题标签 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operationId` | `string` | 加入下一条的操作；撤销下一条时不含 |
| `revision` | `integer` | 受理命令之后的状态版本 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### jitSession.setNativeQueue

<!-- api-schema:begin jitSession.setNativeQueue -->
实验性 API，后续版本可能发生变化。

决定已接纳上下文中哪些队列出现仍待交还以及顺序：列出要保留的每个 pending 或 queued 出现；未列出的被删除，不再交还。被删除的出现若正是下一条，会从缓冲中撤销。标识重复或不认识时以 `INVALID_PARAMS` 失败；`expectedContextRevision` 过期，或列出的出现此间已交给播放时以 `CONFLICT` 失败；在 `jitSession.setNext` 所述条件下删除已缓冲的下一条，以 `CONFLICT` 失败，`details.reason` 为 `nextPreselected`；失败时不改变任何状态

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `sessionId` | `string` | 是 | 要操作的会话不能为空。 |
| `controlToken` | `string` | 是 | 来自 `jitSession.create` 或 `jitSession.claimControl` 的控制令牌不能为空。 |
| `expectedRevision` | `integer` | 是 | 命令所依据的 `revision`不小于 `0`。 |
| `requestId` | `string` | 是 | 调用方选定的键，重试时返回第一次的应答而不是再执行一次不能为空。 |
| `contextId` | `string` | 是 | 已接纳的上下文不能为空。 |
| `expectedContextRevision` | `integer` | 是 | 命令所依据的 `contextRevision`不小于 `0`。 |
| `remainingOccurrenceIds` | `string[]` | 是 | 要保留的出现，按交还的顺序 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `revision` | `integer` | 受理命令之后的状态版本 |
| `contextRevision` | `integer` | 变化之后的上下文版本 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## JIT Session 事件

### jitSession:stateChanged

<!-- api-schema:begin event:jitSession:stateChanged -->
会话状态变化：尝试开始或结束、操作结束、供给或控制变化。携带完整快照。只发给当前控制页面；漏掉事件的页面应读取 `jitSession.getState`

发给订阅或任务所属的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `sessionId` | `string` | 会话 |
| `eventSeq` | `integer` | 本事件在会话内的顺序；消费方丢弃已处理过的事件 |
| `revision` | `integer` | 本事件反映的状态版本 |
| `state` | `JitSessionSnapshot` | 变化之后的快照 |
| `state.sessionId` | `string` | 会话 |
| `state.revision` | `integer` | 状态版本；会话报告的每次变化都会使它增大 |
| `state.lastEventSeq` | `integer` | 本快照已反映的最后一个事件的 `eventSeq` |
| `state.ownerEpoch` | `integer` | 当前控制代次 |
| `state.ownerState` | `"connected" \| "disconnected" \| "none"` | 控制页面是否仍在 |
| `state.phase` | `"active" \| "stopped" \| "released"` | 会话执行播放时为 `active`，停止后为 `stopped`，`jitSession.release` 或被 `jitQueue` 接管后为 `released`；已接纳的原生上下文尚未结束时被接管，会话保持 `stopped` |
| `state.phaseReason` | `"user" \| "command" \| "external" \| "legacyTakeover" \| null` | 会话最近一次停止或释放的原因；没有时为 null |
| `state.current` | `JitSessionAttempt \| null` | 当前尝试；没有时为 null |
| `state.current.entryId` | `string` | 调用方给的条目标识 |
| `state.current.attemptId` | `string` | 宿主给这次尝试的标识；同一条目重播会得到新的标识 |
| `state.current.title` | `string` | 显示标题 |
| `state.current.status` | `"preparing" \| "playing" \| "ended"` | 开始播放之前为 `preparing`，之后为 `playing`；尝试结束后为 `ended` |
| `state.current.nativeRef` | `string` | 条目播放的原生引用，用于接纳的当前曲目或原生供给；资源地址条目不含此字段 |
| `state.next` | `JitSessionNextEntry \| null` | 下一条；没有时为 null |
| `state.next.entryId` | `string` | 调用方给的条目标识 |
| `state.next.title` | `string` | 显示标题 |
| `state.next.operationId` | `string` | 把它加入缓冲的操作 |
| `state.next.nativeRef` | `string` | 条目播放的原生引用，用于原生供给；资源地址条目不含此字段 |
| `state.supplyState` | `"needed" \| "preparing" \| "ready" \| "exhausted"` | `needed`：没有下一条且未声明结束；`preparing`：下一条的操作未结束；`ready`：下一条已缓冲；`exhausted`：已声明队列结束 |
| `state.neededAfterAttemptId` | `string \| null` | 尚未答复的 `jitSession:needNext` 所针对的尝试；没有时为 null |
| `state.neededRequestKey` | `string \| null` | 尚未答复的供给请求标识；没有时为 null |
| `state.pendingOperations` | `string[]` | 未结束的操作，按受理顺序 |
| `state.nativeContext` | `JitNativeContextSummary` | 用 `jitSession.prepareAdoption` 记录的原生上下文；没有时不含此字段 |
| `state.nativeContext.contextId` | `string` | 上下文 |
| `state.nativeContext.contextRevision` | `integer` | 出现、绑定与上下文状态的版本；每次变化都与会话的 `revision` 一起增大 |
| `state.nativeContext.status` | `"prepared" \| "managed" \| "restoring" \| "restored" \| "conflicted" \| "detached"` | 只有快照、原生播放照常进行时为 `prepared` |
| `state.nativeContext.reason` | `"snapshotExpired" \| "nativeChanged" \| null` | 上下文不再适用的原因；没有时为 null。未接纳超过 30 秒为 `snapshotExpired`，播放、队列、设置或记录的播放列表发生变化为 `nativeChanged` |
| `state.nativeContext.controlMode` | `"playback" \| "recovery"` | 会话可以调度播放时为 `playback`；只允许释放时为 `recovery` |
| `state.nativeContext.sourceNextOffset` | `integer` | 交还上下文时原生播放接续的来源行；来源已耗尽时等于 `sourceCount` |
<!-- api-schema:end -->

### jitSession:needNext

<!-- api-schema:begin event:jitSession:needNext -->
会话需要下一条：一次尝试开始时缓冲里没有下一条，也没有声明队列结束。用 `jitSession.setNext` 作答。未答复的请求在快照的 `neededAfterAttemptId` 中可以读回。投递范围同 `jitSession:stateChanged`

发给订阅或任务所属的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `sessionId` | `string` | 会话 |
| `eventSeq` | `integer` | 本事件在会话内的顺序；消费方丢弃已处理过的事件 |
| `revision` | `integer` | 本事件反映的状态版本 |
| `afterAttemptId` | `string` | 没有下一条的那次尝试 |
| `requestKey` | `string` | 本次供给请求的标识，也可以从快照读回 |
<!-- api-schema:end -->

### jitSession:operationCompleted

<!-- api-schema:begin event:jitSession:operationCompleted -->
一个操作到达终态。每个操作只发一次。投递范围同 `jitSession:stateChanged`

发给订阅或任务所属的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `sessionId` | `string` | 会话 |
| `eventSeq` | `integer` | 本事件在会话内的顺序；消费方丢弃已处理过的事件 |
| `revision` | `integer` | 本事件反映的状态版本 |
| `operation` | `JitSessionOperation` | 处于终态的操作 |
| `operation.operationId` | `string` | 操作 |
| `operation.kind` | `"play" \| "setNext" \| "skip"` | 创建它的命令 |
| `operation.entryId` | `string` | 它加入的条目 |
| `operation.attemptId` | `string \| null` | play 或 skip 开始的尝试；准备下一条时为 null |
| `operation.status` | `"pending" \| "completed" \| "failed" \| "superseded"` | `pending`，或终态：`completed`（已加入，`play` 还已开始播放）、`failed`，或被之后的命令 `superseded` |
| `operation.reason` | `string \| null` | 失败或被取代的原因；没有时为 null |
<!-- api-schema:end -->

### jitSession:attemptFinished

<!-- api-schema:begin event:jitSession:attemptFinished -->
一次尝试结束。只有播放到达资源末尾时 `outcome` 才是 `completed`；宿主无法确认原因的结束报 `unknown`，并附上已知的事实。投递范围同 `jitSession:stateChanged`

发给订阅或任务所属的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `sessionId` | `string` | 会话 |
| `eventSeq` | `integer` | 本事件在会话内的顺序；消费方丢弃已处理过的事件 |
| `revision` | `integer` | 本事件反映的状态版本 |
| `attemptId` | `string` | 结束的尝试 |
| `entryId` | `string` | 它的条目 |
| `outcome` | `"completed" \| "stopped" \| "superseded" \| "released" \| "unknown"` | `completed`：播放到达末尾；`stopped`：被用户或 `jitSession.stop` 停止；`superseded`：另一次尝试或别的播放开始了；`released`：会话结束；`unknown`：宿主无法确认原因 |
| `stopReason` | `"user" \| "eof" \| "startingAnother" \| "shuttingDown" \| null` | 已观察到的原生停止原因；由命令或归属变化结束时为 null |
| `position` | `number \| null` | 这次尝试最后读到的播放位置，单位秒；没有时为 null |
| `duration` | `number \| null` | 资源时长，单位秒；未知时为 null |
<!-- api-schema:end -->

### jitSession:attemptRechecked

<!-- api-schema:begin event:jitSession:attemptRechecked -->
尝试以 `unknown` 结束且未播满 1 秒时，宿主重新打开它的资源，报告这次复查遇到的情况。它反映的是复查时资源的状态，不一定是原来那次失败的原因：瞬时的网络故障可能不再出现，一次性地址第二次打开可能以别的原因失败。在该尝试的 `jitSession:attemptFinished` 之后发出，网络错误时最多晚约十秒；其他结束方式不发，宿主在此期间退出也不发。它不改变会话状态，也不保留在快照里，页面错过就读不回来。投递范围同 `jitSession:stateChanged`

发给订阅或任务所属的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `sessionId` | `string` | 会话 |
| `eventSeq` | `integer` | 本事件在会话内的顺序；消费方丢弃已处理过的事件 |
| `revision` | `integer` | 本事件反映的状态版本 |
| `attemptId` | `string` | 被复查资源的尝试 |
| `entryId` | `string` | 它的条目 |
| `result` | `"reproduced" \| "notReproduced"` | `reproduced`：复查同样失败；`notReproduced`：这次资源能打开并解码出第一块 |
| `reason` | `"accessDenied" \| "notFound" \| "unsupportedContent" \| "network" \| "other" \| null` | 复查失败的原因，未失败时为 null：`accessDenied`（如 HTTP 403）、`notFound`（HTTP 404 或文件不存在）、`unsupportedContent`（不是音频，或没有已安装的解码器能读）、`network`（其他 HTTP 错误、连接被拒或超时）、`other` |
| `stage` | `"open" \| "initialize" \| "decode" \| null` | 复查在哪一步失败：打开资源、初始化解码器或解码第一块；未失败时为 null |
| `engineReported` | `boolean` | 最近一分钟内，foobar2000 是否记录过指明该资源的播放失败，例如无法打开它 |
<!-- api-schema:end -->

### jitSession:controlChanged

<!-- api-schema:begin event:jitSession:controlChanged -->
会话的控制发生变化：被其他页面认领、被 `jitQueue` 调用接管，或被释放。发给新的与原来的控制页面

发给订阅或任务所属的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `sessionId` | `string` | 会话 |
| `eventSeq` | `integer` | 本事件在会话内的顺序；消费方丢弃已处理过的事件 |
| `revision` | `integer` | 本事件反映的状态版本 |
| `ownerEpoch` | `integer` | 变化之后的控制代次 |
| `reason` | `"claimed" \| "legacyTakeover" \| "released"` | 被其他页面认领（`claimed`）、被 `jitQueue` 调用接管（`legacyTakeover`），或已释放（`released`） |
<!-- api-schema:end -->


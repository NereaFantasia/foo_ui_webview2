# 更新日志

## v1.13.0 (2026-08-26)

::: warning 本版的破坏性变更
以下四项可能需要修改代码：

- **`library.search` 不再把行数组重复一份 `items`。** 响应此前把同一批行以 `items` 与 `tracks` 两个键各带一份，内容逐字节相同，现在只发 `tracks`——与 `library.query` 一直使用的键一致。读 `result.items` 的代码请改读 `result.tracks`；新旧宿主的每一种响应形状都带 `tracks`，因此先读 `tracks` 的代码在仍发送两份的旧宿主上同样成立。带行数据的 search 响应载荷就此减半。
- **`dialog.confirm` 现在 resolve `{ response }`，而不是 `{ confirmed }`。** 原先声明的类型是错的：宿主一直返回的是所点按钮在 `buttons` 中的零基索引，从未返回过 `confirmed` 标志，读 `result.confirmed` 的 TypeScript 代码读的是从未被填充过的字段。默认按钮集为 `['OK', 'Cancel']`，因此 `0` 表示确认、`1` 表示取消。任务对话框创建时未带 `TDF_ALLOW_DIALOG_CANCELLATION`，所以 Esc 与关闭按钮都关不掉它，每个结果都来自一次真实的按钮点击；`-1` 只出现在宿主的兜底路径上——连普通消息框都显示不出来的时候。
- **`file.*` 的错误消息变了，且被拒绝的路径不再回显。** 该命名空间内所有裸写的错误信封统一改走标准错误信封，`std::filesystem` 异常只外传 Win32 错误号——异常文本与出错路径不再进入 payload 和宿主日志。路径安全拒绝现在形如 `file.read: path security denied for 'path': Access denied: protected system path`：给出方法名、出错参数（数组参数带下标，如 `items[2].destination`）与策略原因，调用方能判断是哪个实参被拒，而宿主不会把一个文件系统位置泄漏到页面可能转发出去的 payload 里。此前解析 `result.error` 取路径或匹配特定措辞的代码，需改用 `result.code` 配合 `result.details`。
- **参数形状不对现在返回 `INVALID_PARAMS`，不再是 `PERMISSION_DENIED`。** 路径安全装饰器此前把「路径被拒」与「实参格式错误」报在同一个 code 下。靠 `PERMISSION_DENIED` 分支去捕获类型错误的处理逻辑，将不再在那里收到它们。

本版仍作为 minor 发布，与本项目的版本号惯例一致（见 1.6.0 与 1.12.0）。需要可控升级时请锁定精确版本号。
:::

### 异步文件操作

- **新增 `file.copyAsync`、`file.moveAsync`、`file.deleteAsync` 与 `file.cancelOp`。** 实际工作在宿主 worker 线程上进行，复制一整张专辑不再像同步的 `file.copy` 那样冻结 UI。调用立即返回 `{ operationId, totalCount }` 回执；结果经 `file:opProgress` 陆续到达，最后由一个 `file:opComplete` 收尾。
- 新事件 `file:opProgress` 与 `file:opComplete`（载荷：`operationId`、`op`、`done`、`total`、`results` / 三项计数与 `cancelled`）。结果是成批发送，绝不是每条一个事件：累积到 64 条待发或距上一批已过 100 ms 时发出，因此快速运行会收敛到约 `ceil(total / 64)` 个事件，而慢速运行则以额外事件换取持续推进的进度信号。`done / total` 可直接当进度分数用，最后一个不满批总在 `file:opComplete` 之前到达。
- 每个请求条目上报一个结果，绝不是每个文件一个：目录条目在整棵树走完后才上报一次。每个结果按请求原样回显 `source`（以及 `destination`，`deleteAsync` 没有），`%变量%` 占位符原样保留不展开，因此可直接当查找键。`status` 取 `'ok'` / `'skipped'` / `'failed'`，`reason` 取自 `already-exists`、`not-found`、`permission`、`cross-volume`、`io-error` 与 `cancelled`。
- **路径校验整批一致。** 任一条目未通过宿主的读或写检查，整个调用以 `PERMISSION_DENIED` 被拒且不产生 `operationId`——绝不会派发半批。`copyAsync` 按 `Read` 校验 `source`、按 `FileWrite` 校验 `destination`；`moveAsync` 两端都按 `FileWrite` 校验，因为 move 会删除源；`deleteAsync` 对每个路径按 `FileWrite` 校验。全进程最多 8 个操作同时进行。
- copy 与 move 在目标已存在时行为不同：`copyAsync` 把目录合并进已存在的目录（其中已有的文件被跳过且不单独上报，该条目仍报 `status: 'ok'`），而 `moveAsync` 报 `skipped` / `already-exists`。`overwrite`（默认 `false`）只覆盖文件目标——Windows 无法原地替换目录，因此已存在的目录目标永不被替换。注意同步的 `file.move` 总是替换文件目标；异步形式不主动这么做。
- 同卷内的 move 是一次重命名，无论文件多大都不花代价。跨卷时宿主回退为「先复制后删源」；该条目仍报 `status: 'ok'`，但带 `reason: 'cross-volume'`，让这份额外开销可见。
- `deleteAsync` 默认 `moveToTrash: true`，它把每个路径交给 shell，因而需要主线程——这类删除在主线程上以 16 条一批进行并在批间让出。`moveToTrash: false` 在 worker 线程上删除，并且会删掉非空目录——同步的 `file.delete` 在该模式下拒绝这么做。
- `file.cancelOp` 在批次中途生效，而不是等到批次末尾：copy 与 move 在一个文件之内停下，中止正在传输的文件并清除其半成品；delete 在下一个条目处停下。已完成的条目保留结果，其余每个条目都上报为 `skipped` / `cancelled`，且该次运行仍以带 `cancelled: true` 的 `file:opComplete` 收尾。关闭 popup 会取消该 popup 发起的操作；面板宿主没有这个钩子，因此其操作除显式取消外会跑到底。操作已结束或从未存在时返回 `cancelled: false`——这两种情况故意不作区分。
- 只要发起调用的窗口还活着，两个事件都投递给它，这也正是 `results` 敢携带真实文件系统路径的前提。该窗口消失后宿主无法再解析它，会回退到主实例，因此迟到的事件可能出现在并未发起该操作的窗口里。有两条路径会跳过 `file:opComplete`——运行中宿主关闭，以及宿主侧意外失败——所以监听者若持有需要清理的状态，应自带超时，而不是无限期等它。

### 元数据探测

- **新增 `metadata.probeBatchAsync` 与 `metadata.cancelProbe`。** 落盘读在宿主 worker 线程上进行，几百个路径不再像 `metadata.readBatch` 那样卡住 UI。调用返回 `{ operationId, totalCount }` 回执；结果经 `metadata:probeProgress` 到达，最后由恰好一个 `metadata:probeComplete` 收尾。
- 每条结果都上报信息来源——`infoSource: 'cached' | 'direct'`——这正是该端点对媒体库从未见过的文件有价值的原因。失败时按情况上报 `'not-found'` / `'unsupported-format'` / `'read-error'` 之一，而 `readBatch` 把这个区分合并成一个笼统的错误字符串。`includeTags`（默认 `true`）附上扁平标签表，上游键按 `readBatch` 的口径大写；只要技术信息时传 `false`。
- 路径可带 `|subsong:N` 后缀、各自独立解析，并原样回显，因此可直接当查找键。与 `metadata.read` 不同，批量表面**不**认旧式 `#N` 子歌写法——它会把恰好以 `#<数字>` 结尾的无扩展名文件名切错。
- 路径校验整批一致：任一路径未通过宿主的媒体读检查，整个调用以 `PERMISSION_DENIED` 被拒且不产生 `operationId`。不提供逐路径拒绝。
- `metadata:probeProgress` 与文件事件用同一套「64 条或 100 ms」成批规则。`metadata:probeComplete` 总会到达，取消与失败路径上也一样。取消会中断正在进行的磁盘读而不是等它结束；尚未触及的路径永不上报，被中断的那个路径既不算成功也不算失败，因此取消的运行里 `successCount + failureCount` 会小于 `total`。

### 拖放

- **拖入的快捷方式现在会告诉你它指向哪里。** Windows 放进拖放文件列表的是 `.lnk` 文件本身，而 foobar2000 播不了它，因此每个路径来源现在都配了一个平行的快捷方式目标数组：`dnd:enter` / `dnd:drop` 载荷与 `dnd.getPathsAsync()` 上的 `resolvedPaths`，以及用于同步快照的新方法 `dnd.getResolvedPaths()`。两个数组长度恒定相等，因此 `paths.map((p, i) => targets[i] ?? p)` 即可得到可播列表。
- 拿不到目标时该项为 `null`：路径不是快捷方式、快捷方式指向回收站一类 shell 命名空间对象而非文件、记录的目标太长无法完整取回（Windows 以 `MAX_PATH` 截断，而被截断的路径会指向另一个文件）、COM 不可用，或为保持投放响应性而跳过了解析。永远不会是空字符串，因此真值判断就够了。
- 目标只说明快捷方式指向何处，不保证那个文件在。已失效的快捷方式上报其 `.lnk` 记录的路径而非 `null`——因为无论目标是否还存在，Windows 都会把那个路径交回来，而宿主在被拖放阻塞的线程上负担不起一次文件系统检查。非空项也可能指向已不存在的文件，请自行处理。只解析 `.lnk`——`.url`、`.library-ms` 与虚拟搜索结果都报 `null`。
- 读 `resolvedPaths` 不产生任何文件系统访问：目标在拖放到达时已解析一次，因此 `getPathsAsync()` 只读宿主内存。
- **已建档的限制** —— 页面侧快照只发布给顶层文档，因此 `dnd.getPaths()` 与 `dnd.getResolvedPaths()` 在 `<iframe>` 内返回空数组，且该槽位在这个文档的生命周期内一直是 `null`。被嵌入的页面若需要路径，须由主框架经 `postMessage` 转交。
- 把曲目**拖出**窗口仍不支持，`dnd.startDrag` 继续 resolve `{ success: false, code: 'NOT_SUPPORTED' }`。一次实测已确定实现方案——它需要一条独立 STA 线程，因为在宿主主线程上执行拖出会让目标应用在整个手势期间冻结——但这项工作有意不放进本版。

### 媒体库查询

- **多值标签不再截断到第一个值。** 自首个版本起，标了多位艺术家的曲目在 `metadata.read` 之外的所有地方都只报第一位。曲目对象现在把各值按标签顺序以 `, ` 拼接、不去重——与 foobar2000 本体的显示一致——作用于 `artist`、`albumArtist`、`genre`、`composer` 四个字段，各 API 仍只带自己本来就有的字段。
- **新增 `artist` 的原子形态 `artists`。** 拼接串分不清一位叫 `"A, B"` 的艺术家和 `A`、`B` 两位艺术家，因此 library 命名空间的曲目对象——`library.getAll`、`library.query`、`library.search`、`library.getByPath` 及其余 `library.*` 曲目端点——额外携带 `artists: string[]` 原样值数组，`artists.join(', ') === artist` 逐字节成立。其他命名空间的曲目对象（`playlist.getTracks`、`playback.getCurrentTrack`、`queue.get`、artwork 与事件载荷）不含此字段。
- **聚合按每位参与艺术家计数。** `library.getArtists` 给每位参与艺术家各建一个条目，`trackCount` 因此成为参与计数：各条目相加会大于曲目总数，`albumCount` 与 `totalDuration` 同样按每位艺术家重复计入。`library.getStats` 的 `totalArtists` 现在与 `getArtists` 的条目数一致；`library.getGenres` 对多值 `genre` 的每个值各建一个条目。
- **新增 `library.search` 与 `library.query` 的字段投影。** `library.search` 接受 `options.fields`，`library.query` 新增第四个 `fields` 参数。返回的每一行随后只持有所请求的键、别无其他，因此 `TrackInfo` 在运行时是局部视图，而声明类型仍是完整的。可用名是 20 个曲目键——`index`、`title`、`artist`、`artists`、`album`、`albumArtist`、`genre`、`date`、`trackNumber`、`discNumber`、`duration`、`path`、`absolutePath`、`fileSize`、`bitrate`、`sampleRate`、`channels`、`codec`、`subsong`、`rating`——区分大小写匹配。省略该参数即返回全部 20 个。传入非数组、空数组、非字符串元素或未知名字都会 resolve（绝不 reject）`{ success: false, code: 'INVALID_PARAMS' }`，并在 `details.unknownFields` 回显出错的名字。
- **`library.search` 与 `library.query` 现在在主线程之外做序列化**，并把结果直写到通道上，不再构建中间对象图。响应管线沿途不再深拷贝，零命中的查询会短路返回。除破坏性变更里列出的 `items` 移除外，JavaScript 契约不变：两者仍从同一个 promise resolve 同样的形状。
- **Spider Monkey Panel 兼容层不再逐页重扫媒体库。** `fb.GetQueryItems` 此前按 500 条一页翻页，而 `library.search` 每次请求都重扫全库，于是几千条命中的查询要为每页付一次全库扫描——大库上是 160 次。现在改为先发一次单行探测取命中总数，再一次请求拉回全部命中，并把投影收窄到 `FbMetadbHandle` 实际会读的五个键。可观测的 handle 属性没有任何变化。代价：若两次调用之间媒体库发生变化，探测到的总数就过期了，期间新增的命中会被丢弃；媒体库被并发修改时不保证返回集合稳定。
- **超大库的实务提示。** 这些改动去掉的是拷贝与载荷，不是产生这些行本身的成本。在六位数规模的库上，全字段全库查询仍会占用宿主主线程相当长的时间，主导项是交给页面的响应体量。不需要全部 20 个键时请用 `fields` 收窄投影——这是目前最有效的单一手段。32 位宿主上还请留意峰值：全字段全库结果在解析期间会同时以多种形态存在，六位数规模的库可能瞬时达到数百 MB。

### 标题格式化

- **`titleformat.eval`、`evalBatch`、`evalFields` 与 `evalFieldsBatch` 现在上报 `infoAvailable`。** 宿主本就知道某曲目的 metadb 信息是否就绪，却把这个信号丢掉了，导致标签派生的输出可能静默出错。`infoAvailable: false` 意味着标签派生的值不可信。批量形式按行携带该标志，失败的行不带它。
- 两个需要知道的边界：在 `evalFields` 形式下一个标志覆盖整份合并脚本，因此说不出是哪个具体字段受影响；它也**从不**覆盖 foo_playcount 虚拟字段。名字恰好叫 `infoAvailable` 的 `fields` 键会覆写该标志，这与 `path`、`success` 的既有行为一致。

### 错误与权限

- **`fb2k.invoke` 在子框架内现在立即失败。** 它以带 `code: 'NOT_SUPPORTED'` 的 `Error` reject，消息为 `fb2k.invoke is unavailable in subframes`，取代此前静默挂到 30 秒超时。分离式调用（`const { invoke } = fb2k`）仍以 rejection 形式暴露其 `TypeError`，而不是同步抛出。
- 跨卷的**目录** move 现在返回 `NOT_SUPPORTED` 并带 `details.reason: 'cross-volume'`，而不是一个笼统的失败。跨卷的*文件* move 本就成功，因为底层重命名会静默改走复制。
- 路径白名单与黑名单条目现在规范化到与判定侧一致的 canonical 真实路径形态。此前用等价但不同写法书写的条目——映射盘、junction、8.3 短名——匹配不上它本想覆盖的路径。
- 拖入快捷方式、元数据探测与异步文件操作都已纳入参考手册的权限矩阵；其中的计数由 64 个 API 上的 67 条 spec 变为 68 个 API 上的 73 条。

::: danger FileWrite 现在接受媒体库监视目录
`FileWrite` 链——所有 `file.*` 写操作背后的通道——新增了一步媒体库监视目录判定，因此监视目录内的路径现在**即使在系统盘上也可写**，而此前只有「非系统盘」那一步才可能放行。`FileWrite` 本就是暴露给主题的最宽写通道，这次进一步加宽。若你审计主题，这是第一个该看的通道，而监视目录列表现在也属于它的攻击面。
:::

### 窗口与菜单

- **菜单收起不再留下看不见的点击陷阱。** WebView 渲染面积现在与菜单关闭同步收敛，不会再留下一块残余透明区域，拦截本该落到下层页面的点击。

### SDK

- 异步文件表面的新导出类型：`FileOpEntry`、`FileOpAsyncOptions`、`FileDeleteAsyncOptions`，以及四个新事件的载荷类型（`FileOpProgressPayload`、`FileOpCompletePayload`、`MetadataProbeProgressPayload`、`MetadataProbeCompletePayload`）与配套联合类型（`FileOpKind`、`FileOpStatus`、`FileOpResultReason`、`MetadataProbeFailure`、`MetadataProbeInfoSource`）。
- `dialog.openFile`、`saveFile` 与 `openFolder` 现在建档了各自的 resolve 形状（`{ canceled, filePaths }` / `{ canceled, filePath }` / `{ canceled, folderPath }`），取消时路径字段为空。
- `dnd.getPathsAsync` 改按共享的会话路径形状标注类型，使 `resolvedPaths` 对 TypeScript 可见。`dnd.getResolvedPaths()` 以 `paths` 为基准补齐，因此早于该字段的宿主会返回长度正确的 null 数组，而不是一个会让按下标配对的循环静默错位的短数组。

### 文档

- 双语 API 参考里的每个参数表现在都带默认值列，通篇删除了只复读参数名的填充话术。
- 必填列逐条对照 C++ handler 的空值检查核验，双语共修正约 50 处标错的参数。
- `library.coverMaxSize` 的单位建档为 KB 而非字节。三张损坏的 `library` 返回字段表按宿主真实结构重建。修正了 12 处文档错误，含两处语义反转与若干失真描述。

## v1.12.0 (2026-08-13)

::: warning 本版的破坏性变更
以下四项可能需要修改代码：

- **拖放投放区注册表已移除。** `dnd.registerDropZone` / `unregisterDropZone` / `getDropZones` 不复存在；宿主现在原生观测拖放，并向光标下的窗口发射 `dnd:enter` / `dnd:leave` / `dnd:drop`，无需注册。真实路径改用 `fb.dnd.getPathsAsync()` 或 `dnd:drop` 载荷读取。
- **`dnd.startDrag` 不再伪装成功。** 拖出窗口需要组件未提供的原生 `IDropSource`；调用现在 resolve `{ success: false, code: 'NOT_SUPPORTED' }`，而不是虚报 `success: true`。
- **窗口尺寸约束改作用于调用方窗口。** `window.setMinSize` / `getMinSize` / `setMaxSize` / `getMaxSize` / `setResizable` / `isResizable` 这六个端点不再回退到主窗口，解析不到目标时调用失败。依赖旧回退行为的 popup，实际约束的是主窗口。
- **`DiscoveryContextMenuCommand` 不再是类型别名。** 读取右键菜单命令的 `path` / `isDynamic` / `subGuid` 不再通过类型检查——这些字段从未被填充过。

本版仍作为 minor 发布：本项目的版本号历史上已有 minor 版本承载破坏性变更的先例（见 1.6.0）。需要可控升级时请锁定精确版本号。
:::

### 拖放

- **拖放改为原生管线。** 宿主通过原生 `IDropTarget` 桥自行观测拖放手势，把 HTML5 刻意隐藏的真实文件系统路径交给页面。标准 HTML5 拖放事件照常触发，`fb.dnd` 作为旁路通道与之并行。
- 新表面：`dnd.getPathsAsync(sessionId?)`（在 `drop` 处理函数内可靠读取）、`dnd.getPaths()` / `dnd.hasFiles()`（同步快照读取，用于乐观 UI）、`dnd.getCapabilities()`（本窗口能否交付路径）。
- 事件 `dnd:enter` / `dnd:leave` / `dnd:drop`（载荷：`sessionId`、`paths`、`x`、`y`、`keyState`）与 `dnd:capabilitiesChanged`，按 `sessionId` 关联同一次手势，点对点投递到光标下的窗口。
- 路径对不受信 origin 扣留，`hasFiles` 仍如实上报；DUI / CUI 面板（`hosting: 'standard'`）拿不到路径——用 `getCapabilities()` 判断，而不是按窗口类型假设。
- **迁移**：删除 `registerDropZone` / `unregisterDropZone` / `getDropZones` 调用与 `zoneId` 簿记；保留（或补上）HTML5 `dragover` / `drop` 监听做视觉与命中判定；在 `drop` 处理函数内用 `await fb.dnd.getPathsAsync()` 读取真实路径；依赖路径的 UI 以 `dnd.getCapabilities()` 为准。

### 主菜单

- **`menu.runMainMenuCommand` 不再透传宿主异常。** 此前在汉化版 foobar2000 上，宿主异常会以宿主语言的原始 `Error` 逃逸到 JavaScript，并使名称与路径形式彻底失效。失败现统一以 `success: false` 加 `code` 返回：`MENU_ITEM_DISABLED`、`MENU_MATCH_AMBIGUOUS`（附 `candidates`）或 `MENU_COMMAND_NOT_FOUND`。
- **`menu.getMainMenu` 的叶子在汉化版宿主上可寻址了。** 叶子现在按宿主渲染菜单所用的同一份文本匹配，并回填 `guid` / `subGuid`；汉化版宿主实测由 158 个叶子中 0 个带 guid 提升到 167 个中 131 个。同时开始上报 `flags`、`enabled`、`checked`、`hidden`——此前这些一个都没有。
- **行为变更** —— 禁用命令会被拒绝执行，而不再报告成功。此前对灰显命令执行会返回 `success: true`，且同一条命令按名字被拒、按 GUID 却「成功」。三种请求形式现在校验一致，返回 `MENU_ITEM_DISABLED`。不在枚举结果中的 GUID 仍会尝试执行——调用方可能持有本次枚举未覆盖的有效地址。
- 名称与路径按段精确匹配。名字有歧义时上报而不替调用方决定：汉化版宿主上可能有三条不同命令共用同一标签，取首个匹配会静默执行错误的命令。要无歧义请用 `guid` 寻址——它是唯一跨宿主稳定的形式，因为汉化版上报的是本地化标签。
- `menu.runMainMenuCommand` 新增 `subGuid` 参数，与所属命令 GUID 搭配用于寻址动态子命令。
- 无法解析出地址的叶子会明示这一点，带 `executable: false` 与 `unaddressableReason`，而不再伪装成一条调用方却无法执行的普通命令。扁平列举结果的 `available` 改为实读宿主状态，不再恒为 `true`，禁用命令不会再与启用命令无从分辨。

### 自绘菜单

- **按调用指定菜单外观。** `menu.show` / `menu.popup` 新增第三个 `MenuPopupOptions` 参数，调用方未传的键不会发送，宿主保留自身默认值：`windowModel`（默认 `'fullscreen'`，可选 `'contentSized'`——把根菜单与一级子菜单绘制为按内容测量的独立紧凑窗口，每块面板各自承载真实 DWM 背景材质与系统窗口阴影；右键菜单推荐用此模型）；`css`（至多 256 KiB）/ `cssReplace` 样式接管；`backdrop`（默认 `'acrylic'`，可选 `'mica'` / `'mica-alt'` / `'none'`）与 `backdropDarkMode`（默认 `true`）；`closeAnimationMs`（默认 `0`，钳制到 `0..1000`）退出淡出时长。
- **富条目。** `MenuPopupItem.type` 新增 `'nowplaying'`、`'rating'`、`'slider'`、`'segmented'` 及其配套字段（`value`、`min` / `max` / `orientation`、`segments`、`cover` / `title` / `subtitle`），任意行可用 `iconSvg` 内联单色图标。图标经运行时允许列表消毒；非法或超限的图标被丢弃，不会使该行失败。
- **新事件 `menu:valueChanged`** 以 `{ menuId, itemId, value }` 上报评分、滑杆或分段控件的变化并保持菜单开启；普通行仍经 `menu:select` 上报并关闭菜单。`menu.popup` 只在选择或取消时 resolve，菜单含值控件时请单独订阅此事件。
- **修复** —— 失焦关闭现在可靠生效；`contentSized` 模型下池化子菜单窗口不再出现空白内容。

### Discovery 菜单

- **行为变更** —— `discovery.searchCommands` 现在同时搜索右键菜单与主菜单，因此结果数增加，`type` 出现新取值 `'contextmenu'`。传 `{ scope: 'mainmenu' }` 可回到原有覆盖面。此前该端点只查主菜单，却把 `type: 'mainmenu'` 写进每条结果，导致右键命令搜不到。
- **行为变更** —— `discovery.searchCommands` 会过滤宿主不会显示的条目，与列举端点保持一致。传 `{ includeHidden: true }` 可取回完整集合。
- **行为变更** —— `discovery.getAllServices` 将右键菜单命令计入 `services.contextMenuCommands` 并纳入 `totalServices`，因此 `totalServices` 数值变化。被过滤掉的数量由 `contextMenuHiddenFiltered` 给出；无选中且无播放曲目时 `stateKnown` 为 `false`。
- 搜索结果携带与列举端点一致的状态字段（`enabled`、`checked`、`radioChecked`、`hidden`、`stateKnown`、`flags`、`source`、`executable`、`unaddressableReason`），调用方无需再发一次请求即可判断能否执行。搜索包含右键菜单但无选中且无播放曲目时，响应的 `stateKnown` 为 `false`，此时这些结果的 `enabled` / `checked` 不得用于过滤。
- 搜索的大小写折叠改为仅作用于 ASCII，不再可能破坏 UTF-8 序列。对中文标签的可观测匹配行为不变——中文没有可折叠的大小写。
- **`discovery.getContextMenuTree` 不再静默截断。** 此前子项截到 50、深度截到 10，却仍上报宿主的真实 `childCount`，导致 `children.length` 与 `childCount` 不符且无任何说明。两个上限现为深度 16、每节点 512 子项，且任何裁剪都会上报：`popup` 节点同时给出 `childCount` 与 `childrenReturned`，子树被裁剪的节点带 `truncated`，并由 `depthExceeded` / `childrenExceeded` 说明原因。标记向上传播，因此顶层 `truncated` 覆盖整棵树；`maxDepth` 与 `maxChildrenPerNode` 回显生效上限。
- `discovery.getContextMenuTree` 的节点现在上报状态——`enabled`、`checked`、`radioChecked`、`hidden`、`stateKnown`、`flags`——以及 `depth`。分隔符只带类型，这也是对它唯一有意义的信息。
- `discovery.executeContextMenuCommand` 在成功路径上也返回 `hidden` 与 `resolved`，不再只在拒绝时返回，调用方因此能区分「未被拒绝」与「本版本不返回该字段」。无注册项拥有该 GUID 时 `resolved` 为 `false`，此时没有可评估的状态。
- `discovery.getMainMenuCommands` 中由动态子菜单展开的条目现在也带 `stateKnown`、`executable`、`unaddressableReason`，与静态槽位一致。

### SDK

- **破坏性类型修正** —— `DiscoveryContextMenuCommand` 原本是 `DiscoveryMainMenuCommand` 的类型别名，因而声明了右键侧根本不会返回的字段。现改为独立接口：右键条目是扁平注册、由宿主决定位置，因此没有菜单 `path`，也没有动态展开相关字段。此前读取右键命令的 `path` / `isDynamic` / `subGuid` 的 TypeScript 代码，读的是从未被填充过的字段。
- 新增导出类型 `MenuNodeState`、`MenuNodeSource`、`MenuUnaddressableReason`，由所有菜单列举结果共用。
- `fb.discovery.searchCommands()` 接受 `{ scope, includeHidden }`；`fb.discovery.getContextMenuCommands()` 接受 `{ includeHidden }`。
- 修复 SMP 主菜单派发。此前菜单 id 优先按 `path` 解析，而路径匹配在汉化宿主上直接失败，导致主菜单的 `ExecuteByID` 在这类宿主上完全不工作。现改为优先 `guid`（由宿主直接解析）；右键菜单会话仍以 `commandId` 为准。
- 修复 SMP 菜单状态解码。此前 `enabled` / `checked` 由原始 `flags` 位重新推导，忽略了宿主已给出的归一化布尔值。现优先使用布尔值，仅在其缺失时回退到 `flags`。标记为 `stateKnown: false` 的条目按可用呈现而非置灰，因为「状态未知」与「可用且未勾选」在 `flags` 上无法区分，把未观测状态当作禁用会隐藏宿主本可执行的命令。

### 托盘与菜单

- **修复** —— 托盘菜单的「显示主窗口」不再依赖主页面。窗口一旦最小化或隐藏到托盘，页面会被深度挂起，任何 `tray:menuItemClicked` handler 都跑不起来，也就无法调用 `window.focus` 把窗口找回来；左键单击图标同样无声。原生项（`_sys_exit`、`playbackAction`）全程正常。
- **行为变更** —— `showSystemItems`（默认 `true`）现在会在 bottom 分区的 `_sys_exit` 之前注入 `_sys_show`（「显示主窗口」）。该项原生恢复并前置主窗口，保留其最大化 / 正常放置状态；与所有原生项一致，它**不发** `tray:menuItemClicked`。因此使用 `showSystemItems: true` 的菜单会多出一行；传 `showSystemItems: false` 可关闭该行为。
- `_sys_show` 与 `_sys_exit` 一同进入精确、大小写敏感的保留 id 允许列表：前端自绘「显示主窗口」行时可获得同样的原生路由并保留自己的 `label` / `icon`，对应注入项自动跳过。形似 id（如 `_sys_show_alt`、`_SYS_SHOW`）仍是普通用户项，且不会抑制注入。`playbackAction` 仍拒绝 `'show-main-window'` 与 `'exit'`——系统路由始终专属于保留 id。

### 窗口

- **行为变更** —— 六个尺寸约束端点（`window.setMinSize` / `getMinSize` / `setMaxSize` / `getMaxSize` / `setResizable` / `isResizable`）现在作用于调用方窗口，或显式传入的 `windowId`，不再回退到主窗口。此前 popup 设置自己的最小尺寸，实际约束的是主窗口。解析不到目标时调用失败，而不是返回另一个窗口的约束。DUI / CUI 面板调用方会被拒绝，并带 `panelMode: true`。
- **文档修正** —— 这些端点的尺寸单位是物理像素，而非此前文档所写的 DIP。按设备像素比换算的调用方，在高 DPI 显示器上会把系数乘两次。经 setter 与 getter 往返的取值精确到 ±1px。

### 界面语言

- 插件原生界面改为跟随 foobar2000 的呈现语言，不再跟随 Windows 界面语言，因此汉化版 foobar2000 不会再显示英文的首选项页面。语言由宿主自身的字符串探测得出，探测不可行时回退到 Windows 界面语言。
- 新增*界面语言*首选项：*自动（跟随 foobar2000）*（默认）、*English* 或 *中文*。新打开的对话框立即生效；菜单标题与面板描述只向宿主注册一次，需重启 foobar2000 才会刷新。

### 播放列表

- **第三方组件的播放列表文件现在能正确展开。** 通过 API 添加播放列表 URL 或文件时，此前用硬编码的 8 种包装扩展名（`.pls` / `.m3u` / `.m3u8` / `.asx` / `.wpl` / `.xspf` / `.fpl` / `.cue`）判断；其他已安装组件注册的播放列表格式会被当作单条曲目。现在运行时查询宿主的 playlist_loader 注册表，宿主能加载的格式都会被展开。

### 性能与稳定性

- **修复** —— WebView2 *浏览器*进程死亡后，窗口不再一直空白。此前只有渲染进程有恢复路径，因此当外部原因（例如第三方钩子注入浏览器进程）导致其崩溃时，窗口会保持空白且无从自愈；实测有一次会话就这样运行了 10 小时。现在会重建窗口，并限制为 10 分钟内最多 3 次重建，使可复现的崩溃不会退化成「重建→崩溃→重建」死循环。窗口期过后计数重置，因此之后无关的失败仍可恢复。这与 v1.11.0 的渲染进程处理是两件事，后者负责重载页面与重建 WebView。

## v1.11.0 (2026-07-27)

### 托盘与菜单

- 新增 `TrayMenuItem.playbackAction`（`'play-pause' | 'previous' | 'next' | 'stop'`）：自定义托盘项可声明由插件原生执行的播放动作。外观仍由调用方控制；声明项**不发** `tray:menuItemClicked`（同 Electron `role` / Tauri `PredefinedMenuItem`）。仅可用于 `type:'normal'` 的叶子项；取值非法，或写在分隔符 / 子菜单 / 富控件上时，整次 `setContextMenu` / `appendMenuItems` 调用会以 `INVALID_PARAMS` 失败，而不是静默忽略。不接受 `'exit'`。仅托盘菜单生效，对 `menu.show` 无效；`getMenuItems` 会原样回读该字段。主页面深挂起（最小化 / 托盘隐藏 / 锁屏）时要后台可靠的托盘播放控制，请用本字段或内置 `showPlaybackControls`；仅靠 click→`playback.*` 不保证执行。自 v1.11.0 起可用；需兼容旧宿主时先用 `config.getVersionInfo().plugin.version` 探测。
- 文档澄清：`tray:menuItemClicked` 仅覆盖普通用户项与富值控件；内置播放 / 系统注入项与声明了 `playbackAction` 的项原生执行且不发点击事件，按钮状态请根据 `playback:*` 事件更新
- `menu.getMainMenu` 新增 `locale`、`i18n`、`withAvailability` 三个选项。`locale` 默认 `'auto'`，保持宿主原生标签不翻译；`i18n: false` 完全关闭 `displayLabel` 翻译；`withAvailability` 默认 `true`，附带各子菜单的命令可用性计数。SDK 侧签名相应变为 `getMainMenu(root?, opts?)`。
- 修复自定义菜单的 UTF-8 序列化与上下文模式选择。

### DSP 与输出

- **`dsp.*` 与 `output.*` 现在真正可用。** 这 11 个处理器（`dsp.getChain` / `getPresets` / `getAvailable` / `addDsp` / `removeDsp` / `moveDsp` / `applyPreset` / `setChain`，`output.getDevices` / `getEntries` / `getSettings`）此前虽有文档，但其源文件从未被加入编译，所以任何调用都会因方法未注册而失败。自本版起已编译并注册。已发布的 `fb.dsp.*` / `fb.output.*` SDK 封装本来就存在，配合本版插件后开始真正生效；插件版本低于本版时，无论 SDK 版本如何都会被拒绝。
- 修复 `output.getDevices` 崩溃。部分输出后端在回调里用「长度未知」哨兵值代替真实长度，原实现直接采用该值，导致读取远超字符串末尾并使 foobar2000 终止。
- 修复 `dsp.moveDsp` 移动到错误位置。升序移动会少一格，因此向上移动一位等于无操作，且任何项都无法移到链尾；降序移动本来是正确的。返回的 `to` 现在是真实的最终索引。
- **行为变更** —— `dsp.setChain` 遇到任何不可用条目时整体拒绝，不再静默跳过。此前用三个条目构造的链可能只应用两个却仍返回 `success: true`。每个条目级失败都返回带索引的原因：`dsps[0] must be an object`（元素不是对象，例如裸字符串或数字）、`dsps[0]: guid is required`（缺失、空串或不是字符串）、`dsps[0]: Invalid GUID format: …`、`dsps[0]: DSP not found or no default preset: …`（GUID 格式合法但该 DSP 未安装）。`dsps` 本身缺失或不是数组时仍返回 `dsps array is required`。任何调用被拒绝时链都保持不变。
- `dsp.getPresets` 在无选中预设时返回 `selectedIndex: -1`。此前返回内部哨兵值 `18446744073709551615`，该值无法用 JavaScript number 表示，到达前端时已是不可用的浮点数。
- `dsp.getChain` 始终包含 `activePreset` 与 `activePresetIndex`，无选中预设或宿主不支持预设时为 `null` / `-1`。此前这两个键在上述情况下完全缺失，调用方不得不自行探测。

### Discovery

- `discovery.getMainMenuCommands` 与 `discovery.searchCommands` 现在默认展开运行时构建子菜单的组件（`mainmenu_commands_v2`，例如 ESLyric）。**这会改变既有返回结果**：除父级槽位外还会含其子命令。传 `{ expandDynamic: false }` 可回到仅静态注册表的旧行为。
- 命令条目新增 `path`、`isDynamic`、`isDynamicParent`、`subGuid`、`flags` 字段；响应新增 `expandDynamic`、`dynamicCount`，`discovery.getAllServices` 新增 `mainMenuDynamicCommands`。标记 `isDynamicParent` 的条目是容器槽位，自身不可执行。
- `discovery.executeMainMenuCommand` 新增第二个可选参数 `subGuid`：执行由动态子菜单展开出的命令时必须一并传入，否则只会派发静态命令 GUID。

### 性能与稳定性

- 页面不可见期间（最小化、被遮挡、托盘隐藏或锁屏），高频可再生流在生产侧即停止：`audio:spectrum`、`playback:time`、`playback:timeHighRes` 不再产生，恢复可见后下一拍自然到达；`window:hoverStateChanged` 与 `cursor:hiddenChanged` 在隐藏期间天然静默。其余事件一律可靠按序投递，不丢弃也不合并，包括 `http:response`、`library:getAllResult`、`audio:fullWaveformReady` 等异步应答与 `playback:itemPlayed` 等逐次事实。绘制频谱或播放进度的主题应把数据中断理解为「页面隐藏」而非「播放停止」。
- 最小化 / 被遮挡 / 锁屏 / 托盘隐藏时新增深度挂起（`TrySuspend`），冻结渲染进程计时器与动画以便系统回收内存。新增 foobar2000 高级设置项 `Deep-suspend WebView when hidden (TrySuspend; frees renderer memory)`（默认开启，关闭后退回原 Low 内存路径）。
- 新增高级设置项 `Keep WebView active in background while CDP remote debugging is on (tray/minimize/lock)`（默认开启）：开启 CDP 远程调试时保持 WebView 后台活跃，以保证截图与时序类自动化工具稳定。
- 收敛 WebView2 崩溃后的僵尸态：进程失败后按重载次数上限决定重载或重建，不再停留在无响应页面。
- 改进封面获取：修正了换曲或改标签后可能返回上一首封面的缓存问题，并把解码移出 UI 线程、改为逐个排队处理，大尺寸封面不再造成界面卡顿。`artwork.*` 的请求与响应结构未变。

### 元数据

- 修复 `metadata.read`、`metadata.readByPath`、`metadata.readBatch` 忽略容器内轨道编号的问题。路径里的 `|subsong:N` 后缀既没有被剥离也没有被采用，因此从 CUE、ISO 镜像或多轨文件里读取单条轨道时，要么直接失败，要么返回首轨的标签。这正是这类文件在 foobar2000 界面里能读、经 API 读不到的原因。`metadata.readRaw` 原本就是正确的。
- `metadata.read` 与 `metadata.readByPath` 新增 `cueIndex` 参数，可显式指定轨道，与 `metadata.readRaw` 一致；其优先级高于路径中的 `|subsong:N` 后缀。SDK 的 `fb.metadata.read()` / `readByPath()` 通过第二个 `opts` 参数传入，MCP 的 `fb2k_metadata_read` / `fb2k_metadata_read_by_path` 已声明该参数。`metadata.readBatch` 不接受此参数，批量读取请在各自路径上使用 `|subsong:N` 后缀。

### SDK

- 新增仅 SDK 层的 additive 二进制适配 helper：`fb.file.readBinary()`、`fb.file.writeBinary()`、`fb.file.writeDataUrl()`、`fb.metadata.embedArtworkBytes()`、`fb.metadata.embedArtworkFromDataUrl()`，以及公开类型 `FileBinaryWriteOptions`、`MetadataArtworkBytesOptions`。
- 这些 helper 只把 `ArrayBuffer` / `Uint8Array` 与严格 Base64 Data URL 适配到既有 `file.read`、`file.write`、`metadata.embedArtwork` wire 契约；不新增 Bridge endpoint，不改变 raw `invoke` 或旧 facade。canonical Base64 与 Data URL 校验发生在 SDK 调用 Host 之前，Host 自身的校验和行为没有变化。
- 修正两处错误的 TypeScript 返回类型声明，使其与宿主实际 wire 契约一致：`ui.isMinimized()` 为 `{ minimized }`（不存在 `isMinimized` 别名）、`ui.isAlwaysOnTop()` 为 `{ enabled, isAlwaysOnTop }`（两个键同值）。**按旧声明编写的 TypeScript 代码需要相应调整。**
- `fb.playcount.set()` 不再发送宿主从未读取的 `count` 键；`window.getBackdropPolicy` / `window.setBackdropPolicy` 补齐 `windowId` 参数类型声明。
- `fb.http.request()` 现在经由文档声明的 `http.get` 端点派发；此前一个失效的内部参数可能转发错配的方法名。动词 helper（`fb.http.post()` / `put()` / `delete()` / `patch()`）仍各自派发到自己的端点，二进制响应也一样。
- 为五个 wrapper 补上可选的末位 `opts` 参数，对应的键宿主 handler 早已读取：`fb.file.delete(path, opts?)`（`moveToTrash`）、`fb.file.copy(source, destination, opts?)`（`overwrite`），以及 `fb.metadata.write(path, tags, opts?)` / `removeField(path, field, opts?)` / `removeTag(path, tags, opts?)`（`cueIndex`）。其中 `metadata.write` 支持 `cueIndex` 补上了一处真实缺口：v1.11.0 只把 `cueIndex` 接进了元数据**读取**侧，因此此前无法通过 SDK 向 CUE 或镜像文件中的单曲写标签。所有新增参数均为可选，既有调用点不受影响。
- `fb.metadata.readByPath()` 的返回类型由裸 `JsonObject` 收窄为 `MetadataReadByPathResponse`。
- 修正两处与宿主契约不符的公开类型声明：`dsp.setChain` 的 `dsps` 是**必填**的 `{ guid }` 对象数组（原声明为 `dsps?: string[]`，在可选性、元素类型、结构三处均错——宿主要求 `dsps` 必须是数组，并逐项读取 `guid`）；`playlist:created` / `playlist:renamed` 的 payload 保持 `name: string`。**按旧 `dsp.setChain` 声明编写的 TypeScript 代码需要相应调整。**

## v1.10.0 (2026-07-16)

- 新增：`TrayMenuItem.orientation`（仅 `type:'slider'`，`'horizontal' | 'vertical'`，默认水平）。仅精确 `vertical` 为纵向（min 底 / max 顶；Up/Right 增、Down/Left 减、Home/End 边界）；`native` 忽略并保持分级子菜单；旧 runtime 忽略未知字段保持水平。范围规范化：`max<min` 交换、`max==min` 常量不发 value、初始 value clamp、IPC 越界拒绝。`getMenuItems` round-trip。需要纵向时先用 `config.getVersionInfo().plugin.version` 探测运行时是否为 1.10.0 及以上
- 变更：自绘菜单两态焦点（导航 roving tabindex + 真实 focus / 富控件编辑态）与 ARIA（`menuitem` / `menuitemcheckbox` / 内部 `role=slider` / segmented `radiogroup`）；`checked:false` 仍为 checkable；默认入退场在 `prefers-reduced-motion: reduce` 下禁用 transform/transition（不改 hide protocol / `closeAnimationMs`）
- 新增：`TrayMenuConfig.layoutMode`（`'flat' | 'zones'`）。默认 `'flat'` 保留 `#menu > .fb-item` 直接子 DOM；显式 `'zones'` 时为非空 top / playback / bottom 生成 `.fb-zone[data-zone]`。`native` 忽略；旧 runtime 忽略未知字段但不生成 wrapper；`menu.show` 不受影响。需要 zones 的主题应先用 `config.getVersionInfo().plugin.version` 探测运行时是否为 1.10.0 及以上
- 变更：自绘菜单受保护 CSS 不再强制可见态 `#menu { display:block !important }`；主题可直接把根菜单 / zone 设为 flex 或 grid，但无法用 `display:* !important` 重新显示已隐藏菜单
- 安全：自绘菜单 SVG 图标改为 DOMParser + 元素/属性白名单克隆，移除 raw `innerHTML` 注入；非法或超限单图标被丢弃，菜单继续显示；`transform` 严格解析（拒绝前缀/函数间杂质与空参），节点必须位于 SVG namespace
- 安全：`tray.setContextMenu` / `tray.appendMenuItems` / `menu.show` 在写入持久配置或打开 overlay 前做资源上限事务性校验（item ≤ 512、`menu.show` 深度 ≤ 8、segmented options ≤ 64、CSS ≤ 256 KiB、SVG 总量 ≤ 256 KiB）；单图 SVG > 32 KiB 仅丢弃该图标。超限返回 `INVALID_PARAMS` 且 `details` 含 `field` / `limit` / `actual`；非法 / 超限输入属有意不兼容
- 安全加固：托盘 / 自绘菜单的内置动作改按可信内部来源路由，不再凭 public id 前缀。唯一兼容例外是 tray API 中精确、大小写敏感的 `_sys_exit`（保持 1.9.0 的真实退出行为）；调用方提供的 `_pb_playPause` / `_pb_prev` / `_pb_next` / `_pb_stop` 仍是普通用户项，且不能通过同名去重抑制 runtime 自动注入的 trusted 播放项；重复 public ID 由 opaque token 区分，公共 `menu.show` 不提权。每次选择 / 值变更由不可预测的单次 token 承载并按本次菜单索引校验，未知 / 过期 token、禁用项、越界富值（rating / slider / segmented）一律拒绝。内部 `menu.__*` IPC 另校验调用方是否为 overlay 自身窗口，且 select / dismiss / ready / submenuPanel / valueChanged 须匹配当前菜单 id；外部调用方或过期 / 伪造的 menuId 一律拒绝且不改变菜单状态
- 自绘 tray `ContentSized` 现在在字体就绪后离屏测量 root 与全部一级 submenu，并等待连续两帧尺寸稳定；C++ 使用 64 位安全槽位分配。固定 HWND 的 region 只覆盖当前可见 root/submenu panel，因此未展开预留区不再形成空白 acrylic/mica 面板，且调用方配置的 backdrop 不会被静默关闭
- 托盘自绘菜单 `segmented` 分段控件的值变更明确纳入“保持菜单打开”契约：一次分段切换经 `tray:menuItemClicked` 回传 `{ id, value }`（`value` = 被选中分段的从 0 起索引）且**不关闭**菜单，与 `rating` / `slider` 一致。`webview` 运行时一直保持菜单打开，本次仅修正此前只列 `rating` / `slider` 的共享契约与事件文档
- 托盘菜单全隐藏（或空）的分区不再产生多余 separator：此前分区内所有项 `visible:false` 时，隐藏项被过滤后会残留一条首/尾分隔线；现在可见性过滤先于 separator 判定，native 与 webview 菜单均不再出现该多余分隔线（默认行为修正）
- 文档修正：`TrayMenuItem.icon`（base64 ICO）为保留字段，当前两种后端均不渲染（`native` 纯文本、webview 绘制 `iconSvg`）；菜单项图标请改用 `iconSvg`

## v1.9.0 (2026-06-18)

- 托盘自绘菜单（`render: 'webview'`）普通/子菜单项支持图标：新增 `TrayMenuItem.iconSvg = { viewBox, content }`，内联单色 SVG、`currentColor` 跟随菜单文字色、左对齐固定 8px 间距；同层有图标时所有普通/子菜单项预留 16px 图标列以对齐文字（`native` 菜单忽略）
- `tray.setContextMenu` 新增 `config.autoNowPlaying`：开启后 `nowplaying` 项的空字段（cover/title/subtitle）在右键弹出时由后端用当前曲目自动补全（前端传了就用前端值，**前端优先**）；`cover` 补全仅 `webview`，取当前曲目封面并缩略为缩略图，title/subtitle 走 `%title%`（自动回退文件名）/`%artist%`，兼容流媒体动态标题
- `TrayMenuItem.cover` 现额外支持 `http(s)://` URL（除既有 `data:` 与裸 base64 外），便于流媒体前端直传实时解析的封面
- SDK 安装包同步到 `1.9.0`

## v1.8.0 (2026-06-10)

- 新增自绘菜单能力：`menu.show` / `menu.close` 以 WebView 渲染菜单内容，支持子菜单递归渲染；菜单窗口采用内容尺寸固定窗策略，消除展开时的闪烁
- `tray.*` 新增 `render: 'webview'` 模式，托盘右键菜单可改用自绘菜单渲染，外观与主题保持统一
- 新增 `tray.setMenuItemState`，可在不重建整个菜单的情况下更新单个菜单项状态
- 修复桌面歌词等置顶弹窗点击后，主窗口偶发被拉至前台并意外置顶的问题（撤销路径 z-order 插入误入 topmost 段 + sink 还原参照反馈环）
- 修复 SDK 发布物缺失 `HTMLElementTagNameMap` 全局声明的问题，npm 用户恢复 `fb-*` 自定义元素的类型提示
- 修复打包脚本在新版 PowerShell 下无法生成 `.fb2k-component` 的兼容性问题
- HttpApi 异步请求异常边界加固；修复 LibraryApi 一处 NUL 字符串处理缺陷
- SDK 安装包同步到 `1.8.0`；`bump-version.ps1` 版本同步覆盖面扩展到 `sdk/package-lock.json` 与 VitePress 导航栏版本号

## v1.7.0 (2026-06-06)

- 新增 Taskbar & Tray 能力：`taskbar.*` 可设置任务栏缩略图按钮、进度条、叠加图标和闪烁提示；`tray.*` 可创建系统托盘图标、气泡通知和右键菜单
- `tray.*` 新增增量菜单管理：`appendMenuItems` / `removeMenuItems` / `clearMenuItems` / `getMenuItems`，可以按 `top` / `playback` / `bottom` 分区动态维护托盘菜单，不必每次重建完整菜单
- 新增 `taskbar:buttonClicked`、`tray:click`、`tray:doubleClick`、`tray:menuItemClicked`、`tray:beforeContextMenu` 事件，主题可以响应任务栏缩略图按钮和托盘交互
- 新增 `webview:processFailed` 事件，WebView2 渲染进程异常时会广播诊断信息，并配合渲染进程自动恢复路径降低空白窗口风险
- 新增高精度播放位置事件 `playback:timeHighRes`，由独立 WinAPI 定时器驱动，适合歌词、进度条等需要亚秒级刷新的界面
- `library.getAll` 冷缓存全量序列化改为后台线程执行；SDK 会等待 `library:getAllResult` 并按 `requestId` 关联结果，避免大媒体库查询卡住 UI
- 修复托盘隐藏后窗口恢复路径，补全 `window.focus` / hidden restore 场景下的 WebView surface 恢复，降低最小化、托盘隐藏、Alt+Tab 切回后的空白风险
- 修复任务栏缩略图 pause 图标 base64 损坏和 HICON 所有权问题，避免播放按钮显示异常和 explorer.exe 崩溃
- SDK 安装包同步到 `1.7.0`，可直接使用新的 Taskbar & Tray 类型和事件声明
- VitePress 文档新增 Taskbar & Tray API 页面，并同步 Cursor、Playback 高频事件和相关示例

## v1.6.1 (2026-05-20)

- 新增 `cursor.*` 命名空间：`cursor.setHidden(hidden)` / `cursor.isHidden()` 显式控制客户区光标可见性；解决 Visual Hosting 模式下 CSS `cursor: none` 不可靠的问题
- 新增 `cursor:hiddenChanged` 事件，每窗口独立派发
- `fb.http.*` 新增 `insecureTls` 参数（双层门禁）— 全局开关 `Allow self-signed / invalid TLS certificates` ON + 每请求 `insecureTls: true` 才生效，访问自签证书的内网服务（Plex / Jellyfin / Lidarr 等）不再被强制拦截
- `fb.http.*` 新增 `responseType: 'arraybuffer' | 'binary'`，body 自动 base64 解码为 `ArrayBuffer`，封面 / 字体等二进制资源不再因 UTF-8 严格校验失败
- VitePress 文档同步上述变化（cursor.md / http.md / events.md）

## v1.6.0 (2026-05-11)

- `playlist.getAll` 不再返回 `duration` 字段，避免为了时长把整批轨道都读一遍；`playlist.getActive` / `playlist.getPlaying` 还是会返回
- `http.get` / `http.post` / `http.head` 默认改成异步；如果你就是要同步调用，需要显式传 `async: false`

## v1.1.17 (2026-02-06) 

- 可以真正开多个窗口了
- 新增 `window.createPopup` / `closePopup` / `closeAllPopups` / `getAllWindows`
- 新增 `window.sendMessage` / `window.broadcast`，窗口之间可以互相发消息
- 支持异步关闭、无标题栏和透明背景

## v1.1.16 (2026-02-06)

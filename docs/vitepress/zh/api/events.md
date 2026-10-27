# 事件 API

foo_ui_webview2 的运行时事件 payload 参考。事件名使用 **冒号格式**（`namespace:eventName`），方法调用使用 **点格式**（`namespace.method`）。

下面每个事件都由宿主的事件声明生成，字段与宿主实际发出的一致。宿主内置菜单覆盖层私用的事件不在此列。

## api 事件

### api:registered

<!-- api-schema:begin event:api:registered -->
外部插件注册了一个方法，或替换了自己先前注册的方法。`isExternal` 恒为 `true`。

发给所有窗口。

**载荷**

载荷是一个 [SystemApiInfo](../reference/types.md#systemapiinfo)。
<!-- api-schema:end -->

### api:unregistered

<!-- api-schema:begin event:api:unregistered -->
外部插件移除了自己的一个方法。注销整个插件时发的是 `plugin:unregistered`。

发给所有窗口。

**载荷**

载荷是一个 [SystemApiInfo](../reference/types.md#systemapiinfo)。
<!-- api-schema:end -->

## app 事件

### app:beforeQuit

<!-- api-schema:begin event:app:beforeQuit -->
foobar2000 正在退出。在插件的任何窗口或面板关闭之前发一次，送到所有页面：主窗口、popup 与面板的页面，不论界面是哪个、是否开了后台模式。只是尽力而为：事件排进各页面的队列，宿主不等待。页面紧接着就会关闭，处理函数不一定来得及运行或跑完，它向宿主发出的调用多半得不到答复。

发给所有窗口。

**载荷**

事件不带字段。
<!-- api-schema:end -->

## audio 事件

### audio:dspPresetChanged

<!-- api-schema:begin event:audio:dspPresetChanged -->
foobar2000 的播放 DSP 链变了，比如加了一个 DSP 或载入了预设。

发给所有窗口。

**载荷**

事件不带字段。
<!-- api-schema:end -->

### audio:fullWaveformFailed

<!-- api-schema:begin event:audio:fullWaveformFailed -->
应答为 `pending` 的 `audio.generateFullWaveform` 请求没有得到波形就结束了：解码失败，或被 `audio.cancelFullWaveform` 取消。投递范围同 `audio:fullWaveformReady`。

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `taskId` | `string` | `pending` 应答里的 id。 |
| `path` | `string` | 请求给的路径。 |
| `error` | `string` | 出错的描述。 |
| `code` | `"CANCELLED" \| "INVALID_HANDLE" \| "NO_INFO" \| "INVALID_PARAMS" \| "DECODER_FAILED" \| "DECODE_FAILED" \| "UNKNOWN_ERROR"` | 被 `audio.cancelFullWaveform` 取消时为 `CANCELLED`；否则是解码失败的原因。 |
<!-- api-schema:end -->

### audio:fullWaveformReady

<!-- api-schema:begin event:audio:fullWaveformReady -->
应答为 `pending` 的 `audio.generateFullWaveform` 请求解码完成。等同一次解码的每个请求各收一个事件，按各自的 `method`、`scale` 与 `signed` 生成。发给发起请求的页面；找不到它时发给同一顶层窗口下的页面，再没有就发给主窗口的页面。

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `taskId` | `string` | `pending` 应答里的 id。 |
| `path` | `string` | 请求给的路径。 |
| `waveform` | `number[]` | 每个点一个值，刻度为 `scale`；`signed` 时在 `[-1, 1]` 内，否则在 `[0, 1]` 内。 |
| `maxAmplitude` | `number` | 归一化前所选序列的最大值，线性满幅单位：`linear` 下 `waveform[i] * maxAmplitude` 是绝对电平；`db` 下 dBFS 为 `(v * 60 - 60) + 20 * log10(maxAmplitude)`。 |
| `duration` | `number` | 曲目时长，秒。 |
| `sampleRate` | `integer` | 实际解码 PCM 的采样率，Hz，可能不同于文件的标称采样率。 |
| `channels` | `integer` | 实际解码 PCM 的声道数。 |
| `resolution` | `integer` | 点数。 |
| `method` | `"rms" \| "peak"` | 每个点的聚合方式，同请求。 |
| `scale` | `"linear" \| "db"` | 点的刻度，同请求。 |
| `signed` | `boolean` | 点保留 PCM 极性，同请求。 |
| `cached` | `boolean` | 恒为 `false`：这个事件只在解码之后发。 |
<!-- api-schema:end -->

### audio:outputDeviceChanged

<!-- api-schema:begin event:audio:outputDeviceChanged -->
foobar2000 的输出配置变了，比如输出设备。

发给所有窗口。

**载荷**

事件不带字段。
<!-- api-schema:end -->

### audio:pcmFailed

<!-- api-schema:begin event:audio:pcmFailed -->
`audio.decodePcm` 任务没有得到样本就结束了：解码失败、页面来源不再可信、缓冲分配或投递失败，或被 `audio.cancelDecodePcm` 取消。投递范围同 `audio:pcmReady`。

发给订阅或任务所属的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `taskId` | `string` | `audio.decodePcm` 应答里的 id。 |
| `path` | `string` | 请求给的路径。 |
| `error` | `string` | 出错的描述。 |
| `code` | `"CANCELLED" \| "ORIGIN_DENIED" \| "OPERATION_FAILED" \| "INVALID_PARAMS" \| "DECODER_FAILED" \| "DECODE_FAILED" \| "NOT_SUPPORTED" \| "UNKNOWN_ERROR"` | 被 `audio.cancelDecodePcm` 取消时为 `CANCELLED`；页面来源不再可信时为 `ORIGIN_DENIED`；缓冲分配或投递失败时为 `OPERATION_FAILED`；否则是解码失败的原因。`INVALID_PARAMS` 表示 `start` 到了或越过了曲目末尾。 |
<!-- api-schema:end -->

### audio:pcmReady

<!-- api-schema:begin event:audio:pcmReady -->
`audio.decodePcm` 任务完成。样本以 `chrome.webview` 上的 `sharedbufferreceived` 事件送达，`additionalData` 里带同一个 `taskId`，可能在本事件之前或之后到达。发给发起任务的页面，且只在该页面仍是发起任务的那个文档时发。

发给订阅或任务所属的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `taskId` | `string` | `audio.decodePcm` 应答里的 id。 |
| `path` | `string` | 请求给的路径。 |
| `sampleRate` | `integer` | 每声道每秒的样本数。 |
| `channels` | `integer` | 缓冲里的声道数；请求设了 `mono` 时为 `1`。 |
| `frames` | `integer` | 缓冲里每声道的帧数。 |
| `start` | `number` | 第一帧的曲目时间，秒。 |
| `end` | `number` | 紧接最后一帧之后的曲目时间，秒。 |
| `duration` | `number` | `frames / sampleRate`，秒。 |
| `truncated` | `boolean` | 音频超出了按曲目时长估算的大小，或中途变了格式；缓冲里是截断之前的部分。 |
| `resampled` | `boolean` | 用 foobar2000 的重采样器转换到了 `sampleRate`。 |
<!-- api-schema:end -->

### audio:replaygainModeChanged

<!-- api-schema:begin event:audio:replaygainModeChanged -->
foobar2000 播放的某项 ReplayGain 设置变了，不只是音源模式。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `mode` | `integer` | 变化后的音源模式：`0` 无，`1` 音轨，`2` 专辑，`3` 按播放顺序。 |
<!-- api-schema:end -->

### audio:spectrum

<!-- api-schema:begin event:audio:spectrum -->
`audio.subscribeSpectrum` 订阅的一帧：播放时按订阅的 `fps` 出帧，暂停或停止时另出一帧静音帧。字段同该订阅的 `audio.getSpectrum` 应答。发给订阅的页面：先按它的窗口 id，再按窗口句柄，再找同一顶层窗口下的页面；都找不到就丢弃这一帧。

发给订阅或任务所属的页面。订阅方可以让它以自己起的名字送达；没起名时用这个名字。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `subscriptionId` | `string` | 帧所属的订阅；请求没指定订阅时不带此键。 |
| `output` | `"bands" \| "bins"` | 帧里给的是什么。 |
| `spectrum` | `number[]` | 频带帧：每个频带一个值，刻度为帧的 `scale`；`weighted` 在 `[0, 1]` 内，`db` 下限 `-160`。`channels: 'mix'` 的频点帧：每个频点一个功率值，dB。 |
| `left` | `number[]` | `channels: 'stereo'` 的频点帧：第一个声道，每个频点一个 dB 值。 |
| `right` | `number[]` | `channels: 'stereo'` 的频点帧：第二个声道，单声道流时重复第一个。 |
| `bands` | `integer` | 频带帧：`spectrum` 的长度。 |
| `firstBin` | `integer` | 频点帧：第一个值的 FFT 频点序号；第 `i` 个值的中心频率为 `(firstBin + i) * sampleRate / fftSize` Hz。范围内没有频点时为 `0`，数组为空。 |
| `fftSize` | `integer` | 实际用的 FFT 点数。 |
| `scale` | `"weighted" \| "db"` | 值的刻度；频点帧恒为 `db`。 |
| `channels` | `"mix" \| "stereo"` | 频点帧：订阅的声道布局。 |
| `channelCount` | `integer` | 频点帧：可视化流的声道数；未知时为 `0`。 |
| `sampleRate` | `integer` | 可视化流的采样率，Hz；未知时为 `0`。 |
| `minFrequency` | `number` | 频率范围下限，Hz。 |
| `maxFrequency` | `number` | 上限，Hz：请求的上限截到 `sampleRate / 2`，没给时就是 `sampleRate / 2`。 |
| `state` | `"playing" \| "paused" \| "stopped"` | 暂停或停止时的帧是静音帧。 |
| `streamTime` | `number` | 可视化流上次起播以来的秒数；帧由截至此刻的 `fftSize` 个样本算出。 |
| `hostTime` | `number` | 宿主算出这一帧时的系统时间，Unix 纪元毫秒。 |
<!-- api-schema:end -->

### audio:stream

<!-- api-schema:begin event:audio:stream -->
采样率或声道数变了，宿主不再写 `audio.subscribeStream` 缓冲的某一代；下一代缓冲以单独的 `sharedbufferreceived` 事件送达。其他结束方式（退订、页面关闭、宿主退出）不这样通知，以缓冲的 `ended` 标志为准。发给订阅的页面。

发给订阅或任务所属的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `subscriptionId` | `string` | `audio.subscribeStream` 应答里的 id。 |
| `type` | `"ended"` | 恒为 `ended`。 |
| `epoch` | `integer` | 结束的那一代；它的缓冲已置 `ended` 标志。 |
| `reason` | `"format-change"` | 恒为 `format-change`。 |
<!-- api-schema:end -->

## cursor 事件

### cursor:hiddenChanged

<!-- api-schema:begin event:cursor:hiddenChanged -->
光标隐藏状态变更事件（由 cursor.setHidden 触发状态翻转时派发给调用窗口；未改变实际状态的调用不触发）

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `hidden` | `boolean` | 翻转后的光标当前是否处于隐藏状态 |
<!-- api-schema:end -->

## dnd 事件

### dnd:enter

<!-- api-schema:begin event:dnd:enter -->
拖动进入了窗口。只发给光标下的窗口，不发给别的窗口，因为真实路径属于敏感信息。拖动不带 `CF_HDROP` 文件列表（浏览器链接、虚拟 shell 对象、压缩包内条目）或文档 origin 不受信时，`paths` 为空。

发给它所涉及的窗口或面板里的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `sessionId` | `string` | 把同一次拖动的 `dnd:enter`、`dnd:leave` 与 `dnd:drop` 关联起来。在整个宿主进程内唯一，因此也能看出拖动属于哪个窗口。 |
| `paths` | `string[]` | 绝对路径，顺序同 `DataTransfer.files`；拖动不带文件列表或文档 origin 不受信时为空。 |
| `resolvedPaths` | `(string \| null)[]` | `paths` 同下标处 `.lnk` 快捷方式的目标，或 `null`；长度恒等于 `paths`，两者都被隐去而为空时也一样。只解析 `.lnk`：指向 shell 对象的快捷方式、目标太长读不完整（Windows 以 `MAX_PATH` 为上限）、COM 套间不可用、为保持放下响应而跳过的条目，都报 `null`，从不为空串。失效的快捷方式报它记录的路径，所以非 null 只说明快捷方式指向哪里，不保证那里有文件。 |
| `hasFiles` | `boolean` | 拖动是否带 `CF_HDROP` 文件列表；即使 `paths` 被隐去也如实报告，因为它本身不泄露任何信息。 |
| `source` | `"self" \| "other-window" \| "external"` | 拖动从哪里来：从本窗口的页面拖出为 `self`；从同一个 foobar2000 的另一个窗口（主窗口或 popup）拖出为 `other-window`；其余都是 `external`，例如资源管理器、别的程序、另一个 foobar2000 进程，以及从 Default UI 或 Columns UI 面板拖出的拖动。宿主给它承载的窗口里开始的每次拖动加一个标记，据此判断；任何程序都能仿造这个标记，所以只把它当作来源提示，不要当作安全检查。 |
| `x` | `integer` | 光标在客户区内的横坐标，单位物理像素；除以 `devicePixelRatio` 得到 CSS 像素。 |
| `y` | `integer` | 光标在客户区内的纵坐标，单位物理像素；除以 `devicePixelRatio` 得到 CSS 像素。 |
<!-- api-schema:end -->

### dnd:leave

<!-- api-schema:begin event:dnd:leave -->
拖动没有放下就离开了窗口。

发给它所涉及的窗口或面板里的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `sessionId` | `string` | 这次拖动的会话，同 `dnd:enter`。 |
<!-- api-schema:end -->

### dnd:drop

<!-- api-schema:begin event:dnd:drop -->
拖动在窗口上放下。只有页面接受的地方才能放下，即页面在 HTML5 `dragover` 处理函数里调了 `preventDefault()`；别处只有在进入后半秒内、页面还没答复时松手才会放下。`paths` 是最终的列表，拖动源可能在 `dnd:enter` 之后改过它。此事件可能早于也可能晚于页面自己的 HTML5 `drop` 处理函数到达；需要与放下同时拿到路径时，在那个处理函数里调 `dnd.getPathsAsync`。

发给它所涉及的窗口或面板里的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `sessionId` | `string` | 这次拖动的会话，同 `dnd:enter`。 |
| `paths` | `string[]` | 绝对路径，顺序同 `DataTransfer.files`；拖动不带文件列表或文档 origin 不受信时为空。 |
| `resolvedPaths` | `(string \| null)[]` | `paths` 同下标处 `.lnk` 快捷方式的目标，或 `null`；长度与规则同 `dnd:enter` 的同名字段。 |
| `x` | `integer` | 光标在客户区内的横坐标，单位物理像素。 |
| `y` | `integer` | 光标在客户区内的纵坐标，单位物理像素。 |
| `keyState` | `integer` | 放下时的 Win32 修饰键与鼠标按键掩码（`MK_*` 标志），供需要按修饰键区分行为的页面使用。它不影响报告给拖动源的放置效果：来自其他程序的拖动报告复制，本组件页面发起的拖动按页面的答复报告。 |
| `source` | `"self" \| "other-window" \| "external"` | 拖动从哪里来：从本窗口的页面拖出为 `self`；从同一个 foobar2000 的另一个窗口（主窗口或 popup）拖出为 `other-window`；其余都是 `external`，例如资源管理器、别的程序、另一个 foobar2000 进程，以及从 Default UI 或 Columns UI 面板拖出的拖动。宿主给它承载的窗口里开始的每次拖动加一个标记，据此判断；任何程序都能仿造这个标记，所以只把它当作来源提示，不要当作安全检查。 |
<!-- api-schema:end -->

### dnd:capabilitiesChanged

<!-- api-schema:begin event:dnd:capabilitiesChanged -->
页面加载之后，窗口的拖放集成能提供的能力变了，例如导航改变了文档 origin。载荷与 `dnd.getCapabilities` 的返回字段相同。

发给它所涉及的窗口或面板里的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `html5` | `boolean` | 页面收得到标准的 HTML5 拖放事件；`false` 表示窗口完全没有拖放支持。 |
| `paths` | `boolean` | 能经 `dnd.getPathsAsync` 与 `dnd:*` 事件拿到真实路径。在窗口生命周期内并非恒定：导航或 Chromium 重新注册自己的放置目标都会收回它，由 `dnd:capabilitiesChanged` 通知。 |
| `hosting` | `"visual" \| "standard"` | 窗口承载 WebView 的方式：主窗口与弹出窗口为 `visual`，DUI / CUI 面板为 `standard`。 |
| `pathsUnavailableReason` | `"register-failed" \| "forward-unavailable" \| "inner-target-not-found" \| "chain-failed" \| "origin-untrusted"` | `paths` 为 `false` 的原因；出现即表示 `paths` 为 `false`。 |
| `dragOut` | `boolean` | 能经 `dnd.prepareDrag` 把文件拖出窗口。与 `paths` 无关。 |
| `dragOutUnavailableReason` | `"not-visual-hosting" \| "runtime-too-old" \| "register-failed"` | `dragOut` 为 `false` 的原因；出现即表示 `dragOut` 为 `false`。 |
<!-- api-schema:end -->

### dnd:dragEnded

<!-- api-schema:begin event:dnd:dragEnded -->
宿主在拖出开始时决定不给这次拖动附带文件。拖动本身照常进行，成为不带文件的普通页面拖动，所以在页面内放下仍然有效；宿主会先抹掉 token 文本，极少数抹不掉的情况下改为取消这次拖动。它不是完成通知：宿主接受 token 并交出文件时不会有后续事件，结果由页面自己的 `dragend` 经 `dataTransfer.dropEffect` 给出（目标收下文件为 `copy`，拖动被取消或拒绝为 `none`）。只有带 drag token 的拖动才会引发它；页面拖自己的文字、图片或链接永远不会。

发给它所涉及的窗口或面板里的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `result` | `"failed"` | 恒为 `failed`：宿主只报告拒绝。 |
| `code` | `"PERMISSION_DENIED" \| "INVALID_PARAMS" \| "OPERATION_FAILED"` | 没有附带文件的原因：token 未知、过期、已用过、被之后的 `prepareDrag` 取代或属于别的窗口时为 `PERMISSION_DENIED`；`dataTransfer.effectAllowed` 不恰好是 `copy` 时为 `INVALID_PARAMS`（`dnd.applyDragToken` 会正确设置它）；文件列表无法附到拖动上时为 `OPERATION_FAILED`。 |
| `error` | `string` | 可读的原因；从不包含文件路径。 |
<!-- api-schema:end -->

## file 事件

### file:opProgress

<!-- api-schema:begin event:file:opProgress -->
分批推送 `file.copyAsync`、`file.moveAsync` 或 `file.deleteAsync` 的批量操作结果；待推送结果达到 64 条或距上次推送已满 100 ms 时发送一批，最后一批残余结果在 `file:opComplete` 之前送达；事件发送给发起操作的页面，若该窗口已销毁则转投主窗口页面

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operationId` | `string` | 发起操作时返回的任务回执 ID |
| `op` | `"copy" \| "move" \| "delete"` | 操作类型：`copy`、`move` 或 `delete` |
| `done` | `integer` | 截至当前批次已处理完成的累计条目数，含失败条目 |
| `total` | `integer` | 本次批量调用受理的条目总数 |
| `results` | `FileOpResultItem[]` | 仅当前批次的条目结果，保持请求时的先后顺序 |
| `results[].source` | `string` | 请求中传入的源路径，原样保留且不展开路径变量 |
| `results[].destination` | `string` | 请求中传入的目标路径，原样保留；`file.deleteAsync` 时无此字段 |
| `results[].status` | `"ok" \| "skipped" \| "failed"` | 条目处理状态：成功执行为 `ok`；因目标已存在或操作提前取消而跳过为 `skipped`；其余为 `failed` |
| `results[].reason` | `"already-exists" \| "not-found" \| "permission" \| "cross-volume" \| "path-too-long" \| "io-error" \| "cancelled"` | 状态原因说明；条目直接成功时无此字段；`skipped` 对应 `already-exists` 或 `cancelled`；`failed` 对应 `not-found`、`permission`、`path-too-long` 或 `io-error`；`ok` 对应 `cross-volume`（表示跨卷移动以复制加删除完成）；其中 `path-too-long` 表示涉及路径超过 259 个字符（含所复制文件夹内部路径）或新建文件夹超过 247 个字符 |
<!-- api-schema:end -->

### file:opComplete

<!-- api-schema:begin event:file:opComplete -->
批量操作的终末事件；操作取消后或存在失败条目时均会发出，仅在 foobar2000 正在退出或 worker 意外失败时不发，依赖此事件清理状态的监听方应自带超时；事件发出后 `operationId` 即失效，对其调用 `file.cancelOp` 将返回 `cancelled: false`；投递规则同 `file:opProgress`

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operationId` | `string` | 发起操作时返回的任务回执 ID |
| `op` | `"copy" \| "move" \| "delete"` | 操作类型：`copy`、`move` 或 `delete` |
| `total` | `integer` | 本次批量调用受理的条目总数 |
| `successCount` | `integer` | 状态为 `ok` 的成功条目数 |
| `skippedCount` | `integer` | 状态为 `skipped` 的跳过条目数 |
| `failureCount` | `integer` | 状态为 `failed` 的失败条目数 |
| `cancelled` | `boolean` | 至少有一个条目的原因上报为 `cancelled` 时为 `true` |
<!-- api-schema:end -->

## http 事件

### http:response

<!-- api-schema:begin event:http:response -->
异步 HTTP 请求结果事件（涵盖 get、post、put、delete、patch 或 head 调用，载荷携带回执中的 requestId；成功时包含各同步响应字段，失败时包含 error 与 code；定向派发给发起请求的页面，若页面不可达则派发给同一顶层窗口的其他页面或主窗口页面）

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `requestId` | `string` | 请求回执中分配的唯一标识符 requestId |
| `success` | `boolean` | 是否成功接收到 HTTP 响应（接收到 404 等 HTTP 错误状态码亦视为网络请求成功） |
| `status` | `integer` | HTTP 状态码（请求成功时为服务端返回的状态码；仅当 redirect: "error" 拒绝重定向时作为失败原因提供重定向状态码） |
| `headers` | `Record<string, string>` | 响应头键值对（键名转为小写；存在重复响应头时以最后出现的数值为准；请求成功时提供） |
| `body` | `string` | 响应体内容（依据 responseType 格式化为纯文本或 Base64 编码字符串；http.head 恒为空字符串；请求成功时提供） |
| `responseType` | `"text" \| "base64"` | 响应体内容的编码格式（text 或 base64；请求成功时提供） |
| `contentLength` | `integer` | 数值形式的 Content-Length 响应头（仅在 http.head 请求成功且服务端返回合法数字时提供） |
| `error` | `string` | 请求失败的具体错误原因描述（仅在请求失败时提供） |
| `code` | `"INVALID_PARAMS" \| "PERMISSION_DENIED" \| "CANCELLED" \| "OPERATION_FAILED"` | 错误码代码（URL 非 http/https 时为 INVALID_PARAMS；未授权访问本地或局域网私有地址为 PERMISSION_DENIED；被 http.abort 中止为 CANCELLED；其余网络错误为 OPERATION_FAILED；仅在失败时提供） |
| `cancelled` | `boolean` | 请求是否由 http.abort 主动中止（仅在被取消时为 true，其余情况缺省） |
<!-- api-schema:end -->

### http:downloadComplete

<!-- api-schema:begin event:http:downloadComplete -->
异步 http.download 任务完成或失败事件（载荷携带回执中的 requestId；成功时包含各同步下载字段，失败时包含 error 与 code；投递范围规则同 http:response）

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `requestId` | `string` | 下载任务回执中分配的唯一标识符 requestId |
| `success` | `boolean` | 文件是否已成功写入本地磁盘（服务端返回 404 等响应内容亦会写入并视为成功） |
| `status` | `integer` | HTTP 状态码（成功时为服务端响应码；仅在 redirect: "error" 拦截重定向时提供重定向状态码） |
| `bytesWritten` | `integer` | 实际写入磁盘的文件总字节数（仅在成功时提供） |
| `path` | `string` | 实际写入文件的完整绝对路径（已展开路径变量；仅在成功时提供） |
| `error` | `string` | 下载失败的具体错误原因描述（仅在失败时提供） |
| `code` | `"INVALID_PARAMS" \| "PERMISSION_DENIED" \| "CANCELLED" \| "OPERATION_FAILED"` | 错误码代码（规则同 http:response；仅在失败时提供） |
| `cancelled` | `boolean` | 下载任务是否由 http.abort 主动中止（仅在被取消时为 true，其余情况缺省） |
<!-- api-schema:end -->

## jitQueue 事件

### jitQueue:trackChanged

<!-- api-schema:begin event:jitQueue:trackChanged -->
影子播放列表曲目起播事件（涵盖重复播放当前曲目；trackId 与 title 反映 playNow 或 enqueueNext 配置的元数据；事件定向派发给启动会话的页面）

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `trackId` | `string` | 会话当前播放曲目的标识符 trackId（缺省时为空字符串） |
| `title` | `string` | 会话当前播放曲目的显示标题（缺省时为空字符串） |
<!-- api-schema:end -->

### jitQueue:needNext

<!-- api-schema:begin event:jitQueue:needNext -->
宿主请求下一首曲目事件（当缓冲为空且无进行中请求时触发，于曲目起播及剩余 30 秒时检查；页面须调用 enqueueNext 预载或 notifyEmpty 告知耗尽）

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `currentTrackId` | `string` | 会话当前播放曲目的标识符 trackId（缺省时为空字符串） |
| `reason` | `"trackChange"` | 请求触发原因（固定为 trackChange） |
<!-- api-schema:end -->

### jitQueue:listExhausted

<!-- api-schema:begin event:jitQueue:listExhausted -->
会话曲目列表耗尽事件（由调用 notifyEmpty 触发或影子列表播放至末尾无后续曲目触发；会话状态流转为 Exhausted）

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `lastTrackId` | `string` | 会话当前播放曲目的标识符 trackId（缺省时为空字符串） |
<!-- api-schema:end -->

### jitQueue:preloadComplete

<!-- api-schema:begin event:jitQueue:preloadComplete -->
批量预载曲目写入影子播放列表完成事件（在 preloadBatch 调用返回前派发）

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | 实际成功插入的曲目总数（与调用返回的 tracksAdded 保持一致） |
| `startIndex` | `integer` | 调用时传入的起始播放索引 startIndex |
| `replace` | `boolean` | 调用时传入的替换模式标记 replace |
<!-- api-schema:end -->

### jitQueue:error

<!-- api-schema:begin event:jitQueue:error -->
曲目加入影子播放列表失败事件（当路径无法解析、列表被外部移除或底层抛出异常时在异步加入完成后派发；若加入前被新操作取代则不派发）

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `trackId` | `string` | 添加失败曲目的原始标识符 trackId |
| `error` | `string` | 底层返回的错误原因描述 |
| `url` | `string` | 发生错误的远程 URL 地址（与 path 互斥提供） |
| `path` | `string` | 发生错误的本地文件绝对路径（与 url 互斥提供） |
<!-- api-schema:end -->

## keyboard 事件

### keyboard:hotkey

<!-- api-schema:begin event:keyboard:hotkey -->
全局热键触发事件；定向投递给发起注册该热键的窗口页面（若注册窗口已关闭，热键仍然保持注册有效，后续触发将重定向投递至主窗口页面）

发给订阅或任务所属的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | `integer` | `keyboard.registerHotkey` 注册成功时返回的热键数值标识符 id |
| `key` | `string` | 注册时传入的按键组合字符串 key |
| `action` | `string` | 注册时绑定的动作名称 action |
<!-- api-schema:end -->

## library 事件

### library:initialized

<!-- api-schema:begin event:library:initialized -->
foobar2000 启动阶段载入媒体库完毕且宿主完成缓存清理后触发；广播通知所有当前已打开的窗口，后续加载的页面不会补发；可通过 `library.getStatus` 主动查询当前就绪状态

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `timestamp` | `integer` | 宿主发出事件的时间戳（自 Unix 纪元起的毫秒数） |
<!-- api-schema:end -->

### library:itemsAdded

<!-- api-schema:begin event:library:itemsAdded -->
媒体库新增曲目时触发；宿主在广播前先行清空自身媒体库缓存

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | 发生变动的曲目数量 |
| `handles` | `string[]` | 发生变动的每首曲目 handle 列表，顺序不固定；`truncated: true` 时为空列表 |
| `truncated` | `boolean` | 单次变动是否超过 5000 首；为 `true` 时 `handles` 为空，前端应全量重新拉取数据 |
| `timestamp` | `integer` | 宿主发出事件的时间戳（自 Unix 纪元起的毫秒数） |
<!-- api-schema:end -->

### library:itemsRemoved

<!-- api-schema:begin event:library:itemsRemoved -->
媒体库移除曲目时触发；宿主在广播前先行清空自身媒体库缓存

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | 发生变动的曲目数量 |
| `handles` | `string[]` | 发生变动的每首曲目 handle 列表，顺序不固定；`truncated: true` 时为空列表 |
| `truncated` | `boolean` | 单次变动是否超过 5000 首；为 `true` 时 `handles` 为空，前端应全量重新拉取数据 |
| `timestamp` | `integer` | 宿主发出事件的时间戳（自 Unix 纪元起的毫秒数） |
<!-- api-schema:end -->

### library:itemsModified

<!-- api-schema:begin event:library:itemsModified -->
媒体库曲目发生变更时触发（如标签更新或 `foo_playcount` 等第三方组件请求刷新播放统计字段 `fromHook`）；由于文件路径未变，宿主保留 `library.getRoots` 与 `library.browseTree` 所需的目录树索引，清空其余媒体库缓存后对外广播；`fromHook` 变更时仅清除缓存的 `library.getAll` 列表（因缓存字段中仅 `rating` 可能受组件影响）

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | 发生变动的曲目数量 |
| `handles` | `string[]` | 发生变动的每首曲目 handle 列表，顺序不固定；`truncated: true` 时为空列表 |
| `truncated` | `boolean` | 单次变动是否超过 5000 首；为 `true` 时 `handles` 为空，前端应全量重新拉取数据 |
| `fromHook` | `boolean` | 文件物理内容未变，仅由 `foo_playcount` 等第三方组件请求刷新统计字段时为 `true`；此时路径、标签与专辑归组均保持不变 |
| `timestamp` | `integer` | 宿主发出事件的时间戳（自 Unix 纪元起的毫秒数） |
<!-- api-schema:end -->

### library:getAllResult

<!-- api-schema:begin event:library:getAllResult -->
`library.getAll` 返回 `{ pending: true, requestId }` 后的异步数据就绪事件；包含在后台线程按媒体库顺序构建的全库曲目列表；仅在构建期间媒体库未发生变更时缓存供后续调用；优先投递至发起调用的页面，若该页面已销毁则退至同一顶层窗口内的页面或主窗口页面

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `requestId` | `string` | 对应异步请求调用返回的标识符 `requestId` |
| `tracks` | `LibraryTrack[]` | 整个媒体库曲目列表，按媒体库原生顺序排列（发生错误时为空列表） |
| `tracks[].…` | [Track](../reference/types.md#track) | 包含 [Track](../reference/types.md#track) 的全部字段。 |
| `tracks[].index` | `integer` | 当前上下文列表中的行索引序号 |
| `items` | `LibraryTrack[]` | 与 `tracks` 完全一致的兼容曲目列表 |
| `items[].…` | [Track](../reference/types.md#track) | 包含 [Track](../reference/types.md#track) 的全部字段。 |
| `items[].index` | `integer` | 当前上下文列表中的行索引序号 |
| `total` | `integer` | 调用时媒体库曲目总数；发生错误时为 `0` |
| `offset` | `integer` | 请求的起始偏移量，固定为 `0` |
| `limit` | `integer` | 请求的分页大小上限 |
| `fromCache` | `boolean` | 固定为 `false` |
| `error` | `string` | 列表异步构建失败的原因说明；仅在失败时出现 |
<!-- api-schema:end -->

## menu 事件

### menu:select

<!-- api-schema:begin event:menu:select -->
自绘菜单项选中事件；菜单项被选中后派发，随后伴随原因为 "select" 的 `menu:dismiss` 事件关闭菜单；定向投递给发起调用的页面（若该页面已失效则退回同顶层窗口其他页面或主窗口页面）

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `menuId` | `string` | 自绘菜单唯一标识符（对应 `menu.show` 返回的 menuId） |
| `itemId` | `string` | 被选中的菜单项条目标识符 itemId |
<!-- api-schema:end -->

### menu:dismiss

<!-- api-schema:begin event:menu:dismiss -->
自绘菜单关闭事件（无论是否选中条目均会触发，包含被外部调用或点击外部关闭情形；投递目标同 `menu:select`）

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `menuId` | `string` | 自绘菜单唯一标识符（对应 `menu.show` 返回的 menuId） |
| `reason` | `string` | 菜单关闭原因标识：条目选中为 "select"、点击外部区域为 "outside"、按下 Esc 为 "escape"、窗口失焦为 "blur"、新菜单顶替为 "replaced"、渲染超时为 "timeout"，或主动关闭时指定的 reason |
<!-- api-schema:end -->

### menu:valueChanged

<!-- api-schema:begin event:menu:valueChanged -->
自绘菜单内部控件值变更事件（评分、滑块或分段控件交互修改时触发且菜单保持展示，投递目标同 `menu:select`）

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `menuId` | `string` | 自绘菜单唯一标识符（对应 `menu.show` 返回的 menuId） |
| `itemId` | `string` | 发生数值变动的控件所在菜单项 itemId |
| `value` | `integer` | 控件更新后的当前数值 |
<!-- api-schema:end -->

## metadata 事件

### metadata:probeProgress

<!-- api-schema:begin event:metadata:probeProgress -->
`metadata.probeBatchAsync` 异步探测任务的进度批次事件；每累积 64 条结果或距上一批超过 100 毫秒即分批推送，最终未满批次在 `metadata:probeComplete` 前推送完毕；被取消或未处理的路径不予上报；定向投递给发起探测的页面窗口（窗口已销毁时退至主窗口页面）

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operationId` | `string` | 对应 `metadata.probeBatchAsync` 返回的操作标识符 `operationId` |
| `done` | `integer` | 截至当前批次各批累计上报的路径总数（包含失败项） |
| `total` | `integer` | 本次探测任务受理的路径总数（对应受理时的 `totalCount`） |
| `results` | `MetadataProbeResultItem[]` | 仅包含本进度批次的探测结果列表，顺序与请求一致 |
| `results[].path` | `string` | 请求传入的原样路径（包含 `\|subsong:N` 后缀） |
| `results[].success` | `boolean` | 该曲目是否成功读取并解析 |
| `results[].infoSource` | `"cached" \| "direct" \| "none"` | 元数据来源：`cached` 表示取自宿主完整内存缓存，`direct` 表示直接读取物理文件，`none` 表示读取失败 |
| `results[].failure` | `"not-found" \| "unsupported-format" \| "read-error"` | 路径读取失败的原因分类：`not-found`、`unsupported-format` 或 `read-error`；仅在 `success: false` 时出现 |
| `results[].info` | `TrackTechnicalInfo` | 曲目的音频技术参数；仅在 `success: true` 时返回 |
| `results[].info.duration` | `number` | 曲目时长（秒） |
| `results[].info.bitrate` | `integer` | 解码器报告的音频平均码率（kbit/s）；未知时为 `0` |
| `results[].info.sampleRate` | `integer` | 音频采样率（Hz）；未知时为 `0` |
| `results[].info.channels` | `integer` | 声道数量；未知时为 `0` |
| `results[].info.codec` | `string` | 解码器报告的音频编码名称（如 `FLAC`、`MP3`）；未知时为空字符串 |
| `results[].tags` | `Record<string, any>` | 曲目的扁平元数据标签字典；仅在 `success: true` 且未指定 `includeTags: false` 时返回 |
<!-- api-schema:end -->

### metadata:probeComplete

<!-- api-schema:begin event:metadata:probeComplete -->
异步探测任务结束事件；任务正常完成、被主动取消或因故失败后均会触发（仅在分发异常如内存耗尽时可能缺省）；触发后原 `operationId` 立即失效（对其调用 `metadata.cancelProbe` 将返回 `cancelled: false`）；投递目标与 `metadata:probeProgress` 相同

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operationId` | `string` | 对应 `metadata.probeBatchAsync` 返回的操作标识符 `operationId` |
| `total` | `integer` | 本次探测任务受理的路径总数 |
| `successCount` | `integer` | 探测成功并上报的路径总数 |
| `failureCount` | `integer` | 探测失败并上报的路径总数 |
| `cancelled` | `boolean` | 探测任务是否被提前中止（通过 `metadata.cancelProbe` 或发起窗口关闭触发）；为 `true` 时成功与失败计数之和小于 `total` |
<!-- api-schema:end -->

### metadata:writeComplete

<!-- api-schema:begin event:metadata:writeComplete -->
由 `metadata.write`、`metadata.writeBatch`、`metadata.removeTag` 或 `metadata.removeField` 排入的后台标签写入完成事件；广播通告所有打开的窗口

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operation` | `"write" \| "removeTag"` | 完成的写入操作类型：`write` 或 `removeTag` |
| `path` | `string` | 请求传入的原样文件路径（包含后缀） |
| `subsong` | `integer` | 实际写入的目标子曲目序号（`cueIndex >= 0` 时取该值，否则取路径解析值，均无时为 `0`） |
| `code` | `integer` | foobar2000 宿主完成状态码：`0` 成功，`1` 中止，`2` 出错 |
| `success` | `boolean` | 写入是否完全成功（当且仅当 `code === 0` 时为 `true`） |
| `status` | `"success" \| "aborted" \| "error"` | 依据状态码转换的语义化状态：`success`、`aborted` 或 `error` |
<!-- api-schema:end -->

## metadb 事件

### metadb:changed

<!-- api-schema:begin event:metadb:changed -->
曲目的元数据变了：标签被写入或重新读取，或 `foo_playcount` 之类的组件请 foobar2000 刷新它提供的字段（`fromHook`）。每一条用 `handle` 指明曲目，与其他方法返回的曲目行里的 `handle` 是同一个键，所以能从 cue 或多曲目文件里分辨出具体是哪一首。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `tracks` | `MetadbChangedTrackItem[]` | 最多 50 首变化的曲目，顺序不固定。无效条目会被跳过，所以条数可能少于 `count` 与 50 中的较小者。全部变化的曲目见 `handles`。 |
| `tracks[].handle` | `string` | 曲目的键，构成方式与曲目行的 `handle` 相同：原生路径（没有原生形式的位置用 foobar2000 自己的路径），`subsong` 不为 `0` 时带 `\|subsong:N`。与同一首曲目在 `library.query`、`playlist.getTracks` 等方法返回的行里的 `handle` 相等。 |
| `tracks[].path` | `string` | 原生形式的文件路径；没有原生形式的位置（如网络流）给 foobar2000 自己的路径。不带 subsong，所以 cue 或多曲目文件里的各首 `path` 相同，要靠 `handle` 区分。 |
| `tracks[].subsong` | `integer` | 文件内的子曲目标识，与曲目行的 `subsong` 相同；整个文件为 `0`。 |
| `tracks[].rating` | `integer` | 评分 `0` 到 `5`，`0` 为未评分，读取规则与 `rating.get` 相同。foobar2000 没有该曲目的信息时只读播放统计，统计里没有评分就不带。 |
| `tracks[].playCount` | `integer` | 文件的 `PLAY_COUNT` 标签，有这个标签时才带；不是播放统计。 |
| `tracks[].title` | `string` | 第一个 `TITLE` 值，有这个标签时才带。 |
| `tracks[].artist` | `string` | 全部 `ARTIST` 值以 `, ` 连接，有这个标签时才带。 |
| `count` | `integer` | 变化的曲目数，至少为 `1`；可能超过 `tracks` 的长度。 |
| `handles` | `string[]` | 每首变化曲目的 `handle`，顺序不固定。`truncated` 时为空。要读取页面为它们显示的内容，把它们作为 `titleformat.evalFieldsBatch` 的 `paths` 传入。 |
| `truncated` | `boolean` | 一次变化超过 5000 首时为 `true`。这时 `handles` 为空，页面应重新读取它显示的全部内容。 |
| `fromHook` | `boolean` | 文件本身没变、而是 `foo_playcount` 之类的组件请求刷新它提供的字段时为 `true`。 |
| `timestamp` | `integer` | 事件发出的时间，单位为自 Unix 纪元起的毫秒数。 |
<!-- api-schema:end -->

## panel 事件

### panel:focus

<!-- api-schema:begin event:panel:focus -->
面板获得键盘焦点时派发（仅在面板启用 grabFocus 选项时触发）

发给它所涉及的窗口或面板里的页面。

**载荷**

事件不带字段。
<!-- api-schema:end -->

### panel:blur

<!-- api-schema:begin event:panel:blur -->
面板失去键盘焦点时派发（仅在面板启用 grabFocus 选项时触发）

发给它所涉及的窗口或面板里的页面。

**载荷**

事件不带字段。
<!-- api-schema:end -->

### panel:initialized

<!-- api-schema:begin event:panel:initialized -->
DUI 元素或 CUI 面板完成 WebView 实例初始化时派发（在 WebView 就绪时单次触发，此时页面可能尚未完成事件订阅；如需检测运行模式建议在页面启动时主动调用 window.getMode）

发给它所涉及的窗口或面板里的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `mode` | `"dui" \| "cui"` | 面板类型（dui 表示 Default UI 元素，cui 表示 Columns UI 面板） |
| `panelMode` | `boolean` | 是否处于面板模式（固定为 true） |
| `windowId` | `string` | 面板所属窗口唯一标识符 windowId（与 window.getMode 返回值一致） |
<!-- api-schema:end -->

### panel:visibilityChanged

<!-- api-schema:begin event:panel:visibilityChanged -->
DUI 元素可见性发生切换时派发（例如选项卡容器中切换标签页；Columns UI 面板不派发此事件）

发给它所涉及的窗口或面板里的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `visible` | `boolean` | 面板当前是否处于可见状态 |
<!-- api-schema:end -->

### panel:configChanged

<!-- api-schema:begin event:panel:configChanged -->
面板配置发生变更时派发（通过 panel.setConfig、面板设置对话框或宿主注入更新触发；数据结构与 panel.getConfig 返回的 config 字段一致）

发给它所涉及的窗口或面板里的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `panelName` | `string` | 面板自定义显示名称 |
| `templateName` | `string` | 面板加载的页面模板名称 |
| `edgeStyle` | `integer` | 面板边框样式（0 无边框、1 凹陷立体效果、2 纯灰边框） |
| `urlOverride` | `string` | 覆盖模板的自定义加载 URL（未设置时为空字符串） |
| `transparentBackground` | `boolean` | 面板是否启用透明背景渲染 |
| `grabFocus` | `boolean` | 点击面板时是否主动获取键盘焦点 |
| `enableDragDrop` | `boolean` | 是否允许向面板拖放文件 |
| `enableDevTools` | `boolean` | 是否启用 WebView2 开发者工具 F12/右键审查 |
<!-- api-schema:end -->

## playback 事件

### playback:trackChanged

<!-- api-schema:begin event:playback:trackChanged -->
一首曲目开始播放了：播放开始，或切到了另一首。载荷就是这首曲目，与 `playback.getCurrentTrack` 应答里的曲目行相同。流报出新标题（网络电台就会这样）时发的是 `playback:dynamicInfoTrack`。

发给所有窗口。

**载荷**

载荷是一个 [Track](../reference/types.md#track)。
<!-- api-schema:end -->

### playback:edited

<!-- api-schema:begin event:playback:edited -->
正在播放的曲目信息变了，比如标签被编辑了。载荷是这首曲目现在读到的样子。

发给所有窗口。

**载荷**

载荷是一个 [Track](../reference/types.md#track)。
<!-- api-schema:end -->

### playback:itemPlayed

<!-- api-schema:begin event:playback:itemPlayed -->
foobar2000 把正在播放的曲目记为已播放：播满了 60 秒，或播到结尾且至少播过三分之一。foobar2000 的播放统计就在这时记一次播放。

发给所有窗口。

**载荷**

载荷是一个 [Track](../reference/types.md#track)。
<!-- api-schema:end -->

### playback:stopAfterCurrentChanged

<!-- api-schema:begin event:playback:stopAfterCurrentChanged -->
foobar2000 的「播完当前曲目后停止」选项变了（经 `playback.setStopAfterCurrent`、foobar2000 的菜单或某个组件）。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 选项的新值。 |
<!-- api-schema:end -->

### playback:followCursorChanged

<!-- api-schema:begin event:playback:followCursorChanged -->
foobar2000 的「播放跟随光标」选项变了。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 选项的新值。 |
<!-- api-schema:end -->

### playback:cursorFollowChanged

<!-- api-schema:begin event:playback:cursorFollowChanged -->
foobar2000 的「光标跟随播放」选项变了。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 选项的新值。 |
<!-- api-schema:end -->

### playback:orderChanged

<!-- api-schema:begin event:playback:orderChanged -->
foobar2000 的播放顺序变了。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `orderIndex` | `integer` | 新顺序的序号，同 `playback.getPlaybackOrder` 应答里的 `order`。 |
| `order` | `integer` | 与 `orderIndex` 相同。 |
<!-- api-schema:end -->

### playback:stopped

<!-- api-schema:begin event:playback:stopped -->
播放停止了：用户停止、播到列表末尾、要开始另一首，或 foobar2000 正在退出。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `reason` | `"user" \| "eof" \| "starting_another" \| "shutting_down" \| "unknown"` | 停止原因：`user`、`eof`（列表播完）、`starting_another`、`shutting_down` 或 `unknown`，取 foobar2000 报的值。播放中调 `playback.next` 是 `starting_another`；播放中用 `playlist.playTrack` 播另一行是 `user`。 |
<!-- api-schema:end -->

### playback:paused

<!-- api-schema:begin event:playback:paused -->
播放暂停或恢复了。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `paused` | `boolean` | 暂停时为 `true`，恢复时为 `false`。 |
<!-- api-schema:end -->

### playback:seeked

<!-- api-schema:begin event:playback:seeked -->
正在播放的曲目跳转了位置。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `position` | `number` | 跳转到的位置，秒。 |
| `hostTime` | `number` | foobar2000 报出这次跳转时宿主的系统时间，Unix 纪元毫秒，带小数，与 `Date.now()` 是同一个时钟。`position` 是跳转目标，不是这一刻读到的位置；紧随其后的 `playback:timeHighRes` 仍可能带着旧位置。 |
<!-- api-schema:end -->

### playback:volumeChanged

<!-- api-schema:begin event:playback:volumeChanged -->
foobar2000 的音量变了，或静音、取消静音了。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `volume` | `number` | 线性百分比音量，`0` 到 `100`，同 `playback.getVolume`。 |
| `volumeDb` | `number` | 音量，dB；满音量为 `0`，最低为 `-100`。 |
| `muted` | `boolean` | foobar2000 是否静音。 |
| `isMuted` | `boolean` | 与 `muted` 相同。 |
<!-- api-schema:end -->

### playback:time

<!-- api-schema:begin event:playback:time -->
播放位置，播放时约每秒一次，即 foobar2000 给时间显示用的那个值。所有窗口都隐藏时不发。`playback:timeHighRes` 更精细。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `position` | `number` | 宿主给时间显示用的粗粒度播放位置，单位秒，不带采样时刻。同步画面请用 `playback:timeHighRes` 或 `playback.getPosition`。 |
<!-- api-schema:end -->

### playback:timeHighRes

<!-- api-schema:begin event:playback:timeHighRes -->
播放位置：播放且未暂停时约每 100 ms 一次，曲目开始、跳转或恢复播放后也立刻发一次。所有窗口都隐藏时不发。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `position` | `number` | 位置，单位秒，带小数部分；与 `playback.getPosition` 一样已扣除输出缓冲与 DSP 延迟，读数呈阶梯变化。 |
| `hostTime` | `number` | 读 `position` 时宿主的系统时间，Unix 纪元毫秒，带小数，与 `Date.now()` 是同一个时钟。`Date.now() - hostTime` 是事件晚到了多久，`position + (Date.now() - hostTime) / 1000` 可估出当前位置。 |
<!-- api-schema:end -->

### playback:dynamicInfo

<!-- api-schema:begin event:playback:dynamicInfo -->
解码器为正在播放的流报告了新的动态信息，比如 VBR 文件的码率、网络电台的标题。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `bitrate` | `integer` | 当前码率，kbit/s；未知时为 `0`。 |
| `streamTitle` | `string` | 动态信息里的第一个 `TITLE` 值，有才带。 |
<!-- api-schema:end -->

### playback:dynamicInfoTrack

<!-- api-schema:begin event:playback:dynamicInfoTrack -->
正在播放的流报出了新曲目，网络电台就会这样。既没有艺术家也没有标题时不发。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `artist` | `string` | 新曲目的艺术家，以 `, ` 连接；没有时不带。 |
| `title` | `string` | 新曲目的第一个标题；没有时不带。 |
<!-- api-schema:end -->

### playback:starting

<!-- api-schema:begin event:playback:starting -->
播放即将开始，起因是播放、下一首、上一首或随机命令。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `command` | `"play" \| "next" \| "previous" \| "random" \| "unknown"` | 启动播放的命令。 |
| `paused` | `boolean` | 是否以暂停状态开始。 |
<!-- api-schema:end -->

### playback:stateChanged

<!-- api-schema:begin event:playback:stateChanged -->
播放状态变了：曲目开始和恢复播放时为 `playing`，暂停或以暂停状态开始播放时为 `paused`，播放以 `user` 或 `eof` 原因停止时为 `stopped`（原因见 `playback:stopped`）。`playback.next` 以 `starting_another` 停止当前曲目，不发 `stopped`；而用 `playlist.playTrack` 播另一行时，foobar2000 报的是 `user` 停止，所以新曲目的事件之前会先来一条 `stopped`。曲目开始播放时，紧跟在 `playback:trackChanged` 之后的那条带的是这首曲目的 `canSeek`；在它之前随 `playback:starting` 发出的那条，曲目还没打开，`canSeek` 可能为 `false`。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `state` | `"playing" \| "paused" \| "stopped"` | 新状态。 |
| `position` | `number` | 发事件时读到的播放位置，单位秒；输出延迟补偿与状态切换后的瞬态同 `playback.getPosition`。 |
| `duration` | `number` | 发事件时读到的当前曲目时长，秒。 |
| `canSeek` | `boolean` | 发事件时读到的当前曲目能否定位，同 `playback.getState` 应答里的 `canSeek`；`stopped` 时恒为 `false`。 |
| `hostTime` | `number` | 读 `position` 时宿主的系统时间，Unix 纪元毫秒，带小数，与 `Date.now()` 是同一个时钟。 |
<!-- api-schema:end -->

### playback:queueChanged

<!-- api-schema:begin event:playback:queueChanged -->
foobar2000 的播放队列变了。一次改动队列多次的 `queue.*` 调用只在最后发一个事件。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `origin` | `"user_added" \| "user_removed" \| "playback_advance" \| "unknown"` | 队列因何变化：`user_added`、`user_removed`、`playback_advance`（播放取走了下一条排队曲目）或 `unknown`。 |
| `count` | `integer` | 变化后的队列长度。 |
| `contentVersion` | `string` | 已包含这次变化。 |
<!-- api-schema:end -->

## playlist 事件

### playlist:defaultFormatChanged

<!-- api-schema:begin event:playlist:defaultFormatChanged -->
foobar2000 报告它的默认播放列表格式变了。

发给所有窗口。

**载荷**

事件不带字段。
<!-- api-schema:end -->

### playlist:itemsAdded

<!-- api-schema:begin event:playlist:itemsAdded -->
曲目插入了某个播放列表。JIT 队列的隐藏播放列表不发。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 播放列表的序号。 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `start` | `integer` | 第一条插入曲目所在的行。 |
| `count` | `integer` | 插入的曲目数。 |
| `contentVersion` | `string` | 已包含这次变化。 |
<!-- api-schema:end -->

### playlist:itemsRemoved

<!-- api-schema:begin event:playlist:itemsRemoved -->
曲目从某个播放列表里移除了。JIT 队列的隐藏播放列表不发。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 播放列表的序号。 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `oldCount` | `integer` | 移除前的曲目数。 |
| `newCount` | `integer` | 移除后的曲目数。 |
| `contentVersion` | `string` | 已包含这次变化。 |
<!-- api-schema:end -->

### playlist:itemsReordered

<!-- api-schema:begin event:playlist:itemsReordered -->
某个播放列表的曲目顺序变了。JIT 队列的隐藏播放列表不发。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 播放列表的序号。 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `count` | `integer` | 播放列表的曲目数。 |
| `contentVersion` | `string` | 已包含这次变化。 |
<!-- api-schema:end -->

### playlist:selectionChanged

<!-- api-schema:begin event:playlist:selectionChanged -->
某个播放列表的选择变了，用 `playlist.getSelection` 读取。JIT 队列的隐藏播放列表不发。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 播放列表的序号。 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
<!-- api-schema:end -->

### playlist:focusChanged

<!-- api-schema:begin event:playlist:focusChanged -->
某个播放列表的焦点行移动了。JIT 队列的隐藏播放列表不发。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 播放列表的序号。 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `from` | `integer` | 之前的焦点行；没有时为 `-1`。 |
| `to` | `integer` | 现在的焦点行；没有时为 `-1`。 |
<!-- api-schema:end -->

### playlist:itemsReplaced

<!-- api-schema:begin event:playlist:itemsReplaced -->
某个播放列表的曲目被原位换成了别的曲目。JIT 队列的隐藏播放列表不发。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 播放列表的序号。 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `count` | `integer` | 被替换的曲目数。 |
| `contentVersion` | `string` | 已包含这次变化。 |
<!-- api-schema:end -->

### playlist:created

<!-- api-schema:begin event:playlist:created -->
新建了一个播放列表。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `index` | `integer` | 新播放列表的序号。 |
| `guid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `name` | `string` | 它的名字。 |
<!-- api-schema:end -->

### playlist:removed

<!-- api-schema:begin event:playlist:removed -->
移除了一个或多个播放列表。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `oldCount` | `integer` | 移除前的播放列表数。 |
| `newCount` | `integer` | 移除后的播放列表数。 |
| `indices` | `integer[]` | 被移除的播放列表在移除之前的序号，升序。 |
| `guids` | `string[]` | 被移除的播放列表的 GUID，与 `indices` 同序；foobar2000 事先没有预告就报告移除、GUID 已无从读取时为空数组。 |
<!-- api-schema:end -->

### playlist:reordered

<!-- api-schema:begin event:playlist:reordered -->
播放列表的顺序变了。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | 播放列表数。 |
| `guids` | `string[]` | 全部播放列表的 GUID，按新的顺序。 |
<!-- api-schema:end -->

### playlist:activated

<!-- api-schema:begin event:playlist:activated -->
活动播放列表变了。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `oldIndex` | `integer` | 之前的活动播放列表序号；没有时为 `-1`。 |
| `newIndex` | `integer` | 现在的活动播放列表序号；没有时为 `-1`。 |
| `newGuid` | `string \| null` | 现在的活动播放列表的 GUID；没有时为 `null`。之前的活动播放列表不给 GUID：它可能正是刚被移除的那张。 |
<!-- api-schema:end -->

### playlist:renamed

<!-- api-schema:begin event:playlist:renamed -->
某个播放列表改了名。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `index` | `integer` | 播放列表的序号。 |
| `guid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `name` | `string` | 新名字。 |
<!-- api-schema:end -->

### playlist:autoplaylistUIResult

<!-- api-schema:begin event:playlist:autoplaylistUIResult -->
已受理的自动列表原生编辑器调用已返回或失败。每个操作至多发一次，只投递给原调用文档；导航或销毁后丢弃。`success` 只描述原生调用，不表示保存、取消或编辑器生命周期。

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operationId` | `string` | 与受理响应相同的不透明标识，用于关联本次操作。 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `success` | `boolean` | 原生调用是否正常返回；锁入口还须返回 `true`。 |
| `error` | `string` | 仅在失败时出现，与 `code`、`reason` 一起提供。 |
| `code` | `string` | 业务错误码，仅在 `success` 为 `false` 时出现。 |
| `reason` | `"targetGone" \| "notAutoplaylist" \| "noEditor" \| "queryFailed" \| "dispatchFailed" \| "providerFailed"` | 失败的检查或原生调用环节，仅在 `success` 为 `false` 时出现。 |
<!-- api-schema:end -->

请先订阅，再调用 [`playlist.showAutoplaylistUI`](playlist.md#playlist-showautoplaylistui)：事件可能早于页面处理受理响应。按 `operationId` 关联，不按到达顺序判断；[SDK 有界示例](../sdk/playlist.md#onautoplaylistuiresult-callback) 展示早到结果处理及清理。失败载荷始终带 `error`、`code` 和 `reason`，成功载荷不带这三个字段。

结果只描述一次原生调用，非模态编辑器可能在调用返回后仍然打开。它不能代替曲目或锁事件，也不报告 saved、cancelled 或 closed 状态。导航或销毁后丢弃投递，不转发到其他文档或主窗口。原文档仍有效但目标已删除时，`NOT_FOUND` / `targetGone` 优先于同时发生的提供者异常。

### playlist:lockChanged

<!-- api-schema:begin event:playlist:lockChanged -->
某个播放列表加上或去掉了锁。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 播放列表的序号。 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `locked` | `boolean` | 现在是否有锁。 |
<!-- api-schema:end -->

### playlist:addComplete

<!-- api-schema:begin event:playlist:addComplete -->
一次 `playlist.addPathsAsync` 调用加完了它的路径。需要展开的路径（如 `.cue`、`.m3u`）先展开；所有路径都能直接加入时，紧跟调用发出。以错误应答的调用不发。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operationId` | `string` | 调用应答里的 id。 |
| `playlistGuid` | `string` | 调用加入的那张播放列表的 GUID，取调用解析出的那张；期间被挪动仍指向它，期间被删掉就不再属于任何列表。 |
| `success` | `boolean` | 恒为 `true`；调用如何结束看 `status`。 |
| `status` | `"completed" \| "partial" \| "failed" \| "cancelled"` | `completed` 表示路径解析出的曲目全部加入。`partial`、`failed` 表示其中部分或全部被丢弃，原因见 `reason`；被丢弃的通常是从播放列表文件与 cue 展开的曲目，因为普通文件在调用返回前已加入。`cancelled` 表示用户取消了展开，此前加入的曲目留在列表里。 |
| `reason` | `"targetRemoved" \| "locked"` | 曲目被丢弃的原因：加入之前列表被删掉（`targetRemoved`）或上锁（`locked`）。`status` 为 `partial`、`failed` 时才有。 |
| `addedCount` | `integer` | 展开播放列表与 cue 之后加入的曲目数。展开完成前列表被删掉或上锁而丢弃的曲目不计入。 |
| `totalCount` | `integer` | 调用受理的路径数。 |
<!-- api-schema:end -->

## plugin 事件

### plugin:registered

<!-- api-schema:begin event:plugin:registered -->
外部插件通过 C++ 的 `PluginRegistry` 注册了自己的命名空间。重复注册同一命名空间只更新信息，不发此事件。此时插件还没有方法，`apis` 为空、`apiCount` 为 `0`；之后每个方法各发一次 `api:registered`。

发给所有窗口。

**载荷**

载荷是一个 [SystemPluginInfo](../reference/types.md#systemplugininfo)。
<!-- api-schema:end -->

### plugin:unregistered

<!-- api-schema:begin event:plugin:unregistered -->
外部插件被注销。`apis` 列出随它一起移除的方法；这些方法不会逐个发 `api:unregistered`。

发给所有窗口。

**载荷**

载荷是一个 [SystemPluginInfo](../reference/types.md#systemplugininfo)。
<!-- api-schema:end -->

## port 事件

### port:connected

<!-- api-schema:begin event:port:connected -->
端口建立连接广播事件（由 port.connect 触发，向当前所有窗口广播派发）

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `portId` | `string` | 新建端口的唯一标识符 portId（与 port.connect 返回一致） |
| `name` | `string` | 端口所属的频道名称 |
| `windowId` | `string` | 打开该端口的源窗口标识符 windowId（若无法匹配到具体窗口则固定为 main） |
<!-- api-schema:end -->

### port:disconnected

<!-- api-schema:begin event:port:disconnected -->
端口断开连接广播事件（由 port.disconnect 显式关闭、页面导航或宿主 WebView 销毁触发，向当前所有窗口广播派发）

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `portId` | `string` | 已关闭端口的唯一标识符 portId |
| `name` | `string` | 该端口原本所属的频道名称 |
| `windowId` | `string` | 打开该端口的源窗口标识符 windowId |
<!-- api-schema:end -->

### port:message

<!-- api-schema:begin event:port:message -->
端口消息到达事件（定向派发给接收端口所属的宿主窗口；通过 port.postMessage 广播时频道内所有对等端口均会收到，通过 port.postMessageTo 单播时仅目标端口收到）

发给订阅或任务所属的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `portId` | `string` | 接收该消息的本窗口端口标识符 portId |
| `sourcePortId` | `string` | 发送该消息的源端口标识符 sourcePortId |
| `sourceWindowId` | `string` | 发送方页面所在的源窗口标识符 sourceWindowId（若无法匹配窗口则固定为 main） |
| `message` | `any` | 发送方传入的消息正文数据（原样透传，非 null） |
<!-- api-schema:end -->

## selection 事件

### selection:changed

<!-- api-schema:begin event:selection:changed -->
foobar2000 全局选区状态变更时广播（两次变更间隔低于 50 ms 时直接丢弃而非延迟派发，连续快速变更中最后一次可能不会触发事件；若需获取最新确切状态请主动调用 selection.get 查询）

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | 当前选中的曲目总数 |
| `type` | `"now_playing" \| "active_playlist_selection" \| "active_playlist" \| "playlist_manager" \| "media_library_viewer" \| "unknown"` | 选区来源类型（取值与 selection.getType 返回的 typeName 一致） |
| `handles` | `string[]` | 前 100 首选中曲目的句柄列表（保持选区原始顺序；原生文件路径，子曲目序号大于 0 时附加 \|subsong:N） |
| `truncated` | `boolean` | 选中曲目总数是否超过 100 条（为 true 时 handles 仅截断保留前 100 首） |
| `track` | [Track](../reference/types.md#track) | 当前选中的单首曲目元数据（仅在全局选区恰好包含唯一一首曲目时提供） |
| `nowPlaying` | [Track](../reference/types.md#track) | 当前已载入播放核心的曲目元数据（包含正在播放或处于暂停状态；未处于播放相关状态时缺省） |
<!-- api-schema:end -->

## state 事件

### state:changed

<!-- api-schema:begin event:state:changed -->
调用 `state.set` 写入共享键值（且未设置 `silent: true`）时触发；若写入前目标键已过期但尚未被清理，`previousValue` 仍记录该旧值且不再广播过期事件

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `key` | `string` | 发生变更的存储键名 |
| `value` | `any` | 存入的键值；不会为 `null`（`state.set` 将 `null` 视作参数缺失） |
| `previousValue` | `any` | 写入前该键原有的旧值；若此前不存在则为 `null` |
| `sourceWindowId` | `string` | 写入该键值的源窗口 ID；无法匹配调用方窗口时为 `main` |
| `expiresAt` | `integer` | 键值的过期时间戳（自 Unix 纪元起的毫秒数）；仅在写入时指定了正数 `ttlMs` 时提供 |
<!-- api-schema:end -->

### state:deleted

<!-- api-schema:begin event:state:deleted -->
共享状态中的键被显式删除或存活期耗尽过期时触发；过期键仅在调用 `state.get` 或 `state.keys` 触发延迟清理时广播事件（事件时间为清理触发时刻而非实际到期时刻），若主动删除已过期但尚未清理的键，事件原因记为 `deleted`

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `key` | `string` | 被删除或过期的存储键名 |
| `sourceWindowId` | `string` | 执行删除操作的源窗口 ID；无法匹配调用方窗口时为 `main`，因超时过期被清理时为空字符串 `""` |
| `reason` | `"deleted" \| "expired"` | 移除原因：`deleted` 表示由 `state.delete` 显式删除，`expired` 表示存活期满过期清理 |
<!-- api-schema:end -->

## system 事件

### system:themeChanged

<!-- api-schema:begin event:system:themeChanged -->
foobar2000 默认界面（Default UI）配色或深浅色模式发生变更时触发；仅宿主在 Default UI 面板内的页面会收到此事件（与 `ui:coloursChanged` 同步触发），Columns UI 面板及独立弹窗不接收此事件

发给它所涉及的窗口或面板里的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `darkMode` | `boolean` | Default UI 当前是否处于深色模式；该状态由 foobar2000 自身外观设置决定，可能与 `system.getTheme` 返回的 Windows 系统主题不一致 |
<!-- api-schema:end -->

## taskbar 事件

### taskbar:buttonClicked

<!-- api-schema:begin event:taskbar:buttonClicked -->
缩略图工具栏自定义按钮被点击时广播（未配置自定义按钮前显示的默认原生播放控制按钮不派发此事件）

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | `string` | 触发点击事件的按钮唯一标识符 id |
<!-- api-schema:end -->

## tray 事件

### tray:beforeContextMenu

<!-- api-schema:begin event:tray:beforeContextMenu -->
鼠标右键点击托盘图标且上下文菜单即将展开前广播（菜单弹出过程不阻塞等待前端响应，事件处理函数中对菜单结构的修改将于下一次右键时生效；即使当前菜单无可见菜单项导致未弹出菜单亦会派发此事件）

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `x` | `integer` | 托盘菜单弹出的屏幕横坐标锚点（像素） |
| `y` | `integer` | 托盘菜单弹出的屏幕纵坐标锚点（像素） |
<!-- api-schema:end -->

### tray:click

<!-- api-schema:begin event:tray:click -->
系统托盘图标被鼠标左键单击释放时广播（仅在 event 点击模式下派发；鼠标右键触发上下文菜单时派发 tray:beforeContextMenu）

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `button` | `integer` | 鼠标按键索引（固定为 0，表示鼠标左键） |
| `x` | `integer` | 光标所在的屏幕横坐标（像素） |
| `y` | `integer` | 光标所在的屏幕纵坐标（像素） |
<!-- api-schema:end -->

### tray:doubleClick

<!-- api-schema:begin event:tray:doubleClick -->
系统托盘图标被鼠标左键双击时广播（仅在 event 点击模式下派发）

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `x` | `integer` | 光标所在的屏幕横坐标（像素） |
| `y` | `integer` | 光标所在的屏幕纵坐标（像素） |
<!-- api-schema:end -->

### tray:menuItemClicked

<!-- api-schema:begin event:tray:menuItemClicked -->
自定义托盘菜单项被点击或内嵌富控件值发生变更时广播（内置播放/系统项及声明了 playbackAction 的项由宿主原生直接执行而不派发此事件；在 render: 'webview' 模式下富控件每次变更均会实时派发且菜单保持展开，在默认 native 模式下每次点击后菜单自动关闭、滑块离散为五档、分段项降级为普通项且不携带 value）

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | `string` | 被触发菜单项的唯一标识符 id（无 id 的项为空字符串） |
| `value` | `integer` | 富控件的最新数值（rating 返回 0 至 5 星，0 表示清除评分；slider 返回 min 至 max 范围内的整数；segmented 返回当前选中分段的索引；普通菜单项不附带该字段） |
<!-- api-schema:end -->

## ui 事件

### ui:coloursChanged

<!-- api-schema:begin event:ui:coloursChanged -->
foobar2000 Default UI 宿主配色方案变更事件；仅投递给承载于 Default UI 面板中的页面（与 `system:themeChanged` 联动派发；Columns UI 面板与插件独立弹窗不接收此事件）

发给它所涉及的窗口或面板里的页面。

**载荷**

事件不带字段。
<!-- api-schema:end -->

### ui:fontChanged

<!-- api-schema:begin event:ui:fontChanged -->
foobar2000 Default UI 宿主字体配置变更事件；仅投递给承载于 Default UI 面板中的页面（Columns UI 面板与插件独立弹窗不接收此事件）

发给它所涉及的窗口或面板里的页面。

**载荷**

事件不带字段。
<!-- api-schema:end -->

### ui:menuItemClicked

<!-- api-schema:begin event:ui:menuItemClicked -->
`ui.showCustomMenu` 原生菜单项选中事件；定向投递至发起菜单调用的页面窗口（附带选中的 `{ id, label }`；用户取消菜单时不触发）

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | `string` | 被点击菜单项的标识符 id（未配置 id 时为空字符串 `""`） |
| `label` | `string` | 被点击菜单项的显示文本（不含右侧快捷键提示；未配置 label 时为空字符串 `""`） |
<!-- api-schema:end -->

### ui:toast

<!-- api-schema:begin event:ui:toast -->
由 `ui.showToast` 触发的 Toast 通知渲染请求；定向投递给发起调用窗口，由前端按载荷参数自行实现视觉呈现

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `message` | `string` | Toast 提示正文文本内容 |
| `duration` | `integer` | 提示显示时长（毫秒；未指定时默认 3000，不执行范围校验） |
| `type` | `"info" \| "success" \| "warning" \| "error"` | Toast 提示类型：info、success、warning 或 error（未指定时默认 info） |
| `position` | `string` | 提示在屏幕中的对齐角落位置（未指定时默认 "bottom-right"） |
<!-- api-schema:end -->

## webview 事件

### webview:processFailed

<!-- api-schema:begin event:webview:processFailed -->
广播本插件关联的 WebView2 核心或辅助进程故障事件。向所有窗口广播；若故障不可恢复（`recovered` 为 `false`），崩溃窗口本身不接收此事件

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `kind` | `"browserProcessExited" \| "renderProcessExited" \| "renderProcessUnresponsive" \| "frameRenderProcessExited" \| "utilityProcessExited" \| "sandboxHelperProcessExited" \| "gpuProcessExited" \| "ppapiPluginProcessExited" \| "ppapiBrokerProcessExited" \| "unknownProcessExited"` | 发生故障的进程类型（未知或未识别的种类返回 `unknownProcessExited`） |
| `kindRaw` | `integer` | WebView2 原始报告的 `COREWEBVIEW2_PROCESS_FAILED_KIND` 枚举数值 |
| `recovered` | `boolean` | WebView 实例是否恢复可用。渲染进程重载成功或运行时自行重启时为 `true`；重载失败或浏览器主进程退出时为 `false` |
| `recoveryAction` | `"reload" \| "needRebuild" \| "none"` | 插件采取的故障恢复动作。渲染进程故障时为 `reload`（恢复结果见 `recovered`）；浏览器主进程退出且需重建实例时为 `needRebuild`；运行时自行重启时为 `none` |
<!-- api-schema:end -->

## window 事件

### window:activated

<!-- api-schema:begin event:window:activated -->
主窗口被激活或失去激活。只有主窗口的页面会收到；popup 与面板收不到。

发给它所涉及的窗口或面板里的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `active` | `boolean` | 主窗口现在是否处于激活状态。 |
<!-- api-schema:end -->

### window:dpiChanged

<!-- api-schema:begin event:window:dpiChanged -->
主窗口的 DPI 变了：移到了另一块显示器，或显示器的缩放改了。只有主窗口的页面会收到。尺寸是新 DPI 下的物理像素；宿主同时更新页面上的标题栏 CSS 变量。

发给它所涉及的窗口或面板里的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `dpi` | `integer` | 新的 DPI；96 对应 100% 缩放。 |
| `dpiScale` | `number` | `dpi / 96`。 |
| `titlebarHeight` | `integer` | 标题栏高度。 |
| `captionButtonWidth` | `integer` | 一个标题栏按钮的宽度。 |
| `captionButtonsWidth` | `integer` | 三个标题栏按钮的总宽度，即 `captionButtonWidth * 3`。 |
<!-- api-schema:end -->

### window:stateChanged

<!-- api-schema:begin event:window:stateChanged -->
主窗口最大化、最小化、还原、激活、失去激活，或进入、退出全屏；popup 只在进入或退出全屏时发。它发给所有窗口，`windowId` 说明是哪个窗口变了：只关心自己窗口的页面拿它与 `window.getCurrentWindowId` 比较。主窗口只在四种状态之一与上次发出的不同时才发。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `windowId` | `string` | 状态变了的窗口的 id：主窗口为 `main`，否则为 popup 的 id。与该窗口的页面从 `window.getCurrentWindowId` 得到的值相同。 |
| `isMaximized` | `boolean` | 窗口是否最大化。 |
| `isMinimized` | `boolean` | 窗口是否最小化。 |
| `maximized` | `boolean` | 同 `isMaximized`。 |
| `minimized` | `boolean` | 同 `isMinimized`。 |
| `isActive` | `boolean` | 窗口是否处于激活状态。 |
| `active` | `boolean` | 同 `isActive`。 |
| `isFullscreen` | `boolean` | 窗口是否全屏。 |
| `fullscreen` | `boolean` | 同 `isFullscreen`。 |
<!-- api-schema:end -->

### window:backdropStateChanged

<!-- api-schema:begin event:window:backdropStateChanged -->
主窗口或 popup 的背景变了：换了效果、在激活与失焦两种变体之间切换，或被强制刷新重新写入。只在背景写入成功之后发。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `windowId` | `string` | 窗口 id；主窗口为 `main`。 |
| `active` | `boolean` | 窗口是否处于激活状态。 |
| `mode` | `"active" \| "inactive"` | 写入的是哪种变体，与 `active` 一致。 |
| `effect` | `"none" \| "system" \| "mica" \| "mica-alt" \| "acrylic"` | 实际使用的效果：`inherit` 已解析，窗口显示不了的效果已回退；`system` 表示由平台绘制边框背景。 |
<!-- api-schema:end -->

### window:beforeClose

<!-- api-schema:begin event:window:beforeClose -->
创建时带 `beforeClose` 的 popup 正在被关闭（关闭按钮或 `window.closePopup`）。它保持打开，直到页面调用 `window.confirmClose` 或 `window.cancelClose`；3 秒内两者都没调用，它照样关闭。

发给它所涉及的窗口或面板里的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `windowId` | `string` | popup 的 id。 |
<!-- api-schema:end -->

### window:behaviorChanged

<!-- api-schema:begin event:window:behaviorChanged -->
`window.setPopupBehavior` 改了 popup 的行为预设或覆盖项。载荷与 `window.getPopupBehavior` 的返回字段相同。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `windowId` | `string` | popup 的 id。 |
| `profile` | `"legacy" \| "standard" \| "miniPlayer" \| "desktopLyrics"` | 行为预设；创建时没给预设为 `legacy`。 |
| `behavior` | `Record<string, any>` | 在 popup 上设置的行为覆盖。 |
| `resolvedBehavior` | `WindowPopupBehaviorState` | 实际生效的行为。 |
| `resolvedBehavior.showInTaskbar` | `boolean` | 是否出现在任务栏上。 |
| `resolvedBehavior.showInAltTab` | `boolean` | 是否出现在 Alt+Tab 里。 |
| `resolvedBehavior.keepVisibleOnShowDesktop` | `boolean` | 显示桌面时是否仍然可见。 |
| `resolvedBehavior.allowMinimize` | `boolean` | 能否最小化。 |
| `resolvedBehavior.owner` | `"none" \| "main"` | `main` 让 popup 位于主窗口之上并随主窗口最小化；`none` 让它独立。 |
| `resolvedBehavior.noActivate` | `boolean` | 显示或点击 popup 时是否不抢焦点。 |
<!-- api-schema:end -->

### window:hoverStateChanged

<!-- api-schema:begin event:window:hoverStateChanged -->
光标进入或离开了一个让点击穿透的 popup（`window.setClickThrough`）。这样的窗口收不到鼠标消息，所以点击穿透开着时由宿主轮询光标位置；关闭点击穿透时发一次 `hovering: false`。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `windowId` | `string` | popup 的 id。 |
| `hovering` | `boolean` | 光标是否在 popup 上。 |
<!-- api-schema:end -->

### window:message

<!-- api-schema:begin event:window:message -->
某个页面用 `window.sendMessage` 或 `window.broadcast` 发了消息。定向消息只发给指定的窗口；广播发给发送方以外的所有窗口，找不到发送方窗口时发给所有窗口。

发给调用方指定的窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `sourceWindowId` | `string` | 发送方页面所在窗口的 id：`main`、popup id 或面板 id；对不上窗口时为 `main`。 |
| `message` | `any` | 发送方传入的消息，原样送达。 |
<!-- api-schema:end -->

### window:minimizeSuppressed

<!-- api-schema:begin event:window:minimizeSuppressed -->
行为带 `keepVisibleOnShowDesktop` 的 popup 忽略了一次最小化命令。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `windowId` | `string` | popup 的 id。 |
| `reason` | `string` | 忽略最小化的原因；目前恒为 `policy.keepVisibleOnShowDesktop`。 |
<!-- api-schema:end -->

### window:popupOpened

<!-- api-schema:begin event:window:popupOpened -->
创建了一个 popup。宿主自己用来显示菜单的窗口不发。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `windowId` | `string` | popup 的 id。 |
| `title` | `string` | 创建 popup 时给的标题。 |
| `url` | `string` | 创建 popup 时给的 URL。 |
<!-- api-schema:end -->

### window:popupClosed

<!-- api-schema:begin event:window:popupClosed -->
一个 popup 关闭了，页面已销毁。宿主自己用来显示菜单的窗口不发。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `windowId` | `string` | popup 的 id。 |
<!-- api-schema:end -->

### window:alwaysOnTopChanged

<!-- api-schema:begin event:window:alwaysOnTopChanged -->
foobar2000 自己的「总在最前」选项变了（经它的菜单或某个组件）。与本插件 popup 的置顶状态无关。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 选项的新值。 |
<!-- api-schema:end -->

## 说明

- `playback:stopAfterCurrentChanged` 的 payload 为 `{ enabled }`，与对应 API 字段相同。
- 播放列表生命周期 payload 的字段在后续版本中可能增加；请把未知键视为增量而非异常。
- 每一行的精确 emit source 和字段集以组件源码中的 C++ `EmitEvent` / `BroadcastEvent` 调用点为准。
### playlist:autoplaylistConfigChanged

<!-- api-schema:begin event:playlist:autoplaylistConfigChanged -->
本组件管理的智能列表配置已变化，包括观察到的保持排序标志变化。用 getAutoplaylistConfig 重读快照。这不是曲目内容版本或写盘通知，不为外部提供者发送。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `revision` | `string` | 观察到的配置版本，编辑前应重新读取配置。 |
<!-- api-schema:end -->

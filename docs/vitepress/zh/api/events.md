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
| `sampleRate` | `integer` | 解码出的采样率，Hz。 |
| `channels` | `integer` | 解码出的声道数。 |
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
`cursor.setHidden` 隐藏或恢复了调用窗口的光标；没有改变状态的调用不发。

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `hidden` | `boolean` | 光标现在是否隐藏。 |
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
| `keyState` | `integer` | 放下时的 Win32 修饰键与鼠标按键掩码（`MK_*` 标志），供需要按修饰键区分行为的页面使用。它不影响报告给拖动源的放置效果，那个从不是移动或链接。 |
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
| `pathsUnavailableReason` | `"register-failed" \| "forward-unavailable" \| "inner-target-not-found" \| "chain-failed" \| "displaced" \| "origin-untrusted"` | `paths` 为 `false` 的原因；出现即表示 `paths` 为 `false`。 |
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
`file.copyAsync`、`file.moveAsync` 或 `file.deleteAsync` 的结果，分批送达：积满 64 条或距上一批过了 100 ms 就发一批，最后不满一批的那些在 `file:opComplete` 之前送达。发给发起操作的页面；那时该窗口已不在的，发给主窗口的页面。

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operationId` | `string` | 发起操作的调用回执里的 id。 |
| `op` | `"copy" \| "move" \| "delete"` | 哪种操作：`copy`、`move` 或 `delete`。 |
| `done` | `integer` | 到目前为止各批累计上报的条目数，含失败的。 |
| `total` | `integer` | 调用受理的条目数。 |
| `results` | `FileOpResultItem[]` | 仅本批的结果，顺序与请求一致。 |
| `results[].source` | `string` | 请求里的源路径，原样，不展开路径变量。 |
| `results[].destination` | `string` | 请求里的目标路径，原样；`file.deleteAsync` 没有。 |
| `results[].status` | `"ok" \| "skipped" \| "failed"` | 完成为 `ok`；因目标已存在或操作先被取消而有意没做为 `skipped`；其余为 `failed`。 |
| `results[].reason` | `"already-exists" \| "not-found" \| "permission" \| "cross-volume" \| "path-too-long" \| "io-error" \| "cancelled"` | 原因：`skipped` 配 `already-exists` 或 `cancelled`；`failed` 配 `not-found`、`permission`、`path-too-long` 或 `io-error`；`ok` 配 `cross-volume`，表示跨卷移动以复制加删除完成。`path-too-long` 表示这一条用到的某个路径（包括复制的文件夹里面的）超过 259 个字符，或要新建的文件夹超过 247 个字符。条目直接完成时不出现。 |
<!-- api-schema:end -->

### file:opComplete

<!-- api-schema:begin event:file:opComplete -->
一次操作的最后一个事件，取消之后、有条目失败时也会发。只有 foobar2000 正在退出或 worker 意外失败时不发，所以靠它收尾的监听方应自带超时。之后 `operationId` 失效，对它调用 `file.cancelOp` 报 `cancelled: false`。投递范围同 `file:opProgress`。

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operationId` | `string` | 发起操作的调用回执里的 id。 |
| `op` | `"copy" \| "move" \| "delete"` | 哪种操作：`copy`、`move` 或 `delete`。 |
| `total` | `integer` | 调用受理的条目数。 |
| `successCount` | `integer` | 上报为 `ok` 的条目数。 |
| `skippedCount` | `integer` | 上报为 `skipped` 的条目数。 |
| `failureCount` | `integer` | 上报为 `failed` 的条目数。 |
| `cancelled` | `boolean` | 至少有一个条目以 `cancelled` 为原因上报时为 `true`。 |
<!-- api-schema:end -->

## http 事件

### http:response

<!-- api-schema:begin event:http:response -->
异步的 `http.get`、`http.post`、`http.put`、`http.delete`、`http.patch` 或 `http.head` 的结果，带回执里的 `requestId`。成功时带同步响应的各字段；失败时带 `error` 与 `code`，同失败的调用。发给发起请求的页面；找不到它时发给同一顶层窗口下的页面，再没有就发给主窗口的页面。

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `requestId` | `string` | 回执里的 id。 |
| `success` | `boolean` | 是否拿到了响应。`404` 之类的错误状态码也算成功。 |
| `status` | `integer` | HTTP 状态码：成功时是响应的状态码；失败时只在 `redirect: "error"` 拒绝了重定向时出现，是该重定向的状态码。 |
| `headers` | `Record<string, string>` | 响应头，以名称为键；重复的响应头取最后一个。成功时出现。 |
| `body` | `string` | 响应体，按 `responseType` 为文本或 Base64；`http.head` 恒为空。成功时出现。 |
| `responseType` | `"text" \| "base64"` | `text` 或 `base64`，即 `body` 的编码。成功时出现。 |
| `contentLength` | `integer` | 数字形式的 `Content-Length` 响应头；`http.head` 成功且服务器发来可解析的值时出现。 |
| `error` | `string` | 请求失败的原因；失败时出现。 |
| `code` | `"INVALID_PARAMS" \| "PERMISSION_DENIED" \| "CANCELLED" \| "OPERATION_FAILED"` | 错误码：URL 不是 `http` 或 `https` 时为 `INVALID_PARAMS`，宿主设置不允许的本地或私有网络地址为 `PERMISSION_DENIED`，`http.abort` 之后为 `CANCELLED`，其余为 `OPERATION_FAILED`。失败时出现。 |
| `cancelled` | `boolean` | 请求被 `http.abort` 取消时为 `true`；否则不出现。 |
<!-- api-schema:end -->

### http:downloadComplete

<!-- api-schema:begin event:http:downloadComplete -->
带 `async: true` 的 `http.download` 的结果，带回执里的 `requestId`。成功时带同步下载的各字段；失败时带 `error` 与 `code`。投递范围同 `http:response`。

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `requestId` | `string` | 回执里的 id。 |
| `success` | `boolean` | 文件是否写成。`404` 之类的错误状态码的响应也会保存，算成功。 |
| `status` | `integer` | HTTP 状态码：成功时是响应的状态码；失败时只在 `redirect: "error"` 拒绝了重定向时出现，是该重定向的状态码。 |
| `bytesWritten` | `integer` | 写入的字节数；成功时出现。 |
| `path` | `string` | 写入的文件（已展开路径变量）；成功时出现。 |
| `error` | `string` | 下载失败的原因；失败时出现。 |
| `code` | `"INVALID_PARAMS" \| "PERMISSION_DENIED" \| "CANCELLED" \| "OPERATION_FAILED"` | 错误码，同 `http:response`。失败时出现。 |
| `cancelled` | `boolean` | 下载被 `http.abort` 取消时为 `true`；否则不出现。 |
<!-- api-schema:end -->

## jitQueue 事件

### jitQueue:trackChanged

<!-- api-schema:begin event:jitQueue:trackChanged -->
影子播放列表里的一首曲目开始播放，重播当前曲目也算。`trackId` 与 `title` 是会话当前曲目在 `jitQueue.playNow` 或 `jitQueue.enqueueNext` 里给的标识与标题。`jitQueue.preloadBatch` 插入的曲目没有标识：播到它们时两者不变，某一批开始播放之后两者都为空。JIT 事件发给开启会话的页面，即调用 `jitQueue.playNow` 或调用了开始播放的 `jitQueue.preloadBatch` 的页面；该页面已不在时发给同一顶层窗口下的页面，再没有就发给主窗口的页面。会话被别的页面重新开启后，原来的页面不再收到它的事件。还没有开启过会话时发给主窗口的页面。

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `trackId` | `string` | 会话当前曲目的标识；没有时为空。 |
| `title` | `string` | 会话当前曲目的标题；没有当前曲目或没给标题时为空。 |
<!-- api-schema:end -->

### jitQueue:needNext

<!-- api-schema:begin event:jitQueue:needNext -->
宿主需要下一首：缓冲里没有，也没有仍在等回复的请求。宿主在影子播放列表的曲目开始时检查（上一首期间没得到回复的请求就此作废），曲目剩 30 秒时再检查一次。页面以 `jitQueue.enqueueNext` 作答，没有更多曲目时以 `jitQueue.notifyEmpty` 作答。投递范围同 `jitQueue:trackChanged`。

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `currentTrackId` | `string` | 会话当前曲目的标识；没有时为空。 |
| `reason` | `"trackChange"` | 恒为 `trackChange`。 |
<!-- api-schema:end -->

### jitQueue:listExhausted

<!-- api-schema:begin event:jitQueue:listExhausted -->
调用了 `jitQueue.notifyEmpty`（没有活动会话时也发），或影子播放列表的播放在一首曲目结束时列表里已没有曲目。会话进入 `Exhausted`。投递范围同 `jitQueue:trackChanged`。

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `lastTrackId` | `string` | 会话当前曲目的标识；没有时为空。 |
<!-- api-schema:end -->

### jitQueue:preloadComplete

<!-- api-schema:begin event:jitQueue:preloadComplete -->
`jitQueue.preloadBatch` 把曲目插入了影子播放列表。在调用返回之前发出；调用失败时不发。投递范围同 `jitQueue:trackChanged`：开始播放的一批发给调用它的页面，追加到正在播放的会话里的一批发给该会话的页面。

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | 插入的曲目数；与调用返回的 `tracksAdded` 相同。 |
| `startIndex` | `integer` | 调用的 `startIndex`。 |
| `replace` | `boolean` | 调用的 `replace`。 |
<!-- api-schema:end -->

### jitQueue:error

<!-- api-schema:begin event:jitQueue:error -->
交给 `jitQueue.playNow` 或 `jitQueue.enqueueNext` 的曲目没能加入影子播放列表：位置解析不出曲目、影子播放列表已不存在，或加入时抛出异常。曲目在调用成功返回之后才加入，所以失败只以这个事件告知。投递范围同 `jitQueue:trackChanged`。

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `trackId` | `string` | 调用给该曲目的标识。 |
| `error` | `string` | 出错原因，英文。 |
| `url` | `string` | 位置，不是本地文件路径时出现。`url` 与 `path` 恰好出现一个。 |
| `path` | `string` | 位置，是本地文件路径时出现。 |
<!-- api-schema:end -->

## keyboard 事件

### keyboard:hotkey

<!-- api-schema:begin event:keyboard:hotkey -->
用 `keyboard.registerHotkey` 注册的热键被按下，发给注册它的窗口。该窗口关闭后热键仍然有效，之后的按键改发给主窗口的页面。

发给订阅或任务所属的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | `integer` | `keyboard.registerHotkey` 返回的 id。 |
| `key` | `string` | 注册时给的组合。 |
| `action` | `string` | 注册时给的动作名。 |
<!-- api-schema:end -->

## library 事件

### library:initialized

<!-- api-schema:begin event:library:initialized -->
foobar2000 报告媒体库已初始化完成。宿主先清掉自己的媒体库缓存再发。所有窗口都会收到；之后才加载的页面收不到。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `timestamp` | `integer` | 宿主发出它的时间，自 Unix 纪元起的毫秒数。 |
<!-- api-schema:end -->

### library:itemsAdded

<!-- api-schema:begin event:library:itemsAdded -->
媒体库里加入了曲目。宿主先清掉自己的媒体库缓存再发。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | 曲目数。 |
| `timestamp` | `integer` | 宿主发出它的时间，自 Unix 纪元起的毫秒数。 |
<!-- api-schema:end -->

### library:itemsRemoved

<!-- api-schema:begin event:library:itemsRemoved -->
媒体库里移除了曲目。宿主先清掉自己的媒体库缓存再发。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | 曲目数。 |
| `timestamp` | `integer` | 宿主发出它的时间，自 Unix 纪元起的毫秒数。 |
<!-- api-schema:end -->

### library:itemsModified

<!-- api-schema:begin event:library:itemsModified -->
媒体库里的曲目被修改了，例如改了标签。宿主先清掉自己的媒体库缓存再发。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | 曲目数。 |
| `timestamp` | `integer` | 宿主发出它的时间，自 Unix 纪元起的毫秒数。 |
<!-- api-schema:end -->

### library:getAllResult

<!-- api-schema:begin event:library:getAllResult -->
回答了 `{ pending: true, requestId }` 的 `library.getAll` 的这一页：在主线程之外构建的整个媒体库，按媒体库顺序。只有构建期间媒体库没有变化，这份列表才保留给之后的调用。发给发起调用的页面；找不到它时发给同一顶层窗口下的页面，再没有就发给主窗口的页面。

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `requestId` | `string` | `{ pending: true, requestId }` 回答里的 id。 |
| `tracks` | `LibraryTrack[]` | 整个媒体库，按媒体库顺序；`index` 是在媒体库中的位置。带 `error` 时为空。 |
| `tracks[].…` | [Track](../reference/types.md#track) | 包含 [Track](../reference/types.md#track) 的全部字段。 |
| `tracks[].index` | `integer` | 行号；数的是什么由所在的列表说明。 |
| `items` | `LibraryTrack[]` | 与 `tracks` 相同的列表。 |
| `items[].…` | [Track](../reference/types.md#track) | 包含 [Track](../reference/types.md#track) 的全部字段。 |
| `items[].index` | `integer` | 行号；数的是什么由所在的列表说明。 |
| `total` | `integer` | 调用时媒体库的曲目数；带 `error` 时为 `0`。 |
| `offset` | `integer` | 调用的 `offset`，恒为 `0`。 |
| `limit` | `integer` | 调用的 `limit`。 |
| `fromCache` | `boolean` | 恒为 `false`。 |
| `error` | `string` | 列表构建失败的原因；只在失败时出现。 |
<!-- api-schema:end -->

## menu 事件

### menu:select

<!-- api-schema:begin event:menu:select -->
`menu.show` 打开的菜单里某一行被选中；菜单关闭时随后还有一个原因为 `select` 的 `menu:dismiss`。发给调用 `menu.show` 的页面；找不到它时发给同一顶层窗口下的页面，再没有就发给主窗口的页面。

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `menuId` | `string` | 菜单的 id，同 `menu.show` 的返回值。 |
| `itemId` | `string` | 被选中的那一行的 `id`。 |
<!-- api-schema:end -->

### menu:dismiss

<!-- api-schema:begin event:menu:dismiss -->
`menu.show` 打开的菜单关闭了，不论有没有选中某一行。投递范围同 `menu:select`，菜单被别的页面的 `menu.show` 或 `menu.close` 关掉时也一样。

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `menuId` | `string` | 菜单的 id，同 `menu.show` 的返回值。 |
| `reason` | `string` | 菜单关闭的原因：选中某一行后为 `select`，在菜单外点击为 `outside`，`escape`，别的窗口抢走焦点为 `blur`，另一个菜单打开为 `replaced`，菜单页面没按时响应为 `timeout`，或调用 `menu.close` 时给的 `reason`。 |
<!-- api-schema:end -->

### menu:valueChanged

<!-- api-schema:begin event:menu:valueChanged -->
`menu.show` 打开的菜单里的评分、滑块或分段控件改了值，菜单保持打开。投递范围同 `menu:select`。

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `menuId` | `string` | 菜单的 id，同 `menu.show` 的返回值。 |
| `itemId` | `string` | 控件所在那一行的 `id`。 |
| `value` | `integer` | 控件的新值。 |
<!-- api-schema:end -->

## metadata 事件

### metadata:probeProgress

<!-- api-schema:begin event:metadata:probeProgress -->
`metadata.probeBatchAsync` 的结果，分批送达：积满 64 条或距上一批过了 100 ms 就发一批，最后不满一批的那些在 `metadata:probeComplete` 之前送达。取消时正在读和尚未轮到的路径不上报。发给发起探测的页面；那时该窗口已不在的，发给主窗口的页面。

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operationId` | `string` | `metadata.probeBatchAsync` 回执里的 id。 |
| `done` | `integer` | 到目前为止各批累计上报的路径数，含失败的。 |
| `total` | `integer` | 调用受理的路径数，即它的 `totalCount`。 |
| `results` | `MetadataProbeResultItem[]` | 仅本批的结果，顺序与请求一致。 |
| `results[].path` | `string` | 请求里的路径，原样，含 `\|subsong:N`。 |
| `results[].success` | `boolean` | 曲目是否读取成功。 |
| `results[].infoSource` | `"cached" \| "direct" \| "none"` | 用了宿主缓存里完整的信息为 `cached`，读了文件为 `direct`，失败时为 `none`。 |
| `results[].failure` | `"not-found" \| "unsupported-format" \| "read-error"` | 路径读取失败的原因；`success` 为 `false` 时出现。 |
| `results[].info` | `TrackTechnicalInfo` | 曲目的技术信息，同 `metadata.read`；`success` 为 `true` 时出现。 |
| `results[].info.duration` | `number` | 时长，单位秒。 |
| `results[].info.bitrate` | `integer` | 解码器报告的码率，单位 kbit/s；未知时为 `0`。 |
| `results[].info.sampleRate` | `integer` | 采样率，单位 Hz；未知时为 `0`。 |
| `results[].info.channels` | `integer` | 声道数；未知时为 `0`。 |
| `results[].info.codec` | `string` | 解码器报告的编码名，如 `FLAC`、`MP3`；未知时为空。 |
| `results[].tags` | `Record<string, any>` | 曲目的扁平字段，见 `includeTags` 的说明；`success` 为 `true` 且调用没传 `includeTags: false` 时出现。 |
<!-- api-schema:end -->

### metadata:probeComplete

<!-- api-schema:begin event:metadata:probeComplete -->
一次探测的最后一个事件，取消之后、探测失败时也会发；只有发送它时抛出异常（实际上只有内存耗尽）才会缺失。之后 `operationId` 失效，对它调用 `metadata.cancelProbe` 报 `cancelled: false`。投递范围同 `metadata:probeProgress`。

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operationId` | `string` | `metadata.probeBatchAsync` 回执里的 id。 |
| `total` | `integer` | 调用受理的路径数。 |
| `successCount` | `integer` | 以 `success: true` 上报的路径数。 |
| `failureCount` | `integer` | 以 `success: false` 上报的路径数。 |
| `cancelled` | `boolean` | 探测被提前停止时为 `true`：`metadata.cancelProbe`，或发起它的 popup 关闭了；此时两个计数之和小于 `total`。 |
<!-- api-schema:end -->

### metadata:writeComplete

<!-- api-schema:begin event:metadata:writeComplete -->
由 `metadata.write`、`metadata.writeBatch`、`metadata.removeTag` 或 `metadata.removeField` 排入的标签写入结束了。所有窗口都会收到。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operation` | `"write" \| "removeTag"` | `metadata.write` 与 `metadata.writeBatch` 为 `write`，`metadata.removeTag` 与 `metadata.removeField` 为 `removeTag`。 |
| `path` | `string` | 调用给出的路径，原样，含后缀。 |
| `subsong` | `integer` | 写入的子曲目：`cueIndex` 不小于 0 时就是它，否则取路径选中的，都没有时为 `0`。 |
| `code` | `integer` | foobar2000 的完成代码：`0` 成功，`1` 中止，`2` 出错。 |
| `success` | `boolean` | `code` 为 `0` 时为 `true`。 |
| `status` | `"success" \| "aborted" \| "error"` | 按 `code` 为 `success`、`aborted` 或 `error`。 |
<!-- api-schema:end -->

## metadb 事件

### metadb:changed

<!-- api-schema:begin event:metadb:changed -->
曲目的元数据变了：标签被写入或重新读取，或 `foo_playcount` 之类的组件请 foobar2000 刷新它提供的字段（`fromHook`）。每一条用 `handle` 指明曲目，与其他方法返回的曲目行里的 `handle` 是同一个键，所以能从 cue 或多曲目文件里分辨出具体是哪一首。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `tracks` | `MetadbChangedTrackItem[]` | 最多 50 首变化的曲目，顺序不固定。无效条目会被跳过，所以条数可能少于 `count` 与 50 中的较小者。 |
| `tracks[].handle` | `string` | 曲目的键，构成方式与曲目行的 `handle` 相同：原生路径（没有原生形式的位置用 foobar2000 自己的路径），`subsong` 不为 `0` 时带 `\|subsong:N`。与同一首曲目在 `library.query`、`playlist.getTracks` 等方法返回的行里的 `handle` 相等。 |
| `tracks[].path` | `string` | 原生形式的文件路径；没有原生形式的位置（如网络流）给 foobar2000 自己的路径。不带 subsong，所以 cue 或多曲目文件里的各首 `path` 相同，要靠 `handle` 区分。 |
| `tracks[].subsong` | `integer` | 文件内的子曲目标识，与曲目行的 `subsong` 相同；整个文件为 `0`。 |
| `tracks[].rating` | `integer` | 评分 `0` 到 `5`，`0` 为未评分，读取规则与 `rating.get` 相同。foobar2000 没有该曲目的信息时只读播放统计，统计里没有评分就不带。 |
| `tracks[].playCount` | `integer` | 文件的 `PLAY_COUNT` 标签，有这个标签时才带；不是播放统计。 |
| `tracks[].title` | `string` | 第一个 `TITLE` 值，有这个标签时才带。 |
| `tracks[].artist` | `string` | 全部 `ARTIST` 值以 `, ` 连接，有这个标签时才带。 |
| `count` | `integer` | 变化的曲目数，至少为 `1`；可能超过 `tracks` 的长度。 |
| `fromHook` | `boolean` | 文件本身没变、而是 `foo_playcount` 之类的组件请求刷新它提供的字段时为 `true`。 |
| `timestamp` | `integer` | 事件发出的时间，单位为自 Unix 纪元起的毫秒数。 |
<!-- api-schema:end -->

## panel 事件

### panel:focus

<!-- api-schema:begin event:panel:focus -->
面板获得了键盘焦点。只在面板的 `grabFocus` 选项打开时发。

发给它所涉及的窗口或面板里的页面。

**载荷**

事件不带字段。
<!-- api-schema:end -->

### panel:blur

<!-- api-schema:begin event:panel:blur -->
面板失去了键盘焦点。只在面板的 `grabFocus` 选项打开时发。

发给它所涉及的窗口或面板里的页面。

**载荷**

事件不带字段。
<!-- api-schema:end -->

### panel:initialized

<!-- api-schema:begin event:panel:initialized -->
DUI 元素或 CUI 面板建好了 WebView。只发一次，WebView 一就绪就发，此时页面可能还没订阅；需要运行模式的页面应在启动时读 `window.getMode`。

发给它所涉及的窗口或面板里的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `mode` | `"dui" \| "cui"` | Default UI 元素为 `dui`，Columns UI 面板为 `cui`。 |
| `panelMode` | `boolean` | 恒为 `true`。 |
| `windowId` | `string` | 面板的 id，与 `window.getMode` 报告的相同。 |
<!-- api-schema:end -->

### panel:visibilityChanged

<!-- api-schema:begin event:panel:visibilityChanged -->
DUI 元素被显示或隐藏，例如在选项卡容器里切换标签页。Columns UI 面板不发。

发给它所涉及的窗口或面板里的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `visible` | `boolean` | 元素现在是否可见。 |
<!-- api-schema:end -->

### panel:configChanged

<!-- api-schema:begin event:panel:configChanged -->
面板配置变了：经 `panel.setConfig`、面板的设置对话框，或 foobar2000 交给元素的一份配置。载荷与 `panel.getConfig` 的 `config` 字段相同。

发给它所涉及的窗口或面板里的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `panelName` | `string` | 面板显示名。 |
| `templateName` | `string` | 面板加载的页面模板名。 |
| `edgeStyle` | `integer` | 面板边框样式：`0` 无边框，`1` 凹陷，`2` 灰边。 |
| `urlOverride` | `string` | 代替模板加载的 URL；没有时为空。 |
| `transparentBackground` | `boolean` | 面板是否透明背景。 |
| `grabFocus` | `boolean` | 点击是否让面板获得键盘焦点。 |
| `enableDragDrop` | `boolean` | 是否允许拖放到面板。 |
| `enableDevTools` | `boolean` | 是否启用开发者工具。 |
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
| `playlistGuid` | `string` | 播放列表的 GUID，写成 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。作为 `playlistGuid` 传入，播放列表清单变了之后仍能指定这个列表。 |
| `start` | `integer` | 第一条插入曲目所在的行。 |
| `count` | `integer` | 插入的曲目数。 |
<!-- api-schema:end -->

### playlist:itemsRemoved

<!-- api-schema:begin event:playlist:itemsRemoved -->
曲目从某个播放列表里移除了。JIT 队列的隐藏播放列表不发。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 播放列表的序号。 |
| `playlistGuid` | `string` | 播放列表的 GUID，写成 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。作为 `playlistGuid` 传入，播放列表清单变了之后仍能指定这个列表。 |
| `oldCount` | `integer` | 移除前的曲目数。 |
| `newCount` | `integer` | 移除后的曲目数。 |
<!-- api-schema:end -->

### playlist:itemsReordered

<!-- api-schema:begin event:playlist:itemsReordered -->
某个播放列表的曲目顺序变了。JIT 队列的隐藏播放列表不发。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 播放列表的序号。 |
| `playlistGuid` | `string` | 播放列表的 GUID，写成 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。作为 `playlistGuid` 传入，播放列表清单变了之后仍能指定这个列表。 |
| `count` | `integer` | 播放列表的曲目数。 |
<!-- api-schema:end -->

### playlist:selectionChanged

<!-- api-schema:begin event:playlist:selectionChanged -->
某个播放列表的选择变了，用 `playlist.getSelection` 读取。JIT 队列的隐藏播放列表不发。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 播放列表的序号。 |
| `playlistGuid` | `string` | 播放列表的 GUID，写成 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。作为 `playlistGuid` 传入，播放列表清单变了之后仍能指定这个列表。 |
<!-- api-schema:end -->

### playlist:focusChanged

<!-- api-schema:begin event:playlist:focusChanged -->
某个播放列表的焦点行移动了。JIT 队列的隐藏播放列表不发。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 播放列表的序号。 |
| `playlistGuid` | `string` | 播放列表的 GUID，写成 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。作为 `playlistGuid` 传入，播放列表清单变了之后仍能指定这个列表。 |
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
| `playlistGuid` | `string` | 播放列表的 GUID，写成 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。作为 `playlistGuid` 传入，播放列表清单变了之后仍能指定这个列表。 |
| `count` | `integer` | 被替换的曲目数。 |
<!-- api-schema:end -->

### playlist:created

<!-- api-schema:begin event:playlist:created -->
新建了一个播放列表。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `index` | `integer` | 新播放列表的序号。 |
| `guid` | `string` | 播放列表的 GUID，写成 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。作为 `playlistGuid` 传入，播放列表清单变了之后仍能指定这个列表。 |
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
| `guid` | `string` | 播放列表的 GUID，写成 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。作为 `playlistGuid` 传入，播放列表清单变了之后仍能指定这个列表。 |
| `name` | `string` | 新名字。 |
<!-- api-schema:end -->

### playlist:lockChanged

<!-- api-schema:begin event:playlist:lockChanged -->
某个播放列表加上或去掉了锁。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 播放列表的序号。 |
| `playlistGuid` | `string` | 播放列表的 GUID，写成 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。作为 `playlistGuid` 传入，播放列表清单变了之后仍能指定这个列表。 |
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
| `success` | `boolean` | 恒为 `true`。 |
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
`port.connect` 打开了一个端口。所有窗口都会收到，包括打开它的窗口。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `portId` | `string` | 新端口的 id，与 `port.connect` 返回的相同。 |
| `name` | `string` | 频道名。 |
| `windowId` | `string` | 打开端口的窗口 id；对不上窗口时为 `main`。 |
<!-- api-schema:end -->

### port:disconnected

<!-- api-schema:begin event:port:disconnected -->
一个端口关闭了：`port.disconnect` 关闭了它，或者打开它的页面开始顶层导航、随 WebView 一起消失（popup 关闭、面板移除、WebView 重建），这时该页面的端口全部关闭。所有窗口都会收到；因导航而关闭端口的页面，只有在卸载之前到达的事件才收得到。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `portId` | `string` | 关闭的端口 id。 |
| `name` | `string` | 端口所在的频道名。 |
| `windowId` | `string` | 打开该端口的窗口 id。 |
<!-- api-schema:end -->

### port:message

<!-- api-schema:begin event:port:message -->
一条消息到达了某个端口。每个接收端口各收到一个事件，发给打开它的窗口：`port.postMessage` 送到发送端口所在频道的其他每个端口（发送方自己窗口里的也算），`port.postMessageTo` 只送到一个端口。

发给订阅或任务所属的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `portId` | `string` | 接收端口的 id，是本窗口打开的端口。 |
| `sourcePortId` | `string` | 发送端口的 id。 |
| `sourceWindowId` | `string` | 发送方页面所在窗口的 id；对不上窗口时为 `main`。 |
| `message` | `any` | 发送方传入的消息，原样送达；不会是 `null`。 |
<!-- api-schema:end -->

## selection 事件

### selection:changed

<!-- api-schema:begin event:selection:changed -->
foobar2000 的全局选择变了。距上一个事件不到 50 ms 的变化直接丢弃而不是推迟，所以连续快速变化中的最后一次可能不发；需要当前值时读 `selection.get`。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | 选中的曲目数。 |
| `type` | `"now_playing" \| "active_playlist_selection" \| "active_playlist" \| "playlist_manager" \| "media_library_viewer" \| "unknown"` | 选择的来源，同 `selection.getType` 应答里的 `typeName`。 |
| `handles` | `string[]` | 前 100 首选中曲目的 handle，按选择顺序：原生路径，子曲目不为 `0` 时附加 `\|subsong:N`。 |
| `truncated` | `boolean` | 是否选中了超过 100 首，因而 `handles` 只列出前 100 首。 |
| `track` | [Track](../reference/types.md#track) | 选中的那首曲目；只在恰好选中一首时出现。 |
| `nowPlaying` | [Track](../reference/types.md#track) | 已载入播放的曲目（正在播放或暂停）；没有时缺省。 |
<!-- api-schema:end -->

## state 事件

### state:changed

<!-- api-schema:begin event:state:changed -->
`state.set` 存了一个值且没带 `silent`。该键原来的值已过期但还没被清扫时，`previousValue` 就是那个旧值，它的过期也不会再通告。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `key` | `string` | 键。 |
| `value` | `any` | 存入的值；不会是 `null`，`state.set` 把 null 当作没传。 |
| `previousValue` | `any` | 该键原来的值；新键为 `null`。 |
| `sourceWindowId` | `string` | 写入它的窗口 id；对不上窗口时为 `main`。 |
| `expiresAt` | `integer` | 值的过期时间，自 Unix 纪元起的毫秒数；只有用正数 `ttlMs` 存的值才有。 |
<!-- api-schema:end -->

### state:deleted

<!-- api-schema:begin event:state:deleted -->
共享状态里的一个值被移除：`state.delete` 删掉了它，或它的存活期到了。过期的值只在 `state.get` 或 `state.keys` 运行时才清扫，所以过期事件在那时发出，而不是在到期的那一刻；删除一个已过期但还没清扫的值报 `deleted`。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `key` | `string` | 键。 |
| `sourceWindowId` | `string` | 删除它的窗口 id，对不上窗口时为 `main`；过期时为 `""`。 |
| `reason` | `"deleted" \| "expired"` | `state.delete` 删除为 `deleted`，存活期到了为 `expired`。 |
<!-- api-schema:end -->

## system 事件

### system:themeChanged

<!-- api-schema:begin event:system:themeChanged -->
foobar2000 默认界面（Default UI）的配色变了，包括深浅色切换。只有本插件 Default UI 面板里的页面会收到，与 `ui:coloursChanged` 一起发；Columns UI 面板与插件自己的窗口收不到。

发给它所涉及的窗口或面板里的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `darkMode` | `boolean` | Default UI 现在是否为深色模式。它跟随 foobar2000 自己的设置，可能与 `system.getTheme` 报告的 Windows 应用主题不同。 |
<!-- api-schema:end -->

## taskbar 事件

### taskbar:buttonClicked

<!-- api-schema:begin event:taskbar:buttonClicked -->
用 `taskbar.setThumbnailButtons` 安装的缩略图工具栏按钮被点击。页面安装自己的按钮之前显示的默认播放按钮由插件原生执行，不发此事件。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | `string` | 按钮的 `id`。 |
<!-- api-schema:end -->

## tray 事件

### tray:beforeContextMenu

<!-- api-schema:begin event:tray:beforeContextMenu -->
托盘上下文菜单即将在右键时打开。菜单不等页面，处理函数对菜单的改动从下一次右键起生效。菜单没有可见的行、什么也不弹出时同样会发。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `x` | `integer` | 光标的屏幕横坐标，菜单在此打开。 |
| `y` | `integer` | 光标的屏幕纵坐标，菜单在此打开。 |
<!-- api-schema:end -->

### tray:click

<!-- api-schema:begin event:tray:click -->
托盘图标被左键单击，在松开按键时发出。右键打开上下文菜单，发的是 `tray:beforeContextMenu`。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `button` | `integer` | 恒为 `0`，即左键。 |
| `x` | `integer` | 光标的屏幕横坐标。 |
| `y` | `integer` | 光标的屏幕纵坐标。 |
<!-- api-schema:end -->

### tray:doubleClick

<!-- api-schema:begin event:tray:doubleClick -->
托盘图标被左键双击。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `x` | `integer` | 光标的屏幕横坐标。 |
| `y` | `integer` | 光标的屏幕纵坐标。 |
<!-- api-schema:end -->

### tray:menuItemClicked

<!-- api-schema:begin event:tray:menuItemClicked -->
托盘菜单的某一行被选中，或其中的富控件改了值。内置的播放行、系统行以及声明了 `playbackAction` 的行由插件原生执行，不发此事件。`render: 'webview'` 时富控件每次改值都会上报，菜单保持打开；默认的原生菜单每次选择后都会关闭，滑块只提供五档，分段行按普通行显示，上报时不带 `value`。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | `string` | 该行的 `id`；没有 id 的行为 `""`。 |
| `value` | `integer` | 富控件的新值：`rating` 为 `0` 到 `5` 星（`0` 为清除），`slider` 为 `min` 到 `max` 内的值，`segmented` 为选中分段的索引。其他行不带。 |
<!-- api-schema:end -->

## ui 事件

### ui:coloursChanged

<!-- api-schema:begin event:ui:coloursChanged -->
foobar2000 默认界面（Default UI）的配色变了。只有本插件 Default UI 面板里的页面会收到，与 `system:themeChanged` 一起发；Columns UI 面板与插件自己的窗口收不到。

发给它所涉及的窗口或面板里的页面。

**载荷**

事件不带字段。
<!-- api-schema:end -->

### ui:fontChanged

<!-- api-schema:begin event:ui:fontChanged -->
foobar2000 默认界面（Default UI）的字体变了。只有本插件 Default UI 面板里的页面会收到；Columns UI 面板与插件自己的窗口收不到。

发给它所涉及的窗口或面板里的页面。

**载荷**

事件不带字段。
<!-- api-schema:end -->

### ui:menuItemClicked

<!-- api-schema:begin event:ui:menuItemClicked -->
`ui.showCustomMenu` 打开的菜单里某一行被选中。打开菜单的页面除了拿到方法返回的 `selectedId`，还会收到此事件；取消菜单时不发。

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | `string` | 该行的 `id`；没有 id 的行为 `""`。 |
| `label` | `string` | 该行的 `label` 原文，不含快捷键提示；没有 label 的行为 `""`。 |
<!-- api-schema:end -->

### ui:toast

<!-- api-schema:begin event:ui:toast -->
`ui.showToast` 请调用它的页面显示一条 toast。宿主不绘制，由页面按这些字段渲染。

发给发起调用的页面。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `message` | `string` | toast 文本，不为空。 |
| `duration` | `integer` | 显示时长（毫秒），原样转交，省略时为 `3000`。不检查范围，可能为 `0` 或负数。 |
| `type` | `"info" \| "success" \| "warning" \| "error"` | toast 种类；省略时为 `info`。 |
| `position` | `string` | 屏幕角落，原样转交、不做检查；省略时为 `bottom-right`。 |
<!-- api-schema:end -->

## webview 事件

### webview:processFailed

<!-- api-schema:begin event:webview:processFailed -->
本插件的某个 WebView2 进程出了故障。此事件发给所有窗口；无法恢复（`recovered` 为 `false`）时，出故障的那个窗口不在其中。

发给所有窗口。

**载荷**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `kind` | `"browserProcessExited" \| "renderProcessExited" \| "renderProcessUnresponsive" \| "frameRenderProcessExited" \| "utilityProcessExited" \| "sandboxHelperProcessExited" \| "gpuProcessExited" \| "ppapiPluginProcessExited" \| "ppapiBrokerProcessExited" \| "unknownProcessExited"` | 出故障的进程；本插件不认识的种类报为 `unknownProcessExited`。 |
| `kindRaw` | `integer` | WebView2 报告的 `COREWEBVIEW2_PROCESS_FAILED_KIND` 值。 |
| `recovered` | `boolean` | WebView 是否已恢复可用：渲染进程重新加载成功、以及运行时自行重启的进程为 `true`；重新加载失败或浏览器进程退出为 `false`。 |
| `recoveryAction` | `"reload" \| "needRebuild" \| "none"` | 插件采取的处理：渲染进程为 `reload`（结果见 `recovered`）；浏览器进程退出、WebView 需要重新创建时为 `needRebuild`；运行时自行重启的进程为 `none`。 |
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

import type { Int } from './common.js';

export interface Api {
  /**
   * Subscribe to spectrum frames of the playing audio. Each subscription keeps its own
   * parameters; frames arrive under `event` in the calling window only, at up to `fps` frames per
   * second while playback runs, plus one silence frame when it pauses or stops.
   * @zh 订阅正在播放的音频的频谱帧。每个订阅保留自己的参数；帧只发给调用方窗口，事件名为 `event`，播放时每秒最多 `fps` 帧，暂停或停止时再发一帧静音帧。
   */
  subscribeSpectrum(params: SubscribeSpectrumParams): SubscribeSpectrumResult;

  /**
   * Remove spectrum subscriptions of the calling page.
   * @zh 移除本页面的频谱订阅。
   */
  unsubscribeSpectrum(params: UnsubscribeSpectrumParams): UnsubscribeSpectrumResult;

  /**
   * Compute one spectrum frame on demand. At least one spectrum subscription must exist.
   * @zh 按需计算一帧频谱。至少要有一个频谱订阅。
   */
  getSpectrum(params: GetSpectrumParams): GetSpectrumResult;

  /**
   * Report the spectrum runtime as the host sees it, for tests and diagnostics. The shape is not a
   * stable contract.
   * @zh 报告宿主眼中的频谱运行状态，供测试与排查用。形状不保证稳定。
   */
  getSpectrumDebugState(): GetSpectrumDebugStateResult;

  /**
   * Return the most recent `duration` seconds of the visualization stream, ending at its current
   * time (`streamTime` in spectrum frames). Needs a spectrum subscription.
   * @zh 返回可视化流最近 `duration` 秒的波形，终点是它的当前时刻（即频谱帧的 `streamTime`）。需要有频谱订阅。
   */
  getWaveform(params: GetWaveformParams): GetWaveformResult;

  /**
   * Choose which channels the visualization stream carries. Applies to spectrum frames and
   * getWaveform, not to what is heard.
   * @zh 选择可视化流带哪些声道。作用于频谱帧与 getWaveform，不影响播放出来的声音。
   */
  setChannelMode(params: SetChannelModeParams): SetChannelModeResult;

  /**
   * Read the track's `BPM` tag. The host does not detect tempo. foobar2000's cached info is used
   * when complete; otherwise the file itself is read, so tracks that are in no playlist or library
   * work too.
   * @zh 读曲目的 `BPM` 标签，宿主不做节拍检测。foobar2000 缓存的信息齐全时用缓存，否则读文件本身，所以不在任何播放列表或媒体库里的曲目也行。
   */
  analyzeBPM(params: AnalyzeBPMParams): AnalyzeBPMResult;

  /**
   * Compute a waveform of a whole track. A cache hit answers `ready` with the waveform; otherwise
   * the answer is `pending` with a task id, and the result arrives as `audio:fullWaveformReady` or
   * `audio:fullWaveformFailed`.
   * @zh 计算整首曲目的波形。命中缓存时直接回 `ready` 与波形；否则回 `pending` 与任务 id，结果随 `audio:fullWaveformReady` 或 `audio:fullWaveformFailed` 到达。
   */
  generateFullWaveform(params: GenerateFullWaveformParams): GenerateFullWaveformResult;

  /**
   * Cancel a pending generateFullWaveform request made by the same page.
   * @zh 取消本页面发起、尚未结束的 generateFullWaveform 请求。
   */
  cancelFullWaveform(params: CancelFullWaveformParams): CancelFullWaveformResult;

  /**
   * Report foobar2000's volume.
   * @zh 报告 foobar2000 的音量。
   */
  getOutputInfo(): GetOutputInfoResult;

  /**
   * Report the format of the playing track.
   * @zh 报告正在播放的曲目的格式。
   */
  getStreamInfo(): GetStreamInfoResult;

  /**
   * Report whether foobar2000 offers a visualization stream. Spectrum frames and getWaveform need
   * one.
   * @zh 报告 foobar2000 是否提供可视化流。频谱帧与 getWaveform 都要靠它。
   */
  isVisualizationAvailable(): IsVisualizationAvailableResult;

  /**
   * Decode a track, or a range of it, into float32 planar PCM delivered through a shared buffer.
   * Answers with a task id right away; the samples arrive as a `sharedbufferreceived` event on
   * `chrome.webview` together with `audio:pcmReady`, or the task ends with `audio:pcmFailed`.
   * @zh 把曲目或其中一段解成 float32 平面 PCM，经共享缓冲交给页面。先回任务 id；样本随 `chrome.webview` 的 `sharedbufferreceived` 事件与 `audio:pcmReady` 一起到达，失败时收到 `audio:pcmFailed`。
   * @experimental
   */
  decodePcm(params: DecodePcmParams): DecodePcmResult;

  /**
   * Cancel a pending decodePcm task started by the same page.
   * @zh 取消本页面发起、尚未结束的 decodePcm 任务。
   * @experimental
   */
  cancelDecodePcm(params: CancelDecodePcmParams): CancelDecodePcmResult;

  /**
   * Subscribe to the audio foobar2000 is playing. Every chunk the core plays is copied into a ring
   * buffer shared with the page, one buffer per subscription. The buffer arrives as a
   * `sharedbufferreceived` event on `chrome.webview` with the first chunk of audio, and again with a
   * new `epoch` whenever the sample rate or channel count changes; `audio:stream` then reports the
   * old epoch as ended. Nothing arrives while playback is stopped or paused.
   * @zh 订阅 foobar2000 正在播放的音频：核心播出的每一块都写进与页面共享的环形缓冲，一个订阅一块。缓冲随第一块音频以 `chrome.webview` 的 `sharedbufferreceived` 事件到达；采样率或声道数变化时再投一块新的（`epoch` 加一），并以 `audio:stream` 通知旧的一代结束。停止或暂停期间没有任何投递。
   * @experimental
   */
  subscribeStream(params: SubscribeStreamParams): SubscribeStreamResult;

  /**
   * Remove stream subscriptions of the calling page. Their buffers get the `ended` flag and are
   * closed on the host side; no event is sent.
   * @zh 移除本页面的流订阅。它们的缓冲置 `ended` 位并在宿主侧关闭；不发事件。
   * @experimental
   */
  unsubscribeStream(params: UnsubscribeStreamParams): UnsubscribeStreamResult;

  /**
   * Report the shared-buffer support of the calling page, the decode task queue and the stream
   * subscriptions, for tests and diagnostics.
   * @zh 报告调用方页面的共享缓冲支持情况、解码任务队列与流订阅，供测试与排查用。
   * @experimental
   */
  getPcmDebugState(): GetPcmDebugStateResult;
}

export interface Events {
  /**
   * A frame of an `audio.subscribeSpectrum` subscription, at the subscription's `fps` while
   * playing, plus one silence frame when playback pauses or stops. The fields are those of an
   * `audio.getSpectrum` answer for the subscription. Sent to the page that subscribed: by its
   * window id, else by its window handle, else to a page under the same top-level window; when
   * none is found the frame is dropped.
   * @zh `audio.subscribeSpectrum` 订阅的一帧：播放时按订阅的 `fps` 出帧，暂停或停止时另出一帧静音帧。字段同该订阅的 `audio.getSpectrum` 应答。发给订阅的页面：先按它的窗口 id，再按窗口句柄，再找同一顶层窗口下的页面；都找不到就丢弃这一帧。
   * @delivery owner
   * @customName
   */
  spectrum: GetSpectrumResult;

  /**
   * An `audio.generateFullWaveform` request that answered `pending` finished decoding. Every
   * request waiting on the same decode gets its own event, rendered with its own `method`,
   * `scale` and `signed`. Sent to the page that made the request, or to a page under the same
   * top-level window when that one cannot be found, or else to the main window's page.
   * @zh 应答为 `pending` 的 `audio.generateFullWaveform` 请求解码完成。等同一次解码的每个请求各收一个事件，按各自的 `method`、`scale` 与 `signed` 生成。发给发起请求的页面；找不到它时发给同一顶层窗口下的页面，再没有就发给主窗口的页面。
   * @delivery caller
   */
  fullWaveformReady: FullWaveformReadyPayload;

  /**
   * An `audio.generateFullWaveform` request that answered `pending` ended without a waveform:
   * decoding failed, or the request was cancelled with `audio.cancelFullWaveform`. Delivered like
   * `audio:fullWaveformReady`.
   * @zh 应答为 `pending` 的 `audio.generateFullWaveform` 请求没有得到波形就结束了：解码失败，或被 `audio.cancelFullWaveform` 取消。投递范围同 `audio:fullWaveformReady`。
   * @delivery caller
   */
  fullWaveformFailed: FullWaveformFailedPayload;

  /**
   * An `audio.decodePcm` task finished. Its samples reach the page as a `sharedbufferreceived`
   * event on `chrome.webview` with the same `taskId` in `additionalData`, before or after this
   * event. Sent to the page that started the task, only while that page still shows the document
   * that started it.
   * @zh `audio.decodePcm` 任务完成。样本以 `chrome.webview` 上的 `sharedbufferreceived` 事件送达，`additionalData` 里带同一个 `taskId`，可能在本事件之前或之后到达。发给发起任务的页面，且只在该页面仍是发起任务的那个文档时发。
   * @delivery owner
   */
  pcmReady: PcmReadyPayload;

  /**
   * An `audio.decodePcm` task ended without samples: decoding failed, the page origin stopped
   * being trusted, the buffer could not be allocated or posted, or the task was cancelled with
   * `audio.cancelDecodePcm`. Delivered like `audio:pcmReady`.
   * @zh `audio.decodePcm` 任务没有得到样本就结束了：解码失败、页面来源不再可信、缓冲分配或投递失败，或被 `audio.cancelDecodePcm` 取消。投递范围同 `audio:pcmReady`。
   * @delivery owner
   */
  pcmFailed: PcmFailedPayload;

  /**
   * The host stopped writing one epoch of an `audio.subscribeStream` buffer because the sample
   * rate or channel count changed; the next epoch's buffer arrives as its own
   * `sharedbufferreceived` event. Other endings (unsubscribe, page gone, host exit) are not
   * reported this way: the buffer's `ended` flag is the signal. Sent to the page that subscribed.
   * @zh 采样率或声道数变了，宿主不再写 `audio.subscribeStream` 缓冲的某一代；下一代缓冲以单独的 `sharedbufferreceived` 事件送达。其他结束方式（退订、页面关闭、宿主退出）不这样通知，以缓冲的 `ended` 标志为准。发给订阅的页面。
   * @delivery owner
   */
  stream: StreamPayload;

  /**
   * foobar2000's output configuration changed, such as the output device.
   * @zh foobar2000 的输出配置变了，比如输出设备。
   * @delivery broadcast
   */
  outputDeviceChanged: void;

  /**
   * foobar2000's playback DSP chain changed, for example a DSP was added or a preset was loaded.
   * @zh foobar2000 的播放 DSP 链变了，比如加了一个 DSP 或载入了预设。
   * @delivery broadcast
   */
  dspPresetChanged: void;

  /**
   * A ReplayGain setting of foobar2000's playback changed, not only the source mode.
   * @zh foobar2000 播放的某项 ReplayGain 设置变了，不只是音源模式。
   * @delivery broadcast
   */
  replaygainModeChanged: ReplaygainModeChangedPayload;
}

/**
 * Band scale: `weighted` is a display curve in `[0, 1]`; `db` is band power in dB, summed over the
 * FFT bins of the band, where a full-scale sine reads 0 dB. Bin output is always `db`.
 * @zh 频带刻度：`weighted` 是 `[0, 1]` 的显示曲线；`db` 是频带功率（dB），按频带内的 FFT 频点求和，满幅正弦读 0 dB。频点输出恒为 `db`。
 */
type SpectrumScale = 'weighted' | 'db';

/**
 * `bins`: the FFT's linear bins as power in dB; `bands`: log-spaced bands, kept for compatibility.
 * @zh `bins`：FFT 的线性频点，功率 dB；`bands`：按对数划分的频带，为兼容保留。
 */
type SpectrumOutput = 'bands' | 'bins';

/**
 * Channel layout of bin output: `mix` averages the power of all channels into `spectrum`; `stereo`
 * delivers the first two channels as `left` and `right`.
 * @zh 频点输出的声道布局：`mix` 把各声道功率平均成 `spectrum`；`stereo` 把前两个声道分别给成 `left` 与 `right`。
 */
type SpectrumChannels = 'mix' | 'stereo';

/**
 * Playback state a frame was computed in.
 * @zh 出帧时的播放状态。
 */
type SpectrumFrameState = 'playing' | 'paused' | 'stopped';

/**
 * Timer behind pushed frames: `high-resolution`, or `standard` on Windows 10 releases before 1803.
 * @zh 推送帧用的定时器：`high-resolution`，Windows 10 1803 之前的版本为 `standard`。
 */
type SpectrumBeatSource = 'high-resolution' | 'standard';

/**
 * `mix` averages the channels; `stereo` keeps the first two apart.
 * @zh `mix` 把各声道平均；`stereo` 分开给前两个声道。
 */
type WaveformChannels = 'mix' | 'stereo';

/**
 * Aggregation of a full-track waveform: window RMS or peak.
 * @zh 整轨波形的取值方式：窗口 RMS 或峰值。
 */
type WaveformMethod = 'rms' | 'peak';

/**
 * Scale of a full-track waveform: `linear`, or `db` where 60 dB below the track's own maximum maps
 * to 0.
 * @zh 整轨波形的刻度：`linear`，或 `db`（比曲目自身最大值低 60 dB 处映射为 0）。
 */
type WaveformScale = 'linear' | 'db';

/**
 * Channels of the visualization stream: all of them, a mono downmix, or only the front or back
 * pair.
 * @zh 可视化流的声道：全部、混成单声道、只取前置或只取后置一对。
 */
type ChannelMode = 'default' | 'mono' | 'front' | 'back';

// ---- subscribeSpectrum ----

interface SubscribeSpectrumParams {
  /**
   * Key of the subscription; subscribing again with the same id replaces that subscription. When
   * omitted, the key is derived from the calling window and `event`.
   * @zh 订阅的键；用同一 id 再订阅会替换该订阅。缺省时由调用方窗口与 `event` 推出。
   * @minLength 1
   */
  subscriptionId?: string;
  /**
   * FFT size, a power of two; other values fail with `INVALID_PARAMS`. Band output raises it for 32
   * or more bands; bin output uses it as given. 65536 is this component's ceiling, not a foobar2000
   * limit, and costs about 1.5 s of PCM per frame.
   * @zh FFT 点数，须为 2 的幂，否则返回 `INVALID_PARAMS`。频带输出在 32 个频带及以上时会提升它；频点输出按给定值用。65536 是本组件自定的上限，不是 foobar2000 的限制，每帧要约 1.5 s 的 PCM。
   * @minimum 256
   * @maximum 65536
   * @default 1024
   */
  fftSize?: Int;
  /**
   * Band output only: number of bands, clamped to 8 to `fftSize / 2`.
   * @zh 仅频带输出：频带数，夹到 8 到 `fftSize / 2`。
   * @default 48
   */
  bands?: Int;
  /**
   * Frames per second, clamped to 1 to 60.
   * @zh 每秒帧数，夹到 1 到 60。
   * @default 30
   */
  fps?: Int;
  /**
   * Band scale, `weighted` when omitted. With `output: 'bins'` omit it or pass `db`; anything else
   * fails with `INVALID_PARAMS`.
   * @zh 频带刻度，缺省为 `weighted`。`output: 'bins'` 时不传或传 `db`，其他值返回 `INVALID_PARAMS`。
   */
  scale?: SpectrumScale;
  /**
   * What the frames carry; `bands` is ignored for bin output.
   * @zh 帧里给什么；频点输出时忽略 `bands`。
   * @default "bands"
   */
  output?: SpectrumOutput;
  /**
   * Bin output only; `stereo` with band output fails with `INVALID_PARAMS`.
   * @zh 仅频点输出；频带输出配 `stereo` 返回 `INVALID_PARAMS`。
   * @default "mix"
   */
  channels?: SpectrumChannels;
  /**
   * `true` limits this subscription to at most 12 frames per second, usually 10 to 12, while
   * another application is in the foreground; `false` keeps the full rate.
   * @zh 为 `true` 时，别的应用在前台期间本订阅每秒最多 12 帧，通常 10 到 12；`false` 保持全速。
   * @default true
   */
  backgroundThrottle?: boolean;
  /**
   * Lower edge of the frequency range in Hz; bands divide the range logarithmically.
   * @zh 频率范围下限，Hz；频带在范围内按对数划分。
   * @minimum 1
   * @default 20
   */
  minFrequency?: number;
  /**
   * Upper edge in Hz, greater than `minFrequency`. Capped at half the stream's sample rate when
   * frames are computed, and follows it when omitted. A range entirely above that frequency yields
   * silence frames.
   * @zh 上限，Hz，须大于 `minFrequency`。出帧时截到流采样率的一半，缺省时就取它。整个范围都高于它时出静音帧。
   */
  maxFrequency?: number;
  /**
   * Event name the frames are delivered under.
   * @zh 帧所用的事件名。
   * @minLength 1
   * @default "audio:spectrum"
   */
  event?: string;
}

interface SubscribeSpectrumResult {
  /**
   * The subscription's key, as given or derived.
   * @zh 订阅的键，调用方给的或推出的。
   */
  subscriptionId: string;
  /**
   * Requested FFT size; frames report the size actually used.
   * @zh 请求的 FFT 点数；帧里报实际用的点数。
   */
  fftSize: Int;
  /**
   * Band count after clamping.
   * @zh 夹取后的频带数。
   */
  bands: Int;
  /**
   * Frame rate after clamping.
   * @zh 夹取后的帧率。
   */
  fps: Int;
  /**
   * Scale in use; always `db` for bin output.
   * @zh 使用的刻度；频点输出恒为 `db`。
   */
  scale: SpectrumScale;
  /**
   * As requested.
   * @zh 同请求。
   */
  backgroundThrottle: boolean;
  /**
   * Lower edge of the range in Hz, as requested.
   * @zh 范围下限，Hz，同请求。
   */
  minFrequency: number;
  /**
   * Requested upper edge in Hz; `null` when the range follows half the stream's sample rate.
   * @zh 请求的上限，Hz；范围跟随流采样率一半时为 `null`。
   */
  maxFrequency: number | null;
  /**
   * Output registered.
   * @zh 登记的输出。
   */
  output: SpectrumOutput;
  /**
   * Channel layout registered.
   * @zh 登记的声道布局。
   */
  channels: SpectrumChannels;
  /**
   * Event name the frames arrive under.
   * @zh 帧所用的事件名。
   */
  event: string;
  /**
   * `true` once the visualization stream exists, which includes the stopped state; whether audio
   * flows shows in the frames' `state`.
   * @zh 可视化流建好后为 `true`，停止态也算；有没有音频看帧的 `state`。
   */
  streamReady: boolean;
}

// ---- unsubscribeSpectrum ----

interface UnsubscribeSpectrumParams {
  /**
   * Subscription to remove; omit to remove every spectrum subscription of the calling page.
   * @zh 要移除的订阅；缺省移除本页面的全部频谱订阅。
   * @minLength 1
   */
  subscriptionId?: string;
}

interface UnsubscribeSpectrumResult {
  /**
   * Subscriptions removed: 0 or 1 with a `subscriptionId`, any number without. Subscriptions of
   * other pages are never counted or removed.
   * @zh 移除的订阅数：带 `subscriptionId` 时为 0 或 1，不带时为本页面的全部。别的页面的订阅不计也不动。
   */
  removed: Int;
  /**
   * The requested id; empty when none was given.
   * @zh 请求里的 id；没给时为空串。
   */
  subscriptionId: string;
}

// ---- getSpectrum ----

interface GetSpectrumParams {
  /**
   * Compute the frame with this subscription's parameters, bin output included; the other
   * parameters are then ignored. An unknown id fails with `NOT_FOUND`.
   * @zh 用这个订阅的参数出帧，含频点输出；此时其余参数不起作用。未知 id 返回 `NOT_FOUND`。
   * @minLength 1
   */
  subscriptionId?: string;
  /**
   * Used without `subscriptionId`. `0` takes the largest band count among all subscriptions; the
   * value is capped at `fftSize / 2`.
   * @zh 不带 `subscriptionId` 时用。`0` 取所有订阅里最大的频带数；不超过 `fftSize / 2`。
   * @minimum 0
   * @default 0
   */
  bands?: Int;
  /**
   * Used without `subscriptionId`; as in subscribeSpectrum.
   * @zh 不带 `subscriptionId` 时用，同 subscribeSpectrum。
   * @default "weighted"
   */
  scale?: SpectrumScale;
  /**
   * Used without `subscriptionId`; as in subscribeSpectrum.
   * @zh 不带 `subscriptionId` 时用，同 subscribeSpectrum。
   * @minimum 1
   * @default 20
   */
  minFrequency?: number;
  /**
   * Used without `subscriptionId`; as in subscribeSpectrum.
   * @zh 不带 `subscriptionId` 时用，同 subscribeSpectrum。
   */
  maxFrequency?: number;
  /**
   * Without `subscriptionId` only `bands` is accepted: the FFT size then follows the other
   * subscriptions, so bin output needs a subscription of its own.
   * @zh 不带 `subscriptionId` 时只接受 `bands`：这时 FFT 点数跟随其他订阅，频点输出要有自己的订阅。
   * @default "bands"
   */
  output?: SpectrumOutput;
  /**
   * Without `subscriptionId` only `mix` is accepted.
   * @zh 不带 `subscriptionId` 时只接受 `mix`。
   * @default "mix"
   */
  channels?: SpectrumChannels;
}

interface GetSpectrumResult {
  /**
   * Subscription the frame belongs to; absent when the request named none.
   * @zh 帧所属的订阅；请求没指定订阅时不带此键。
   */
  subscriptionId?: string;
  /**
   * What the frame carries.
   * @zh 帧里给的是什么。
   */
  output: SpectrumOutput;
  /**
   * Band frames: one value per band in the frame's `scale`; `weighted` values lie in `[0, 1]`, `db`
   * values have a floor of `-160`. Bin frames with `channels: 'mix'`: one power value in dB per bin.
   * @zh 频带帧：每个频带一个值，刻度为帧的 `scale`；`weighted` 在 `[0, 1]` 内，`db` 下限 `-160`。`channels: 'mix'` 的频点帧：每个频点一个功率值，dB。
   */
  spectrum?: number[];
  /**
   * Bin frames with `channels: 'stereo'`: the first channel, one dB value per bin.
   * @zh `channels: 'stereo'` 的频点帧：第一个声道，每个频点一个 dB 值。
   */
  left?: number[];
  /**
   * Bin frames with `channels: 'stereo'`: the second channel, or the first again for a mono stream.
   * @zh `channels: 'stereo'` 的频点帧：第二个声道，单声道流时重复第一个。
   */
  right?: number[];
  /**
   * Band frames: length of `spectrum`.
   * @zh 频带帧：`spectrum` 的长度。
   */
  bands?: Int;
  /**
   * Bin frames: FFT bin index of the first value; value `i` is centred at
   * `(firstBin + i) * sampleRate / fftSize` Hz. `0`, with empty arrays, when the range holds no bin.
   * @zh 频点帧：第一个值的 FFT 频点序号；第 `i` 个值的中心频率为 `(firstBin + i) * sampleRate / fftSize` Hz。范围内没有频点时为 `0`，数组为空。
   */
  firstBin?: Int;
  /**
   * FFT size actually used.
   * @zh 实际用的 FFT 点数。
   */
  fftSize: Int;
  /**
   * Scale of the values; always `db` for bin frames.
   * @zh 值的刻度；频点帧恒为 `db`。
   */
  scale: SpectrumScale;
  /**
   * Bin frames: the subscription's channel layout.
   * @zh 频点帧：订阅的声道布局。
   */
  channels?: SpectrumChannels;
  /**
   * Bin frames: channel count of the visualization stream; `0` when unknown.
   * @zh 频点帧：可视化流的声道数；未知时为 `0`。
   */
  channelCount?: Int;
  /**
   * Sample rate of the visualization stream in Hz; `0` when unknown.
   * @zh 可视化流的采样率，Hz；未知时为 `0`。
   */
  sampleRate: Int;
  /**
   * Lower edge of the frequency range in Hz.
   * @zh 频率范围下限，Hz。
   */
  minFrequency: number;
  /**
   * Upper edge in Hz: the requested one capped at `sampleRate / 2`, or `sampleRate / 2` when none
   * was given.
   * @zh 上限，Hz：请求的上限截到 `sampleRate / 2`，没给时就是 `sampleRate / 2`。
   */
  maxFrequency: number;
  /**
   * Paused or stopped frames are silence.
   * @zh 暂停或停止时的帧是静音帧。
   */
  state: SpectrumFrameState;
  /**
   * Seconds since the visualization stream last started; the frame is computed from the `fftSize`
   * samples ending at this time.
   * @zh 可视化流上次起播以来的秒数；帧由截至此刻的 `fftSize` 个样本算出。
   */
  streamTime: number;
  /**
   * Host system time when the frame was computed, Unix epoch milliseconds.
   * @zh 宿主算出这一帧时的系统时间，Unix 纪元毫秒。
   */
  hostTime: number;
}

// ---- getSpectrumDebugState ----

interface GetSpectrumDebugStateResult {
  /**
   * At least one spectrum subscription is registered.
   * @zh 至少登记了一个频谱订阅。
   */
  active: boolean;
  /**
   * The beat thread that pushes frames is running.
   * @zh 推送帧的计时线程在运行。
   */
  timerRunning: boolean;
  /**
   * Deprecated; always `0`.
   * @zh 已废弃，恒为 `0`。
   */
  timerHwnd: Int;
  /**
   * Timer of the beat thread; `null` while it is not running.
   * @zh 计时线程用的定时器；线程没在运行时为 `null`。
   */
  beatSource: SpectrumBeatSource | null;
  /**
   * Current beat in milliseconds, 1000 / `effectiveFps`; `null` while the beat thread is not running.
   * @zh 当前拍长，毫秒，即 1000 / `effectiveFps`；计时线程没在运行时为 `null`。
   */
  beatIntervalMs: number | null;
  /**
   * Beats dropped because the previous one had not reached the main thread yet; cumulative.
   * @zh 因上一拍还没到主线程而丢掉的拍数，累计值。
   */
  beatsCoalesced: Int;
  /**
   * Largest FFT size requested among all subscriptions; each still computes with its own.
   * @zh 所有订阅请求的最大 FFT 点数；各订阅仍按自己的算。
   */
  effectiveFftSize: Int;
  /**
   * Largest frame rate requested among all subscriptions.
   * @zh 所有订阅请求的最大帧率。
   */
  effectiveFps: Int;
  /**
   * Largest band count requested among all subscriptions.
   * @zh 所有订阅请求的最大频带数。
   */
  effectiveBands: Int;
  /**
   * Ticks the pushed frames will skip because the last one ran over its interval.
   * @zh 因上一拍超时而要跳过的推送拍数。
   */
  skipFrames: Int;
  /**
   * FFTs computed since the host started; flat while playback is paused or stopped.
   * @zh 宿主启动以来算过的 FFT 次数；暂停或停止时不增长。
   */
  framesComputed: Int;
  /**
   * The visualization stream exists.
   * @zh 可视化流已建好。
   */
  streamReady: boolean;
  /**
   * Length of `subscriptions`.
   * @zh `subscriptions` 的长度。
   */
  subscriptionCount: Int;
  /**
   * Length of `dispatchTargets`.
   * @zh `dispatchTargets` 的长度。
   */
  dispatchTargetCount: Int;
  /**
   * Every spectrum subscription of every page.
   * @zh 所有页面的全部频谱订阅。
   */
  subscriptions: SpectrumDebugSubscription[];
  /**
   * Windows the pushed frames are delivered to.
   * @zh 推送帧的投递目标窗口。
   */
  dispatchTargets: SpectrumDebugTarget[];
  /**
   * WebView2 instances in the component.
   * @zh 组件里的 WebView2 实例数。
   */
  instanceCount: Int;
  /**
   * Window handle of the calling page, as a number; `0` when unknown.
   * @zh 调用方页面的窗口句柄，数值；未知时为 `0`。
   */
  callerHwnd: Int;
  /**
   * Window id of the calling page; empty when unknown.
   * @zh 调用方页面的窗口 id；未知时为空串。
   */
  callerWindowId: string;
  /**
   * The calling page owns at least one subscription.
   * @zh 调用方页面至少拥有一个订阅。
   */
  callerOwnsSubscription: boolean;
  /**
   * Handle of the foreground window, as a number.
   * @zh 前台窗口的句柄，数值。
   */
  foregroundHwnd: Int;
  /**
   * Process id of the foreground window.
   * @zh 前台窗口所属进程的 id。
   */
  foregroundPid: Int;
  /**
   * The foreground window belongs to another process, so the background frame limit applies.
   * @zh 前台窗口属于别的进程，后台限帧此时生效。
   */
  foregroundIsExternal: boolean;
  /**
   * Title of the foreground window.
   * @zh 前台窗口的标题。
   */
  foregroundTitle: string;
}

interface SpectrumDebugSubscription {
  /**
   * The subscription's key.
   * @zh 订阅的键。
   */
  token: string;
  /**
   * Window id of the subscribing page.
   * @zh 订阅页面的窗口 id。
   */
  windowId: string;
  /**
   * Handle of the window that owns the subscription, as a number.
   * @zh 拥有该订阅的窗口句柄，数值。
   */
  ownerHwnd: Int;
  /**
   * Event name of the frames.
   * @zh 帧的事件名。
   */
  event: string;
  /**
   * Requested FFT size.
   * @zh 请求的 FFT 点数。
   */
  fftSize: Int;
  /**
   * Frame rate after clamping.
   * @zh 夹取后的帧率。
   */
  fps: Int;
  /**
   * Band count after clamping.
   * @zh 夹取后的频带数。
   */
  bands: Int;
  /**
   * Scale in use.
   * @zh 使用的刻度。
   */
  scale: SpectrumScale;
  /**
   * As requested.
   * @zh 同请求。
   */
  backgroundThrottle: boolean;
  /**
   * Lower edge of the range in Hz.
   * @zh 范围下限，Hz。
   */
  minFrequency: number;
  /**
   * Requested upper edge in Hz; `null` when the range follows half the stream's sample rate.
   * @zh 请求的上限，Hz；跟随流采样率一半时为 `null`。
   */
  maxFrequency: number | null;
  /**
   * Output in use.
   * @zh 使用的输出。
   */
  output: SpectrumOutput;
  /**
   * Channel layout in use.
   * @zh 使用的声道布局。
   */
  channels: SpectrumChannels;
}

interface SpectrumDebugTarget {
  /**
   * Window id the frames go to.
   * @zh 帧投递到的窗口 id。
   */
  windowId: string;
  /**
   * Handle of that window, as a number.
   * @zh 该窗口的句柄，数值。
   */
  ownerHwnd: Int;
  /**
   * Event name of the frames.
   * @zh 帧的事件名。
   */
  event: string;
}

// ---- getWaveform ----

interface GetWaveformParams {
  /**
   * Window length in seconds, greater than 0.
   * @zh 窗口长度，秒，须大于 0。
   * @maximum 1
   * @default 0.05
   */
  duration?: number;
  /**
   * `true` keeps PCM polarity, clamped to `[-1, 1]`; `false` maps the magnitude from -70 to 0 dB
   * onto `[0, 1]`.
   * @zh 为 `true` 时保留 PCM 正负，夹到 `[-1, 1]`；为 `false` 时把 -70 到 0 dB 的幅度映射到 `[0, 1]`。
   * @default false
   */
  signed?: boolean;
  /**
   * `mix` averages the channels into `waveform`; `stereo` returns the first two channels as `left`
   * and `right`.
   * @zh `mix` 把各声道平均成 `waveform`；`stereo` 把前两个声道分别给成 `left` 与 `right`。
   * @default "mix"
   */
  channels?: WaveformChannels;
  /**
   * Thin the window to this many evenly spaced samples, without averaging; every sample is
   * returned when omitted.
   * @zh 把窗口抽成这么多个等距样本，不做平均；缺省返回全部样本。
   * @minimum 2
   * @maximum 65536
   */
  points?: Int;
}

interface GetWaveformResult {
  /**
   * `mix` answers: one value per sample.
   * @zh `mix` 应答：每个样本一个值。
   */
  waveform?: number[];
  /**
   * `stereo` answers: the first channel.
   * @zh `stereo` 应答：第一个声道。
   */
  left?: number[];
  /**
   * `stereo` answers: the second channel; a copy of `left` when the stream has one channel.
   * @zh `stereo` 应答：第二个声道；流只有一个声道时与 `left` 相同。
   */
  right?: number[];
  /**
   * Window length in seconds, as requested.
   * @zh 窗口长度，秒，同请求。
   */
  duration: number;
  /**
   * As requested.
   * @zh 同请求。
   */
  signed: boolean;
  /**
   * As requested.
   * @zh 同请求。
   */
  channels: WaveformChannels;
  /**
   * Sample rate of the visualization stream in Hz.
   * @zh 可视化流的采样率，Hz。
   */
  sampleRate: Int;
  /**
   * Channel count of the visualization stream; `1` after setChannelMode with `mono`.
   * @zh 可视化流的声道数；setChannelMode 设为 `mono` 后为 `1`。
   */
  channelCount: Int;
}

// ---- setChannelMode ----

interface SetChannelModeParams {
  /**
   * Channels the visualization stream carries.
   * @zh 可视化流带的声道。
   * @default "default"
   */
  mode?: ChannelMode;
}

interface SetChannelModeResult {
  /**
   * The mode now in effect.
   * @zh 现在生效的模式。
   */
  mode: ChannelMode;
}

// ---- analyzeBPM ----

interface AnalyzeBPMParams {
  /**
   * Track path; a `|subsong:N` suffix selects a subsong.
   * @zh 曲目路径；带 `|subsong:N` 后缀时选子曲目。
   * @minLength 1
   * @security MediaRead
   */
  path: string;
}

interface AnalyzeBPMResult {
  /**
   * Value of the `BPM` tag, between 0 and 500 (exclusive); a tag outside that range fails with
   * `NOT_FOUND`.
   * @zh `BPM` 标签的值，在 0 与 500 之间（不含两端）；超出这个范围返回 `NOT_FOUND`。
   */
  bpm: number;
  /**
   * Always `1`.
   * @zh 恒为 `1`。
   */
  confidence: number;
  /**
   * Always `metadata`.
   * @zh 恒为 `metadata`。
   */
  source: 'metadata';
}

// ---- generateFullWaveform ----

interface GenerateFullWaveformParams {
  /**
   * Track path; a `|subsong:N` suffix selects a subsong.
   * @zh 曲目路径；带 `|subsong:N` 后缀时选子曲目。
   * @minLength 1
   * @security MediaRead
   */
  path: string;
  /**
   * Subsong index, 0-based; takes precedence over a subsong in `path`.
   * @zh 子曲目序号，从 0 起；优先于路径里的子曲目。
   * @minimum 0
   */
  cueIndex?: Int;
  /**
   * Number of points, clamped to 64 to 4096. A track is decoded once per resolution; the other
   * options are computed from the cache.
   * @zh 点数，夹到 64 到 4096。每个点数只解码一次，其余选项从缓存算。
   * @default 256
   */
  resolution?: Int;
  /**
   * How each point is taken.
   * @zh 每个点的取值方式。
   * @default "rms"
   */
  method?: WaveformMethod;
  /**
   * Scale of the points: `linear`, or `db`, where 60 dB below the track's own maximum maps to 0.
   * Ignored when `signed` is set.
   * @zh 点的刻度：`linear`，或 `db`（比曲目自身最大值低 60 dB 处映射为 0）；`signed` 时忽略。
   * @default "linear"
   */
  scale?: WaveformScale;
  /**
   * Keep PCM polarity; points fall in `[-1, 1]`.
   * @zh 保留 PCM 正负；点在 `[-1, 1]` 内。
   * @default false
   */
  signed?: boolean;
  /**
   * Answer from the cache when possible.
   * @zh 能用缓存时直接用缓存回答。
   * @default true
   */
  preferCache?: boolean;
}

interface GenerateFullWaveformResult {
  /**
   * `ready`: the waveform is in this answer. `pending`: it arrives as an event for `taskId`.
   * @zh `ready`：波形在本应答里。`pending`：它随 `taskId` 的事件到达。
   */
  status: 'ready' | 'pending';
  /**
   * The answer came from the cache.
   * @zh 应答来自缓存。
   */
  cached: boolean;
  /**
   * Identifies the task in events and cancelFullWaveform; `pending` answers only.
   * @zh 在事件与 cancelFullWaveform 里标识任务；只有 `pending` 应答带。
   */
  taskId?: string;
  /**
   * `ready` answers: the points, normalized by `maxAmplitude`.
   * @zh `ready` 应答：各点，按 `maxAmplitude` 归一化。
   */
  waveform?: number[];
  /**
   * `ready` answers: the largest value of the selected sequence before normalization, in linear
   * full-scale units. On the `linear` scale `waveform[i] * maxAmplitude` restores the level; on
   * `db`, dBFS is `(v * 60 - 60) + 20 * log10(maxAmplitude)`.
   * @zh `ready` 应答：归一化前所选序列的最大值，线性满幅单位。`linear` 刻度下 `waveform[i] * maxAmplitude` 还原电平；`db` 刻度下 dBFS 为 `(v * 60 - 60) + 20 * log10(maxAmplitude)`。
   */
  maxAmplitude?: number;
  /**
   * `ready` answers: track length in seconds.
   * @zh `ready` 应答：曲目长度，秒。
   */
  duration?: number;
  /**
   * `ready` answers: sample rate in Hz.
   * @zh `ready` 应答：采样率，Hz。
   */
  sampleRate?: Int;
  /**
   * `ready` answers: channel count.
   * @zh `ready` 应答：声道数。
   */
  channels?: Int;
  /**
   * Point count in use.
   * @zh 使用的点数。
   */
  resolution: Int;
  /**
   * As requested.
   * @zh 同请求。
   */
  method: WaveformMethod;
  /**
   * As requested.
   * @zh 同请求。
   */
  scale: WaveformScale;
  /**
   * As requested.
   * @zh 同请求。
   */
  signed: boolean;
  /**
   * The requested path.
   * @zh 请求里的路径。
   */
  path: string;
}

// ---- cancelFullWaveform ----

interface CancelFullWaveformParams {
  /**
   * Task id from a `pending` answer of generateFullWaveform.
   * @zh generateFullWaveform 的 `pending` 应答里的任务 id。
   * @minLength 1
   */
  taskId: string;
}

interface CancelFullWaveformResult {
  /**
   * `false` when the task has already ended, does not exist or belongs to another page; these
   * cases are not told apart. A cancelled task receives one `audio:fullWaveformFailed` with
   * `code: 'CANCELLED'`.
   * @zh 为 `false` 表示任务已结束、不存在或属于别的页面，三者不区分。被取消的任务收到一次 `code: 'CANCELLED'` 的 `audio:fullWaveformFailed`。
   */
  cancelled: boolean;
}

// ---- getOutputInfo ----

interface GetOutputInfoResult {
  /**
   * Volume in dB, `-100` (silence) to `0` (full).
   * @zh 音量，dB，`-100`（静音）到 `0`（最大）。
   */
  volume: number;
  /**
   * The same volume as a linear percentage, `100 * 10^(volume / 20)`.
   * @zh 同一音量的线性百分比，`100 * 10^(volume / 20)`。
   */
  volumePercent: number;
}

// ---- getStreamInfo ----

interface GetStreamInfoResult {
  /**
   * A track is playing or paused; the other fields are present only then.
   * @zh 有曲目在播放或暂停；只有这时才带其余字段。
   */
  playing: boolean;
  /**
   * Sample rate in Hz.
   * @zh 采样率，Hz。
   */
  sampleRate?: Int;
  /**
   * Channel count.
   * @zh 声道数。
   */
  channels?: Int;
  /**
   * Bitrate in kbps.
   * @zh 码率，kbps。
   */
  bitrate?: Int;
  /**
   * Codec name as foobar2000 reports it, or `unknown`.
   * @zh foobar2000 报的编码名，没有时为 `unknown`。
   */
  codec?: string;
  /**
   * Track length in seconds.
   * @zh 曲目长度，秒。
   */
  duration?: number;
}

// ---- isVisualizationAvailable ----

interface IsVisualizationAvailableResult {
  /**
   * foobar2000 offers a visualization stream.
   * @zh foobar2000 提供可视化流。
   */
  available: boolean;
}

// ---- decodePcm ----

interface DecodePcmParams {
  /**
   * Path of the track to decode; a `|subsong:N` suffix selects a subsong.
   * @zh 要解码的曲目路径；带 `|subsong:N` 后缀时选子曲目。
   * @minLength 1
   * @security MediaRead
   */
  path: string;
  /**
   * Cue subsong index, 0-based; takes precedence over a subsong in `path`.
   * @zh cue 子曲目序号，从 0 起；优先于路径里的子曲目。
   * @minimum 0
   */
  cueIndex?: Int;
  /**
   * Start of the range, in seconds.
   * @zh 起点，秒。
   * @minimum 0
   */
  start?: number;
  /**
   * End of the range, in seconds; defaults to the end of the track and is cut to the track length.
   * Must be greater than `start`.
   * @zh 终点，秒；缺省到曲目末尾，超过曲目长度时按末尾截。必须大于 start。
   * @minimum 0
   */
  end?: number;
  /**
   * Sample rate to convert to with foobar2000's resampler; defaults to the source rate.
   * @zh 用 foobar2000 的重采样器转换到的采样率；缺省用源采样率。
   * @minimum 8000
   * @maximum 192000
   */
  sampleRate?: Int;
  /**
   * Average all channels into one.
   * @zh 各声道等权平均成单声道。
   */
  mono?: boolean;
}

interface DecodePcmResult {
  /**
   * Identifies the task in events, shared-buffer metadata and cancelDecodePcm; `pcm_N`.
   * @zh 在事件、共享缓冲附带数据与 cancelDecodePcm 里标识这个任务，形如 `pcm_N`。
   */
  taskId: string;
  /**
   * Always `pending`: decoded PCM is never cached.
   * @zh 恒为 `pending`：解码结果不缓存。
   */
  status: 'pending';
}

// ---- cancelDecodePcm ----

interface CancelDecodePcmParams {
  /**
   * Task id from the decodePcm answer.
   * @zh decodePcm 应答里的任务 id。
   * @minLength 1
   */
  taskId: string;
}

interface CancelDecodePcmResult {
  /**
   * `false` when the task has already ended, does not exist or belongs to another page; these
   * cases are not told apart. A cancelled task receives one `audio:pcmFailed` with
   * `code: 'CANCELLED'`.
   * @zh 为 `false` 表示任务已结束、不存在或属于别的页面，三者不区分。被取消的任务收到一次 `code: 'CANCELLED'` 的 `audio:pcmFailed`。
   */
  cancelled: boolean;
}

// ---- subscribeStream ----

interface SubscribeStreamParams {
  /**
   * Subscription id chosen by the caller; the host generates one (`pcmstream_N`) when absent.
   * Subscribing again with the same id from the same page replaces the earlier subscription.
   * @zh 调用方给的订阅 id；缺省时宿主生成（`pcmstream_N`）。同一页面用同一 id 再订阅会替换先前的订阅。
   * @minLength 1
   */
  subscriptionId?: string;
  /**
   * Callback interval to request from foobar2000, in seconds. The core rounds it to its own tick of
   * about 16 ms and never goes above 200 ms; one subscription asking for a short interval shortens
   * it for every stream subscription in the component. Defaults to the core's 200 ms.
   * @zh 向 foobar2000 请求的回调间隔，秒。核心按自己约 16 ms 的节拍取整，最长 200 ms；一个订阅要短间隔，全组件的流订阅都会跟着变短。缺省用核心的 200 ms。
   * @minimum 0.01
   * @maximum 0.2
   */
  interval?: number;
  /**
   * Ring buffer length, in seconds. Reduced when the buffer would exceed 64 MiB.
   * @zh 环形缓冲长度，秒。整块缓冲超过 64 MiB 时按 64 MiB 折算。
   * @minimum 0.1
   * @maximum 10
   * @default 1
   */
  bufferSeconds?: number;
}

interface SubscribeStreamResult {
  /**
   * The subscription's id, as given or generated.
   * @zh 订阅 id，调用方给的或宿主生成的。
   */
  subscriptionId: string;
  /**
   * Interval requested from the core; absent when the core's default is used.
   * @zh 实际请求核心的间隔；交给核心缺省时不带此键。
   */
  interval?: number;
  /**
   * Ring buffer length registered, in seconds.
   * @zh 登记的环形缓冲长度，秒。
   */
  bufferSeconds: number;
}

// ---- unsubscribeStream ----

interface UnsubscribeStreamParams {
  /**
   * Subscription to remove; omit to remove every stream subscription of the calling page.
   * @zh 要移除的订阅；缺省移除本页面的全部流订阅。
   * @minLength 1
   */
  subscriptionId?: string;
}

interface UnsubscribeStreamResult {
  /**
   * Subscriptions removed: 0 or 1 with a `subscriptionId`, any number without. Subscriptions of
   * other pages are never counted or removed.
   * @zh 移除的订阅数：带 `subscriptionId` 时为 0 或 1，不带时为本页面的全部。别的页面的订阅不计也不动。
   */
  removed: Int;
}

// ---- getPcmDebugState ----

interface GetPcmDebugStateResult {
  /**
   * Shared-buffer support of the calling page; both flags are `false` when the page cannot be
   * located.
   * @zh 调用方页面的共享缓冲支持情况；找不到页面时两个标志都为 `false`。
   */
  runtime: PcmRuntimeState;
  /**
   * The decodePcm task queue.
   * @zh decodePcm 任务队列。
   */
  decode: PcmDecodeState;
  /**
   * The stream capture callback and every stream subscription of every page.
   * @zh 流捕获回调与所有页面的全部流订阅。
   */
  stream: PcmStreamState;
}

interface PcmStreamState {
  /**
   * The component has a capture callback registered with the core; `false` when no subscription
   * exists.
   * @zh 组件在核心登记了捕获回调；没有订阅时为 `false`。
   */
  callbackRegistered: boolean;
  /**
   * Interval currently requested from the core, in seconds; absent when the core's default is used.
   * @zh 当前向核心请求的间隔，秒；交给核心缺省时不带此键。
   */
  interval?: number;
  /**
   * Chunks the callback has received since the component loaded.
   * @zh 组件加载以来回调收到的块数。
   */
  chunkCount: Int;
  /**
   * CPU cycles the callback has spent in total, from `QueryThreadCycleTime`.
   * @zh 回调累计消耗的 CPU 周期，按 `QueryThreadCycleTime` 计。
   */
  chunkCycles: Int;
  /**
   * Every live stream subscription.
   * @zh 全部在途的流订阅。
   */
  subscriptions: PcmStreamDebugEntry[];
}

interface PcmStreamDebugEntry {
  /**
   * The subscription's id.
   * @zh 订阅 id。
   */
  subscriptionId: string;
  /**
   * Window id of the subscribing page.
   * @zh 订阅页面的窗口 id。
   */
  windowId: string;
  /**
   * Buffer generation, `0` before the first chunk of audio arrived.
   * @zh 缓冲代数，首块音频到达前为 `0`。
   */
  epoch: Int;
  /**
   * Frames the current ring holds; `0` before the first chunk.
   * @zh 当前环的容量（帧）；首块前为 `0`。
   */
  capacityFrames: Int;
  /**
   * Frames written into the current ring so far, wrapping at 2^32.
   * @zh 当前环累计写入的帧数，2^32 回绕。
   */
  writeFrames: Int;
}

interface PcmRuntimeState {
  /**
   * Version of the WebView2 runtime installed on the machine, such as `153.0.4234.48`; empty when
   * it cannot be read.
   * @zh 本机安装的 WebView2 运行时版本，如 `153.0.4234.48`；读不到时为空串。
   */
  version: string;
  /**
   * The page's environment can create shared buffers (`ICoreWebView2Environment12`).
   * @zh 页面所在环境能建共享缓冲（`ICoreWebView2Environment12`）。
   */
  environment12: boolean;
  /**
   * The page's webview can receive shared buffers (`ICoreWebView2_17`).
   * @zh 页面的 webview 能接收共享缓冲（`ICoreWebView2_17`）。
   */
  webview17: boolean;
}

interface PcmDecodeState {
  /**
   * Tasks decoding, including aborted ones whose worker has not returned yet.
   * @zh 解码中的任务数，含已发中止、worker 还没返回的。
   */
  active: Int;
  /**
   * Tasks waiting for a free slot.
   * @zh 排队等空位的任务数。
   */
  queued: Int;
  /**
   * Bytes of decode buffers the host has not closed yet.
   * @zh 宿主侧还没关闭的解码缓冲字节数合计。
   */
  openBufferBytes: Int;
}

// ---- events ----

interface FullWaveformReadyPayload {
  /**
   * Id from the `pending` answer.
   * @zh `pending` 应答里的 id。
   */
  taskId: string;
  /**
   * The path the request gave.
   * @zh 请求给的路径。
   */
  path: string;
  /**
   * One value per point, in `scale`; in `[-1, 1]` when `signed`, else in `[0, 1]`.
   * @zh 每个点一个值，刻度为 `scale`；`signed` 时在 `[-1, 1]` 内，否则在 `[0, 1]` 内。
   */
  waveform: number[];
  /**
   * The largest value of the selected sequence before normalisation, in linear full-scale units:
   * with `linear`, `waveform[i] * maxAmplitude` is the absolute level; with `db`, dBFS is
   * `(v * 60 - 60) + 20 * log10(maxAmplitude)`.
   * @zh 归一化前所选序列的最大值，线性满幅单位：`linear` 下 `waveform[i] * maxAmplitude` 是绝对电平；`db` 下 dBFS 为 `(v * 60 - 60) + 20 * log10(maxAmplitude)`。
   */
  maxAmplitude: number;
  /**
   * Track duration in seconds.
   * @zh 曲目时长，秒。
   */
  duration: number;
  /**
   * Sample rate of the decoded track in Hz.
   * @zh 解码出的采样率，Hz。
   */
  sampleRate: Int;
  /**
   * Channel count of the decoded track.
   * @zh 解码出的声道数。
   */
  channels: Int;
  /**
   * Number of points.
   * @zh 点数。
   */
  resolution: Int;
  /**
   * How each point was aggregated, as requested.
   * @zh 每个点的聚合方式，同请求。
   */
  method: WaveformMethod;
  /**
   * Scale of the points, as requested.
   * @zh 点的刻度，同请求。
   */
  scale: WaveformScale;
  /**
   * The points keep PCM polarity, as requested.
   * @zh 点保留 PCM 极性，同请求。
   */
  signed: boolean;
  /**
   * Always `false`: this event only follows a decode.
   * @zh 恒为 `false`：这个事件只在解码之后发。
   */
  cached: boolean;
}

interface FullWaveformFailedPayload {
  /**
   * Id from the `pending` answer.
   * @zh `pending` 应答里的 id。
   */
  taskId: string;
  /**
   * The path the request gave.
   * @zh 请求给的路径。
   */
  path: string;
  /**
   * What went wrong.
   * @zh 出错的描述。
   */
  error: string;
  /**
   * `CANCELLED` after `audio.cancelFullWaveform`; otherwise why decoding failed.
   * @zh 被 `audio.cancelFullWaveform` 取消时为 `CANCELLED`；否则是解码失败的原因。
   */
  code: 'CANCELLED' | 'INVALID_HANDLE' | 'NO_INFO' | 'INVALID_PARAMS' | 'DECODER_FAILED' | 'DECODE_FAILED' | 'UNKNOWN_ERROR';
}

interface PcmReadyPayload {
  /**
   * Id from the `audio.decodePcm` answer.
   * @zh `audio.decodePcm` 应答里的 id。
   */
  taskId: string;
  /**
   * The path the request gave.
   * @zh 请求给的路径。
   */
  path: string;
  /**
   * Samples per second per channel.
   * @zh 每声道每秒的样本数。
   */
  sampleRate: Int;
  /**
   * Channel count in the buffer; `1` when the request set `mono`.
   * @zh 缓冲里的声道数；请求设了 `mono` 时为 `1`。
   */
  channels: Int;
  /**
   * Frames per channel in the buffer.
   * @zh 缓冲里每声道的帧数。
   */
  frames: Int;
  /**
   * Track time of the first frame, in seconds.
   * @zh 第一帧的曲目时间，秒。
   */
  start: number;
  /**
   * Track time right after the last frame, in seconds.
   * @zh 紧接最后一帧之后的曲目时间，秒。
   */
  end: number;
  /**
   * `frames / sampleRate`, in seconds.
   * @zh `frames / sampleRate`，秒。
   */
  duration: number;
  /**
   * The audio ran past the size estimated from the track length, or its format changed partway;
   * the buffer holds the part before the cut.
   * @zh 音频超出了按曲目时长估算的大小，或中途变了格式；缓冲里是截断之前的部分。
   */
  truncated: boolean;
  /**
   * Converted to `sampleRate` with foobar2000's resampler.
   * @zh 用 foobar2000 的重采样器转换到了 `sampleRate`。
   */
  resampled: boolean;
}

interface PcmFailedPayload {
  /**
   * Id from the `audio.decodePcm` answer.
   * @zh `audio.decodePcm` 应答里的 id。
   */
  taskId: string;
  /**
   * The path the request gave.
   * @zh 请求给的路径。
   */
  path: string;
  /**
   * What went wrong.
   * @zh 出错的描述。
   */
  error: string;
  /**
   * `CANCELLED` after `audio.cancelDecodePcm`; `ORIGIN_DENIED` when the page origin stopped being
   * trusted; `OPERATION_FAILED` when the buffer could not be allocated or posted; otherwise why
   * decoding failed. `INVALID_PARAMS` means `start` is at or beyond the end of the track.
   * @zh 被 `audio.cancelDecodePcm` 取消时为 `CANCELLED`；页面来源不再可信时为 `ORIGIN_DENIED`；缓冲分配或投递失败时为 `OPERATION_FAILED`；否则是解码失败的原因。`INVALID_PARAMS` 表示 `start` 到了或越过了曲目末尾。
   */
  code:
    | 'CANCELLED'
    | 'ORIGIN_DENIED'
    | 'OPERATION_FAILED'
    | 'INVALID_PARAMS'
    | 'DECODER_FAILED'
    | 'DECODE_FAILED'
    | 'NOT_SUPPORTED'
    | 'UNKNOWN_ERROR';
}

interface StreamPayload {
  /**
   * Id from the `audio.subscribeStream` answer.
   * @zh `audio.subscribeStream` 应答里的 id。
   */
  subscriptionId: string;
  /**
   * Always `ended`.
   * @zh 恒为 `ended`。
   */
  type: 'ended';
  /**
   * The epoch that ended; its buffer has the `ended` flag set.
   * @zh 结束的那一代；它的缓冲已置 `ended` 标志。
   */
  epoch: Int;
  /**
   * Always `format-change`.
   * @zh 恒为 `format-change`。
   */
  reason: 'format-change';
}

interface ReplaygainModeChangedPayload {
  /**
   * The source mode after the change: `0` none, `1` track, `2` album, `3` by playback order.
   * @zh 变化后的音源模式：`0` 无，`1` 音轨，`2` 专辑，`3` 按播放顺序。
   */
  mode: Int;
}

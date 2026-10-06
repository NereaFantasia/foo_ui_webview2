import type { Int, ReportedPlaylistGuid, Track } from './common.js';

export interface Api {
  /**
   * Start playback, or resume when paused.
   * @zh 开始播放；暂停中则继续。
   * @effect write
   */
  play(): void;

  /**
   * Pause playback. Does nothing when stopped.
   * @zh 暂停播放；停止状态下什么也不做。
   * @effect write
   * @idempotent
   */
  pause(): void;

  /**
   * Stop playback.
   * @zh 停止播放。
   * @effect write
   * @idempotent
   */
  stop(): void;

  /**
   * Toggle between playing and paused; starts playback when stopped. Reports the state that
   * results.
   * @zh 在播放与暂停之间切换；停止状态下开始播放。报告切换后的状态。
   * @effect write
   */
  playPause(): PlayPauseResult;

  /**
   * The same as `playPause`.
   * @zh 与 `playPause` 相同。
   */
  playOrPause(): PlayOrPauseResult;

  /**
   * Skip to the next track under the current playback order.
   * @zh 按当前播放顺序跳到下一首。
   * @effect write
   */
  next(): void;

  /**
   * Skip to the previous track under the current playback order.
   * @zh 按当前播放顺序跳到上一首。
   * @effect write
   */
  previous(): void;

  /**
   * Start a random track of the active playlist.
   * @zh 从活动播放列表里随机起播一首。
   * @effect write
   */
  random(): void;

  /**
   * Report the transport state and what the current track allows.
   * @zh 报告播放状态以及当前曲目允许的操作。
   * @effect read
   */
  getState(): GetStateResult;

  /**
   * Report the playback position together with the length and identity of the track it is in.
   * Everything is zero or empty when nothing is playing.
   * @zh 报告播放位置，以及所在曲目的时长与身份。没有曲目在播放时全部为零或空。
   * @effect read
   */
  getPosition(): GetPositionResult;

  /**
   * Seek within the current track, playing or paused. The position is clamped to the track:
   * negative values seek to the start, values past the end stop just short of it so playback
   * does not advance to the next track; a track of unknown length has no upper bound. With
   * nothing playing the call fails with `NO_ACTIVE_ITEM`, and a track that cannot seek, such as
   * most streams, fails with `NOT_SUPPORTED`.
   * @zh 在当前曲目内定位，播放中或暂停中均可。位置会被夹到曲目范围内：负数定位到开头，超过时长的值停在结尾稍前，免得跳到下一首；时长未知的曲目没有上限。没在播放时以 `NO_ACTIVE_ITEM` 失败，不能定位的曲目（大多数网络流）以 `NOT_SUPPORTED` 失败。
   * @effect write
   * @idempotent
   */
  setPosition(params: SetPositionParams): SetPositionResult;

  /**
   * Report the output volume on both scales the host uses, and whether it is muted.
   * @zh 以宿主用的两种刻度报告输出音量，以及是否静音。
   * @effect read
   */
  getVolume(): GetVolumeResult;

  /**
   * Set the output volume as a percentage; values outside `0` to `100` are clamped. `0` mutes.
   * @zh 按百分比设置输出音量；超出 `0` 到 `100` 的值被夹到范围内。`0` 即静音。
   * @effect write
   * @idempotent
   */
  setVolume(params: SetVolumeParams): void;

  /**
   * Raise the volume by one of the host's steps: one decibel, landing on a whole decibel.
   * @zh 按宿主的步长调高音量：一分贝，落在整数分贝上。
   * @effect write
   */
  volumeUp(): void;

  /**
   * Lower the volume by one of the host's steps: one decibel, landing on a whole decibel.
   * @zh 按宿主的步长调低音量：一分贝，落在整数分贝上。
   * @effect write
   */
  volumeDown(): void;

  /**
   * Mute or unmute; a call that asks for the state already in effect does nothing. Unmuting
   * restores the level from before the mute.
   * @zh 静音或取消静音；要求的状态已经生效时什么也不做。取消静音恢复静音前的音量。
   * @effect write
   * @idempotent
   */
  mute(params: MuteParams): void;

  /**
   * Flip the mute state and report the new one.
   * @zh 翻转静音状态并报告新状态。
   * @effect write
   */
  toggleMute(): ToggleMuteResult;

  /**
   * Report the track now loaded, playing or paused. Always succeeds; `found` says whether there
   * is one.
   * @zh 报告当前载入（播放中或暂停）的曲目。恒成功，有没有曲目看 `found`。
   * @effect read
   */
  getCurrentTrack(): GetCurrentTrackResult;

  /**
   * Report the active playback order as its index and its name.
   * @zh 报告当前播放顺序，同时给出序号与名称。
   * @effect read
   */
  getPlaybackOrder(): GetPlaybackOrderResult;

  /**
   * Select a playback order by index or by name; exactly one of the two must be given. Reports
   * the order in effect afterwards.
   * @zh 按序号或名称选择播放顺序；两者必须恰好给一个。报告设置之后生效的顺序。
   * @effect write
   * @idempotent
   */
  setPlaybackOrder(params: SetPlaybackOrderParams): SetPlaybackOrderResult;

  /**
   * Report whether playback stops after the current track.
   * @zh 报告是否在当前曲目结束后停止。
   * @effect read
   */
  getStopAfterCurrent(): GetStopAfterCurrentResult;

  /**
   * Arm or disarm stopping after the current track.
   * @zh 设置或取消在当前曲目结束后停止。
   * @effect write
   * @idempotent
   */
  setStopAfterCurrent(params: SetStopAfterCurrentParams): SetStopAfterCurrentResult;

  /**
   * Flip the stop-after-current flag and report the new value.
   * @zh 翻转「当前曲目后停止」并报告新值。
   * @effect write
   */
  toggleStopAfterCurrent(): ToggleStopAfterCurrentResult;

  /**
   * Append one file to the active playlist and play it; a playlist is created when there is
   * none. A `|subsong:N` suffix selects a subsong of the file. When the active playlist is locked
   * the call fails with `LOCKED`, and nothing is added or played. A path the media-root check
   * refuses fails with `PERMISSION_DENIED`; one foobar2000 resolves to nothing fails with
   * `NOT_FOUND`, the failure carrying `path` and `subsong`.
   * @zh 把一个文件追加到活动播放列表并播放；没有活动列表时新建一个。`|subsong:N` 后缀选择文件内的子曲目。活动播放列表已上锁时以 `LOCKED` 失败，不添加也不播放。没通过媒体根检查的路径以 `PERMISSION_DENIED` 失败；foobar2000 解析不出任何曲目的路径以 `NOT_FOUND` 失败，失败里带 `path` 与 `subsong`。
   * @effect write
   */
  playPath(params: PlayPathParams): PlayPathResult;

  /**
   * Add several files to the active playlist and start playing one of them. Entries that are not
   * strings or fail the media-root check are dropped and counted in `skippedPaths`, which only a
   * successful call reports; when every entry is dropped the call fails with `INVALID_PARAMS`.
   * Each remaining path becomes a track as written: folders and playlist files are not expanded
   * and the file is not opened, so a missing file is added too. When no track can be made the
   * call fails with `NOT_FOUND`. When the active playlist is locked the call fails with `LOCKED`,
   * and nothing is cleared, added or played.
   * @zh 把多个文件加入活动播放列表并从其中一首起播。不是字符串或没通过媒体根检查的条目被丢弃并计入 `skippedPaths`，只有成功的调用才报这个数；全部被丢弃时以 `INVALID_PARAMS` 失败。其余每条路径按原样做成一首曲目：不展开文件夹与播放列表文件，也不打开文件，所以不存在的文件也会被加入。一首都做不出时以 `NOT_FOUND` 失败。活动播放列表已上锁时以 `LOCKED` 失败，不清空、不添加也不播放。
   */
  playPaths(params: PlayPathsParams): PlayPathsResult;

  /**
   * Locate the playing track in its playlist. Known only when playback started from a playlist
   * item; a track played from elsewhere has no location even while a playing playlist exists.
   * The location is also lost once the playing row is removed from the playlist it started
   * from, the playlist included, while the track keeps playing; and there is none while stopped.
   * @zh 定位正在播放的曲目在播放列表中的位置。只有从播放列表项起播时才有位置；从别处起播的曲目没有位置，即便存在正在播放的列表。正在播放的那一行从起播的列表里删掉（连同列表一起删也算）之后，曲目照常播放，但位置随之丢失；停止时也没有位置。
   * @effect read
   */
  getCurrentTrackIndex(params: GetCurrentTrackIndexParams): GetCurrentTrackIndexResult;

  /**
   * Report the playlist playback was last started from. foobar2000 keeps the pointer after
   * playback stops, so a stopped instance that has played before still reports one.
   * @zh 报告最近一次起播所在的播放列表。foobar2000 在停止后仍保留这个指针，所以播放过的实例在停止态也会报告。
   * @effect read
   */
  getPlayingPlaylist(): GetPlayingPlaylistResult;
}

export interface Events {
  /**
   * foobar2000's Stop after current option changed, through `playback.setStopAfterCurrent`,
   * foobar2000's menu or a component.
   * @zh foobar2000 的「播完当前曲目后停止」选项变了（经 `playback.setStopAfterCurrent`、foobar2000 的菜单或某个组件）。
   * @delivery broadcast
   */
  stopAfterCurrentChanged: StopAfterCurrentChangedPayload;

  /**
   * foobar2000's Playback follows cursor option changed.
   * @zh foobar2000 的「播放跟随光标」选项变了。
   * @delivery broadcast
   */
  followCursorChanged: FollowCursorChangedPayload;

  /**
   * foobar2000's Cursor follows playback option changed.
   * @zh foobar2000 的「光标跟随播放」选项变了。
   * @delivery broadcast
   */
  cursorFollowChanged: CursorFollowChangedPayload;

  /**
   * foobar2000's playback order changed.
   * @zh foobar2000 的播放顺序变了。
   * @delivery broadcast
   */
  orderChanged: OrderChangedPayload;

  /**
   * Playback stopped: by the user, at the end of the playlist, because another track is starting,
   * or because foobar2000 is shutting down.
   * @zh 播放停止了：用户停止、播到列表末尾、要开始另一首，或 foobar2000 正在退出。
   * @delivery broadcast
   */
  stopped: StoppedPayload;

  /**
   * Playback was paused or resumed.
   * @zh 播放暂停或恢复了。
   * @delivery broadcast
   */
  paused: PausedPayload;

  /**
   * The playing track was seeked.
   * @zh 正在播放的曲目跳转了位置。
   * @delivery broadcast
   */
  seeked: SeekedPayload;

  /**
   * foobar2000's volume changed, or it was muted or unmuted.
   * @zh foobar2000 的音量变了，或静音、取消静音了。
   * @delivery broadcast
   */
  volumeChanged: VolumeChangedPayload;

  /**
   * The playback position, about once a second while playing, as foobar2000 reports it for time
   * displays. Not sent while every window is hidden. `playback:timeHighRes` is finer.
   * @zh 播放位置，播放时约每秒一次，即 foobar2000 给时间显示用的那个值。所有窗口都隐藏时不发。`playback:timeHighRes` 更精细。
   * @delivery broadcast
   */
  time: TimePayload;

  /**
   * The playback position, about every 100 ms while playing and not paused, and once more right
   * after a track starts, a seek or a resume. Not sent while every window is hidden.
   * @zh 播放位置：播放且未暂停时约每 100 ms 一次，曲目开始、跳转或恢复播放后也立刻发一次。所有窗口都隐藏时不发。
   * @delivery broadcast
   */
  timeHighRes: TimeHighResPayload;

  /**
   * The decoder reported new dynamic info for the playing stream, such as the bitrate of a VBR file
   * or the title of an internet radio stream.
   * @zh 解码器为正在播放的流报告了新的动态信息，比如 VBR 文件的码率、网络电台的标题。
   * @delivery broadcast
   */
  dynamicInfo: DynamicInfoPayload;

  /**
   * The playing stream announced a new track, as internet radio does. Not sent when it carries
   * neither an artist nor a title.
   * @zh 正在播放的流报出了新曲目，网络电台就会这样。既没有艺术家也没有标题时不发。
   * @delivery broadcast
   */
  dynamicInfoTrack: DynamicInfoTrackPayload;

  /**
   * Playback is starting, after a play, next, previous or random command.
   * @zh 播放即将开始，起因是播放、下一首、上一首或随机命令。
   * @delivery broadcast
   */
  starting: StartingPayload;

  /**
   * The playback state changed: `playing` when a track starts and on resume, `paused` on pause or
   * when playback starts paused, `stopped` when playback stops with reason `user` or `eof` (see
   * `playback:stopped`). `playback.next` stops the playing track with reason `starting_another`
   * and sends no `stopped` state, but foobar2000 reports `playlist.playTrack` on another row as a
   * `user` stop, so a `stopped` state arrives before the new track's events. When a track starts,
   * the event sent right after `playback:trackChanged` carries that track's `canSeek`; an earlier
   * one sent with `playback:starting`, before the track is opened, can report `false`.
   * @zh 播放状态变了：曲目开始和恢复播放时为 `playing`，暂停或以暂停状态开始播放时为 `paused`，播放以 `user` 或 `eof` 原因停止时为 `stopped`（原因见 `playback:stopped`）。`playback.next` 以 `starting_another` 停止当前曲目，不发 `stopped`；而用 `playlist.playTrack` 播另一行时，foobar2000 报的是 `user` 停止，所以新曲目的事件之前会先来一条 `stopped`。曲目开始播放时，紧跟在 `playback:trackChanged` 之后的那条带的是这首曲目的 `canSeek`；在它之前随 `playback:starting` 发出的那条，曲目还没打开，`canSeek` 可能为 `false`。
   * @delivery broadcast
   */
  stateChanged: StateChangedPayload;

  /**
   * foobar2000's playback queue changed. A `queue.*` call that changes the queue several times
   * sends one event at the end.
   * @zh foobar2000 的播放队列变了。一次改动队列多次的 `queue.*` 调用只在最后发一个事件。
   * @delivery broadcast
   */
  queueChanged: QueueChangedPayload;

  /**
   * A track started playing: playback began, or moved on to another track. The payload is that
   * track, the same row `playback.getCurrentTrack` answers with. A stream that announces a new
   * title, as internet radio does, sends `playback:dynamicInfoTrack` instead.
   * @zh 一首曲目开始播放了：播放开始，或切到了另一首。载荷就是这首曲目，与 `playback.getCurrentTrack` 应答里的曲目行相同。流报出新标题（网络电台就会这样）时发的是 `playback:dynamicInfoTrack`。
   * @delivery broadcast
   */
  trackChanged: Track;

  /**
   * The information of the playing track changed, for example its tags were edited. The payload
   * is the track as it reads now.
   * @zh 正在播放的曲目信息变了，比如标签被编辑了。载荷是这首曲目现在读到的样子。
   * @delivery broadcast
   */
  edited: Track;

  /**
   * foobar2000 counted the playing track as played: 60 seconds of it have been played, or it
   * reached its end after at least a third of it was played. This is when foobar2000's playback
   * statistics record a play.
   * @zh foobar2000 把正在播放的曲目记为已播放：播满了 60 秒，或播到结尾且至少播过三分之一。foobar2000 的播放统计就在这时记一次播放。
   * @delivery broadcast
   */
  itemPlayed: Track;
}

interface OrderChangedPayload {
  /**
   * Index of the new order, as `order` in the `playback.getPlaybackOrder` answer.
   * @zh 新顺序的序号，同 `playback.getPlaybackOrder` 应答里的 `order`。
   */
  orderIndex: Int;
  /**
   * The same as `orderIndex`.
   * @zh 与 `orderIndex` 相同。
   */
  order: Int;
}

interface StopAfterCurrentChangedPayload {
  /**
   * The new value of the option.
   * @zh 选项的新值。
   */
  enabled: boolean;
}

interface FollowCursorChangedPayload {
  /**
   * The new value of the option.
   * @zh 选项的新值。
   */
  enabled: boolean;
}

interface CursorFollowChangedPayload {
  /**
   * The new value of the option.
   * @zh 选项的新值。
   */
  enabled: boolean;
}

/**
 * Transport state: `stopped`, `playing` or `paused`.
 * @zh 播放状态：`stopped`、`playing` 或 `paused`。
 */
type PlaybackStateName = 'stopped' | 'playing' | 'paused';

/**
 * A playback order by name, in the order foobar2000 numbers them from `0`.
 * @zh 播放顺序的名称，按 foobar2000 从 `0` 起的编号排列。
 */
type PlaybackOrderName =
  | 'default'
  | 'repeat-playlist'
  | 'repeat-track'
  | 'random'
  | 'shuffle-tracks'
  | 'shuffle-albums'
  | 'shuffle-folders';

interface PlayPauseResult {
  /**
   * Whether playback is running after the toggle.
   * @zh 切换后是否在播放。
   */
  isPlaying: boolean;
}

interface PlayOrPauseResult {
  /**
   * Whether playback is running after the toggle.
   * @zh 切换后是否在播放。
   */
  isPlaying: boolean;
}

interface GetStateResult {
  /**
   * Transport state.
   * @zh 播放状态。
   */
  state: PlaybackStateName;
  /**
   * Whether the current track can be seeked; `false` when stopped and for streams that do not
   * support it.
   * @zh 当前曲目能否定位；停止态与不支持定位的流为 `false`。
   */
  canSeek: boolean;
  /**
   * Whether the current track can be paused; always `true`.
   * @zh 当前曲目能否暂停；恒为 `true`。
   */
  canPause: boolean;
}

interface GetPositionResult {
  /**
   * Audible position in seconds from the start of the track, already adjusted for output-buffer
   * and DSP latency; do not subtract that latency again. The host advances this reading in
   * steps, typically 10–30 ms apart. It can briefly stall or jump after starting, seeking or
   * resuming, and during crossfades; it is not a sample-accurate clock.
   * @zh 距曲目开头的可听位置，单位秒，已扣除输出缓冲与 DSP 延迟，不要再次扣减。宿主读数呈阶梯变化，通常每 10–30 ms 更新一次；起播、定位、恢复播放后及交叉渐变期间可能短暂停滞或跳变，不是采样级精确时钟。
   */
  position: number;
  /**
   * Length of the track in seconds; `0` when nothing is playing or the length is unknown.
   * @zh 曲目时长，单位秒；没有曲目在播放或时长未知时为 `0`。
   */
  duration: number;
  /**
   * Subsong identifier of the track; `0` for a whole file.
   * @zh 曲目的子曲目标识；整个文件为 `0`。
   */
  subsong: Int;
  /**
   * Path as foobar2000 stores it, without a subsong suffix; empty when nothing is playing.
   * @zh foobar2000 存储形式的路径，不带子曲目后缀；没有曲目在播放时为空。
   */
  path: string;
  /**
   * Host system time when `position` was read, Unix epoch milliseconds with a fractional part,
   * the same clock as `Date.now()`. While playing and not paused,
   * `position + (Date.now() - hostTime) / 1000` estimates the current position.
   * @zh 读 `position` 时宿主的系统时间，Unix 纪元毫秒，带小数，与 `Date.now()` 是同一个时钟。播放且未暂停时，`position + (Date.now() - hostTime) / 1000` 可估出当前位置。
   */
  hostTime: number;
}

interface SetPositionParams {
  /**
   * Target position in seconds.
   * @zh 目标位置，单位秒。
   */
  position: number;
}

interface SetPositionResult {
  /**
   * The position asked for, before clamping.
   * @zh 请求的位置，未夹取。
   */
  requestedPosition: number;
  /**
   * The position handed to the host after clamping to the track.
   * @zh 夹到曲目范围内后交给宿主的位置。
   */
  actualPosition: number;
  /**
   * Position read before the seek, in seconds, with the same latency compensation as
   * `playback.getPosition`.
   * @zh 定位前读到的位置，单位秒，与 `playback.getPosition` 一样已补偿输出延迟。
   */
  oldPosition: number;
  /**
   * Position read right after the seek, in seconds, with the same latency compensation as
   * `playback.getPosition`; the engine may still be settling, so this can still be the old position.
   * @zh 定位后立即读到的位置，单位秒，与 `playback.getPosition` 一样已补偿输出延迟；引擎可能尚未稳定，因此仍可能是旧位置。
   */
  newPosition: number;
  /**
   * Host system time when `newPosition` was read, Unix epoch milliseconds with a fractional part,
   * the same clock as `Date.now()`.
   * @zh 读 `newPosition` 时宿主的系统时间，Unix 纪元毫秒，带小数，与 `Date.now()` 是同一个时钟。
   */
  hostTime: number;
  /**
   * Length of the track in seconds; `0` or less when the length is unknown.
   * @zh 曲目时长，单位秒；时长未知时为 `0` 或更小。
   */
  duration: number;
  /**
   * Subsong identifier of the track.
   * @zh 曲目的子曲目标识。
   */
  subsong: Int;
}

interface GetVolumeResult {
  /**
   * Volume as a percentage from `0` to `100`, converted from the decibel value; `0` when muted.
   * @zh 音量百分比，`0` 到 `100`，由分贝值换算；静音时为 `0`。
   */
  volume: number;
  /**
   * Volume in decibels, `-100` (silence) to `0` (full).
   * @zh 音量分贝值，`-100`（静音）到 `0`（最大）。
   */
  volumeDb: number;
  /**
   * Whether output is muted; volume `0` counts as muted.
   * @zh 是否静音；音量为 `0` 也算静音。
   */
  muted: boolean;
  /**
   * The same as `muted`.
   * @zh 与 `muted` 相同。
   */
  isMuted: boolean;
}

interface SetVolumeParams {
  /**
   * Volume as a percentage; clamped to `0` to `100`.
   * @zh 音量百分比；夹到 `0` 到 `100`。
   */
  volume: number;
}

interface MuteParams {
  /**
   * `true` to mute, `false` to unmute.
   * @zh `true` 静音，`false` 取消静音。
   * @default true
   */
  muted?: boolean;
}

interface ToggleMuteResult {
  /**
   * Whether output is muted after the toggle.
   * @zh 翻转后是否静音。
   */
  muted: boolean;
}

interface GetCurrentTrackResult {
  /**
   * Whether a track is loaded.
   * @zh 是否有曲目载入。
   */
  found: boolean;
  /**
   * The loaded track; absent when `found` is `false`.
   * @zh 载入的曲目；`found` 为 `false` 时没有这个键。
   */
  track?: Track;
}

interface GetPlaybackOrderResult {
  /**
   * Index of the active order, `0` to `6`.
   * @zh 当前顺序的序号，`0` 到 `6`。
   */
  order: Int;
  /**
   * Name of the active order.
   * @zh 当前顺序的名称。
   */
  orderName: PlaybackOrderName;
  /**
   * The same as `orderName`.
   * @zh 与 `orderName` 相同。
   */
  name: PlaybackOrderName;
  /**
   * The same as `order`.
   * @zh 与 `order` 相同。
   */
  orderIndex: Int;
}

interface SetPlaybackOrderParams {
  /**
   * Order by index, `0` to `6` in the order of `PlaybackOrderName`.
   * @zh 按序号选择，`0` 到 `6`，顺序同 `PlaybackOrderName`。
   * @minimum 0
   * @maximum 6
   */
  order?: Int;
  /**
   * Order by name.
   * @zh 按名称选择。
   */
  name?: PlaybackOrderName;
}

interface SetPlaybackOrderResult {
  /**
   * Index of the order in effect after the call.
   * @zh 调用后生效的顺序序号。
   */
  order: Int;
  /**
   * Name of the order in effect after the call.
   * @zh 调用后生效的顺序名称。
   */
  orderName: PlaybackOrderName;
}

interface GetStopAfterCurrentResult {
  /**
   * Whether playback stops after the current track.
   * @zh 是否在当前曲目结束后停止。
   */
  enabled: boolean;
}

interface SetStopAfterCurrentParams {
  /**
   * `true` to stop after the current track.
   * @zh `true` 表示当前曲目结束后停止。
   */
  enabled: boolean;
}

interface SetStopAfterCurrentResult {
  /**
   * The value now in effect.
   * @zh 现在生效的值。
   */
  enabled: boolean;
}

interface ToggleStopAfterCurrentResult {
  /**
   * The value after the toggle.
   * @zh 翻转后的值。
   */
  enabled: boolean;
}

interface PlayPathParams {
  /**
   * File path, optionally with a `|subsong:N` suffix.
   * @zh 文件路径，可带 `|subsong:N` 后缀。
   * @minLength 1
   * @security MediaRead
   */
  path: string;
}

interface PlayPathResult {
  /**
   * Number of playlist rows added; `1`.
   * @zh 加入播放列表的行数；为 `1`。
   */
  tracksAdded: Int;
  /**
   * The file path without the subsong suffix.
   * @zh 去掉子曲目后缀的文件路径。
   */
  path: string;
  /**
   * Subsong identifier of the track that plays: the one asked for with `|subsong:N`, even when
   * foobar2000 did not list it among the file's subsongs; without the suffix, the file's first.
   * @zh 实际播放曲目的子曲目标识：用 `|subsong:N` 请求的就是它，即便 foobar2000 没把它列为文件的子曲目；不带后缀时是文件的第一个。
   */
  subsong: Int;
}

interface PlayPathsParams {
  /**
   * File paths, each optionally with a `|subsong:N` suffix.
   * @zh 文件路径，每条可带 `|subsong:N` 后缀。
   * @minItems 1
   * @security MediaRead
   * @skipInvalid
   */
  paths: string[];
  /**
   * Which of the added tracks to start with, counted among the tracks that resolved; a value
   * past the end starts the first.
   * @zh 从加入的第几首起播，按解析成功的曲目计数；超出范围时从第一首起播。
   * @minimum 0
   * @default 0
   */
  startIndex?: Int;
  /**
   * `true` clears the active playlist before adding; `false` appends.
   * @zh `true` 先清空活动播放列表再加入；`false` 追加。
   * @default false
   */
  replace?: boolean;
}

interface PlayPathsResult {
  /**
   * Number of tracks added to the playlist.
   * @zh 加入播放列表的曲目数。
   */
  tracksAdded: Int;
  /**
   * The `startIndex` given, echoed even when it was past the end and playback started at the
   * first added track.
   * @zh 传入的 `startIndex` 原样回显；它超出范围、实际从第一首起播时也是如此。
   */
  startedAt: Int;
}

interface GetCurrentTrackIndexParams {
  /**
   * `true` adds the track row at that location.
   * @zh `true` 时附带该位置的曲目行。
   * @default false
   */
  includeTrackInfo?: boolean;
}

interface GetCurrentTrackIndexResult {
  /**
   * Whether the playing track has a playlist location.
   * @zh 正在播放的曲目是否有播放列表位置。
   */
  found: boolean;
  /**
   * Playlist index; `null` when there is no location.
   * @zh 播放列表序号；没有位置时为 `null`。
   */
  playlist: Int | null;
  /**
   * GUID of that playlist, as `playlistGuid` takes it; `null` when there is no location.
   * @zh 该播放列表的 GUID，写法与 `playlistGuid` 接受的一致；没有位置时为 `null`。
   */
  playlistGuid: ReportedPlaylistGuid | null;
  /**
   * Row in that playlist; `null` when there is no location.
   * @zh 在该播放列表中的行号；没有位置时为 `null`。
   */
  index: Int | null;
  /**
   * The track at that location; only with `includeTrackInfo`, and only when the row still
   * exists.
   * @zh 该位置的曲目；只在 `includeTrackInfo` 为真且该行仍存在时给出。
   */
  track?: Track;
}

interface GetPlayingPlaylistResult {
  /**
   * Whether a playing playlist is known.
   * @zh 是否有已知的正在播放的列表。
   */
  found: boolean;
  /**
   * Playlist index; `null` when none is known.
   * @zh 播放列表序号；没有时为 `null`。
   */
  playlist: Int | null;
  /**
   * GUID of that playlist, as `playlistGuid` takes it; `null` when none is known.
   * @zh 该播放列表的 GUID，写法与 `playlistGuid` 接受的一致；没有时为 `null`。
   */
  playlistGuid: ReportedPlaylistGuid | null;
  /**
   * Name of that playlist; absent when none is known.
   * @zh 该播放列表的名称；没有时没有这个键。
   */
  name?: string;
}

// ---- events ----

interface StoppedPayload {
  /**
   * Why playback stopped: `user`, `eof` (end of the playlist), `starting_another`,
   * `shutting_down`, or `unknown`, as foobar2000 reports it. `playback.next` while playing is
   * `starting_another`; `playlist.playTrack` on another row while playing is `user`.
   * @zh 停止原因：`user`、`eof`（列表播完）、`starting_another`、`shutting_down` 或 `unknown`，取 foobar2000 报的值。播放中调 `playback.next` 是 `starting_another`；播放中用 `playlist.playTrack` 播另一行是 `user`。
   */
  reason: 'user' | 'eof' | 'starting_another' | 'shutting_down' | 'unknown';
}

interface PausedPayload {
  /**
   * `true` when paused, `false` when resumed.
   * @zh 暂停时为 `true`，恢复时为 `false`。
   */
  paused: boolean;
}

interface SeekedPayload {
  /**
   * The position seeked to, in seconds.
   * @zh 跳转到的位置，秒。
   */
  position: number;
  /**
   * Host system time when foobar2000 reported the seek, Unix epoch milliseconds with a fractional
   * part, the same clock as `Date.now()`. `position` is the seek target, not a position read at
   * that time; the `playback:timeHighRes` sent right after can still carry the old position.
   * @zh foobar2000 报出这次跳转时宿主的系统时间，Unix 纪元毫秒，带小数，与 `Date.now()` 是同一个时钟。`position` 是跳转目标，不是这一刻读到的位置；紧随其后的 `playback:timeHighRes` 仍可能带着旧位置。
   */
  hostTime: number;
}

interface VolumeChangedPayload {
  /**
   * Volume as a linear percentage, `0` to `100`, as `playback.getVolume` reports it.
   * @zh 线性百分比音量，`0` 到 `100`，同 `playback.getVolume`。
   */
  volume: number;
  /**
   * Volume in dB, `0` at full volume and `-100` at the bottom.
   * @zh 音量，dB；满音量为 `0`，最低为 `-100`。
   */
  volumeDb: number;
  /**
   * Whether foobar2000 is muted.
   * @zh foobar2000 是否静音。
   */
  muted: boolean;
  /**
   * The same as `muted`.
   * @zh 与 `muted` 相同。
   */
  isMuted: boolean;
}

interface TimePayload {
  /**
   * Coarse playback position in seconds, supplied by the host for time displays, without a
   * sample timestamp. Use `playback:timeHighRes` or `playback.getPosition` for synchronization.
   * @zh 宿主给时间显示用的粗粒度播放位置，单位秒，不带采样时刻。同步画面请用 `playback:timeHighRes` 或 `playback.getPosition`。
   */
  position: number;
}

interface TimeHighResPayload {
  /**
   * Position in seconds, with its fractional part, using the same output-buffer and DSP latency
   * compensation and stepwise reading as `playback.getPosition`.
   * @zh 位置，单位秒，带小数部分；与 `playback.getPosition` 一样已扣除输出缓冲与 DSP 延迟，读数呈阶梯变化。
   */
  position: number;
  /**
   * Host system time when `position` was read, Unix epoch milliseconds with a fractional part,
   * the same clock as `Date.now()`. `Date.now() - hostTime` is how late the event arrived, and
   * `position + (Date.now() - hostTime) / 1000` estimates the current position.
   * @zh 读 `position` 时宿主的系统时间，Unix 纪元毫秒，带小数，与 `Date.now()` 是同一个时钟。`Date.now() - hostTime` 是事件晚到了多久，`position + (Date.now() - hostTime) / 1000` 可估出当前位置。
   */
  hostTime: number;
}

interface DynamicInfoPayload {
  /**
   * Current bitrate in kbit/s; `0` when unknown.
   * @zh 当前码率，kbit/s；未知时为 `0`。
   */
  bitrate: Int;
  /**
   * The first `TITLE` value of the dynamic info, when it has one.
   * @zh 动态信息里的第一个 `TITLE` 值，有才带。
   */
  streamTitle?: string;
}

interface DynamicInfoTrackPayload {
  /**
   * Artists of the new track, joined with `, `; absent when there is none.
   * @zh 新曲目的艺术家，以 `, ` 连接；没有时不带。
   */
  artist?: string;
  /**
   * The first title of the new track; absent when there is none.
   * @zh 新曲目的第一个标题；没有时不带。
   */
  title?: string;
}

interface StartingPayload {
  /**
   * The command that started playback.
   * @zh 启动播放的命令。
   */
  command: 'play' | 'next' | 'previous' | 'random' | 'unknown';
  /**
   * Whether playback starts paused.
   * @zh 是否以暂停状态开始。
   */
  paused: boolean;
}

interface StateChangedPayload {
  /**
   * The new state.
   * @zh 新状态。
   */
  state: 'playing' | 'paused' | 'stopped';
  /**
   * Playback position in seconds, read when the event is sent, with the same latency compensation
   * and transient behavior as `playback.getPosition`.
   * @zh 发事件时读到的播放位置，单位秒；输出延迟补偿与状态切换后的瞬态同 `playback.getPosition`。
   */
  position: number;
  /**
   * Length of the playing track in seconds, read when the event is sent.
   * @zh 发事件时读到的当前曲目时长，秒。
   */
  duration: number;
  /**
   * Whether the playing track can be seeked, read when the event is sent, as `canSeek` in the
   * `playback.getState` answer; always `false` for `stopped`.
   * @zh 发事件时读到的当前曲目能否定位，同 `playback.getState` 应答里的 `canSeek`；`stopped` 时恒为 `false`。
   */
  canSeek: boolean;
  /**
   * Host system time when `position` was read, Unix epoch milliseconds with a fractional part,
   * the same clock as `Date.now()`.
   * @zh 读 `position` 时宿主的系统时间，Unix 纪元毫秒，带小数，与 `Date.now()` 是同一个时钟。
   */
  hostTime: number;
}

interface QueueChangedPayload {
  /**
   * What changed the queue: `user_added`, `user_removed`, `playback_advance` (playback took the
   * next queued track), or `unknown`.
   * @zh 队列因何变化：`user_added`、`user_removed`、`playback_advance`（播放取走了下一条排队曲目）或 `unknown`。
   */
  origin: 'user_added' | 'user_removed' | 'playback_advance' | 'unknown';
  /**
   * Queue length after the change.
   * @zh 变化后的队列长度。
   */
  count: Int;
}

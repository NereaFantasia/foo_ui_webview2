import type { Int } from './common.js';

export interface Api {
  /**
   * Read the whole ReplayGain configuration: source mode, processing mode, both preamps and
   * whether ReplayGain is in effect.
   * @zh 读取完整的 ReplayGain 配置：音源模式、处理模式、两个预增益，以及 ReplayGain 是否生效。
   */
  getSettings(): GetSettingsResult;

  /**
   * Read the source mode and the processing mode.
   * @zh 读取音源模式与处理模式。
   */
  getMode(): GetModeResult;

  /**
   * Change the source mode, the processing mode, or both; a key left out keeps its value. The
   * response reports the modes in effect afterwards.
   * @zh 修改音源模式、处理模式或两者；没给的键保持原值。响应报告修改后生效的模式。
   */
  setMode(params: SetModeParams): SetModeResult;

  /**
   * Read the two preamps, in dB.
   * @zh 读取两个预增益，单位 dB。
   */
  getPreamp(): GetPreampResult;

  /**
   * Change one or both preamps; a key left out keeps its value. The response reports the values
   * in effect afterwards.
   * @zh 修改一个或两个预增益；没给的键保持原值。响应报告修改后生效的值。
   */
  setPreamp(params: SetPreampParams): SetPreampResult;

  /**
   * Read the ReplayGain values stored in files, one row per path. A file the host cannot read
   * is a failed row, not a failed call.
   * @zh 读取文件里存储的 ReplayGain 值，每个路径一行。读不了的文件是失败的行，不是失败的调用。
   */
  get(params: GetParams): GetResult;

  /**
   * Remove the ReplayGain values from files. The tags are rewritten in the background; read the
   * files again to see the values gone.
   * @zh 从文件中移除 ReplayGain 值。标签在后台重写，再读一次文件才能看到值消失。
   */
  clear(params: ClearParams): ClearResult;

  /**
   * Start foobar2000's own ReplayGain scanner on the files through its context menu command. The
   * call returns when the scan has been started; results are written to the files by the scanner.
   * @zh 通过右键菜单命令对文件启动 foobar2000 自带的 ReplayGain 扫描。扫描发起后即返回，结果由扫描器写入文件。
   */
  scan(params: ScanParams): ScanResult;
}

/**
 * Where the gain applied during playback comes from: `none` applies no gain, `track` and
 * `album` use the respective stored value, `auto` follows the playback order (track gain in
 * shuffle, album gain otherwise).
 * @zh 播放时应用的增益来源：`none` 不应用，`track`、`album` 用对应的存储值，`auto` 跟随播放顺序（随机时用音轨增益，否则用专辑增益）。
 */
type SourceMode = 'none' | 'track' | 'album' | 'auto';

/**
 * A source mode to set; `byPlaybackOrder` is the older spelling of `auto`.
 * @zh 要设置的音源模式；`byPlaybackOrder` 是 `auto` 的旧写法。
 */
type SourceModeInput = 'none' | 'track' | 'album' | 'auto' | 'byPlaybackOrder';

/**
 * What is applied: `gain` the gain only, `peak` peak limiting only, `gain_and_peak` both, `none`
 * nothing.
 * @zh 应用什么：`gain` 只应用增益，`peak` 只做峰值限制，`gain_and_peak` 两者都做，`none` 什么都不做。
 */
type ProcessingMode = 'none' | 'gain' | 'gain_and_peak' | 'peak';

interface GetSettingsResult {
  sourceMode: SourceMode;
  processingMode: ProcessingMode;
  /**
   * Preamp applied to tracks that carry ReplayGain, in dB.
   * @zh 对带 ReplayGain 的曲目应用的预增益，单位 dB。
   */
  preampWithRg: number;
  /**
   * Preamp applied to tracks without ReplayGain, in dB.
   * @zh 对不带 ReplayGain 的曲目应用的预增益，单位 dB。
   */
  preampWithoutRg: number;
  /**
   * Whether ReplayGain changes the output: `false` when the source mode is `none` or `auto`, or
   * the processing mode is `none`.
   * @zh ReplayGain 是否在改变输出：音源模式为 `none` 或 `auto`、或处理模式为 `none` 时为 `false`。
   */
  active: boolean;
}

interface GetModeResult {
  sourceMode: SourceMode;
  processingMode: ProcessingMode;
}

interface SetModeParams {
  sourceMode?: SourceModeInput;
  processingMode?: ProcessingMode;
}

interface SetModeResult {
  sourceMode: SourceMode;
  processingMode: ProcessingMode;
  /**
   * Whether the call changed anything.
   * @zh 这次调用是否改了什么。
   */
  changed: boolean;
}

interface GetPreampResult {
  /**
   * Preamp applied to tracks that carry ReplayGain, in dB.
   * @zh 对带 ReplayGain 的曲目应用的预增益，单位 dB。
   */
  withRg: number;
  /**
   * Preamp applied to tracks without ReplayGain, in dB.
   * @zh 对不带 ReplayGain 的曲目应用的预增益，单位 dB。
   */
  withoutRg: number;
}

interface SetPreampParams {
  /**
   * Preamp for tracks that carry ReplayGain, in dB.
   * @zh 带 ReplayGain 的曲目的预增益，单位 dB。
   * @minimum -24
   * @maximum 24
   */
  withRg?: number;
  /**
   * Preamp for tracks without ReplayGain, in dB.
   * @zh 不带 ReplayGain 的曲目的预增益，单位 dB。
   * @minimum -24
   * @maximum 24
   */
  withoutRg?: number;
}

interface SetPreampResult {
  /**
   * Preamp applied to tracks that carry ReplayGain, in dB.
   * @zh 对带 ReplayGain 的曲目应用的预增益，单位 dB。
   */
  withRg: number;
  /**
   * Preamp applied to tracks without ReplayGain, in dB.
   * @zh 对不带 ReplayGain 的曲目应用的预增益，单位 dB。
   */
  withoutRg: number;
  /**
   * Whether the call changed anything.
   * @zh 这次调用是否改了什么。
   */
  changed: boolean;
}

interface GetParams {
  /**
   * Tracks to read. A `|subsong:N` suffix selects a subsong, such as one track of a CUE sheet;
   * a path without it reads the first track.
   * @zh 要读取的曲目。带 `|subsong:N` 后缀时选子曲目，如 CUE 里的一首；不带后缀读第一首。
   * @security MediaRead
   */
  paths: string[];
}

/** The ReplayGain values of one file. Each value appears only when the file stores it. */
interface ReplayGainTrackInfo {
  /**
   * The path as requested, including any `|subsong:N` suffix.
   * @zh 请求时给的路径，后缀 `|subsong:N` 原样保留。
   */
  path: string;
  /**
   * Whether the file could be read.
   * @zh 文件是否读得出来。
   */
  success: boolean;
  /**
   * Why the file could not be read; present when `success` is `false`.
   * @zh 读不出来的原因；`success` 为 `false` 时出现。
   */
  error?: string;
  /**
   * Whether the file stores a track gain or an album gain. Present when `success` is `true`.
   * @zh 文件是否存有音轨增益或专辑增益。`success` 为 `true` 时出现。
   */
  hasReplayGain?: boolean;
  /**
   * Track gain formatted with two decimals and a `dB` suffix, such as `-7.25 dB`.
   * @zh 音轨增益，两位小数加 `dB` 后缀，如 `-7.25 dB`。
   */
  trackGain?: string;
  /**
   * Track gain in dB.
   * @zh 音轨增益，单位 dB。
   */
  trackGainRaw?: number;
  /**
   * Track peak formatted with six decimals.
   * @zh 音轨峰值，六位小数。
   */
  trackPeak?: string;
  /**
   * Track peak as a linear amplitude.
   * @zh 音轨峰值，线性幅度。
   */
  trackPeakRaw?: number;
  /**
   * Album gain formatted with two decimals and a `dB` suffix.
   * @zh 专辑增益，两位小数加 `dB` 后缀。
   */
  albumGain?: string;
  /**
   * Album gain in dB.
   * @zh 专辑增益，单位 dB。
   */
  albumGainRaw?: number;
  /**
   * Album peak formatted with six decimals.
   * @zh 专辑峰值，六位小数。
   */
  albumPeak?: string;
  /**
   * Album peak as a linear amplitude.
   * @zh 专辑峰值，线性幅度。
   */
  albumPeakRaw?: number;
}

interface GetResult {
  /**
   * Number of rows in `results`.
   * @zh `results` 的行数。
   */
  count: Int;
  /**
   * One row per requested path, in request order.
   * @zh 每个请求路径一行，按请求顺序。
   */
  results: ReplayGainTrackInfo[];
}

interface ClearParams {
  /**
   * Tracks whose ReplayGain values are removed. A `|subsong:N` suffix selects a subsong, such as
   * one track of a CUE sheet; a path without it means the first track.
   * @zh 要移除 ReplayGain 值的曲目。带 `|subsong:N` 后缀时选子曲目，如 CUE 里的一首；不带后缀指第一首。
   * @minItems 1
   * @security MediaWrite
   */
  paths: string[];
}

interface ClearResult {
  /**
   * Number of files handed to the tag writer.
   * @zh 交给标签写入器的文件数。
   */
  clearedCount: Int;
}

interface ScanParams {
  /**
   * Tracks to scan. A `|subsong:N` suffix selects a subsong, such as one track of a CUE sheet;
   * a path without it means the first track.
   * @zh 要扫描的曲目。带 `|subsong:N` 后缀时选子曲目，如 CUE 里的一首；不带后缀指第一首。
   * @minItems 1
   * @security MediaRead
   */
  paths: string[];
  /**
   * `track` scans each file's own gain; `album` scans the files as one album.
   * @zh `track` 逐文件扫描音轨增益；`album` 把这些文件当一张专辑扫描。
   * @default "track"
   */
  mode?: 'track' | 'album';
}

interface ScanResult {
  /**
   * Number of files handed to the scanner.
   * @zh 交给扫描器的文件数。
   */
  scannedCount: Int;
  /**
   * The mode the scan runs in.
   * @zh 扫描使用的模式。
   */
  mode: 'track' | 'album';
  /**
   * A fixed English note saying the scan runs in the background.
   * @zh 一句固定的英文说明，指出扫描在后台进行。
   */
  note: string;
}

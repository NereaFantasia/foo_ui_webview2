import type { Int } from './common.js';

export interface Api {
  /**
   * Read the whole ReplayGain configuration: source mode, processing mode, both preamps and
   * whether ReplayGain is in effect.
   * @zh 获取完整 ReplayGain 配置（包含音源模式、处理模式、两项预增益 dB 值及当前是否实际生效）
   */
  getSettings(): GetSettingsResult;

  /**
   * Read the source mode and the processing mode.
   * @zh 获取当前配置的 ReplayGain 音源模式与处理模式
   */
  getMode(): GetModeResult;

  /**
   * Change the source mode, the processing mode, or both; a key left out keeps its value. The
   * response reports the modes in effect afterwards.
   * @zh 修改 ReplayGain 音源模式或处理模式（缺省参数保持原值；调用返回修改后实际生效的模式）
   */
  setMode(params: SetModeParams): SetModeResult;

  /**
   * Read the two preamps, in dB.
   * @zh 获取带 ReplayGain 与不带 ReplayGain 曲目的预增益数值（单位 dB）
   */
  getPreamp(): GetPreampResult;

  /**
   * Change one or both preamps; a key left out keeps its value. The response reports the values
   * in effect afterwards.
   * @zh 设置带 ReplayGain 与不带 ReplayGain 曲目的预增益数值（缺省参数保持原值；取值范围 -24 到 24 dB；调用返回修改后实际生效的值）
   */
  setPreamp(params: SetPreampParams): SetPreampResult;

  /**
   * Read the ReplayGain values stored in files, one row per path. A file the host cannot read
   * is a failed row, not a failed call.
   * @zh 批量读取音频文件标签中存储的 ReplayGain 增益与峰值数据（单文件读取失败仅标记对应结果项失败，不导致整批调用报错）
   */
  get(params: GetParams): GetResult;

  /**
   * Remove the ReplayGain values from files. The tags are rewritten in the background; read the
   * files again to see the values gone.
   * @zh 批量从指定音频文件标签中清除 ReplayGain 增益信息（标签在后台异步重写）
   */
  clear(params: ClearParams): ClearResult;

  /**
   * Start foobar2000's own ReplayGain scanner on the files through its context menu command. The
   * call returns when the scan has been started; results are written to the files by the scanner.
   * @zh 调用原生上下文菜单命令启动 foobar2000 原生 ReplayGain 扫描器（任务启动后即刻返回，扫描结果由原生组件异步回写文件）
   */
  scan(params: ScanParams): ScanResult;
}

/**
 * Where the gain applied during playback comes from: `none` applies no gain, `track` and
 * `album` use the respective stored value, `auto` follows the playback order (track gain in
 * shuffle, album gain otherwise).
 * @zh 播放时应用的增益音源模式（none 为不应用；track 与 album 使用对应存储的增益值；auto 跟随播放顺序，随机模式使用音轨增益，其余顺序使用专辑增益）
 */
type SourceMode = 'none' | 'track' | 'album' | 'auto';

/**
 * A source mode to set; `byPlaybackOrder` is the older spelling of `auto`.
 * @zh 待设置的音源模式输入值（byPlaybackOrder 为 auto 的向后兼容旧别名）
 */
type SourceModeInput = 'none' | 'track' | 'album' | 'auto' | 'byPlaybackOrder';

/**
 * What is applied: `gain` the gain only, `peak` peak limiting only, `gain_and_peak` both, `none`
 * nothing.
 * @zh ReplayGain 处理模式（gain 仅应用增益调节；peak 仅执行峰值防削波限制；gain_and_peak 两者兼具；none 不进行任何处理）
 */
type ProcessingMode = 'none' | 'gain' | 'gain_and_peak' | 'peak';

interface GetSettingsResult {
  sourceMode: SourceMode;
  processingMode: ProcessingMode;
  /**
   * Preamp applied to tracks that carry ReplayGain, in dB.
   * @zh 对带有 ReplayGain 标签的曲目应用的预增益数值（单位 dB）
   */
  preampWithRg: number;
  /**
   * Preamp applied to tracks without ReplayGain, in dB.
   * @zh 对缺少 ReplayGain 标签的曲目应用的预增益数值（单位 dB）
   */
  preampWithoutRg: number;
  /**
   * Whether ReplayGain changes the output: `false` when the source mode is `none` or `auto`, or
   * the processing mode is `none`.
   * @zh ReplayGain 是否正在实际调节音频输出（音源模式为 none、auto 或处理模式为 none 时为 false）
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
   * @zh 本次设置调用是否实际修改了模式配置
   */
  changed: boolean;
}

interface GetPreampResult {
  /**
   * Preamp applied to tracks that carry ReplayGain, in dB.
   * @zh 对带有 ReplayGain 标签的曲目应用的预增益数值（单位 dB）
   */
  withRg: number;
  /**
   * Preamp applied to tracks without ReplayGain, in dB.
   * @zh 对缺少 ReplayGain 标签的曲目应用的预增益数值（单位 dB）
   */
  withoutRg: number;
}

interface SetPreampParams {
  /**
   * Preamp for tracks that carry ReplayGain, in dB.
   * @zh 带 ReplayGain 标签曲目的预增益目标值（单位 dB）
   * @minimum -24
   * @maximum 24
   */
  withRg?: number;
  /**
   * Preamp for tracks without ReplayGain, in dB.
   * @zh 不带 ReplayGain 标签曲目的预增益目标值（单位 dB）
   * @minimum -24
   * @maximum 24
   */
  withoutRg?: number;
}

interface SetPreampResult {
  /**
   * Preamp applied to tracks that carry ReplayGain, in dB.
   * @zh 修改后对带 ReplayGain 标签曲目生效的预增益数值（单位 dB）
   */
  withRg: number;
  /**
   * Preamp applied to tracks without ReplayGain, in dB.
   * @zh 修改后对不带 ReplayGain 标签曲目生效的预增益数值（单位 dB）
   */
  withoutRg: number;
  /**
   * Whether the call changed anything.
   * @zh 本次设置调用是否实际修改了预增益数值
   */
  changed: boolean;
}

interface GetParams {
  /**
   * Tracks to read. A `|subsong:N` suffix selects a subsong, such as one track of a CUE sheet;
   * a path without it reads the first track.
   * @zh 待读取的曲目路径列表（支持 |subsong:N 后缀指定单曲音轨，如 CUE 分轨；缺省后缀时默认读取首个音轨）
   * @security MediaRead
   */
  paths: string[];
}

/** The ReplayGain values of one file. Each value appears only when the file stores it. */
interface ReplayGainTrackInfo {
  /**
   * The path as requested, including any `|subsong:N` suffix.
   * @zh 原始请求传入的曲目路径（保留 |subsong:N 后缀）
   */
  path: string;
  /**
   * Whether the file could be read.
   * @zh 该曲目元数据是否成功读取
   */
  success: boolean;
  /**
   * Why the file could not be read; present when `success` is `false`.
   * @zh 元数据读取失败的具体原因描述（仅在 success 为 false 时提供）
   */
  error?: string;
  /**
   * Whether the file stores a track gain or an album gain. Present when `success` is `true`.
   * @zh 文件中是否记录了音轨增益或专辑增益数据（仅在 success 为 true 时提供）
   */
  hasReplayGain?: boolean;
  /**
   * Track gain formatted with two decimals and a `dB` suffix, such as `-7.25 dB`.
   * @zh 格式化音轨增益字符串（保留两位小数并附带 dB 后缀，例如 -7.25 dB）
   */
  trackGain?: string;
  /**
   * Track gain in dB.
   * @zh 原始数值形式的音轨增益（单位 dB）
   */
  trackGainRaw?: number;
  /**
   * Track peak formatted with six decimals.
   * @zh 格式化音轨峰值字符串（保留六位小数）
   */
  trackPeak?: string;
  /**
   * Track peak as a linear amplitude.
   * @zh 原始线性幅度形式的音轨峰值
   */
  trackPeakRaw?: number;
  /**
   * Album gain formatted with two decimals and a `dB` suffix.
   * @zh 格式化专辑增益字符串（保留两位小数并附带 dB 后缀）
   */
  albumGain?: string;
  /**
   * Album gain in dB.
   * @zh 原始数值形式的专辑增益（单位 dB）
   */
  albumGainRaw?: number;
  /**
   * Album peak formatted with six decimals.
   * @zh 格式化专辑峰值字符串（保留六位小数）
   */
  albumPeak?: string;
  /**
   * Album peak as a linear amplitude.
   * @zh 原始线性幅度形式的专辑峰值
   */
  albumPeakRaw?: number;
}

interface GetResult {
  /**
   * Number of rows in `results`.
   * @zh results 数组包含的结果记录总数
   */
  count: Int;
  /**
   * One row per requested path, in request order.
   * @zh 逐文件查询结果列表（严格保持请求传入顺序）
   */
  results: ReplayGainTrackInfo[];
}

interface ClearParams {
  /**
   * Tracks whose ReplayGain values are removed. A `|subsong:N` suffix selects a subsong, such as
   * one track of a CUE sheet; a path without it means the first track.
   * @zh 待清除 ReplayGain 信息的曲目路径列表（支持 |subsong:N 后缀指定子曲目；缺省后缀时指首个音轨）
   * @minItems 1
   * @security MediaWrite
   */
  paths: string[];
}

interface ClearResult {
  /**
   * Number of files handed to the tag writer.
   * @zh 成功提交至标签写入器的文件总数
   */
  clearedCount: Int;
}

interface ScanParams {
  /**
   * Tracks to scan. A `|subsong:N` suffix selects a subsong, such as one track of a CUE sheet;
   * a path without it means the first track.
   * @zh 待扫描 ReplayGain 的曲目路径列表（支持 |subsong:N 后缀指定子曲目；缺省后缀时指首个音轨）
   * @minItems 1
   * @security MediaRead
   */
  paths: string[];
  /**
   * `track` scans each file's own gain; `album` scans the files as one album.
   * @zh 扫描模式（track 为逐文件扫描独立音轨增益；album 为将传入文件整体作为单张专辑扫描）
   * @default "track"
   */
  mode?: 'track' | 'album';
}

interface ScanResult {
  /**
   * Number of files handed to the scanner.
   * @zh 成功提交至扫描队列的文件总数
   */
  scannedCount: Int;
  /**
   * The mode the scan runs in.
   * @zh 实际执行扫描的模式（track 或 album）
   */
  mode: 'track' | 'album';
  /**
   * A fixed English note saying the scan runs in the background.
   * @zh 固定的英文提示文本（指出扫描任务正在后台异步运行）
   */
  note: string;
}

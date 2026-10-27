import type { Int, Json } from './common.js';

export interface Api {
  /**
   * Store a value under a key in this component's own key-value store, replacing any value the
   * key held. The store is kept in foobar2000's configuration store, so it survives a restart.
   * Fails with `OPERATION_FAILED` when the store cannot be read or the value cannot be saved.
   * @zh 将指定值存储至本组件专属的持久化键值存储中，覆盖该键原有数据；配置保存在 foobar2000 宿主配置区中，软件重启后依然保留；若存储读取失败或值无法保存则以 `OPERATION_FAILED` 失败
   */
  set(params: SetParams): SetResult;

  /**
   * Read the value stored under a key. A key that is not there is not an error: `found` is
   * `false` and `value` is `default`, or `null` when no default was given.
   * @zh 读取指定键下存储的值；键不存在时不报错，返回 `found: false`，且 `value` 取 `default` 传入值（未提供时为 `null`）
   */
  get(params: GetParams): GetResult;

  /**
   * Delete a key from the store. Deleting a key that is not there succeeds with `existed: false`.
   * @zh 从键值存储中删除指定的键；删除不存在的键仍视为成功，返回 `existed: false`
   */
  remove(params: RemoveParams): RemoveResult;

  /**
   * Read the whole store: every key with its value.
   * @zh 获取持久化存储中的全部键值对数据
   */
  getAll(): GetAllResult;

  /**
   * Read the whole store both as a map and as one JSON text.
   * @zh 导出持久化存储中的全部配置数据，同时提供对象映射与序列化 JSON 文本两种形式
   */
  export(): ExportResult;

  /**
   * List the devices of every output module and flag the one in effect. These are the devices
   * `output.getDevices` lists, in a flat shape that pairs each with the GUID of its module.
   * @zh 列出所有输出模块下的音频设备并标记当前生效设备；数据形态为扁平列表，每个设备项均关联其所属模块的 GUID
   */
  getOutputDevices(): GetOutputDevicesResult;

  /**
   * Report the output settings in effect: module, device, buffer length, bit depth, dithering and
   * fades.
   * @zh 获取当前生效的音频输出配置，包括所属模块、设备 GUID、缓冲长度、输出位深、抖动与淡入淡出设置
   */
  getOutputConfig(): GetOutputConfigResult;

  /**
   * Switch output to a device of an output module; take both GUIDs from
   * `config.getOutputDevices`.
   * @zh 将音频输出切换至指定输出模块下的目标设备；模块与设备 GUID 均通过 `config.getOutputDevices` 获取
   */
  setOutputDevice(params: SetOutputDeviceParams): void;

  /**
   * Set the output buffer length, in seconds (`bufferLength`) or in milliseconds
   * (`milliseconds`). One of the two is required; `milliseconds` wins when both are given, and
   * `bufferLength` is range-checked even then.
   * @zh 设置音频输出缓冲长度；支持传入以秒为单位的 `bufferLength` 或以毫秒为单位的 `milliseconds`（两者至少传其一，均提供时以 `milliseconds` 为准且 `bufferLength` 仍受范围检查）
   */
  setOutputBuffer(params: SetOutputBufferParams): void;

  /**
   * List foobar2000's Advanced preferences as a tree, from the root or from below `parentGuid`.
   * @zh 获取 foobar2000 高级首选项树；可从根节点遍历或从指定的 `parentGuid` 分支节点展开
   */
  getAdvancedConfig(params: GetAdvancedConfigParams): GetAdvancedConfigResult;

  /**
   * Read one Advanced preferences entry by GUID. An integer entry's value comes back as a number
   * here, while `config.getAdvancedConfig` lists it as its decimal text.
   * @zh 根据 GUID 读取单个高级首选项条目；整数类型条目在此方法中直接以数字格式返回，而非十进制文本
   */
  getAdvancedConfigValue(params: GetAdvancedConfigValueParams): GetAdvancedConfigValueResult;

  /**
   * Write one Advanced preferences entry. The value has to suit the entry: a boolean for a
   * `checkbox` or `radio` entry; a string or a number for an `integer` or `string` entry, where a
   * number is stored as its decimal text (an `integer` entry drops the fraction, a `string` entry
   * keeps six decimals). A `branch` or an entry of another kind cannot be written.
   * @zh 写入单个高级首选项条目的值；传入的值须与条目类型严格匹配：`checkbox` 与 `radio` 需传入布尔值，`integer` 与 `string` 需传入字符串或数字（数值会以十进制文本保存，`integer` 舍去小数部分，`string` 保留六位小数）；分支节点（`branch`）及其他未知类别条目不可写入
   */
  setAdvancedConfigValue(params: SetAdvancedConfigValueParams): void;

  /**
   * Return one Advanced preferences entry to its default value.
   * @zh 将指定 GUID 的高级首选项条目重置恢复为其默认值
   */
  resetAdvancedConfig(params: ResetAdvancedConfigParams): void;

  /**
   * List every registered preferences page and preferences branch: the pages first, then the
   * branches.
   * @zh 列出所有已注册的首选项页面与分支节点，先按顺序排列常规页面，随后排列分支节点
   */
  getPreferencesPages(): GetPreferencesPagesResult;

  /**
   * Report the GUIDs of foobar2000's standard preferences parents, which registered pages and
   * branches name as their parent.
   * @zh 获取 foobar2000 内置标准首选项父节点的 GUID 集合；扩展页面与分支以此作为挂载父级
   */
  getPreferencesStandardGuids(): GetPreferencesStandardGuidsResult;

  /**
   * List the installed components with their versions.
   * @zh 获取当前已安装的 foobar2000 原生组件列表及其版本与模块文件名
   */
  getComponents(): GetComponentsResult;

  /**
   * Report the foobar2000 version, this component's version and the profile folder.
   * @zh 获取宿主运行环境版本信息，包括 foobar2000 核心版本、本组件版本、系统架构及配置目录绝对路径
   */
  getVersionInfo(): GetVersionInfoResult;

  /**
   * Report whether the media library is enabled and loaded, and how many items it holds. The
   * count walks the whole library on every call.
   * @zh 获取媒体库状态，包括是否启用、初始化是否就绪以及曲目总数；条目总数每次调用时重新遍历统计
   */
  getLibraryStatus(): GetLibraryStatusResult;

  /**
   * Report where foobar2000 puts newly encoded, copied or moved tracks and album images. A pattern
   * that is not configured is absent, so with neither configured the response is
   * `{ success: true }` alone.
   * @zh 获取媒体库在新编码、复制或移动操作时归档曲目与专辑封面的目标路径规则；未配置的规则不会出现在结果中，两者均未配置时仅返回成功信封
   */
  getLibraryFilePatterns(): GetLibraryFilePatternsResult;

  /**
   * Open foobar2000's Media Library preferences page.
   * @zh 弹出并定位到 foobar2000 的媒体库（Media Library）首选项设置窗口
   */
  showLibraryPreferences(): void;

  /**
   * List the DSP presets stored in the profile. A host without DSP preset support reports an
   * empty list.
   * @zh 获取用户配置目录中已保存的 DSP 预设列表；宿主不支持 DSP 预设时返回空列表
   */
  getDspPresets(): GetDspPresetsResult;

  /**
   * Report which DSP preset is selected. `index` and `name` are `null` and `isActive` is `false`
   * when none is selected or presets are unavailable.
   * @zh 获取当前生效选中的 DSP 预设；未选中预设或预设功能不可用时，`index` 与 `name` 返回 `null` 且 `isActive` 为 `false`
   */
  getActiveDspPreset(): GetActiveDspPresetResult;

  /**
   * Select a DSP preset by index, which replaces the whole active DSP chain with the preset's;
   * the same operation as `dsp.applyPreset` by index. Nothing selects "no preset" again.
   * @zh 根据索引激活指定的 DSP 预设并整体替换当前活动 DSP 链（效果等同于调用 `dsp.applyPreset` 传入索引）；激活后无法通过此接口恢复为未选中状态
   */
  setActiveDspPreset(params: SetActiveDspPresetParams): void;

  /**
   * Report foobar2000's "cursor follows playback" setting.
   * @zh 查询 foobar2000 的「光标跟随播放（Cursor Follows Playback）」开关状态
   */
  getCursorFollowPlayback(): GetCursorFollowPlaybackResult;

  /**
   * Turn foobar2000's "cursor follows playback" setting on or off.
   * @zh 设置 foobar2000 的「光标跟随播放（Cursor Follows Playback）」启用状态
   */
  setCursorFollowPlayback(params: SetCursorFollowPlaybackParams): SetCursorFollowPlaybackResult;

  /**
   * Report foobar2000's "playback follows cursor" setting.
   * @zh 查询 foobar2000 的「播放跟随光标（Playback Follows Cursor）」开关状态
   */
  getPlaybackFollowCursor(): GetPlaybackFollowCursorResult;

  /**
   * Turn foobar2000's "playback follows cursor" setting on or off.
   * @zh 设置 foobar2000 的「播放跟随光标（Playback Follows Cursor）」启用状态
   */
  setPlaybackFollowCursor(params: SetPlaybackFollowCursorParams): SetPlaybackFollowCursorResult;

  /**
   * Report the ReplayGain source mode as a number. `replaygain.getMode` reports the same setting
   * by name.
   * @zh 以数值编号形式获取当前 ReplayGain 音源模式（`replaygain.getMode` 提供同项设置的字符串名称）
   */
  getReplaygainMode(): GetReplaygainModeResult;

  /**
   * Set the ReplayGain source mode by number (`mode`) or by name (`sourceMode`). One of the two is
   * required; `mode` wins when both are given. `replaygain.setMode` changes the same setting by
   * name.
   * @zh 设置 ReplayGain 音源模式；支持传入数值编号 `mode` 或字符串名称 `sourceMode`（两者至少传其一，均提供时以 `mode` 优先，亦可通过 `replaygain.setMode` 修改）
   */
  setReplaygainMode(params: SetReplaygainModeParams): SetReplaygainModeResult;
}

/**
 * Kind of an Advanced preferences entry: `branch` holds other entries, `checkbox` and `radio`
 * hold a boolean, `integer` and `string` hold text, `unknown` is any other kind.
 * @zh 高级首选项条目类别：`branch` 容纳子条目，`checkbox` 与 `radio` 存储布尔值，`integer` 与 `string` 存储文本，`unknown` 为其他类型
 */
type AdvancedConfigType = 'branch' | 'checkbox' | 'radio' | 'integer' | 'string' | 'unknown';

interface SetParams {
  /**
   * Key to store under. Every page of this component shares one store, so give your keys a
   * prefix of your own.
   * @zh 存储键名；本组件所有页面共享同一存储空间，建议添加主题特有前缀
   * @minLength 1
   */
  key: string;
  /**
   * Value to store, any JSON value. A top-level `null` counts as missing and is refused, so clear
   * a key with `config.remove`; a `null` inside an object or an array is kept.
   * @zh 待存储的值，支持任意可序列化的 JSON 数据；顶层 `null` 会被视作无效键值拒绝，清除键请调用 `config.remove`，嵌套对象或数组内的 `null` 照常保留
   */
  value: Json;
}

interface SetResult {
  /**
   * The key written.
   * @zh 成功写入的存储键名
   */
  key: string;
}

interface GetParams {
  /**
   * Key to read.
   * @zh 待读取的存储键名
   * @minLength 1
   */
  key: string;
  /**
   * Value to answer with when the key is absent.
   * @zh 目标键不存在时返回的默认兜底值
   */
  default?: Json;
}

interface GetResult {
  /**
   * The key asked for.
   * @zh 请求读取的存储键名
   */
  key: string;
  /**
   * The stored value; when the key is absent, `default` if it was given, otherwise `null`.
   * @zh 存储的值；键不存在时返回 `default` 传入值（未提供时返回 `null`）
   */
  value: Json;
  /**
   * Whether the key is in the store.
   * @zh 目标键是否存在于存储中
   */
  found: boolean;
}

interface RemoveParams {
  /**
   * Key to delete.
   * @zh 待删除的存储键名
   * @minLength 1
   */
  key: string;
}

interface RemoveResult {
  /**
   * The key asked for.
   * @zh 请求删除的存储键名
   */
  key: string;
  /**
   * Whether the key was in the store before the call.
   * @zh 本次删除操作前该键是否存在于存储中
   */
  existed: boolean;
}

interface GetAllResult {
  /**
   * Every stored key with its value.
   * @zh 包含所有存储键值对的字典对象
   */
  items: Record<string, Json>;
  /**
   * The same map as `items`, under its older name.
   * @zh 与 `items` 完全一致的旧名兼容字典
   */
  configs: Record<string, Json>;
  /**
   * Number of keys in the store.
   * @zh 存储中包含的键值对总数
   */
  count: Int;
}

interface ExportResult {
  /**
   * Every stored key with its value; the map `config.getAll` reports.
   * @zh 包含所有存储键值对的字典对象，同 `config.getAll` 的 `items`
   */
  data: Record<string, Json>;
  /**
   * `data` serialized as one compact JSON text.
   * @zh 将 `data` 序列化后的紧凑 JSON 字符串
   */
  json: string;
  /**
   * Number of keys in the store.
   * @zh 存储中包含的键值对总数
   */
  count: Int;
}

/**
 * One output device as `config.getOutputDevices` lists it.
 * @zh `config.getOutputDevices` 返回的单个输出设备条目
 */
interface ConfigOutputDevice {
  /**
   * Display name: the full name the output manager reports, or `<module>: <device>` when the
   * device list comes from the output modules themselves.
   * @zh 设备显示名称；输出管理器提供时为完整名称，若直接源自输出模块则为 `<模块名>: <设备名>`
   */
  name: string;
  /**
   * The same as `deviceId`.
   * @zh 设备标识符，与 `deviceId` 相同
   */
  id: string;
  /**
   * GUID of the output module, rendered as `{...}`.
   * @zh 输出模块 GUID，格式形如 `{...}`
   */
  outputId: string;
  /**
   * Device GUID rendered as `{...}`. Not unique on its own: a module reports its default device
   * with the all-zero GUID, so key devices by `(outputId, deviceId)`.
   * @zh 设备 GUID，格式形如 `{...}`；单独并不保证全局唯一（模块默认设备全为零 GUID），需以 `(outputId, deviceId)` 二元组唯一标识
   */
  deviceId: string;
  /**
   * Whether this module and device are the ones in effect, as `config.getOutputConfig` reports
   * them.
   * @zh 该设备是否为当前系统生效的活动输出设备
   */
  isCurrent: boolean;
}

interface GetOutputDevicesResult {
  /**
   * Devices of all output modules.
   * @zh 所有已安装输出模块下的设备列表
   */
  devices: ConfigOutputDevice[];
  /**
   * Number of entries in `devices`.
   * @zh `devices` 列表包含的设备总数
   */
  count: Int;
}

interface GetOutputConfigResult {
  /**
   * GUID of the output module in effect, rendered as `{...}`.
   * @zh 当前生效的输出模块 GUID，格式形如 `{...}`
   */
  outputId: string;
  /**
   * GUID of the device in effect, rendered as `{...}`.
   * @zh 当前生效的设备 GUID，格式形如 `{...}`
   */
  deviceId: string;
  /**
   * Output buffer length in seconds.
   * @zh 当前配置的输出缓冲长度，单位为秒
   */
  bufferLength: number;
  /**
   * Output bit depth setting, in bits.
   * @zh 当前配置的输出位深，单位为比特（bit）
   */
  bitDepth: Int;
  /**
   * Whether dithering is on.
   * @zh 是否启用了音频抖动处理
   */
  useDither: boolean;
  /**
   * Whether fades are on.
   * @zh 是否启用了淡入淡出效果
   */
  useFades: boolean;
  /**
   * Display name of the output module; absent when no installed module has that GUID.
   * @zh 当前生效输出模块的显示名称；未匹配到对应模块时不返回
   */
  outputName?: string;
  /**
   * Display name of the device; absent when the module does not name it.
   * @zh 当前生效输出设备的显示名称；模块未提供时不返回
   */
  deviceName?: string;
}

interface SetOutputDeviceParams {
  /**
   * GUID of the output module, as `config.getOutputDevices` reports it in `outputId`.
   * @zh 目标输出模块 GUID，取自 `config.getOutputDevices` 返回的 `outputId`
   * @minLength 1
   */
  outputId: string;
  /**
   * GUID of the device, as `config.getOutputDevices` reports it in `deviceId`.
   * @zh 目标设备 GUID，取自 `config.getOutputDevices` 返回的 `deviceId`
   * @minLength 1
   */
  deviceId: string;
}

interface SetOutputBufferParams {
  /**
   * Buffer length in seconds. Ignored when `milliseconds` is given, but range-checked all the
   * same.
   * @zh 缓冲长度，单位为秒；若同时传入 `milliseconds` 则以此为辅助校验范围，以毫秒值为准
   * @minimum 0.05
   * @maximum 2
   */
  bufferLength?: number;
  /**
   * Buffer length in milliseconds; takes precedence over `bufferLength`.
   * @zh 缓冲长度，单位为毫秒；优先级高于 `bufferLength`
   * @minimum 50
   * @maximum 2000
   */
  milliseconds?: number;
}

/**
 * One entry of foobar2000's Advanced preferences tree. Every entry carries `type`, and which of
 * the other optional keys it carries follows from it: a `branch` has `children`, a `checkbox` or
 * `radio` entry a boolean `value`, an `integer` or `string` entry a text `value` and the three
 * `is*` flags.
 * @zh foobar2000 高级首选项树节点条目；每项必含 `type`，其余字段根据类型提供：`branch` 包含 `children`，`checkbox`/`radio` 包含布尔 `value`，`integer`/`string` 包含文本 `value` 及三个 `is*` 标记
 */
interface AdvancedConfigItem {
  /**
   * Display name.
   * @zh 节点条目的显示名称
   */
  name: string;
  /**
   * Entry GUID rendered as `{...}`, as `config.getAdvancedConfigValue`,
   * `config.setAdvancedConfigValue` and `config.resetAdvancedConfig` take it.
   * @zh 节点 GUID，格式形如 `{...}`，供读取、修改与重置接口传参使用
   */
  guid: string;
  /**
   * Sort priority the entry reports.
   * @zh 条目声明的排序优先级
   */
  sortPriority: number;
  type?: AdvancedConfigType;
  /**
   * Current value: a boolean on `checkbox` and `radio`; the stored text on `integer` and `string`,
   * so an integer entry reads as its decimal text here while `config.getAdvancedConfigValue`
   * reports a number; absent on `branch` and `unknown`.
   * @zh 条目当前值；`checkbox`/`radio` 为布尔值，`integer`/`string` 为存储的原始文本（整数条目在此为十进制文本格式，而 `config.getAdvancedConfigValue` 返回数字），`branch` 与 `unknown` 省略此字段
   */
  value?: Json;
  /**
   * Default value in the same form as `value`; present only when the entry reports one.
   * @zh 条目的默认值，数据类型与 `value` 相同；仅在条目提供默认值时出现
   */
  defaultValue?: Json;
  /**
   * Whether the entry is flagged as a signed integer; present on `integer` and `string`.
   * @zh 是否标记为有符号整数；仅在 `integer` 与 `string` 类型条目上提供
   */
  isSigned?: boolean;
  /**
   * Whether the entry is flagged as holding a file path; present on `integer` and `string`.
   * @zh 是否标记为文件路径属性；仅在 `integer` 与 `string` 类型条目上提供
   */
  isFilePath?: boolean;
  /**
   * Whether the entry is flagged as holding a folder path; present on `integer` and `string`.
   * @zh 是否标记为文件夹路径属性；仅在 `integer` 与 `string` 类型条目上提供
   */
  isFolderPath?: boolean;
  /**
   * The entries inside a branch; present on `branch` only. The tree stops eleven levels below
   * the requested parent: a branch on that level is listed with an empty `children`.
   * @zh 分支节点下包含的子条目列表；仅在 `branch` 类型出现，超过 11 层的深层分支子列表置空截断
   */
  children?: AdvancedConfigItem[];
}

interface GetAdvancedConfigParams {
  /**
   * GUID of the branch to list, rendered as `{...}`; the root of the tree when omitted or empty.
   * A well-formed GUID that names no branch lists nothing.
   * @zh 待遍历展开的分支节点 GUID，格式形如 `{...}`；缺省或为空串时从根节点开始展开，格式合法但非分支的 GUID 返回空列表
   */
  parentGuid?: string;
}

interface GetAdvancedConfigResult {
  /**
   * Entries directly below the requested branch; each branch carries its own entries in
   * `children`.
   * @zh 目标分支下一层直接包含的条目列表；子分支项在各自的 `children` 中嵌套包含
   */
  entries: AdvancedConfigItem[];
  /**
   * Number of entries in `entries`; nested entries are not counted.
   * @zh `entries` 顶层包含的条目总数（不计深层嵌套项）
   */
  count: Int;
}

interface GetAdvancedConfigValueParams {
  /**
   * Entry GUID rendered as `{...}`.
   * @zh 待查询条目的 GUID，格式形如 `{...}`
   * @minLength 1
   */
  guid: string;
}

interface GetAdvancedConfigValueResult {
  /**
   * Display name of the entry.
   * @zh 目标条目的显示名称
   */
  name: string;
  /**
   * The GUID as it was sent; its letter case is not normalized.
   * @zh 请求中传入的 GUID 原样回显，未做大小写归一化
   */
  guid: string;
  type: AdvancedConfigType;
  /**
   * Current value: a boolean on `checkbox` and `radio`, a number on `integer` (`0` when the
   * stored text does not start with a number), a string on `string`, `null` on `branch` and
   * `unknown`.
   * @zh 目标条目当前值；`checkbox`/`radio` 为布尔值，`integer` 为数字（文本不以数字开头时为 `0`），`string` 为字符串，`branch`/`unknown` 为 `null`
   */
  value: Json;
}

interface SetAdvancedConfigValueParams {
  /**
   * Entry GUID rendered as `{...}`.
   * @zh 待写入条目的 GUID，格式形如 `{...}`
   * @minLength 1
   */
  guid: string;
  /**
   * Value to write; what each kind of entry accepts is in the method description.
   * @zh 待写入的新值；各条目类型可接受的数据格式详见方法说明
   */
  value: Json;
}

interface ResetAdvancedConfigParams {
  /**
   * Entry GUID rendered as `{...}`.
   * @zh 待重置条目的 GUID，格式形如 `{...}`
   * @minLength 1
   */
  guid: string;
}

/**
 * One preferences page or preferences branch as `config.getPreferencesPages` lists it.
 * @zh `config.getPreferencesPages` 列出的首选项页面或分支条目
 */
interface ConfigPreferencesPage {
  /**
   * Display name.
   * @zh 首选项页面或分支的显示名称
   */
  name: string;
  /**
   * GUID of the page or branch, rendered as `{...}`.
   * @zh 页面或分支自身的 GUID，格式形如 `{...}`
   */
  guid: string;
  /**
   * GUID of the page or branch it sits under, rendered as `{...}`; a standard parent from
   * `config.getPreferencesStandardGuids` for a top-level page.
   * @zh 其上级页面或分支的 GUID，格式形如 `{...}`；顶层页面指向标准预设父节点之一
   */
  parentGuid: string;
  /**
   * Sort priority it reports: lower sorts first and `0` sorts by name; `0` as well when it
   * reports none.
   * @zh 页面报告的排序优先级，数值越小越靠前，`0` 按名称字母排序；未声明时默认为 `0`
   */
  sortPriority: number;
  /**
   * `true` on a branch; absent on a page.
   * @zh 是否为分支节点；为 `true` 时为分支，页面项省略此字段
   */
  isBranch?: boolean;
}

interface GetPreferencesPagesResult {
  /**
   * Every page, then every branch.
   * @zh 所有首选项页面与分支列表，页面在前，分支在后
   */
  pages: ConfigPreferencesPage[];
  /**
   * Number of entries in `pages`.
   * @zh `pages` 包含的条目总数
   */
  count: Int;
}

interface GetPreferencesStandardGuidsResult {
  /**
   * `preferences_page::guid_root`, rendered as `{...}`: the root of the preferences tree.
   * @zh `preferences_page::guid_root` GUID，代表首选项根节点
   */
  root: string;
  /**
   * `preferences_page::guid_hidden`, rendered as `{...}`.
   * @zh `preferences_page::guid_hidden` GUID，代表隐藏首选项节点
   */
  hidden: string;
  /**
   * `preferences_page::guid_tools`, rendered as `{...}`.
   * @zh `preferences_page::guid_tools` GUID，代表工具类首选项父节点
   */
  tools: string;
  /**
   * `preferences_page::guid_core`, rendered as `{...}`.
   * @zh `preferences_page::guid_core` GUID，代表核心类首选项父节点
   */
  core: string;
  /**
   * `preferences_page::guid_display`, rendered as `{...}`.
   * @zh `preferences_page::guid_display` GUID，代表界面与显示类首选项父节点
   */
  display: string;
  /**
   * `preferences_page::guid_playback`, rendered as `{...}`.
   * @zh `preferences_page::guid_playback` GUID，代表播放类首选项父节点
   */
  playback: string;
  /**
   * `preferences_page::guid_visualisations`, rendered as `{...}`.
   * @zh `preferences_page::guid_visualisations` GUID，代表可视化类首选项父节点
   */
  visualisations: string;
  /**
   * `preferences_page::guid_input`, rendered as `{...}`.
   * @zh `preferences_page::guid_input` GUID，代表解码与输入类首选项父节点
   */
  input: string;
  /**
   * `preferences_page::guid_tag_writing`, rendered as `{...}`.
   * @zh `preferences_page::guid_tag_writing` GUID，代表标签写入类首选项父节点
   */
  tagWriting: string;
  /**
   * `preferences_page::guid_media_library`, rendered as `{...}`.
   * @zh `preferences_page::guid_media_library` GUID，代表媒体库类首选项父节点
   */
  mediaLibrary: string;
  /**
   * `preferences_page::guid_tagging`, rendered as `{...}`.
   * @zh `preferences_page::guid_tagging` GUID，代表标签处理类首选项父节点
   */
  tagging: string;
  /**
   * `preferences_page::guid_output`, rendered as `{...}`.
   * @zh `preferences_page::guid_output` GUID，代表音频输出类首选项父节点
   */
  output: string;
  /**
   * `preferences_page::guid_advanced`, rendered as `{...}`.
   * @zh `preferences_page::guid_advanced` GUID，代表高级首选项父节点
   */
  advanced: string;
  /**
   * `preferences_page::guid_components`, rendered as `{...}`.
   * @zh `preferences_page::guid_components` GUID，代表组件管理类首选项父节点
   */
  components: string;
  /**
   * `preferences_page::guid_dsp`, rendered as `{...}`.
   * @zh `preferences_page::guid_dsp` GUID，代表 DSP 管理类首选项父节点
   */
  dsp: string;
  /**
   * `preferences_page::guid_shell`, rendered as `{...}`.
   * @zh `preferences_page::guid_shell` GUID，代表系统外壳类首选项父节点
   */
  shell: string;
  /**
   * `preferences_page::guid_keyboard_shortcuts`, rendered as `{...}`.
   * @zh `preferences_page::guid_keyboard_shortcuts` GUID，代表快捷键设置类首选项父节点
   */
  keyboardShortcuts: string;
}

/**
 * One installed component as `config.getComponents` lists it.
 * @zh `config.getComponents` 返回的已安装原生组件信息条目
 */
interface ConfigComponentInfo {
  /**
   * Name the component reports.
   * @zh 组件自身报告的显示名称
   */
  name: string;
  /**
   * Version string the component reports.
   * @zh 组件自身报告的版本号字符串
   */
  version: string;
  /**
   * File name the component reports for its module; absent when it reports none.
   * @zh 组件对应的 DLL 模块文件名；组件未声明时不返回
   */
  filename?: string;
  /**
   * The same as `filename`.
   * @zh 与 `filename` 完全一致的兼容别名字段
   */
  fileName?: string;
}

interface GetComponentsResult {
  /**
   * Every installed component, in enumeration order.
   * @zh 按枚举顺序排列的所有已安装组件信息列表
   */
  components: ConfigComponentInfo[];
  /**
   * Number of entries in `components`.
   * @zh `components` 列表包含的组件总数
   */
  count: Int;
}

interface GetVersionInfoResult {
  /**
   * foobar2000's version string as the core reports it; it includes the product name.
   * @zh 核心报告的 foobar2000 版本字符串，包含产品名称
   */
  version: string;
  /**
   * The same as `version`.
   * @zh 与 `version` 完全一致的兼容别名字段
   */
  foobar2000: string;
  /**
   * What `core_version_info_v2::get_name()` reports, which the foobar2000 SDK documents as the
   * product name `foobar2000`; despite the key, not a longer form of `version`.
   * @zh SDK `core_version_info_v2::get_name()` 报告的产品名称（固定为 `"foobar2000"`，并非完整版本字符串）
   */
  versionFull: string;
  /**
   * Whether this is a 64-bit build.
   * @zh 是否为 64 位（x64）编译版本
   */
  is64bit: boolean;
  /**
   * Whether foobar2000 runs as a portable installation.
   * @zh foobar2000 当前是否以便携（Portable）模式运行
   */
  isPortable: boolean;
  /**
   * This component.
   * @zh 本 WebView2 UI 组件信息
   */
  plugin: {
    /**
     * Always `foo_ui_webview2`.
     * @zh 固定为 `"foo_ui_webview2"`
     */
    name: string;
    /**
     * This component's version, `major.minor.patch`.
     * @zh 本组件版本号，格式为 `主.次.修订`
     */
    version: string;
  };
  /**
   * The profile folder as a display path.
   * @zh 用户配置（Profile）目录的显示路径
   */
  profilePath: string;
}

interface GetLibraryStatusResult {
  /**
   * Whether the media library is enabled, which foobar2000 takes to mean that at least one
   * library folder is configured.
   * @zh 媒体库是否已启用（foobar2000 以至少配置了一个媒体库文件夹为准）
   */
  enabled: boolean;
  /**
   * Number of items in the media library, counted on every call.
   * @zh 媒体库当前包含的音频条目总数，每次调用时重新遍历统计
   */
  itemCount: Int;
  /**
   * Whether the media library has finished loading; `true` on a host that cannot tell.
   * @zh 媒体库是否已载入初始化完成；宿主无法获知时默认为 `true`
   */
  initialized: boolean;
}

/**
 * Where foobar2000 puts one kind of new file.
 * @zh foobar2000 媒体库归档特定新文件的路径规则
 */
interface ConfigLibraryFilePattern {
  /**
   * Target folder as configured.
   * @zh 配置的目标存储文件夹路径
   */
  directory: string;
  /**
   * Title formatting pattern for the subfolders and the file name below that folder.
   * @zh 用于生成子文件夹与文件名的标题格式化（Titleformat）模式串
   */
  format: string;
}

interface GetLibraryFilePatternsResult {
  /**
   * Pattern for newly encoded, copied or moved tracks; absent when not configured.
   * @zh 针对新编码、复制或移动的曲目归档命名规则；未配置时不返回
   */
  tracks?: ConfigLibraryFilePattern;
  /**
   * Pattern for newly encoded, copied or moved album images; absent when not configured.
   * @zh 针对新编码、复制或移动的专辑封面归档命名规则；未配置时不返回
   */
  images?: ConfigLibraryFilePattern;
}

/**
 * One stored DSP preset as `config.getDspPresets` lists it.
 * @zh `config.getDspPresets` 返回的单个 DSP 预设条目
 */
interface ConfigDspPreset {
  /**
   * Position in the preset list, as `config.setActiveDspPreset` takes it.
   * @zh 预设在列表中的位置索引，供 `config.setActiveDspPreset` 传参调用
   */
  index: Int;
  /**
   * Preset name.
   * @zh 预设配置名称
   */
  name: string;
}

interface GetDspPresetsResult {
  /**
   * Every stored preset, in list order.
   * @zh 所有已保存的 DSP 预设条目列表，保持列表排序
   */
  presets: ConfigDspPreset[];
  /**
   * Number of entries in `presets`.
   * @zh `presets` 列表包含的预设总数
   */
  count: Int;
}

interface GetActiveDspPresetResult {
  /**
   * Position of the selected preset; `null` when none is selected.
   * @zh 当前激活选中的 DSP 预设索引；未选中预设时为 `null`
   */
  index: Int | null;
  /**
   * Name of the selected preset; `null` when none is selected.
   * @zh 当前激活选中的 DSP 预设名称；未选中预设时为 `null`
   */
  name: string | null;
  /**
   * Whether a preset is selected.
   * @zh 是否有 DSP 预设处于激活生效状态
   */
  isActive: boolean;
}

interface SetActiveDspPresetParams {
  /**
   * Position of the preset in `config.getDspPresets`.
   * @zh 待激活的 DSP 预设在 `config.getDspPresets` 中的位置索引
   * @minimum 0
   */
  index: Int;
}

interface GetCursorFollowPlaybackResult {
  /**
   * Whether the setting is on.
   * @zh 「光标跟随播放」是否已开启
   */
  enabled: boolean;
  /**
   * The same as `enabled`.
   * @zh 与 `enabled` 完全一致的兼容别名字段
   */
  value: boolean;
}

interface SetCursorFollowPlaybackParams {
  /**
   * `true` turns the setting on, `false` turns it off.
   * @zh 是否开启「光标跟随播放」；`true` 为开启，`false` 为关闭
   */
  enabled: boolean;
}

interface SetCursorFollowPlaybackResult {
  /**
   * The value written.
   * @zh 成功设置后的「光标跟随播放」状态
   */
  enabled: boolean;
}

interface GetPlaybackFollowCursorResult {
  /**
   * Whether the setting is on.
   * @zh 「播放跟随光标」是否已开启
   */
  enabled: boolean;
  /**
   * The same as `enabled`.
   * @zh 与 `enabled` 完全一致的兼容别名字段
   */
  value: boolean;
}

interface SetPlaybackFollowCursorParams {
  /**
   * `true` turns the setting on, `false` turns it off.
   * @zh 是否开启「播放跟随光标」；`true` 为开启，`false` 为关闭
   */
  enabled: boolean;
}

interface SetPlaybackFollowCursorResult {
  /**
   * The value written.
   * @zh 成功设置后的「播放跟随光标」状态
   */
  enabled: boolean;
}

interface GetReplaygainModeResult {
  /**
   * Source mode: `0` none, `1` track, `2` album, `3` by playback order.
   * @zh 音源模式数值编号：`0` 为无，`1` 为音轨，`2` 为专辑，`3` 为按播放顺序
   */
  mode: Int;
  /**
   * The same as `mode`.
   * @zh 与 `mode` 完全一致的兼容别名字段
   */
  value: Int;
}

interface SetReplaygainModeParams {
  /**
   * Source mode by number, as `config.getReplaygainMode` reports it; takes precedence over
   * `sourceMode`.
   * @zh 以数值编号指定的音源模式（取值同 `config.getReplaygainMode`），优先级高于 `sourceMode`
   * @minimum 0
   * @maximum 3
   */
  mode?: Int;
  /**
   * Source mode by name: `none` is `0`, `track` `1`, `album` `2`, and `byPlaybackOrder` or its
   * other spelling `auto` is `3`. Read only when `mode` is absent.
   * @zh 以名称字符串指定的音源模式：`"none"` (0)、`"track"` (1)、`"album"` (2)、`"byPlaybackOrder"` / `"auto"` (3)；仅在未提供 `mode` 时生效
   */
  sourceMode?: 'none' | 'track' | 'album' | 'byPlaybackOrder' | 'auto';
}

interface SetReplaygainModeResult {
  /**
   * The source mode now in effect, as a number.
   * @zh 成功应用后的生效音源模式数值编号
   */
  mode: Int;
  /**
   * The same as `mode`.
   * @zh 与 `mode` 完全一致的兼容别名字段
   */
  value: Int;
}

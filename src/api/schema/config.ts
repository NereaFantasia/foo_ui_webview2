import type { Int, Json } from './common.js';

export interface Api {
  /**
   * Store a value under a key in this component's own key-value store, replacing any value the
   * key held. The store is kept in foobar2000's configuration store, so it survives a restart.
   * Fails with `OPERATION_FAILED` when the store cannot be read or the value cannot be saved.
   * @zh 把一个值存到本组件自己的键值存储里的某个键下，覆盖该键原有的值。存储放在 foobar2000 的配置存储里，重启后仍在。存储读不出或值保存不了时以 `OPERATION_FAILED` 失败。
   */
  set(params: SetParams): SetResult;

  /**
   * Read the value stored under a key. A key that is not there is not an error: `found` is
   * `false` and `value` is `default`, or `null` when no default was given.
   * @zh 读取某个键下存的值。键不存在不算错误：`found` 为 `false`，`value` 为 `default`，没给时为 `null`。
   */
  get(params: GetParams): GetResult;

  /**
   * Delete a key from the store. Deleting a key that is not there succeeds with `existed: false`.
   * @zh 从存储中删除一个键。删除不存在的键也成功，`existed` 为 `false`。
   */
  remove(params: RemoveParams): RemoveResult;

  /**
   * Read the whole store: every key with its value.
   * @zh 读取整个存储：每个键及其值。
   */
  getAll(): GetAllResult;

  /**
   * Read the whole store both as a map and as one JSON text.
   * @zh 读取整个存储，同时给出映射与一段 JSON 文本两种形式。
   */
  export(): ExportResult;

  /**
   * List the devices of every output module and flag the one in effect. These are the devices
   * `output.getDevices` lists, in a flat shape that pairs each with the GUID of its module.
   * @zh 列出所有输出模块的设备，并标出当前生效的那个。与 `output.getDevices` 列出的是同一批设备，这里是扁平形状，每项都带所属模块的 GUID。
   */
  getOutputDevices(): GetOutputDevicesResult;

  /**
   * Report the output settings in effect: module, device, buffer length, bit depth, dithering and
   * fades.
   * @zh 报告当前生效的输出设置：模块、设备、缓冲长度、位深、抖动与淡入淡出。
   */
  getOutputConfig(): GetOutputConfigResult;

  /**
   * Switch output to a device of an output module; take both GUIDs from
   * `config.getOutputDevices`.
   * @zh 把输出切换到某个输出模块的某个设备；两个 GUID 都取自 `config.getOutputDevices`。
   */
  setOutputDevice(params: SetOutputDeviceParams): void;

  /**
   * Set the output buffer length, in seconds (`bufferLength`) or in milliseconds
   * (`milliseconds`). One of the two is required; `milliseconds` wins when both are given, and
   * `bufferLength` is range-checked even then.
   * @zh 设置输出缓冲长度，单位秒（`bufferLength`）或毫秒（`milliseconds`）。两者至少给一个；都给时以 `milliseconds` 为准，此时 `bufferLength` 仍会做范围检查。
   */
  setOutputBuffer(params: SetOutputBufferParams): void;

  /**
   * List foobar2000's Advanced preferences as a tree, from the root or from below `parentGuid`.
   * @zh 以树的形式列出 foobar2000 的高级首选项，从根开始，或从 `parentGuid` 之下开始。
   */
  getAdvancedConfig(params: GetAdvancedConfigParams): GetAdvancedConfigResult;

  /**
   * Read one Advanced preferences entry by GUID. An integer entry's value comes back as a number
   * here, while `config.getAdvancedConfig` lists it as its decimal text.
   * @zh 按 GUID 读取一个高级首选项条目。整数条目的值在这里是数字，而 `config.getAdvancedConfig` 列出的是它的十进制文本。
   */
  getAdvancedConfigValue(params: GetAdvancedConfigValueParams): GetAdvancedConfigValueResult;

  /**
   * Write one Advanced preferences entry. The value has to suit the entry: a boolean for a
   * `checkbox` or `radio` entry; a string or a number for an `integer` or `string` entry, where a
   * number is stored as its decimal text (an `integer` entry drops the fraction, a `string` entry
   * keeps six decimals). A `branch` or an entry of another kind cannot be written.
   * @zh 写入一个高级首选项条目。值要与条目相符：`checkbox` 或 `radio` 条目要布尔值；`integer` 或 `string` 条目要字符串或数字，数字按十进制文本存储（`integer` 条目舍去小数部分，`string` 条目保留六位小数）。`branch` 与其他类别的条目不能写。
   */
  setAdvancedConfigValue(params: SetAdvancedConfigValueParams): void;

  /**
   * Return one Advanced preferences entry to its default value.
   * @zh 把一个高级首选项条目恢复为默认值。
   */
  resetAdvancedConfig(params: ResetAdvancedConfigParams): void;

  /**
   * List every registered preferences page and preferences branch: the pages first, then the
   * branches.
   * @zh 列出所有已注册的首选项页面与首选项分支：先列页面，再列分支。
   */
  getPreferencesPages(): GetPreferencesPagesResult;

  /**
   * Report the GUIDs of foobar2000's standard preferences parents, which registered pages and
   * branches name as their parent.
   * @zh 报告 foobar2000 标准首选项父节点的 GUID；已注册的页面与分支以它们作为父节点。
   */
  getPreferencesStandardGuids(): GetPreferencesStandardGuidsResult;

  /**
   * List the installed components with their versions.
   * @zh 列出已安装的组件及其版本。
   */
  getComponents(): GetComponentsResult;

  /**
   * Report the foobar2000 version, this component's version and the profile folder.
   * @zh 报告 foobar2000 的版本、本组件的版本与配置目录。
   */
  getVersionInfo(): GetVersionInfoResult;

  /**
   * Report whether the media library is enabled and loaded, and how many items it holds. The
   * count walks the whole library on every call.
   * @zh 报告媒体库是否启用、是否载入完成，以及其中有多少条目。计数在每次调用时遍历整个媒体库。
   */
  getLibraryStatus(): GetLibraryStatusResult;

  /**
   * Report where foobar2000 puts newly encoded, copied or moved tracks and album images. A pattern
   * that is not configured is absent, so with neither configured the response is
   * `{ success: true }` alone.
   * @zh 报告 foobar2000 把新编码、复制或移动的曲目与专辑图片放到哪里。没有配置的模式不出现，两者都没配置时响应只有 `{ success: true }`。
   */
  getLibraryFilePatterns(): GetLibraryFilePatternsResult;

  /**
   * Open foobar2000's Media Library preferences page.
   * @zh 打开 foobar2000 的媒体库首选项页面。
   */
  showLibraryPreferences(): void;

  /**
   * List the DSP presets stored in the profile. A host without DSP preset support reports an
   * empty list.
   * @zh 列出配置目录里保存的 DSP 预设。宿主不支持 DSP 预设时报告空列表。
   */
  getDspPresets(): GetDspPresetsResult;

  /**
   * Report which DSP preset is selected. `index` and `name` are `null` and `isActive` is `false`
   * when none is selected or presets are unavailable.
   * @zh 报告当前选中的 DSP 预设。没有选中或预设不可用时 `index` 与 `name` 为 `null`，`isActive` 为 `false`。
   */
  getActiveDspPreset(): GetActiveDspPresetResult;

  /**
   * Select a DSP preset by index, which replaces the whole active DSP chain with the preset's;
   * the same operation as `dsp.applyPreset` by index. Nothing selects "no preset" again.
   * @zh 按索引选中一个 DSP 预设，用它整条替换当前 DSP 链；与按索引调用 `dsp.applyPreset` 是同一个操作。选中之后无法再回到「未选中」。
   */
  setActiveDspPreset(params: SetActiveDspPresetParams): void;

  /**
   * Report foobar2000's "cursor follows playback" setting.
   * @zh 报告 foobar2000 的「光标跟随播放」设置。
   */
  getCursorFollowPlayback(): GetCursorFollowPlaybackResult;

  /**
   * Turn foobar2000's "cursor follows playback" setting on or off.
   * @zh 打开或关闭 foobar2000 的「光标跟随播放」设置。
   */
  setCursorFollowPlayback(params: SetCursorFollowPlaybackParams): SetCursorFollowPlaybackResult;

  /**
   * Report foobar2000's "playback follows cursor" setting.
   * @zh 报告 foobar2000 的「播放跟随光标」设置。
   */
  getPlaybackFollowCursor(): GetPlaybackFollowCursorResult;

  /**
   * Turn foobar2000's "playback follows cursor" setting on or off.
   * @zh 打开或关闭 foobar2000 的「播放跟随光标」设置。
   */
  setPlaybackFollowCursor(params: SetPlaybackFollowCursorParams): SetPlaybackFollowCursorResult;

  /**
   * Report the ReplayGain source mode as a number. `replaygain.getMode` reports the same setting
   * by name.
   * @zh 以数字报告 ReplayGain 音源模式。`replaygain.getMode` 以名称报告同一项设置。
   */
  getReplaygainMode(): GetReplaygainModeResult;

  /**
   * Set the ReplayGain source mode by number (`mode`) or by name (`sourceMode`). One of the two is
   * required; `mode` wins when both are given. `replaygain.setMode` changes the same setting by
   * name.
   * @zh 按数字（`mode`）或名称（`sourceMode`）设置 ReplayGain 音源模式。两者至少给一个；都给时以 `mode` 为准。`replaygain.setMode` 按名称修改同一项设置。
   */
  setReplaygainMode(params: SetReplaygainModeParams): SetReplaygainModeResult;
}

/**
 * Kind of an Advanced preferences entry: `branch` holds other entries, `checkbox` and `radio`
 * hold a boolean, `integer` and `string` hold text, `unknown` is any other kind.
 * @zh 高级首选项条目的类别：`branch` 容纳其他条目，`checkbox` 与 `radio` 存布尔值，`integer` 与 `string` 存文本，`unknown` 是其他类别。
 */
type AdvancedConfigType = 'branch' | 'checkbox' | 'radio' | 'integer' | 'string' | 'unknown';

interface SetParams {
  /**
   * Key to store under. Every page of this component shares one store, so give your keys a
   * prefix of your own.
   * @zh 存储用的键。本组件的所有页面共用一个存储，请给键加上自己的前缀。
   * @minLength 1
   */
  key: string;
  /**
   * Value to store, any JSON value. A top-level `null` counts as missing and is refused, so clear
   * a key with `config.remove`; a `null` inside an object or an array is kept.
   * @zh 要存的值，任意 JSON 值。顶层 `null` 按缺失处理并被拒绝，清除键请用 `config.remove`；对象或数组里的 `null` 照常保存。
   */
  value: Json;
}

interface SetResult {
  /**
   * The key written.
   * @zh 写入的键。
   */
  key: string;
}

interface GetParams {
  /**
   * Key to read.
   * @zh 要读取的键。
   * @minLength 1
   */
  key: string;
  /**
   * Value to answer with when the key is absent.
   * @zh 键不存在时用来作答的值。
   */
  default?: Json;
}

interface GetResult {
  /**
   * The key asked for.
   * @zh 请求的键。
   */
  key: string;
  /**
   * The stored value; when the key is absent, `default` if it was given, otherwise `null`.
   * @zh 存储的值；键不存在时为 `default`（给了的话），否则为 `null`。
   */
  value: Json;
  /**
   * Whether the key is in the store.
   * @zh 该键是否在存储中。
   */
  found: boolean;
}

interface RemoveParams {
  /**
   * Key to delete.
   * @zh 要删除的键。
   * @minLength 1
   */
  key: string;
}

interface RemoveResult {
  /**
   * The key asked for.
   * @zh 请求的键。
   */
  key: string;
  /**
   * Whether the key was in the store before the call.
   * @zh 调用之前该键是否在存储中。
   */
  existed: boolean;
}

interface GetAllResult {
  /**
   * Every stored key with its value.
   * @zh 存储里的每个键及其值。
   */
  items: Record<string, Json>;
  /**
   * The same map as `items`, under its older name.
   * @zh 与 `items` 相同的映射，这是它的旧名字。
   */
  configs: Record<string, Json>;
  /**
   * Number of keys in the store.
   * @zh 存储里的键数。
   */
  count: Int;
}

interface ExportResult {
  /**
   * Every stored key with its value; the map `config.getAll` reports.
   * @zh 存储里的每个键及其值；即 `config.getAll` 报告的映射。
   */
  data: Record<string, Json>;
  /**
   * `data` serialized as one compact JSON text.
   * @zh `data` 序列化成的一段紧凑 JSON 文本。
   */
  json: string;
  /**
   * Number of keys in the store.
   * @zh 存储里的键数。
   */
  count: Int;
}

/**
 * One output device as `config.getOutputDevices` lists it.
 * @zh `config.getOutputDevices` 列出的一个输出设备。
 */
interface ConfigOutputDevice {
  /**
   * Display name: the full name the output manager reports, or `<module>: <device>` when the
   * device list comes from the output modules themselves.
   * @zh 显示名：输出管理器报告的完整名称；设备列表取自各输出模块本身时为 `<模块名>: <设备名>`。
   */
  name: string;
  /**
   * The same as `deviceId`.
   * @zh 与 `deviceId` 相同。
   */
  id: string;
  /**
   * GUID of the output module, rendered as `{...}`.
   * @zh 输出模块的 GUID，形如 `{...}`。
   */
  outputId: string;
  /**
   * Device GUID rendered as `{...}`. Not unique on its own: a module reports its default device
   * with the all-zero GUID, so key devices by `(outputId, deviceId)`.
   * @zh 设备 GUID，形如 `{...}`。单独不唯一：模块用全零 GUID 表示其默认设备，要用 `(outputId, deviceId)` 作键。
   */
  deviceId: string;
  /**
   * Whether this module and device are the ones in effect, as `config.getOutputConfig` reports
   * them.
   * @zh 该模块与设备是否就是当前生效的那一对，即 `config.getOutputConfig` 报告的那一对。
   */
  isCurrent: boolean;
}

interface GetOutputDevicesResult {
  /**
   * Devices of all output modules.
   * @zh 所有输出模块的设备。
   */
  devices: ConfigOutputDevice[];
  /**
   * Number of entries in `devices`.
   * @zh `devices` 的条目数。
   */
  count: Int;
}

interface GetOutputConfigResult {
  /**
   * GUID of the output module in effect, rendered as `{...}`.
   * @zh 当前生效的输出模块的 GUID，形如 `{...}`。
   */
  outputId: string;
  /**
   * GUID of the device in effect, rendered as `{...}`.
   * @zh 当前生效的设备的 GUID，形如 `{...}`。
   */
  deviceId: string;
  /**
   * Output buffer length in seconds.
   * @zh 输出缓冲长度，单位秒。
   */
  bufferLength: number;
  /**
   * Output bit depth setting, in bits.
   * @zh 输出位深设置，单位位。
   */
  bitDepth: Int;
  /**
   * Whether dithering is on.
   * @zh 是否开启抖动。
   */
  useDither: boolean;
  /**
   * Whether fades are on.
   * @zh 是否开启淡入淡出。
   */
  useFades: boolean;
  /**
   * Display name of the output module; absent when no installed module has that GUID.
   * @zh 输出模块的显示名；没有已安装的模块对应该 GUID 时不出现。
   */
  outputName?: string;
  /**
   * Display name of the device; absent when the module does not name it.
   * @zh 设备的显示名；模块不提供名称时不出现。
   */
  deviceName?: string;
}

interface SetOutputDeviceParams {
  /**
   * GUID of the output module, as `config.getOutputDevices` reports it in `outputId`.
   * @zh 输出模块的 GUID，取自 `config.getOutputDevices` 的 `outputId`。
   * @minLength 1
   */
  outputId: string;
  /**
   * GUID of the device, as `config.getOutputDevices` reports it in `deviceId`.
   * @zh 设备的 GUID，取自 `config.getOutputDevices` 的 `deviceId`。
   * @minLength 1
   */
  deviceId: string;
}

interface SetOutputBufferParams {
  /**
   * Buffer length in seconds. Ignored when `milliseconds` is given, but range-checked all the
   * same.
   * @zh 缓冲长度，单位秒。给了 `milliseconds` 时不采用，但仍做范围检查。
   * @minimum 0.05
   * @maximum 2
   */
  bufferLength?: number;
  /**
   * Buffer length in milliseconds; takes precedence over `bufferLength`.
   * @zh 缓冲长度，单位毫秒；优先于 `bufferLength`。
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
 * @zh foobar2000 高级首选项树里的一个条目。每个条目都带 `type`，其余可选键带哪些由它决定：`branch` 带 `children`，`checkbox` 与 `radio` 带布尔 `value`，`integer` 与 `string` 带文本 `value` 和三个 `is*` 标志。
 */
interface AdvancedConfigItem {
  /**
   * Display name.
   * @zh 显示名。
   */
  name: string;
  /**
   * Entry GUID rendered as `{...}`, as `config.getAdvancedConfigValue`,
   * `config.setAdvancedConfigValue` and `config.resetAdvancedConfig` take it.
   * @zh 条目 GUID，形如 `{...}`，`config.getAdvancedConfigValue`、`config.setAdvancedConfigValue` 与 `config.resetAdvancedConfig` 用它。
   */
  guid: string;
  /**
   * Sort priority the entry reports.
   * @zh 条目报告的排序优先级。
   */
  sortPriority: number;
  type?: AdvancedConfigType;
  /**
   * Current value: a boolean on `checkbox` and `radio`; the stored text on `integer` and `string`,
   * so an integer entry reads as its decimal text here while `config.getAdvancedConfigValue`
   * reports a number; absent on `branch` and `unknown`.
   * @zh 当前值：`checkbox` 与 `radio` 上是布尔值；`integer` 与 `string` 上是存储的文本，所以整数条目在这里是十进制文本，而 `config.getAdvancedConfigValue` 报告的是数字；`branch` 与 `unknown` 上没有这个键。
   */
  value?: Json;
  /**
   * Default value in the same form as `value`; present only when the entry reports one.
   * @zh 默认值，形式与 `value` 相同；只在条目提供默认值时出现。
   */
  defaultValue?: Json;
  /**
   * Whether the entry is flagged as a signed integer; present on `integer` and `string`.
   * @zh 条目是否标记为有符号整数；只在 `integer` 与 `string` 上出现。
   */
  isSigned?: boolean;
  /**
   * Whether the entry is flagged as holding a file path; present on `integer` and `string`.
   * @zh 条目是否标记为存放文件路径；只在 `integer` 与 `string` 上出现。
   */
  isFilePath?: boolean;
  /**
   * Whether the entry is flagged as holding a folder path; present on `integer` and `string`.
   * @zh 条目是否标记为存放文件夹路径；只在 `integer` 与 `string` 上出现。
   */
  isFolderPath?: boolean;
  /**
   * The entries inside a branch; present on `branch` only. The tree stops eleven levels below
   * the requested parent: a branch on that level is listed with an empty `children`.
   * @zh 分支里的条目；只在 `branch` 上出现。树在请求的父节点之下第十一层截止：这一层的分支以空的 `children` 列出。
   */
  children?: AdvancedConfigItem[];
}

interface GetAdvancedConfigParams {
  /**
   * GUID of the branch to list, rendered as `{...}`; the root of the tree when omitted or empty.
   * A well-formed GUID that names no branch lists nothing.
   * @zh 要列出的分支的 GUID，形如 `{...}`；省略或为空串时从树根开始。格式正确但不是分支的 GUID 列出空结果。
   */
  parentGuid?: string;
}

interface GetAdvancedConfigResult {
  /**
   * Entries directly below the requested branch; each branch carries its own entries in
   * `children`.
   * @zh 请求的分支下一层的条目；每个分支在 `children` 里带着自己的条目。
   */
  entries: AdvancedConfigItem[];
  /**
   * Number of entries in `entries`; nested entries are not counted.
   * @zh `entries` 的条目数，不计嵌套的条目。
   */
  count: Int;
}

interface GetAdvancedConfigValueParams {
  /**
   * Entry GUID rendered as `{...}`.
   * @zh 条目 GUID，形如 `{...}`。
   * @minLength 1
   */
  guid: string;
}

interface GetAdvancedConfigValueResult {
  /**
   * Display name of the entry.
   * @zh 条目的显示名。
   */
  name: string;
  /**
   * The GUID as it was sent; its letter case is not normalized.
   * @zh 原样回显请求中的 GUID，不规范化大小写。
   */
  guid: string;
  type: AdvancedConfigType;
  /**
   * Current value: a boolean on `checkbox` and `radio`, a number on `integer` (`0` when the
   * stored text does not start with a number), a string on `string`, `null` on `branch` and
   * `unknown`.
   * @zh 当前值：`checkbox` 与 `radio` 上是布尔值，`integer` 上是数字（存储的文本不以数字开头时为 `0`），`string` 上是字符串，`branch` 与 `unknown` 上是 `null`。
   */
  value: Json;
}

interface SetAdvancedConfigValueParams {
  /**
   * Entry GUID rendered as `{...}`.
   * @zh 条目 GUID，形如 `{...}`。
   * @minLength 1
   */
  guid: string;
  /**
   * Value to write; what each kind of entry accepts is in the method description.
   * @zh 要写入的值；各类条目接受什么见方法说明。
   */
  value: Json;
}

interface ResetAdvancedConfigParams {
  /**
   * Entry GUID rendered as `{...}`.
   * @zh 条目 GUID，形如 `{...}`。
   * @minLength 1
   */
  guid: string;
}

/**
 * One preferences page or preferences branch as `config.getPreferencesPages` lists it.
 * @zh `config.getPreferencesPages` 列出的一个首选项页面或首选项分支。
 */
interface ConfigPreferencesPage {
  /**
   * Display name.
   * @zh 显示名。
   */
  name: string;
  /**
   * GUID of the page or branch, rendered as `{...}`.
   * @zh 页面或分支的 GUID，形如 `{...}`。
   */
  guid: string;
  /**
   * GUID of the page or branch it sits under, rendered as `{...}`; a standard parent from
   * `config.getPreferencesStandardGuids` for a top-level page.
   * @zh 它所在的上级页面或分支的 GUID，形如 `{...}`；顶层页面是 `config.getPreferencesStandardGuids` 里的某个标准父节点。
   */
  parentGuid: string;
  /**
   * Sort priority it reports: lower sorts first and `0` sorts by name; `0` as well when it
   * reports none.
   * @zh 它报告的排序优先级：越小越靠前，`0` 按名称排序；不提供时也是 `0`。
   */
  sortPriority: number;
  /**
   * `true` on a branch; absent on a page.
   * @zh 分支上为 `true`；页面上没有这个键。
   */
  isBranch?: boolean;
}

interface GetPreferencesPagesResult {
  /**
   * Every page, then every branch.
   * @zh 先是所有页面，然后是所有分支。
   */
  pages: ConfigPreferencesPage[];
  /**
   * Number of entries in `pages`.
   * @zh `pages` 的条目数。
   */
  count: Int;
}

interface GetPreferencesStandardGuidsResult {
  /**
   * `preferences_page::guid_root`, rendered as `{...}`: the root of the preferences tree.
   * @zh `preferences_page::guid_root`，形如 `{...}`：首选项树的根。
   */
  root: string;
  /**
   * `preferences_page::guid_hidden`, rendered as `{...}`.
   * @zh `preferences_page::guid_hidden`，形如 `{...}`。
   */
  hidden: string;
  /**
   * `preferences_page::guid_tools`, rendered as `{...}`.
   * @zh `preferences_page::guid_tools`，形如 `{...}`。
   */
  tools: string;
  /**
   * `preferences_page::guid_core`, rendered as `{...}`.
   * @zh `preferences_page::guid_core`，形如 `{...}`。
   */
  core: string;
  /**
   * `preferences_page::guid_display`, rendered as `{...}`.
   * @zh `preferences_page::guid_display`，形如 `{...}`。
   */
  display: string;
  /**
   * `preferences_page::guid_playback`, rendered as `{...}`.
   * @zh `preferences_page::guid_playback`，形如 `{...}`。
   */
  playback: string;
  /**
   * `preferences_page::guid_visualisations`, rendered as `{...}`.
   * @zh `preferences_page::guid_visualisations`，形如 `{...}`。
   */
  visualisations: string;
  /**
   * `preferences_page::guid_input`, rendered as `{...}`.
   * @zh `preferences_page::guid_input`，形如 `{...}`。
   */
  input: string;
  /**
   * `preferences_page::guid_tag_writing`, rendered as `{...}`.
   * @zh `preferences_page::guid_tag_writing`，形如 `{...}`。
   */
  tagWriting: string;
  /**
   * `preferences_page::guid_media_library`, rendered as `{...}`.
   * @zh `preferences_page::guid_media_library`，形如 `{...}`。
   */
  mediaLibrary: string;
  /**
   * `preferences_page::guid_tagging`, rendered as `{...}`.
   * @zh `preferences_page::guid_tagging`，形如 `{...}`。
   */
  tagging: string;
  /**
   * `preferences_page::guid_output`, rendered as `{...}`.
   * @zh `preferences_page::guid_output`，形如 `{...}`。
   */
  output: string;
  /**
   * `preferences_page::guid_advanced`, rendered as `{...}`.
   * @zh `preferences_page::guid_advanced`，形如 `{...}`。
   */
  advanced: string;
  /**
   * `preferences_page::guid_components`, rendered as `{...}`.
   * @zh `preferences_page::guid_components`，形如 `{...}`。
   */
  components: string;
  /**
   * `preferences_page::guid_dsp`, rendered as `{...}`.
   * @zh `preferences_page::guid_dsp`，形如 `{...}`。
   */
  dsp: string;
  /**
   * `preferences_page::guid_shell`, rendered as `{...}`.
   * @zh `preferences_page::guid_shell`，形如 `{...}`。
   */
  shell: string;
  /**
   * `preferences_page::guid_keyboard_shortcuts`, rendered as `{...}`.
   * @zh `preferences_page::guid_keyboard_shortcuts`，形如 `{...}`。
   */
  keyboardShortcuts: string;
}

/**
 * One installed component as `config.getComponents` lists it.
 * @zh `config.getComponents` 列出的一个已安装组件。
 */
interface ConfigComponentInfo {
  /**
   * Name the component reports.
   * @zh 组件报告的名称。
   */
  name: string;
  /**
   * Version string the component reports.
   * @zh 组件报告的版本字符串。
   */
  version: string;
  /**
   * File name the component reports for its module; absent when it reports none.
   * @zh 组件报告的模块文件名；不提供时不出现。
   */
  filename?: string;
  /**
   * The same as `filename`.
   * @zh 与 `filename` 相同。
   */
  fileName?: string;
}

interface GetComponentsResult {
  /**
   * Every installed component, in enumeration order.
   * @zh 所有已安装的组件，按枚举顺序。
   */
  components: ConfigComponentInfo[];
  /**
   * Number of entries in `components`.
   * @zh `components` 的条目数。
   */
  count: Int;
}

interface GetVersionInfoResult {
  /**
   * foobar2000's version string as the core reports it; it includes the product name.
   * @zh 核心报告的 foobar2000 版本字符串，其中含产品名。
   */
  version: string;
  /**
   * The same as `version`.
   * @zh 与 `version` 相同。
   */
  foobar2000: string;
  /**
   * What `core_version_info_v2::get_name()` reports, which the foobar2000 SDK documents as the
   * product name `foobar2000`; despite the key, not a longer form of `version`.
   * @zh `core_version_info_v2::get_name()` 的返回值，foobar2000 SDK 注明它是产品名 `foobar2000`；虽然键名如此，它并不是 `version` 的完整形式。
   */
  versionFull: string;
  /**
   * Whether this is a 64-bit build.
   * @zh 是否为 64 位构建。
   */
  is64bit: boolean;
  /**
   * Whether foobar2000 runs as a portable installation.
   * @zh foobar2000 是否以便携模式运行。
   */
  isPortable: boolean;
  /**
   * This component.
   * @zh 本组件。
   */
  plugin: {
    /**
     * Always `foo_ui_webview2`.
     * @zh 恒为 `foo_ui_webview2`。
     */
    name: string;
    /**
     * This component's version, `major.minor.patch`.
     * @zh 本组件的版本，`主.次.修订`。
     */
    version: string;
  };
  /**
   * The profile folder as a display path.
   * @zh 配置目录，显示路径形式。
   */
  profilePath: string;
}

interface GetLibraryStatusResult {
  /**
   * Whether the media library is enabled, which foobar2000 takes to mean that at least one
   * library folder is configured.
   * @zh 媒体库是否启用；foobar2000 以「至少配置了一个媒体库文件夹」为准。
   */
  enabled: boolean;
  /**
   * Number of items in the media library, counted on every call.
   * @zh 媒体库中的条目数，每次调用时重新计数。
   */
  itemCount: Int;
  /**
   * Whether the media library has finished loading; `true` on a host that cannot tell.
   * @zh 媒体库是否已载入完成；宿主无法判断时为 `true`。
   */
  initialized: boolean;
}

/**
 * Where foobar2000 puts one kind of new file.
 * @zh foobar2000 放置某一类新文件的位置。
 */
interface ConfigLibraryFilePattern {
  /**
   * Target folder as configured.
   * @zh 配置的目标文件夹。
   */
  directory: string;
  /**
   * Title formatting pattern for the subfolders and the file name below that folder.
   * @zh 该文件夹之下的子文件夹与文件名所用的标题格式化模式。
   */
  format: string;
}

interface GetLibraryFilePatternsResult {
  /**
   * Pattern for newly encoded, copied or moved tracks; absent when not configured.
   * @zh 新编码、复制或移动的曲目所用的模式；没有配置时不出现。
   */
  tracks?: ConfigLibraryFilePattern;
  /**
   * Pattern for newly encoded, copied or moved album images; absent when not configured.
   * @zh 新编码、复制或移动的专辑图片所用的模式；没有配置时不出现。
   */
  images?: ConfigLibraryFilePattern;
}

/**
 * One stored DSP preset as `config.getDspPresets` lists it.
 * @zh `config.getDspPresets` 列出的一个已保存的 DSP 预设。
 */
interface ConfigDspPreset {
  /**
   * Position in the preset list, as `config.setActiveDspPreset` takes it.
   * @zh 在预设列表中的位置，`config.setActiveDspPreset` 用它。
   */
  index: Int;
  /**
   * Preset name.
   * @zh 预设名。
   */
  name: string;
}

interface GetDspPresetsResult {
  /**
   * Every stored preset, in list order.
   * @zh 所有已保存的预设，按列表顺序。
   */
  presets: ConfigDspPreset[];
  /**
   * Number of entries in `presets`.
   * @zh `presets` 的条目数。
   */
  count: Int;
}

interface GetActiveDspPresetResult {
  /**
   * Position of the selected preset; `null` when none is selected.
   * @zh 选中预设的位置；没有选中时为 `null`。
   */
  index: Int | null;
  /**
   * Name of the selected preset; `null` when none is selected.
   * @zh 选中预设的名称；没有选中时为 `null`。
   */
  name: string | null;
  /**
   * Whether a preset is selected.
   * @zh 是否有预设被选中。
   */
  isActive: boolean;
}

interface SetActiveDspPresetParams {
  /**
   * Position of the preset in `config.getDspPresets`.
   * @zh 预设在 `config.getDspPresets` 中的位置。
   * @minimum 0
   */
  index: Int;
}

interface GetCursorFollowPlaybackResult {
  /**
   * Whether the setting is on.
   * @zh 该设置是否打开。
   */
  enabled: boolean;
  /**
   * The same as `enabled`.
   * @zh 与 `enabled` 相同。
   */
  value: boolean;
}

interface SetCursorFollowPlaybackParams {
  /**
   * `true` turns the setting on, `false` turns it off.
   * @zh `true` 打开该设置，`false` 关闭。
   */
  enabled: boolean;
}

interface SetCursorFollowPlaybackResult {
  /**
   * The value written.
   * @zh 写入的值。
   */
  enabled: boolean;
}

interface GetPlaybackFollowCursorResult {
  /**
   * Whether the setting is on.
   * @zh 该设置是否打开。
   */
  enabled: boolean;
  /**
   * The same as `enabled`.
   * @zh 与 `enabled` 相同。
   */
  value: boolean;
}

interface SetPlaybackFollowCursorParams {
  /**
   * `true` turns the setting on, `false` turns it off.
   * @zh `true` 打开该设置，`false` 关闭。
   */
  enabled: boolean;
}

interface SetPlaybackFollowCursorResult {
  /**
   * The value written.
   * @zh 写入的值。
   */
  enabled: boolean;
}

interface GetReplaygainModeResult {
  /**
   * Source mode: `0` none, `1` track, `2` album, `3` by playback order.
   * @zh 音源模式：`0` 无，`1` 音轨，`2` 专辑，`3` 按播放顺序。
   */
  mode: Int;
  /**
   * The same as `mode`.
   * @zh 与 `mode` 相同。
   */
  value: Int;
}

interface SetReplaygainModeParams {
  /**
   * Source mode by number, as `config.getReplaygainMode` reports it; takes precedence over
   * `sourceMode`.
   * @zh 按数字给出的音源模式，与 `config.getReplaygainMode` 报告的一致；优先于 `sourceMode`。
   * @minimum 0
   * @maximum 3
   */
  mode?: Int;
  /**
   * Source mode by name: `none` is `0`, `track` `1`, `album` `2`, and `byPlaybackOrder` or its
   * other spelling `auto` is `3`. Read only when `mode` is absent.
   * @zh 按名称给出的音源模式：`none` 为 `0`，`track` 为 `1`，`album` 为 `2`，`byPlaybackOrder` 或其另一写法 `auto` 为 `3`。只在没给 `mode` 时读取。
   */
  sourceMode?: 'none' | 'track' | 'album' | 'byPlaybackOrder' | 'auto';
}

interface SetReplaygainModeResult {
  /**
   * The source mode now in effect, as a number.
   * @zh 现在生效的音源模式，以数字表示。
   */
  mode: Int;
  /**
   * The same as `mode`.
   * @zh 与 `mode` 相同。
   */
  value: Int;
}

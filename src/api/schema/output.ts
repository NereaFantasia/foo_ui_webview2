import type { Int } from './common.js';

export interface Api {
  /**
   * List the devices of every output module.
   * @zh 获取全部输出模块下的音频设备列表
   */
  getDevices(): GetDevicesResult;

  /**
   * List the output modules (foobar2000's output entries) with their capability flags.
   * @zh 获取全部输出模块（output entry）及其能力标志列表
   */
  getEntries(): GetEntriesResult;

  /**
   * Report the display names of the output modules. Informational only: the output settings are
   * edited in foobar2000 Preferences, and `config.setOutputDevice` switches the device.
   * @zh 获取各输出模块的显示名称（仅供参考展示；输出配置须在 foobar2000 首选项调整，切换设备请调用 config.setOutputDevice）
   */
  getSettings(): GetSettingsResult;
}

/** One output device. */
interface OutputDevice {
  /**
   * Device GUID rendered as `{...}`. Not unique on its own: a module reports its default device
   * with the all-zero GUID, so key devices by `(entryGuid, guid)`.
   * @zh 输出设备唯一标识符 GUID（格式为 {...}；由于默认设备为全零 GUID，需结合 (entryGuid, guid) 作为唯一复合键）
   */
  guid: string;
  /**
   * Display name of the device.
   * @zh 音频输出设备显示名称
   */
  name: string;
  /**
   * Display name of the output module that provides the device.
   * @zh 提供该设备的输出模块显示名称
   */
  entry: string;
  /**
   * GUID of that output module, rendered as `{...}`.
   * @zh 提供该设备的输出模块 GUID（格式为 {...}）
   */
  entryGuid: string;
}

interface GetDevicesResult {
  /**
   * Devices of all output modules, module by module.
   * @zh 全部输出模块下的设备列表（按模块归属依次排列）
   */
  devices: OutputDevice[];
  /**
   * Number of entries in `devices`.
   * @zh devices 数组包含的设备总数
   */
  count: Int;
}

/** One output module and what it needs configured. */
interface OutputEntry {
  /**
   * Module GUID rendered as `{...}`.
   * @zh 输出模块唯一标识符 GUID（格式为 {...}）
   */
  guid: string;
  /**
   * Display name of the module. Several modules may share one, and some report an empty name.
   * @zh 输出模块显示名称（可能存在同名或空名称）
   */
  name: string;
  /**
   * The module wants an output bit depth configured.
   * @zh 该模块是否需要显式配置输出位深
   */
  needsBitdepthConfig: boolean;
  /**
   * The module wants dithering configured.
   * @zh 该模块是否需要配置抖动处理（Dithering）
   */
  needsDitherConfig: boolean;
  /**
   * The module can play several streams at once.
   * @zh 该模块是否支持同时播放多路音频流
   */
  supportsMultipleStreams: boolean;
  /**
   * The module declares itself high latency.
   * @zh 该模块是否声明为高延迟输出
   */
  isHighLatency: boolean;
  /**
   * The module declares itself low latency.
   * @zh 该模块是否声明为低延迟输出
   */
  isLowLatency: boolean;
}

interface GetEntriesResult {
  /**
   * Every output module.
   * @zh 全部已安装的输出模块列表
   */
  entries: OutputEntry[];
  /**
   * Number of entries in `entries`.
   * @zh entries 数组包含的模块总数
   */
  count: Int;
}

interface GetSettingsResult {
  /**
   * Fixed English sentence saying where output settings are edited.
   * @zh 固定的英文提示文本（指出首选项中修改输出设置的路径）
   */
  note: string;
  /**
   * Display names of the output modules. Names can repeat or be empty, and the order is not
   * stable between calls; prefer `output.getEntries`, which pairs each name with its GUID.
   * @zh 各输出模块显示名称列表（名称可能重复或为空且调用间顺序不固定；建议优先使用 output.getEntries 配合 GUID 唯一标识）
   */
  availableOutputs: string[];
}

import type { Int } from './common.js';

export interface Api {
  /**
   * List the devices of every output module.
   * @zh 列出所有输出模块的设备。
   */
  getDevices(): GetDevicesResult;

  /**
   * List the output modules (foobar2000's output entries) with their capability flags.
   * @zh 列出输出模块（foobar2000 的 output entry）及其能力标志。
   */
  getEntries(): GetEntriesResult;

  /**
   * Report the display names of the output modules. Informational only: the output settings are
   * edited in foobar2000 Preferences, and `config.setOutputDevice` switches the device.
   * @zh 报告各输出模块的显示名。仅供参考：输出设置在 foobar2000 首选项里修改，切换设备用 `config.setOutputDevice`。
   */
  getSettings(): GetSettingsResult;
}

/** One output device. */
interface OutputDevice {
  /**
   * Device GUID rendered as `{...}`. Not unique on its own: a module reports its default device
   * with the all-zero GUID, so key devices by `(entryGuid, guid)`.
   * @zh 设备 GUID，形如 `{...}`。单独不唯一：模块用全零 GUID 表示其默认设备，要用 `(entryGuid, guid)` 作键。
   */
  guid: string;
  /**
   * Display name of the device.
   * @zh 设备的显示名。
   */
  name: string;
  /**
   * Display name of the output module that provides the device.
   * @zh 提供该设备的输出模块的显示名。
   */
  entry: string;
  /**
   * GUID of that output module, rendered as `{...}`.
   * @zh 该输出模块的 GUID，形如 `{...}`。
   */
  entryGuid: string;
}

interface GetDevicesResult {
  /**
   * Devices of all output modules, module by module.
   * @zh 所有输出模块的设备，按模块依次排列。
   */
  devices: OutputDevice[];
  /**
   * Number of entries in `devices`.
   * @zh `devices` 的条目数。
   */
  count: Int;
}

/** One output module and what it needs configured. */
interface OutputEntry {
  /**
   * Module GUID rendered as `{...}`.
   * @zh 模块 GUID，形如 `{...}`。
   */
  guid: string;
  /**
   * Display name of the module. Several modules may share one, and some report an empty name.
   * @zh 模块的显示名。可能有多个模块同名，也有模块报告空名字。
   */
  name: string;
  /**
   * The module wants an output bit depth configured.
   * @zh 该模块需要配置输出位深。
   */
  needsBitdepthConfig: boolean;
  /**
   * The module wants dithering configured.
   * @zh 该模块需要配置抖动。
   */
  needsDitherConfig: boolean;
  /**
   * The module can play several streams at once.
   * @zh 该模块能同时播放多路流。
   */
  supportsMultipleStreams: boolean;
  /**
   * The module declares itself high latency.
   * @zh 该模块声明自己是高延迟的。
   */
  isHighLatency: boolean;
  /**
   * The module declares itself low latency.
   * @zh 该模块声明自己是低延迟的。
   */
  isLowLatency: boolean;
}

interface GetEntriesResult {
  /**
   * Every output module.
   * @zh 所有输出模块。
   */
  entries: OutputEntry[];
  /**
   * Number of entries in `entries`.
   * @zh `entries` 的条目数。
   */
  count: Int;
}

interface GetSettingsResult {
  /**
   * Fixed English sentence saying where output settings are edited.
   * @zh 一句固定的英文说明，指出输出设置在哪里修改。
   */
  note: string;
  /**
   * Display names of the output modules. Names can repeat or be empty, and the order is not
   * stable between calls; prefer `output.getEntries`, which pairs each name with its GUID.
   * @zh 各输出模块的显示名。名字可能重复或为空，顺序在两次调用之间也不稳定；新代码请用 `output.getEntries`，它给出名字与 GUID 的对应。
   */
  availableOutputs: string[];
}

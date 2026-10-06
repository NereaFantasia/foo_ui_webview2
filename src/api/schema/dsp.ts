import type { Int } from './common.js';

export interface Api {
  /**
   * Read the active DSP chain and which preset, if any, it currently corresponds to.
   * @zh 读取当前生效的 DSP 链，以及它当前对应哪个预设（如果有）。
   */
  getChain(): GetChainResult;

  /**
   * List the DSP presets stored in the profile and which one is selected.
   * @zh 列出配置目录里保存的 DSP 预设，以及当前选中的是哪个。
   */
  getPresets(): GetPresetsResult;

  /**
   * Make a preset the active chain, by index or by name; `index` wins when both are given.
   * Replaces the whole active chain and never writes the preset file. A name matches exactly,
   * case included, and the first preset of that name is taken. Giving neither fails with
   * `INVALID_PARAMS`, an index past the last preset with `INVALID_INDEX`, a name no preset has
   * with `NOT_FOUND`.
   * @zh 按索引或名称把一个预设设为当前链；两者都给时以 `index` 为准。整条替换当前链，不改写预设文件。名称须完全一致（区分大小写），同名时取第一个。两者都没给以 `INVALID_PARAMS` 失败，索引超出最后一个预设以 `INVALID_INDEX` 失败，没有预设叫这个名称以 `NOT_FOUND` 失败。
   */
  applyPreset(params: ApplyPresetParams): ApplyPresetResult;

  /**
   * List every installed DSP processor.
   * @zh 列出已安装的全部 DSP 处理器。
   */
  getAvailable(): GetAvailableResult;

  /**
   * Insert a processor into the active chain with its default settings.
   * @zh 用默认设置把一个处理器插入当前链。
   */
  addDsp(params: AddDspParams): AddDspResult;

  /**
   * Remove one entry from the active chain.
   * @zh 从当前链移除一项。
   */
  removeDsp(params: RemoveDspParams): RemoveDspResult;

  /**
   * Move one entry of the active chain to another position, keeping its settings. Prefer this
   * over `setChain` for reordering: `setChain` rebuilds every entry from its default settings.
   * @zh 把当前链里的一项移到另一位置，保留它的设置。重排请用它而不是 `setChain`：`setChain` 会用默认设置重建每一项。
   */
  moveDsp(params: MoveDspParams): MoveDspResult;

  /**
   * Replace the whole active chain, each entry built from its processor's default settings. An
   * empty list clears the chain. One entry that does not resolve to an installed processor fails
   * the whole call and leaves the chain untouched.
   * @zh 整条替换当前链，每一项按处理器的默认设置构建。空列表即清空链。任一项解析不到已安装的处理器，整次调用失败且链保持不变。
   */
  setChain(params: SetChainParams): SetChainResult;
}

/** One entry of the active chain. */
interface DspChainEntry {
  /**
   * Position in the chain.
   * @zh 在链中的位置。
   */
  index: Int;
  /**
   * GUID of the processor, rendered as `{...}`.
   * @zh 处理器的 GUID，形如 `{...}`。
   */
  guid: string;
  /**
   * Display name of the processor.
   * @zh 处理器的显示名。
   */
  name: string;
}

interface GetChainResult {
  /**
   * The chain in processing order.
   * @zh 按处理顺序排列的链。
   */
  dsps: DspChainEntry[];
  /**
   * Name of the selected preset; `null` when the chain matches no preset, as after any edit
   * through `addDsp`, `removeDsp`, `moveDsp` or `setChain`.
   * @zh 当前选中预设的名称；链不对应任何预设时为 `null`，经 `addDsp`、`removeDsp`、`moveDsp` 或 `setChain` 改过之后就是这样。
   */
  activePreset: string | null;
  /**
   * Index of the selected preset in `dsp.getPresets`; `-1` when none is selected.
   * @zh 选中预设在 `dsp.getPresets` 中的索引；没有选中时为 `-1`。
   */
  activePresetIndex: Int;
}

/** One stored preset. */
interface DspPresetEntry {
  /**
   * Index in the preset list, as `applyPreset` takes it.
   * @zh 在预设列表中的索引，`applyPreset` 用它。
   */
  index: Int;
  /**
   * Preset name, which is also its file name under `dsp-presets` in the profile.
   * @zh 预设名，也是配置目录 `dsp-presets` 下的文件名。
   */
  name: string;
  /**
   * Whether this is the selected preset.
   * @zh 是否为当前选中的预设。
   */
  active: boolean;
}

interface GetPresetsResult {
  /**
   * Every stored preset, in list order.
   * @zh 全部保存的预设，按列表顺序。
   */
  presets: DspPresetEntry[];
  /**
   * Number of entries in `presets`.
   * @zh `presets` 的条目数。
   */
  count: Int;
  /**
   * Index of the selected preset; `-1` when none is selected.
   * @zh 选中预设的索引；没有选中时为 `-1`。
   */
  selectedIndex: Int;
}

interface ApplyPresetParams {
  /**
   * Preset index from `dsp.getPresets`.
   * @zh 来自 `dsp.getPresets` 的预设索引。
   * @minimum 0
   */
  index?: Int;
  /**
   * Preset name from `dsp.getPresets`.
   * @zh 来自 `dsp.getPresets` 的预设名。
   */
  name?: string;
}

interface ApplyPresetResult {
  /**
   * Name of the preset now active.
   * @zh 现在生效的预设名。
   */
  appliedPreset: string;
  /**
   * Index of the preset now active.
   * @zh 现在生效的预设索引。
   */
  appliedIndex: Int;
}

/** One installed processor. */
interface DspAvailableEntry {
  /**
   * GUID of the processor, rendered as `{...}`; what `addDsp` and `setChain` take.
   * @zh 处理器的 GUID，形如 `{...}`；`addDsp` 与 `setChain` 用它。
   */
  guid: string;
  /**
   * Display name of the processor.
   * @zh 处理器的显示名。
   */
  name: string;
  /**
   * Whether the processor has a configuration dialog.
   * @zh 处理器是否有配置对话框。
   */
  hasConfig: boolean;
}

interface GetAvailableResult {
  /**
   * Every installed processor, in service enumeration order.
   * @zh 已安装的全部处理器，按服务枚举顺序。
   */
  dsps: DspAvailableEntry[];
  /**
   * Number of entries in `dsps`.
   * @zh `dsps` 的条目数。
   */
  count: Int;
}

interface AddDspParams {
  /**
   * GUID of an installed processor, as `dsp.getAvailable` reports it.
   * @zh 已安装处理器的 GUID，取自 `dsp.getAvailable`。
   * @minLength 1
   */
  guid: string;
  /**
   * Position to insert at; any negative position, or one past the end, appends.
   * @zh 插入位置；`-1` 或超出链尾的位置都表示追加到末尾。
   * @default -1
   */
  position?: Int;
}

interface AddDspResult {
  /**
   * Display name of the processor added.
   * @zh 加入的处理器的显示名。
   */
  addedDsp: string;
  /**
   * Position the entry landed at.
   * @zh 该项最终所在的位置。
   */
  position: Int;
}

interface RemoveDspParams {
  /**
   * Position of the entry to remove.
   * @zh 要移除的项的位置。
   * @minimum 0
   */
  index: Int;
}

interface RemoveDspResult {
  /**
   * Display name of the processor removed.
   * @zh 移除的处理器的显示名。
   */
  removedDsp: string;
  /**
   * Position it was removed from.
   * @zh 它原来所在的位置。
   */
  removedIndex: Int;
}

interface MoveDspParams {
  /**
   * Current position of the entry.
   * @zh 该项现在的位置。
   * @minimum 0
   */
  from: Int;
  /**
   * Final position in the reordered chain.
   * @zh 重排后的最终位置。
   * @minimum 0
   */
  to: Int;
}

interface MoveDspResult {
  /**
   * Display name of the processor moved; absent when `from` equals `to` and nothing moved.
   * @zh 移动的处理器的显示名；`from` 等于 `to`、什么都没动时不出现。
   */
  movedDsp?: string;
  /**
   * The position it came from.
   * @zh 它原来的位置。
   */
  from: Int;
  /**
   * The position it is at now.
   * @zh 它现在的位置。
   */
  to: Int;
  /**
   * `No change needed` when `from` equals `to`.
   * @zh `from` 等于 `to` 时为 `No change needed`。
   */
  message?: string;
}

/** One entry of a chain to apply. */
interface DspChainSpec {
  /**
   * GUID of an installed processor, as `dsp.getAvailable` reports it.
   * @zh 已安装处理器的 GUID，取自 `dsp.getAvailable`。
   * @minLength 1
   */
  guid: string;
}

interface SetChainParams {
  /**
   * The new chain in processing order; empty clears the chain.
   * @zh 新链，按处理顺序；空即清空链。
   */
  dsps: DspChainSpec[];
}

interface SetChainResult {
  /**
   * Number of entries in the chain now.
   * @zh 现在链里的条目数。
   */
  count: Int;
}

import type { Int } from './common.js';

export interface Api {
  /**
   * Read the active DSP chain and which preset, if any, it currently corresponds to.
   * @zh 获取当前生效的 DSP 效果器链及匹配的活动预设信息（若未匹配预设则返回 null）
   */
  getChain(): GetChainResult;

  /**
   * List the DSP presets stored in the profile and which one is selected.
   * @zh 获取用户配置目录中已保存的全部 DSP 预设列表及当前选中的预设序号
   */
  getPresets(): GetPresetsResult;

  /**
   * Make a preset the active chain, by index or by name; `index` wins when both are given.
   * Replaces the whole active chain and never writes the preset file. A name matches exactly,
   * case included, and the first preset of that name is taken. Giving neither fails with
   * `INVALID_PARAMS`, an index past the last preset with `INVALID_INDEX`, a name no preset has
   * with `NOT_FOUND`.
   * @zh 将指定 DSP 预设应用为当前活动链（支持按预设索引或名称指定，两者均传入时以 `index` 为准）；整条替换当前活动效果器链且不改写磁盘预设文件；名称匹配严格区分大小写（存在同名预设时匹配首个）；未指定任何参数以 `INVALID_PARAMS` 失败，索引超出范围以 `INVALID_INDEX` 失败，未查得匹配预设名称以 `NOT_FOUND` 失败
   */
  applyPreset(params: ApplyPresetParams): ApplyPresetResult;

  /**
   * List every installed DSP processor.
   * @zh 列出宿主中已安装的所有 DSP 音效处理器及其配置支持状态
   */
  getAvailable(): GetAvailableResult;

  /**
   * Insert a processor into the active chain with its default settings.
   * @zh 以默认参数将指定的 DSP 效果器插入当前活动链的指定位置
   */
  addDsp(params: AddDspParams): AddDspResult;

  /**
   * Remove one entry from the active chain.
   * @zh 从当前活动链中移除指定位置的 DSP 效果器条目
   */
  removeDsp(params: RemoveDspParams): RemoveDspResult;

  /**
   * Move one entry of the active chain to another position, keeping its settings. Prefer this
   * over `setChain` for reordering: `setChain` rebuilds every entry from its default settings.
   * @zh 将当前活动链中的指定条目移动至目标位置并保留现有参数；重排效果器顺序时优先使用此方法（`setChain` 会以默认参数重建各项）
   */
  moveDsp(params: MoveDspParams): MoveDspResult;

  /**
   * Replace the whole active chain, each entry built from its processor's default settings. An
   * empty list clears the chain. One entry that does not resolve to an installed processor fails
   * the whole call and leaves the chain untouched.
   * @zh 全量替换当前生效的 DSP 效果器链（各项均按处理器默认配置构建，传入空列表代表清空链）；若任一条目无法解析为已安装的处理器，整次调用以失败返回且保持原活动链不变
   */
  setChain(params: SetChainParams): SetChainResult;
}

/** One entry of the active chain. */
interface DspChainEntry {
  /**
   * Position in the chain.
   * @zh 在 DSP 链中的索引位置
   */
  index: Int;
  /**
   * GUID of the processor, rendered as `{...}`.
   * @zh 效果器处理器的 GUID 标识符（格式形如 `{...}`）
   */
  guid: string;
  /**
   * Display name of the processor.
   * @zh 效果器处理器的显示名称
   */
  name: string;
}

interface GetChainResult {
  /**
   * The chain in processing order.
   * @zh 按音频处理顺序排列的活动 DSP 条目列表
   */
  dsps: DspChainEntry[];
  /**
   * Name of the selected preset; `null` when the chain matches no preset, as after any edit
   * through `addDsp`, `removeDsp`, `moveDsp` or `setChain`.
   * @zh 当前匹配的预设名称；当活动链未匹配任何预设（如经由 `addDsp`、`removeDsp`、`moveDsp` 或 `setChain` 修改后）时为 `null`
   */
  activePreset: string | null;
  /**
   * Index of the selected preset in `dsp.getPresets`; `-1` when none is selected.
   * @zh 选中预设在 `dsp.getPresets` 预设列表中的序号；未选中任何预设时为 `-1`
   */
  activePresetIndex: Int;
}

/** One stored preset. */
interface DspPresetEntry {
  /**
   * Index in the preset list, as `applyPreset` takes it.
   * @zh 在预设列表中的索引序号（可直接传入 `dsp.applyPreset`）
   */
  index: Int;
  /**
   * Preset name, which is also its file name under `dsp-presets` in the profile.
   * @zh 预设名称（对应用户配置目录 `dsp-presets` 下的文件名）
   */
  name: string;
  /**
   * Whether this is the selected preset.
   * @zh 是否为当前处于激活状态的预设
   */
  active: boolean;
}

interface GetPresetsResult {
  /**
   * Every stored preset, in list order.
   * @zh 配置目录中保存的全部预设列表（按枚举顺序排列）
   */
  presets: DspPresetEntry[];
  /**
   * Number of entries in `presets`.
   * @zh `presets` 列表包含的预设总数
   */
  count: Int;
  /**
   * Index of the selected preset; `-1` when none is selected.
   * @zh 当前选中的预设索引序号；未选中任何预设时为 `-1`
   */
  selectedIndex: Int;
}

interface ApplyPresetParams {
  /**
   * Preset index from `dsp.getPresets`.
   * @zh 目标预设在 `dsp.getPresets` 中的索引序号
   * @minimum 0
   */
  index?: Int;
  /**
   * Preset name from `dsp.getPresets`.
   * @zh 目标预设的完整名称（区分大小写）
   */
  name?: string;
}

interface ApplyPresetResult {
  /**
   * Name of the preset now active.
   * @zh 成功应用并生效的预设名称
   */
  appliedPreset: string;
  /**
   * Index of the preset now active.
   * @zh 成功应用并生效的预设索引序号
   */
  appliedIndex: Int;
}

/** One installed processor. */
interface DspAvailableEntry {
  /**
   * GUID of the processor, rendered as `{...}`; what `addDsp` and `setChain` take.
   * @zh 处理器的 GUID 标识符（格式形如 `{...}`，作为 `addDsp` 与 `setChain` 的参数）
   */
  guid: string;
  /**
   * Display name of the processor.
   * @zh 效果器处理器的显示名称
   */
  name: string;
  /**
   * Whether the processor has a configuration dialog.
   * @zh 该处理器是否提供专属参数配置对话框
   */
  hasConfig: boolean;
}

interface GetAvailableResult {
  /**
   * Every installed processor, in service enumeration order.
   * @zh 宿主中已安装的所有处理器列表（按服务枚举顺序排列）
   */
  dsps: DspAvailableEntry[];
  /**
   * Number of entries in `dsps`.
   * @zh 已安装的处理器总数
   */
  count: Int;
}

interface AddDspParams {
  /**
   * GUID of an installed processor, as `dsp.getAvailable` reports it.
   * @zh 待添加处理器的 GUID 标识符（取自 `dsp.getAvailable`）
   * @minLength 1
   */
  guid: string;
  /**
   * Position to insert at; any negative position, or one past the end, appends.
   * @zh 插入的目标索引位置；传入 `-1` 或超出链尾的数值均表示追加至末尾
   * @default -1
   */
  position?: Int;
}

interface AddDspResult {
  /**
   * Display name of the processor added.
   * @zh 成功加入的处理器的显示名称
   */
  addedDsp: string;
  /**
   * Position the entry landed at.
   * @zh 插入条目在活动链中的实际位置序号
   */
  position: Int;
}

interface RemoveDspParams {
  /**
   * Position of the entry to remove.
   * @zh 待移除条目在活动链中的索引位置
   * @minimum 0
   */
  index: Int;
}

interface RemoveDspResult {
  /**
   * Display name of the processor removed.
   * @zh 成功移除的处理器的显示名称
   */
  removedDsp: string;
  /**
   * Position it was removed from.
   * @zh 该条目在移除前所处的索引位置
   */
  removedIndex: Int;
}

interface MoveDspParams {
  /**
   * Current position of the entry.
   * @zh 待移动条目的当前索引位置
   * @minimum 0
   */
  from: Int;
  /**
   * Final position in the reordered chain.
   * @zh 条目重排后的目标索引位置
   * @minimum 0
   */
  to: Int;
}

interface MoveDspResult {
  /**
   * Display name of the processor moved; absent when `from` equals `to` and nothing moved.
   * @zh 移动的处理器的显示名称；当 `from === to` 未发生实际位移时缺省
   */
  movedDsp?: string;
  /**
   * The position it came from.
   * @zh 移动操作前的原始索引位置
   */
  from: Int;
  /**
   * The position it is at now.
   * @zh 移动操作完成后的目标索引位置
   */
  to: Int;
  /**
   * `No change needed` when `from` equals `to`.
   * @zh 状态提示文本；当 `from === to` 时固定为 `"No change needed"`
   */
  message?: string;
}

/** One entry of a chain to apply. */
interface DspChainSpec {
  /**
   * GUID of an installed processor, as `dsp.getAvailable` reports it.
   * @zh 待添加处理器的 GUID 标识符（取自 `dsp.getAvailable`）
   * @minLength 1
   */
  guid: string;
}

interface SetChainParams {
  /**
   * The new chain in processing order; empty clears the chain.
   * @zh 替换后的新 DSP 链列表（按音频处理顺序排列，传入空数组代表清空链）
   */
  dsps: DspChainSpec[];
}

interface SetChainResult {
  /**
   * Number of entries in the chain now.
   * @zh 链重置后包含的效果器条目总数
   */
  count: Int;
}

import type { Int } from './common.js';

export interface Api {
  /**
   * Count the discoverable service families in the running process: how many main-menu
   * commands, context-menu commands, groups, input formats, UI elements, DSPs, output backends,
   * preference pages and components are registered. Both menu families are counted through the
   * same walks the listing endpoints use, filtered to what the host would show.
   * @zh 统计当前进程里可发现的各类服务数量：主菜单命令、右键菜单命令、菜单组、输入格式、UI 元素、DSP、输出后端、首选项页与组件。两个菜单族与列举端点走同一次枚举，只计宿主会显示的条目。
   */
  getAllServices(): GetAllServicesResult;

  /**
   * List the main-menu commands, flattened, with their label path, GUID and display state.
   * Submenus that components build at runtime (`mainmenu_commands_v2`) are expanded by default,
   * so their child commands appear next to the parent slot.
   * @zh 列出主菜单命令（扁平），带标签路径、GUID 与显示状态。组件在运行时构建的子菜单（`mainmenu_commands_v2`）默认展开，子命令与父槽位并列。
   */
  getMainMenuCommands(params: GetMainMenuCommandsParams): GetMainMenuCommandsResult;

  /**
   * List the groups main-menu commands are filed under.
   * @zh 列出主菜单命令所属的分组。
   */
  getMainMenuGroups(): GetMainMenuGroupsResult;

  /**
   * Run a main-menu command by GUID. A child of a dynamic submenu is addressed by its parent's
   * `guid` plus its own `subGuid`. Fails with `NOT_FOUND` when no command owns the GUID.
   * @zh 按 GUID 执行主菜单命令。动态子菜单里的子命令用父命令的 `guid` 加自己的 `subGuid` 寻址。没有命令拥有该 GUID 时以 `NOT_FOUND` 失败。
   */
  executeMainMenuCommand(params: ExecuteMainMenuCommandParams): ExecuteMainMenuCommandResult;

  /**
   * List the registered context-menu commands, flattened, with their state for the current
   * target. `enabled` and `checked` are only observable with a track selected or playing; check
   * `stateKnown` before trusting them.
   * @zh 列出已注册的右键菜单命令（扁平），带针对当前目标的状态。`enabled` 与 `checked` 只有在有选中或正在播放的曲目时才可观测，先看 `stateKnown`。
   */
  getContextMenuCommands(params: GetContextMenuCommandsParams): GetContextMenuCommandsResult;

  /**
   * Run a context-menu command by GUID against the playing track, or the active playlist
   * selection when nothing is playing. A command the host would never draw (`FORCE_OFF`) is
   * refused with `NOT_SUPPORTED` unless `force` is set; no target at all is `NO_ACTIVE_ITEM`.
   * @zh 按 GUID 对正在播放的曲目执行右键菜单命令，没有播放时对活动播放列表的选中项执行。宿主永不绘制的命令（`FORCE_OFF`）以 `NOT_SUPPORTED` 拒绝，除非传 `force`；没有目标时是 `NO_ACTIVE_ITEM`。
   */
  executeContextMenuCommand(params: ExecuteContextMenuCommandParams): ExecuteContextMenuCommandResult;

  /**
   * Run a context-menu command addressed by its slash-separated label path, such as
   * `Playback Statistics/Rating/5`. Each segment must match one label exactly after
   * normalization (mnemonic `&`, trailing ellipsis, accelerator text and ASCII case are ignored).
   * A path matching several commands is refused with `INVALID_PARAMS` and the candidates listed;
   * one matching none is `NOT_FOUND`.
   * @zh 按斜杠分隔的标签路径执行右键菜单命令，如 `Playback Statistics/Rating/5`。每一段都要在规范化后（忽略助记符 `&`、末尾省略号、快捷键文本与 ASCII 大小写）恰好匹配一个标签。匹配到多个命令以 `INVALID_PARAMS` 拒绝并列出候选；一个都没有是 `NOT_FOUND`。
   */
  executeContextMenuByPath(params: ExecuteContextMenuByPathParams): ExecuteContextMenuByPathResult;

  /**
   * Dump the context menu for the current target as a nested tree, keeping submenus and
   * separators. Depth and children per node are bounded; any clipping is reported through the
   * `truncated` flags rather than left silent.
   * @zh 把当前目标的右键菜单整棵导出为嵌套树，保留子菜单与分隔符。深度与每个节点的子项数有上限，任何裁剪都通过 `truncated` 系列标记报告，不会静默。
   */
  getContextMenuTree(): GetContextMenuTreeResult;

  /**
   * List the playable input file types, each with a display name and a filename mask.
   * @zh 列出可播放的输入文件类型，每项带显示名与文件名掩码。
   */
  getInputFormats(): GetInputFormatsResult;

  /**
   * List the installed components.
   * @zh 列出已安装的组件。
   */
  getComponents(): GetComponentsResult;

  /**
   * List the registered UI elements.
   * @zh 列出已注册的 UI 元素。
   */
  getUIElements(): GetUIElementsResult;

  /**
   * List the registered DSP entries.
   * @zh 列出已注册的 DSP 条目。
   */
  getDspEntries(): GetDspEntriesResult;

  /**
   * List the registered output backends by GUID.
   * @zh 按 GUID 列出已注册的输出后端。
   */
  getOutputDevices(): GetOutputDevicesResult;

  /**
   * List the pages of the Preferences dialog.
   * @zh 列出首选项对话框的页面。
   */
  getPreferencePages(): GetPreferencePagesResult;

  /**
   * Search command names, descriptions and menu paths across both menu families,
   * case-insensitively (ASCII folding only). Each hit carries the same state fields the listing
   * endpoints return, so a caller can tell whether it is invocable without a second call.
   * @zh 在两个菜单族里按命令名、描述与菜单路径搜索，不区分大小写（只折叠 ASCII）。每条命中都带列举端点那套状态字段，不用再调一次就能判断可否执行。
   */
  searchCommands(params: SearchCommandsParams): SearchCommandsResult;
}

/**
 * Where a menu entry came from.
 * @zh 菜单项的来源。
 */
type MenuNodeSource =
  | 'mainmenu_static'
  | 'mainmenu_dynamic'
  | 'contextmenu_static'
  | 'contextmenu_dynamic'
  | 'hmenu_fallback';

/**
 * Why an entry cannot be executed; empty when it can.
 * @zh 条目不能执行的原因；能执行时为空串。
 */
type MenuUnaddressableReason = '' | 'separator' | 'dynamicParent' | 'noStableIdentifier' | 'emptyNode';

/**
 * Display state of a menu entry, as the host would draw it.
 * @zh 菜单项的显示状态，与宿主绘制的一致。
 */
interface MenuNodeState {
  /**
   * Whether the entry can be clicked. Meaningless while `stateKnown` is false.
   * @zh 是否可点击。`stateKnown` 为 false 时无意义。
   */
  enabled: boolean;
  /**
   * Whether the entry shows a check mark (a radio mark counts too).
   * @zh 是否带勾选标记（单选标记也算）。
   */
  checked: boolean;
  /**
   * Whether the check mark is a radio mark.
   * @zh 勾选标记是否为单选样式。
   */
  radioChecked: boolean;
  /**
   * Whether the host would not draw the entry.
   * @zh 宿主是否不会绘制该条目。
   */
  hidden: boolean;
  /**
   * Whether `enabled` and `checked` were observed. False when the state could not be evaluated,
   * such as a context-menu entry with nothing selected or playing.
   * @zh `enabled` 与 `checked` 是否真正观测到。无法评估时为 false，例如没有选中或播放曲目时的右键菜单项。
   */
  stateKnown: boolean;
  /**
   * The raw display flags the component reported. Main menu: `1` disabled, `2` checked, `4` radio,
   * `8` default-hidden. Context menu: `1` checked, `2` disabled, `4` grayed, `8` radio.
   * @zh 组件报告的原始显示位。主菜单：`1` 禁用、`2` 勾选、`4` 单选、`8` 默认隐藏。右键菜单：`1` 勾选、`2` 禁用、`4` 灰显、`8` 单选。
   */
  flags: Int;
}

/**
 * One main-menu command, static or expanded from a dynamic submenu.
 * @zh 一条主菜单命令，静态的或从动态子菜单展开的。
 */
interface DiscoveryMainMenuCommand extends MenuNodeState {
  /**
   * Label as the host reports it (localized on a translated build).
   * @zh 宿主报告的标签（翻译版本上是本地化文本）。
   */
  name: string;
  /**
   * Description the component provides; empty when it has none.
   * @zh 组件提供的描述；没有时为空。
   */
  description: string;
  /**
   * Command GUID. For an entry expanded from a dynamic submenu this is the owning command's GUID.
   * @zh 命令 GUID。从动态子菜单展开的条目，这是其所属命令的 GUID。
   */
  guid: string;
  /**
   * GUID of the group the command is filed under.
   * @zh 命令所属分组的 GUID。
   */
  parentGuid: string;
  /**
   * Index of the command within its service.
   * @zh 命令在其服务中的序号。
   */
  index: Int;
  /**
   * Slash-separated label path; for a static slot just its label.
   * @zh 斜杠分隔的标签路径；静态槽位就是它的标签。
   */
  path: string;
  /**
   * Whether the entry belongs to a dynamic submenu, as its parent slot or as an expanded child.
   * @zh 是否属于动态子菜单，无论是父槽位还是展开出的子项。
   */
  isDynamic: boolean;
  /**
   * Whether the entry is the parent slot of a dynamic submenu: a container that cannot be executed
   * on its own.
   * @zh 是否为动态子菜单的父槽位：只是容器，本身不能执行。
   */
  isDynamicParent: boolean;
  /**
   * GUID of the child node inside a dynamic submenu; pass it with `guid` to
   * `discovery.executeMainMenuCommand`. Absent on static entries.
   * @zh 动态子菜单里子节点的 GUID；执行时与 `guid` 一起传给 `discovery.executeMainMenuCommand`。静态条目没有。
   */
  subGuid?: string;
  /**
   * Where the entry came from.
   * @zh 条目的来源。
   */
  source: MenuNodeSource;
  /**
   * Whether the entry can be executed.
   * @zh 能否执行。
   */
  executable: boolean;
  /**
   * Why it cannot be executed; empty when it can.
   * @zh 不能执行的原因；能执行时为空串。
   */
  unaddressableReason: MenuUnaddressableReason;
}

/**
 * One group of the main menu.
 * @zh 主菜单的一个分组。
 */
interface DiscoveryMainMenuGroup {
  /**
   * Group GUID.
   * @zh 分组 GUID。
   */
  guid: string;
  /**
   * GUID of the parent group.
   * @zh 上级分组的 GUID。
   */
  parentGuid: string;
  /**
   * Display name; empty for a group that is not a popup.
   * @zh 显示名；不是弹出菜单的分组为空。
   */
  name: string;
  /**
   * Sort priority within the parent.
   * @zh 在上级里的排序优先级。
   */
  sortPriority: Int;
}

/**
 * One playable file type.
 * @zh 一种可播放的文件类型。
 */
interface DiscoveryInputFormatType {
  /**
   * Display name, such as `FLAC`.
   * @zh 显示名，如 `FLAC`。
   */
  name: string;
  /**
   * Filename mask, such as `*.FLAC`.
   * @zh 文件名掩码，如 `*.FLAC`。
   */
  mask: string;
  /**
   * Index within the service that registered it.
   * @zh 在注册它的服务中的序号。
   */
  index: Int;
}

/**
 * One installed component.
 * @zh 一个已安装的组件。
 */
interface DiscoveryComponentInfo {
  /**
   * DLL file name.
   * @zh DLL 文件名。
   */
  filename: string;
  /**
   * Component name.
   * @zh 组件名。
   */
  name: string;
  /**
   * Version string as the component reports it.
   * @zh 组件报告的版本串。
   */
  version: string;
  /**
   * About text.
   * @zh 关于文本。
   */
  about: string;
}

/**
 * One registered UI element.
 * @zh 一个已注册的 UI 元素。
 */
interface DiscoveryUIElementInfo {
  /**
   * Element GUID.
   * @zh 元素 GUID。
   */
  guid: string;
  /**
   * Subclass GUID.
   * @zh 子类 GUID。
   */
  subclassGuid: string;
  /**
   * Display name.
   * @zh 显示名。
   */
  name: string;
  /**
   * Description.
   * @zh 描述。
   */
  description: string;
  /**
   * Whether a user may add it to a layout.
   * @zh 用户能否把它加进布局。
   */
  isUserAddable: boolean;
}

/**
 * One registered DSP.
 * @zh 一个已注册的 DSP。
 */
interface DiscoveryDspEntryInfo {
  /**
   * DSP GUID.
   * @zh DSP 的 GUID。
   */
  guid: string;
  /**
   * Display name.
   * @zh 显示名。
   */
  name: string;
}

/**
 * One registered output backend.
 * @zh 一个已注册的输出后端。
 */
interface DiscoveryOutputDeviceEntry {
  /**
   * Backend GUID.
   * @zh 后端 GUID。
   */
  guid: string;
}

/**
 * One context-menu command.
 * @zh 一条右键菜单命令。
 */
interface DiscoveryContextMenuCommand extends MenuNodeState {
  /**
   * Label as the host reports it.
   * @zh 宿主报告的标签。
   */
  name: string;
  /**
   * Description the component provides; empty when it has none.
   * @zh 组件提供的描述；没有时为空。
   */
  description: string;
  /**
   * Command GUID.
   * @zh 命令 GUID。
   */
  guid: string;
  /**
   * GUID of the parent group; the null GUID for a service without one.
   * @zh 上级分组的 GUID；没有分组的服务为空 GUID。
   */
  parentGuid: string;
  /**
   * Index of the command within its service.
   * @zh 命令在其服务中的序号。
   */
  index: Int;
  /**
   * Where the entry came from.
   * @zh 条目的来源。
   */
  source: MenuNodeSource;
  /**
   * Whether the entry can be executed.
   * @zh 能否执行。
   */
  executable: boolean;
  /**
   * Why it cannot be executed; empty when it can.
   * @zh 不能执行的原因；能执行时为空串。
   */
  unaddressableReason: MenuUnaddressableReason;
}

/**
 * One node of the context-menu tree. State fields are absent on a separator, which has neither
 * state nor identity.
 * @zh 右键菜单树的一个节点。分隔符没有状态也没有身份，状态字段在它上面缺省。
 */
interface DiscoveryContextMenuTreeNode {
  /**
   * Label as the host reports it.
   * @zh 宿主报告的标签。
   */
  name: string;
  /**
   * Node kind.
   * @zh 节点类型。
   */
  type: 'command' | 'popup' | 'separator' | 'unknown';
  /**
   * Depth below the root, which is `0`.
   * @zh 相对根节点的深度，根为 `0`。
   */
  depth: Int;
  /**
   * Whether the entry can be clicked. Absent on a separator.
   * @zh 是否可点击。分隔符没有。
   */
  enabled?: boolean;
  /**
   * Whether the entry shows a check mark. Absent on a separator.
   * @zh 是否带勾选标记。分隔符没有。
   */
  checked?: boolean;
  /**
   * Whether the check mark is a radio mark. Absent on a separator.
   * @zh 勾选标记是否为单选样式。分隔符没有。
   */
  radioChecked?: boolean;
  /**
   * Whether the host would not draw the entry; always false here, because the tree is built from
   * what the host shows. Absent on a separator.
   * @zh 宿主是否不会绘制该条目；这里恒为 false，因为树本来就按宿主显示的内容构建。分隔符没有。
   */
  hidden?: boolean;
  /**
   * Whether the state was observed; always true here. Absent on a separator.
   * @zh 状态是否真正观测到；这里恒为 true。分隔符没有。
   */
  stateKnown?: boolean;
  /**
   * The raw display flags. Absent on a separator.
   * @zh 原始显示位。分隔符没有。
   */
  flags?: Int;
  /**
   * Full slash-separated name; only on a `command` node.
   * @zh 完整的斜杠分隔名；只有 `command` 节点有。
   */
  fullName?: string;
  /**
   * The host's real number of children; only on a `popup` node.
   * @zh 宿主的真实子项数；只有 `popup` 节点有。
   */
  childCount?: Int;
  /**
   * How many children this response holds; only on a `popup` node.
   * @zh 本次响应里包含的子项数；只有 `popup` 节点有。
   */
  childrenReturned?: Int;
  /**
   * The children, in menu order; only on a `popup` node.
   * @zh 子项，按菜单顺序；只有 `popup` 节点有。
   */
  children?: DiscoveryContextMenuTreeNode[];
  /**
   * Whether anything below this node was clipped.
   * @zh 该节点之下是否有内容被裁剪。
   */
  truncated: boolean;
  /**
   * Whether the depth limit clipped something below this node.
   * @zh 该节点之下是否因深度上限被裁剪。
   */
  depthExceeded: boolean;
  /**
   * Whether the per-node children limit clipped something below this node.
   * @zh 该节点之下是否因每节点子项上限被裁剪。
   */
  childrenExceeded: boolean;
}

/**
 * One page of the Preferences dialog.
 * @zh 首选项对话框的一个页面。
 */
interface DiscoveryPreferencePageInfo {
  /**
   * Page GUID.
   * @zh 页面 GUID。
   */
  guid: string;
  /**
   * GUID of the parent page.
   * @zh 上级页面的 GUID。
   */
  parentGuid: string;
  /**
   * Display name.
   * @zh 显示名。
   */
  name: string;
}

/**
 * How many entries each service family holds.
 * @zh 各类服务的数量。
 */
interface DiscoveryServiceCounts {
  /**
   * Main-menu commands the host would show, dynamic children included.
   * @zh 宿主会显示的主菜单命令数，含动态子项。
   */
  mainMenuCommands: Int;
  /**
   * How many of `mainMenuCommands` were expanded from dynamic submenus.
   * @zh `mainMenuCommands` 里有多少是从动态子菜单展开的。
   */
  mainMenuDynamicCommands: Int;
  /**
   * Main-menu groups.
   * @zh 主菜单分组数。
   */
  mainMenuGroups: Int;
  /**
   * Context-menu commands the host would show.
   * @zh 宿主会显示的右键菜单命令数。
   */
  contextMenuCommands: Int;
  /**
   * Playable file types.
   * @zh 可播放文件类型数。
   */
  inputFormats: Int;
  /**
   * Registered UI elements.
   * @zh 已注册的 UI 元素数。
   */
  uiElements: Int;
  /**
   * Registered DSPs.
   * @zh 已注册的 DSP 数。
   */
  dspEntries: Int;
  /**
   * Registered output backends.
   * @zh 已注册的输出后端数。
   */
  outputDevices: Int;
  /**
   * Preference pages.
   * @zh 首选项页数。
   */
  preferencePages: Int;
  /**
   * Installed components.
   * @zh 已安装组件数。
   */
  components: Int;
}

/**
 * One search hit.
 * @zh 一条搜索命中。
 */
interface DiscoverySearchResult extends MenuNodeState {
  /**
   * Label as the host reports it.
   * @zh 宿主报告的标签。
   */
  name: string;
  /**
   * Description; empty when the component has none.
   * @zh 描述；组件没有时为空。
   */
  description: string;
  /**
   * Command GUID; for a dynamic child, the owning command's GUID.
   * @zh 命令 GUID；动态子项是其所属命令的 GUID。
   */
  guid: string;
  /**
   * Slash-separated label path. A context-menu entry is registered flat, so its path is just its
   * label.
   * @zh 斜杠分隔的标签路径。右键菜单项是扁平注册的，路径就是标签。
   */
  path: string;
  /**
   * Whether the hit was expanded from a dynamic submenu.
   * @zh 命中是否从动态子菜单展开而来。
   */
  isDynamic: boolean;
  /**
   * Which menu family the hit belongs to.
   * @zh 命中属于哪个菜单族。
   */
  type: 'mainmenu' | 'contextmenu';
  /**
   * GUID of the dynamic child node; pass it with `guid` to `discovery.executeMainMenuCommand`.
   * @zh 动态子节点的 GUID；执行时与 `guid` 一起传给 `discovery.executeMainMenuCommand`。
   */
  subGuid?: string;
  /**
   * Where the entry came from.
   * @zh 条目的来源。
   */
  source: MenuNodeSource;
  /**
   * Whether the entry can be executed.
   * @zh 能否执行。
   */
  executable: boolean;
  /**
   * Why it cannot be executed; empty when it can.
   * @zh 不能执行的原因；能执行时为空串。
   */
  unaddressableReason: MenuUnaddressableReason;
}

interface GetAllServicesResult {
  /**
   * Count per service family.
   * @zh 各类服务的数量。
   */
  services: DiscoveryServiceCounts;
  /**
   * How many context-menu entries the "would the host show it" filter removed.
   * @zh 「宿主是否会显示」这道过滤去掉了多少右键菜单条目。
   */
  contextMenuHiddenFiltered: Int;
  /**
   * Whether context-menu state was observable; false when nothing was selected or playing.
   * @zh 右键菜单状态是否可观测；没有选中或播放曲目时为 false。
   */
  stateKnown: boolean;
  /**
   * Sum of every family except `mainMenuDynamicCommands`, which `mainMenuCommands` already
   * includes.
   * @zh 各类之和，不含已计入 `mainMenuCommands` 的 `mainMenuDynamicCommands`。
   */
  totalServices: Int;
}

interface GetMainMenuCommandsParams {
  /**
   * Expand submenus that components build at runtime. `false` lists the static registry only.
   * @zh 是否展开组件在运行时构建的子菜单。`false` 只列静态注册表。
   * @default true
   */
  expandDynamic?: boolean;
  /**
   * Also list entries the host would not show.
   * @zh 是否把宿主不会显示的条目也列出。
   * @default false
   */
  includeHidden?: boolean;
}

interface GetMainMenuCommandsResult {
  /**
   * The commands, static slots first within each service, followed by their expanded children.
   * @zh 命令列表，每个服务内先是静态槽位，随后是展开出的子项。
   */
  commands: DiscoveryMainMenuCommand[];
  /**
   * Number of entries in `commands`.
   * @zh `commands` 的条数。
   */
  count: Int;
  /**
   * The `expandDynamic` that applied.
   * @zh 实际生效的 `expandDynamic`。
   */
  expandDynamic: boolean;
  /**
   * The `includeHidden` that applied.
   * @zh 实际生效的 `includeHidden`。
   */
  includeHidden: boolean;
  /**
   * How many entries were expanded from dynamic submenus.
   * @zh 从动态子菜单展开出的条数。
   */
  dynamicCount: Int;
}

interface GetMainMenuGroupsResult {
  /**
   * The groups.
   * @zh 分组列表。
   */
  groups: DiscoveryMainMenuGroup[];
  /**
   * Number of entries in `groups`.
   * @zh `groups` 的条数。
   */
  count: Int;
}

interface ExecuteMainMenuCommandParams {
  /**
   * Command GUID in `{...}` form. For a dynamic child, its parent command's GUID.
   * @zh `{...}` 形式的命令 GUID。动态子项传其父命令的 GUID。
   * @minLength 1
   */
  guid: string;
  /**
   * `subGuid` of a dynamic child, as `discovery.getMainMenuCommands` reports it. Empty means
   * absent.
   * @zh 动态子项的 `subGuid`，取自 `discovery.getMainMenuCommands`。空串视为没传。
   */
  subGuid?: string;
}

interface ExecuteMainMenuCommandResult {
  /**
   * The GUID that was executed.
   * @zh 执行的 GUID。
   */
  guid: string;
  /**
   * The `subGuid` that was executed; only on the dynamic path.
   * @zh 执行的 `subGuid`；只在动态路径上有。
   */
  subGuid?: string;
  /**
   * Whether the dynamic execution path was taken.
   * @zh 是否走了动态执行路径。
   */
  dynamic: boolean;
}

interface GetContextMenuCommandsParams {
  /**
   * Also list entries the host would not show (`FORCE_OFF`, shortcut-list-only).
   * @zh 是否把宿主不会显示的条目（`FORCE_OFF`，只出现在快捷键列表）也列出。
   * @default false
   */
  includeHidden?: boolean;
}

interface GetContextMenuCommandsResult {
  /**
   * The commands.
   * @zh 命令列表。
   */
  commands: DiscoveryContextMenuCommand[];
  /**
   * Number of entries in `commands`.
   * @zh `commands` 的条数。
   */
  count: Int;
  /**
   * The `includeHidden` that applied.
   * @zh 实际生效的 `includeHidden`。
   */
  includeHidden: boolean;
  /**
   * How many entries the filter removed; `0` when `includeHidden` is set.
   * @zh 过滤去掉的条数；`includeHidden` 时为 `0`。
   */
  hiddenFiltered: Int;
  /**
   * Whether `enabled` and `checked` were observed: true only with a track selected or playing.
   * @zh `enabled` 与 `checked` 是否真正观测到：只有有选中或播放曲目时为 true。
   */
  stateKnown: boolean;
  /**
   * How many tracks the state was evaluated against.
   * @zh 状态是针对多少首曲目评估的。
   */
  selectionCount: Int;
}

interface ExecuteContextMenuCommandParams {
  /**
   * Command GUID in `{...}` form, as `discovery.getContextMenuCommands` reports it.
   * @zh `{...}` 形式的命令 GUID，取自 `discovery.getContextMenuCommands`。
   * @minLength 1
   */
  guid: string;
  /**
   * Dispatch even a command the host would never draw.
   * @zh 即使宿主永不绘制该命令也照样派发。
   * @default false
   */
  force?: boolean;
}

interface ExecuteContextMenuCommandResult {
  /**
   * The GUID that was executed.
   * @zh 执行的 GUID。
   */
  guid: string;
  /**
   * Label of the command.
   * @zh 命令的标签。
   */
  name: string;
  /**
   * Whether the host would never draw the command (`FORCE_OFF`).
   * @zh 宿主是否永不绘制该命令（`FORCE_OFF`）。
   */
  hidden: boolean;
  /**
   * Whether a registered item owns the GUID.
   * @zh 是否有注册项拥有该 GUID。
   */
  resolved: boolean;
  /**
   * The `force` that applied.
   * @zh 实际生效的 `force`。
   */
  force: boolean;
  /**
   * How many tracks the command ran against.
   * @zh 命令作用于多少首曲目。
   */
  itemCount: Int;
}

interface ExecuteContextMenuByPathParams {
  /**
   * Slash-separated label path of the command, such as `Playback Statistics/Rating/5`.
   * @zh 斜杠分隔的命令标签路径，如 `Playback Statistics/Rating/5`。
   * @minLength 1
   */
  path: string;
  /**
   * Run against this track instead of the current target. A `|subsong:N` suffix selects a
   * subsong; without one the first subsong of the file is used.
   * @zh 对这首曲目执行，而不是当前目标。带 `|subsong:N` 后缀时选子曲目，不带时指文件的第一首。
   * @security MediaRead
   */
  trackPath?: string;
}

interface ExecuteContextMenuByPathResult {
  /**
   * The path as given.
   * @zh 传入的路径。
   */
  path: string;
  /**
   * Full name of the command that ran.
   * @zh 实际执行的命令的完整名。
   */
  foundName: string;
  /**
   * Always `unique` on success: exactly one command matched.
   * @zh 成功时恒为 `unique`：恰好匹配到一个命令。
   */
  match: 'unique';
  /**
   * Number of commands the path matched; `1` on success.
   * @zh 路径匹配到的命令数；成功时为 `1`。
   */
  candidateCount: Int;
  /**
   * How many tracks the command ran against.
   * @zh 命令作用于多少首曲目。
   */
  itemCount: Int;
}

interface GetContextMenuTreeResult {
  /**
   * The root node.
   * @zh 根节点。
   */
  tree: DiscoveryContextMenuTreeNode;
  /**
   * Whether anything in the tree was clipped.
   * @zh 树里是否有内容被裁剪。
   */
  truncated: boolean;
  /**
   * Whether the depth limit clipped something.
   * @zh 是否因深度上限裁剪。
   */
  depthExceeded: boolean;
  /**
   * Whether the per-node children limit clipped something.
   * @zh 是否因每节点子项上限裁剪。
   */
  childrenExceeded: boolean;
  /**
   * The depth limit that applied.
   * @zh 生效的深度上限。
   */
  maxDepth: Int;
  /**
   * The per-node children limit that applied.
   * @zh 生效的每节点子项上限。
   */
  maxChildrenPerNode: Int;
  /**
   * How many tracks the menu was built for.
   * @zh 菜单是为多少首曲目构建的。
   */
  itemCount: Int;
}

interface GetInputFormatsResult {
  /**
   * The file types.
   * @zh 文件类型列表。
   */
  fileTypes: DiscoveryInputFormatType[];
  /**
   * Number of entries in `fileTypes`.
   * @zh `fileTypes` 的条数。
   */
  count: Int;
}

interface GetComponentsResult {
  /**
   * The components.
   * @zh 组件列表。
   */
  components: DiscoveryComponentInfo[];
  /**
   * Number of entries in `components`.
   * @zh `components` 的条数。
   */
  count: Int;
}

interface GetUIElementsResult {
  /**
   * The elements.
   * @zh 元素列表。
   */
  elements: DiscoveryUIElementInfo[];
  /**
   * Number of entries in `elements`.
   * @zh `elements` 的条数。
   */
  count: Int;
}

interface GetDspEntriesResult {
  /**
   * The DSPs.
   * @zh DSP 列表。
   */
  entries: DiscoveryDspEntryInfo[];
  /**
   * Number of entries in `entries`.
   * @zh `entries` 的条数。
   */
  count: Int;
}

interface GetOutputDevicesResult {
  /**
   * The backends.
   * @zh 后端列表。
   */
  devices: DiscoveryOutputDeviceEntry[];
  /**
   * Number of entries in `devices`.
   * @zh `devices` 的条数。
   */
  count: Int;
}

interface GetPreferencePagesResult {
  /**
   * The pages.
   * @zh 页面列表。
   */
  pages: DiscoveryPreferencePageInfo[];
  /**
   * Number of entries in `pages`.
   * @zh `pages` 的条数。
   */
  count: Int;
}

interface SearchCommandsParams {
  /**
   * Text to look for in names, descriptions and paths; ASCII case is ignored.
   * @zh 在名称、描述与路径里查找的文本；忽略 ASCII 大小写。
   * @minLength 1
   */
  query: string;
  /**
   * Also search commands expanded from dynamic submenus.
   * @zh 是否连从动态子菜单展开的命令一起搜索。
   * @default true
   */
  expandDynamic?: boolean;
  /**
   * Which menu family to search.
   * @zh 搜索哪个菜单族。
   * @default "all"
   */
  scope?: 'all' | 'mainmenu' | 'contextmenu';
  /**
   * Also search entries the host would not show.
   * @zh 是否连宿主不会显示的条目一起搜索。
   * @default false
   */
  includeHidden?: boolean;
}

interface SearchCommandsResult {
  /**
   * The query as given.
   * @zh 传入的查询。
   */
  query: string;
  /**
   * The hits, main menu first.
   * @zh 命中列表，主菜单在前。
   */
  results: DiscoverySearchResult[];
  /**
   * Number of entries in `results`.
   * @zh `results` 的条数。
   */
  count: Int;
  /**
   * The `expandDynamic` that applied.
   * @zh 实际生效的 `expandDynamic`。
   */
  expandDynamic: boolean;
  /**
   * The `scope` that applied.
   * @zh 实际生效的 `scope`。
   */
  scope: 'all' | 'mainmenu' | 'contextmenu';
  /**
   * The `includeHidden` that applied.
   * @zh 实际生效的 `includeHidden`。
   */
  includeHidden: boolean;
  /**
   * Hits from the main menu.
   * @zh 来自主菜单的命中数。
   */
  mainMenuHits: Int;
  /**
   * Hits from the context menu.
   * @zh 来自右键菜单的命中数。
   */
  contextMenuHits: Int;
  /**
   * Whether context-menu state was observed. False when the context family was searched with
   * nothing selected or playing; the hits' `enabled` and `checked` must not be filtered on then.
   * @zh 右键菜单状态是否真正观测到。没有选中或播放曲目时搜右键菜单为 false，此时命中的 `enabled` 与 `checked` 不能用来过滤。
   */
  stateKnown: boolean;
}

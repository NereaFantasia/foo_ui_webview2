import type { Int } from './common.js';

export interface Api {
  /**
   * Count the discoverable service families in the running process: how many main-menu
   * commands, context-menu commands, groups, input formats, UI elements, DSPs, output backends,
   * preference pages and components are registered. Both menu families are counted through the
   * same walks the listing endpoints use, filtered to what the host would show.
   * @zh 统计当前进程中可发现的各类服务数量（涵盖主菜单命令、右键菜单命令、菜单组、输入格式、UI 元素、DSP、输出后端、首选项页与组件；菜单统计与列举端点复用同一枚举遍历，仅计入宿主界面实际显示的条目）
   */
  getAllServices(): GetAllServicesResult;

  /**
   * List the main-menu commands, flattened, with their label path, GUID and display state.
   * Submenus that components build at runtime (`mainmenu_commands_v2`) are expanded by default,
   * so their child commands appear next to the parent slot.
   * @zh 扁平列出已注册的主菜单命令（附带标签路径、GUID 与界面显示状态；默认展开组件在运行时动态构建的子菜单 mainmenu_commands_v2，子命令与父级槽位并列呈现）
   */
  getMainMenuCommands(params: GetMainMenuCommandsParams): GetMainMenuCommandsResult;

  /**
   * List the groups main-menu commands are filed under.
   * @zh 列出主菜单命令所属的分组列表
   */
  getMainMenuGroups(): GetMainMenuGroupsResult;

  /**
   * Run a main-menu command by GUID. A child of a dynamic submenu is addressed by its parent's
   * `guid` plus its own `subGuid`. The address must identify one current leaf; dynamic containers
   * cannot be executed. Missing addresses fail with `NOT_FOUND`, ambiguous ones with
   * `MENU_MATCH_AMBIGUOUS`, disabled leaves with `MENU_ITEM_DISABLED`, and unreadable state with
   * `OPERATION_FAILED`.
   * @zh 按 GUID 执行主菜单命令（动态子项须同时提供父命令 guid 与自身 subGuid，且必须唯一定位至具体叶子项；动态容器不可执行；命令不存在返回 NOT_FOUND，定位产生歧义返回 MENU_MATCH_AMBIGUOUS，菜单项禁用返回 MENU_ITEM_DISABLED，状态不可读返回 OPERATION_FAILED）
   */
  executeMainMenuCommand(params: ExecuteMainMenuCommandParams): ExecuteMainMenuCommandResult;

  /**
   * List the registered context-menu commands, flattened, with their state for the current
   * target. `enabled` and `checked` are only observable with a track selected or playing; check
   * `stateKnown` before trusting them.
   * @zh 扁平列出已注册的右键菜单命令及针对当前目标的状态（enabled 与 checked 仅在存在选中或正在播放的曲目时可观测，使用前须核对 stateKnown）
   */
  getContextMenuCommands(params: GetContextMenuCommandsParams): GetContextMenuCommandsResult;

  /**
   * Run a context-menu command by GUID against the playing track, or the active playlist
   * selection when nothing is playing. A command the host would never draw (`FORCE_OFF`) is
   * refused with `NOT_SUPPORTED` unless `force` is set; no target at all is `NO_ACTIVE_ITEM`.
   * @zh 按 GUID 对当前目标曲目执行右键菜单命令（优先作用于正在播放的曲目，未播放时作用于活动播放列表的选中项；宿主永不绘制的命令 FORCE_OFF 默认以 NOT_SUPPORTED 拒绝，除非显式指定 force 为 true；无可用目标曲目时返回 NO_ACTIVE_ITEM）
   */
  executeContextMenuCommand(params: ExecuteContextMenuCommandParams): ExecuteContextMenuCommandResult;

  /**
   * Run a context-menu command addressed by its slash-separated label path, such as
   * `Playback Statistics/Rating/5`. Each segment must match one label exactly after
   * normalization (mnemonic `&`, trailing ellipsis, accelerator text and ASCII case are ignored).
   * A path matching several commands is refused with `INVALID_PARAMS` and the candidates listed;
   * one matching none is `NOT_FOUND`.
   * @zh 按斜杠分隔的标签路径执行右键菜单命令（如 Playback Statistics/Rating/5；各级路径在规范化后须唯一匹配一个菜单标签，忽略助记符 &、末尾省略号、快捷键文本与 ASCII 大小写；命中多个候选时返回 INVALID_PARAMS 并列出歧义候选，未命中任何命令返回 NOT_FOUND）
   */
  executeContextMenuByPath(params: ExecuteContextMenuByPathParams): ExecuteContextMenuByPathResult;

  /**
   * Dump the context menu for the current target as a nested tree, keeping submenus and
   * separators. Depth and children per node are bounded; any clipping is reported through the
   * `truncated` flags rather than left silent.
   * @zh 导出当前目标曲目的完整右键菜单嵌套树（保留各级子菜单与分隔符；树深度与单节点子项数设有限额，任何裁剪均通过 truncated 状态显式标注）
   */
  getContextMenuTree(): GetContextMenuTreeResult;

  /**
   * List the playable input file types, each with a display name and a filename mask.
   * @zh 列出可播放的输入文件格式列表（包含显示名称与文件名掩码）
   */
  getInputFormats(): GetInputFormatsResult;

  /**
   * List the installed components.
   * @zh 列出当前安装的所有组件信息
   */
  getComponents(): GetComponentsResult;

  /**
   * List the registered UI elements.
   * @zh 列出已注册的 UI 元素列表
   */
  getUIElements(): GetUIElementsResult;

  /**
   * List the registered DSP entries.
   * @zh 列出已注册的 DSP 条目列表
   */
  getDspEntries(): GetDspEntriesResult;

  /**
   * List the registered output backends by GUID.
   * @zh 列出已注册的音频输出后端设备列表
   */
  getOutputDevices(): GetOutputDevicesResult;

  /**
   * List the pages of the Preferences dialog.
   * @zh 列出首选项设置对话框的页面层级列表
   */
  getPreferencePages(): GetPreferencePagesResult;

  /**
   * Search command names, descriptions and menu paths across both menu families,
   * case-insensitively (ASCII folding only). Each hit carries the same state fields the listing
   * endpoints return, so a caller can tell whether it is invocable without a second call.
   * @zh 跨主菜单与右键菜单搜索命令名称、描述与路径（不区分 ASCII 大小写；搜索命中直接携带完整菜单状态字段，无需二次调用即可判断可执行性）
   */
  searchCommands(params: SearchCommandsParams): SearchCommandsResult;
}

/**
 * Where a menu entry came from.
 * @zh 菜单项的来源类型
 */
type MenuNodeSource =
  | 'mainmenu_static'
  | 'mainmenu_dynamic'
  | 'contextmenu_static'
  | 'contextmenu_dynamic'
  | 'hmenu_fallback';

/**
 * Why an entry cannot be executed; empty when it can.
 * @zh 菜单项不可执行的具体原因（可执行时为空字符串）
 */
type MenuUnaddressableReason = '' | 'separator' | 'dynamicParent' | 'noStableIdentifier' | 'emptyNode';

/**
 * Display state of a menu entry, as the host would draw it.
 * @zh 菜单项的界面显示状态（与宿主绘制状态保持一致）
 */
interface MenuNodeState {
  /**
   * Whether the entry can be clicked. Meaningless while `stateKnown` is false.
   * @zh 菜单项是否可点击（stateKnown 为 false 时无参考意义）
   */
  enabled: boolean;
  /**
   * Whether the entry shows a check mark (a radio mark counts too).
   * @zh 是否显示勾选标记（含单选勾选标记）
   */
  checked: boolean;
  /**
   * Whether the check mark is a radio mark.
   * @zh 勾选标记是否为单选圆点样式
   */
  radioChecked: boolean;
  /**
   * Whether the host would not draw the entry.
   * @zh 宿主界面是否不绘制该菜单项
   */
  hidden: boolean;
  /**
   * Whether `enabled` and `checked` were observed. False when the state could not be evaluated,
   * such as a context-menu entry with nothing selected or playing.
   * @zh enabled 与 checked 状态是否已成功观测（无选中曲目或未播放时评估失败返回 false）
   */
  stateKnown: boolean;
  /**
   * The raw display flags the component reported. Main menu: `1` disabled, `2` checked, `4` radio,
   * `8` default-hidden. Context menu: `1` checked, `2` disabled, `4` grayed, `8` radio.
   * @zh 组件报告的原始显示标志位（主菜单：1 禁用、2 勾选、4 单选、8 默认隐藏；右键菜单：1 勾选、2 禁用、4 灰显、8 单选）
   */
  flags: Int;
}

/**
 * One main-menu command, static or expanded from a dynamic submenu.
 * @zh 主菜单命令（支持静态注册项与动态子菜单展开项）
 */
interface DiscoveryMainMenuCommand extends MenuNodeState {
  /**
   * Label as the host reports it (localized on a translated build).
   * @zh 宿主报告的命令标签（在汉化/多语言版本中为本地化文本）
   */
  name: string;
  /**
   * Description the component provides; empty when it has none.
   * @zh 组件提供的命令描述（无描述时为空字符串）
   */
  description: string;
  /**
   * Command GUID. For an entry expanded from a dynamic submenu this is the owning command's GUID.
   * @zh 命令 GUID（对于动态子菜单展开项，此项为所属父命令 GUID）
   */
  guid: string;
  /**
   * GUID of the group the command is filed under.
   * @zh 命令所属分组 GUID
   */
  parentGuid: string;
  /**
   * Index of the command within its service.
   * @zh 命令在其所属服务中的索引序号
   */
  index: Int;
  /**
   * Slash-separated label path; for a static slot just its label.
   * @zh 斜杠分隔的标签路径（顶级静态槽位直接为命令标签）
   */
  path: string;
  /**
   * Whether the entry belongs to a dynamic submenu, as its parent slot or as an expanded child.
   * @zh 是否属于动态子菜单（包含父级槽位与展开子项）
   */
  isDynamic: boolean;
  /**
   * Whether the entry is the parent slot of a dynamic submenu: a container that cannot be executed
   * on its own.
   * @zh 是否为动态子菜单的父级槽位（仅作为菜单容器，本身不可执行）
   */
  isDynamicParent: boolean;
  /**
   * GUID of the child node inside a dynamic submenu; pass it with `guid` to
   * `discovery.executeMainMenuCommand`. Absent on static entries.
   * @zh 动态子菜单项的专属 GUID（执行动态子命令时与 guid 一同传入；静态条目缺省）
   */
  subGuid?: string;
  /**
   * Where the entry came from.
   * @zh 菜单项的来源分类
   */
  source: MenuNodeSource;
  /**
   * Whether the entry can be executed.
   * @zh 菜单项是否可被调用执行
   */
  executable: boolean;
  /**
   * Why it cannot be executed; empty when it can.
   * @zh 菜单项不可执行的具体原因（可执行时为空字符串）
   */
  unaddressableReason: MenuUnaddressableReason;
}

/**
 * One group of the main menu.
 * @zh 主菜单分组信息
 */
interface DiscoveryMainMenuGroup {
  /**
   * Group GUID.
   * @zh 分组 GUID
   */
  guid: string;
  /**
   * GUID of the parent group.
   * @zh 上级分组 GUID
   */
  parentGuid: string;
  /**
   * Display name; empty for a group that is not a popup.
   * @zh 分组显示名称（非弹出菜单分组为空字符串）
   */
  name: string;
  /**
   * Sort priority within the parent.
   * @zh 在上级分组内的排序优先级权重
   */
  sortPriority: Int;
}

/**
 * One playable file type.
 * @zh 可播放音频输入格式
 */
interface DiscoveryInputFormatType {
  /**
   * Display name, such as `FLAC`.
   * @zh 格式显示名称（例如 FLAC）
   */
  name: string;
  /**
   * Filename mask, such as `*.FLAC`.
   * @zh 文件名匹配掩码（例如 *.FLAC）
   */
  mask: string;
  /**
   * Index within the service that registered it.
   * @zh 在注册服务内部的格式序号
   */
  index: Int;
}

/**
 * One installed component.
 * @zh 已安装的组件信息
 */
interface DiscoveryComponentInfo {
  /**
   * DLL file name.
   * @zh 组件 DLL 文件名
   */
  filename: string;
  /**
   * Component name.
   * @zh 组件名称
   */
  name: string;
  /**
   * Version string as the component reports it.
   * @zh 组件报告的版本字符串
   */
  version: string;
  /**
   * About text.
   * @zh 组件关于说明文本
   */
  about: string;
}

/**
 * One registered UI element.
 * @zh 已注册的 UI 元素信息
 */
interface DiscoveryUIElementInfo {
  /**
   * Element GUID.
   * @zh 元素 GUID
   */
  guid: string;
  /**
   * Subclass GUID.
   * @zh 元素子类 GUID
   */
  subclassGuid: string;
  /**
   * Display name.
   * @zh 元素显示名称
   */
  name: string;
  /**
   * Description.
   * @zh 元素功能描述
   */
  description: string;
  /**
   * Whether a user may add it to a layout.
   * @zh 用户是否可将该元素添加至界面布局
   */
  isUserAddable: boolean;
}

/**
 * One registered DSP.
 * @zh 已注册的 DSP 条目信息
 */
interface DiscoveryDspEntryInfo {
  /**
   * DSP GUID.
   * @zh DSP 唯一标识 GUID
   */
  guid: string;
  /**
   * Display name.
   * @zh DSP 显示名称
   */
  name: string;
}

/**
 * One registered output backend.
 * @zh 已注册的音频输出后端设备
 */
interface DiscoveryOutputDeviceEntry {
  /**
   * Backend GUID.
   * @zh 输出后端 GUID
   */
  guid: string;
}

/**
 * One context-menu command.
 * @zh 右键上下文菜单命令
 */
interface DiscoveryContextMenuCommand extends MenuNodeState {
  /**
   * Label as the host reports it.
   * @zh 宿主报告的命令标签
   */
  name: string;
  /**
   * Description the component provides; empty when it has none.
   * @zh 组件提供的命令描述（无描述时为空字符串）
   */
  description: string;
  /**
   * Command GUID.
   * @zh 命令 GUID
   */
  guid: string;
  /**
   * GUID of the parent group; the null GUID for a service without one.
   * @zh 上级分组 GUID（无所属分组时为空 GUID）
   */
  parentGuid: string;
  /**
   * Index of the command within its service.
   * @zh 命令在其所属服务中的索引序号
   */
  index: Int;
  /**
   * Where the entry came from.
   * @zh 菜单项的来源分类
   */
  source: MenuNodeSource;
  /**
   * Whether the entry can be executed.
   * @zh 菜单项是否可被调用执行
   */
  executable: boolean;
  /**
   * Why it cannot be executed; empty when it can.
   * @zh 菜单项不可执行的具体原因（可执行时为空字符串）
   */
  unaddressableReason: MenuUnaddressableReason;
}

/**
 * One node of the context-menu tree. State fields are absent on a separator, which has neither
 * state nor identity.
 * @zh 右键菜单树节点（分隔符节点无状态与标识，其状态字段均缺省）
 */
interface DiscoveryContextMenuTreeNode {
  /**
   * Label as the host reports it.
   * @zh 宿主报告的节点标签
   */
  name: string;
  /**
   * Node kind.
   * @zh 菜单节点类型
   */
  type: 'command' | 'popup' | 'separator' | 'unknown';
  /**
   * Depth below the root, which is `0`.
   * @zh 相对根节点的嵌套深度（根节点为 0）
   */
  depth: Int;
  /**
   * Whether the entry can be clicked. Absent on a separator.
   * @zh 菜单项是否可点击（分隔符节点缺省）
   */
  enabled?: boolean;
  /**
   * Whether the entry shows a check mark. Absent on a separator.
   * @zh 是否显示勾选标记（分隔符节点缺省）
   */
  checked?: boolean;
  /**
   * Whether the check mark is a radio mark. Absent on a separator.
   * @zh 勾选标记是否为单选样式（分隔符节点缺省）
   */
  radioChecked?: boolean;
  /**
   * Whether the host would not draw the entry; always false here, because the tree is built from
   * what the host shows. Absent on a separator.
   * @zh 宿主界面是否不绘制该项（从宿主实际显示构建时恒为 false；分隔符节点缺省）
   */
  hidden?: boolean;
  /**
   * Whether the state was observed; always true here. Absent on a separator.
   * @zh 状态是否已成功观测（树节点恒为 true；分隔符节点缺省）
   */
  stateKnown?: boolean;
  /**
   * The raw display flags. Absent on a separator.
   * @zh 组件原始显示标志位（分隔符节点缺省）
   */
  flags?: Int;
  /**
   * Full slash-separated name; only on a `command` node.
   * @zh 斜杠分隔的完整命令路径（仅 command 节点提供）
   */
  fullName?: string;
  /**
   * The host's real number of children; only on a `popup` node.
   * @zh 宿主中的实际子菜单项总数（仅 popup 节点提供）
   */
  childCount?: Int;
  /**
   * How many children this response holds; only on a `popup` node.
   * @zh 当前响应实际返回的子项数量（仅 popup 节点提供）
   */
  childrenReturned?: Int;
  /**
   * The children, in menu order; only on a `popup` node.
   * @zh 按菜单顺序排列的子项列表（仅 popup 节点提供）
   */
  children?: DiscoveryContextMenuTreeNode[];
  /**
   * Whether anything below this node was clipped.
   * @zh 该节点下属内容是否发生截断裁剪
   */
  truncated: boolean;
  /**
   * Whether the depth limit clipped something below this node.
   * @zh 该节点下属内容是否因超出深度限额而被截断
   */
  depthExceeded: boolean;
  /**
   * Whether the per-node children limit clipped something below this node.
   * @zh 该节点下属内容是否因超出单节点子项限额而被截断
   */
  childrenExceeded: boolean;
}

/**
 * One page of the Preferences dialog.
 * @zh 首选项设置页面信息
 */
interface DiscoveryPreferencePageInfo {
  /**
   * Page GUID.
   * @zh 页面唯一标识 GUID
   */
  guid: string;
  /**
   * GUID of the parent page.
   * @zh 上级父页面 GUID
   */
  parentGuid: string;
  /**
   * Display name.
   * @zh 页面显示名称
   */
  name: string;
}

/**
 * How many entries each service family holds.
 * @zh 各类服务与命令的统计数量
 */
interface DiscoveryServiceCounts {
  /**
   * Main-menu commands the host would show, dynamic children included.
   * @zh 宿主界面会显示的主菜单命令数（含动态子项）
   */
  mainMenuCommands: Int;
  /**
   * How many of `mainMenuCommands` were expanded from dynamic submenus.
   * @zh 主菜单命令中自动态子菜单展开的子项数量
   */
  mainMenuDynamicCommands: Int;
  /**
   * Main-menu groups.
   * @zh 主菜单分组总数
   */
  mainMenuGroups: Int;
  /**
   * Context-menu commands the host would show.
   * @zh 宿主界面会显示的右键菜单命令总数
   */
  contextMenuCommands: Int;
  /**
   * Playable file types.
   * @zh 可播放音频输入格式总数
   */
  inputFormats: Int;
  /**
   * Registered UI elements.
   * @zh 已注册的 UI 元素总数
   */
  uiElements: Int;
  /**
   * Registered DSPs.
   * @zh 已注册的 DSP 条目总数
   */
  dspEntries: Int;
  /**
   * Registered output backends.
   * @zh 已注册的音频输出后端总数
   */
  outputDevices: Int;
  /**
   * Preference pages.
   * @zh 首选项设置页面总数
   */
  preferencePages: Int;
  /**
   * Installed components.
   * @zh 已安装组件总数
   */
  components: Int;
}

/**
 * One search hit.
 * @zh 命令搜索命中项
 */
interface DiscoverySearchResult extends MenuNodeState {
  /**
   * Label as the host reports it.
   * @zh 宿主报告的命令标签
   */
  name: string;
  /**
   * Description; empty when the component has none.
   * @zh 命令功能描述（无描述时为空字符串）
   */
  description: string;
  /**
   * Command GUID; for a dynamic child, the owning command's GUID.
   * @zh 命令 GUID（动态子项为其所属父命令 GUID）
   */
  guid: string;
  /**
   * Slash-separated label path. A context-menu entry is registered flat, so its path is just its
   * label.
   * @zh 斜杠分隔的标签路径（右键菜单项为扁平注册，路径即为自身标签）
   */
  path: string;
  /**
   * Whether the hit was expanded from a dynamic submenu.
   * @zh 搜索命中项是否自动态子菜单展开
   */
  isDynamic: boolean;
  /**
   * Which menu family the hit belongs to.
   * @zh 命令所属菜单族（mainmenu 或 contextmenu）
   */
  type: 'mainmenu' | 'contextmenu';
  /**
   * GUID of the dynamic child node; pass it with `guid` to `discovery.executeMainMenuCommand`.
   * @zh 动态子节点 GUID（执行动态主菜单命令时与 guid 一同传入）
   */
  subGuid?: string;
  /**
   * Where the entry came from.
   * @zh 菜单项的来源分类
   */
  source: MenuNodeSource;
  /**
   * Whether the entry can be executed.
   * @zh 菜单项是否可被调用执行
   */
  executable: boolean;
  /**
   * Why it cannot be executed; empty when it can.
   * @zh 菜单项不可执行的具体原因（可执行时为空字符串）
   */
  unaddressableReason: MenuUnaddressableReason;
}

interface GetAllServicesResult {
  /**
   * Count per service family.
   * @zh 各类服务的统计数量
   */
  services: DiscoveryServiceCounts;
  /**
   * How many context-menu entries the "would the host show it" filter removed.
   * @zh 因宿主不显示而被过滤排除的右键菜单项总数
   */
  contextMenuHiddenFiltered: Int;
  /**
   * Whether context-menu state was observable; false when nothing was selected or playing.
   * @zh 右键菜单状态是否可观测（无选中或未播放曲目时为 false）
   */
  stateKnown: boolean;
  /**
   * Sum of every family except `mainMenuDynamicCommands`, which `mainMenuCommands` already
   * includes.
   * @zh 各类服务总量（不重复计算已计入 mainMenuCommands 的动态子项）
   */
  totalServices: Int;
}

interface GetMainMenuCommandsParams {
  /**
   * Expand submenus that components build at runtime. `false` lists the static registry only.
   * @zh 是否展开组件在运行时动态构建的子菜单（传入 false 仅列出静态注册表）
   * @default true
   */
  expandDynamic?: boolean;
  /**
   * Also list entries the host would not show.
   * @zh 是否包含宿主界面通常不显示的隐藏菜单项
   * @default false
   */
  includeHidden?: boolean;
}

interface GetMainMenuCommandsResult {
  /**
   * The commands, static slots first within each service, followed by their expanded children.
   * @zh 主菜单命令列表（每个服务内优先排列静态槽位，后随动态展开子项）
   */
  commands: DiscoveryMainMenuCommand[];
  /**
   * Number of entries in `commands`.
   * @zh commands 列表条目总数
   */
  count: Int;
  /**
   * The `expandDynamic` that applied.
   * @zh 实际生效的 expandDynamic 参数值
   */
  expandDynamic: boolean;
  /**
   * The `includeHidden` that applied.
   * @zh 实际生效的 includeHidden 参数值
   */
  includeHidden: boolean;
  /**
   * How many entries were expanded from dynamic submenus.
   * @zh 从动态子菜单中展开出的条目数量
   */
  dynamicCount: Int;
}

interface GetMainMenuGroupsResult {
  /**
   * The groups.
   * @zh 主菜单分组列表
   */
  groups: DiscoveryMainMenuGroup[];
  /**
   * Number of entries in `groups`.
   * @zh groups 列表分组总数
   */
  count: Int;
}

interface ExecuteMainMenuCommandParams {
  /**
   * Command GUID in `{...}` form. For a dynamic child, its parent command's GUID.
   * @zh {...} 格式的命令 GUID（动态子项传入其所属父命令 GUID）
   * @minLength 1
   */
  guid: string;
  /**
   * `subGuid` of a dynamic child, as `discovery.getMainMenuCommands` reports it. Empty means
   * absent.
   * @zh 动态子项专属 subGuid（取自 getMainMenuCommands；空字符串视为未指定）
   */
  subGuid?: string;
}

interface ExecuteMainMenuCommandResult {
  /**
   * The GUID that was executed.
   * @zh 执行的目标命令 GUID
   */
  guid: string;
  /**
   * The `subGuid` that was executed; only on the dynamic path.
   * @zh 执行的动态子项 subGuid（仅在动态执行路径返回）
   */
  subGuid?: string;
  /**
   * Whether the dynamic execution path was taken.
   * @zh 是否通过动态子菜单路径执行
   */
  dynamic: boolean;
}

interface GetContextMenuCommandsParams {
  /**
   * Also list entries the host would not show (`FORCE_OFF`, shortcut-list-only).
   * @zh 是否包含宿主界面通常不绘制的隐藏菜单项（FORCE_OFF，仅出现在快捷键配置列表）
   * @default false
   */
  includeHidden?: boolean;
}

interface GetContextMenuCommandsResult {
  /**
   * The commands.
   * @zh 右键菜单命令列表
   */
  commands: DiscoveryContextMenuCommand[];
  /**
   * Number of entries in `commands`.
   * @zh commands 列表命令总数
   */
  count: Int;
  /**
   * The `includeHidden` that applied.
   * @zh 实际生效的 includeHidden 参数值
   */
  includeHidden: boolean;
  /**
   * How many entries the filter removed; `0` when `includeHidden` is set.
   * @zh 因宿主不显示而被过滤排除的条目数（includeHidden 为 true 时为 0）
   */
  hiddenFiltered: Int;
  /**
   * Whether `enabled` and `checked` were observed: true only with a track selected or playing.
   * @zh enabled 与 checked 状态是否已成功观测（仅在有选中或正在播放曲目时为 true）
   */
  stateKnown: boolean;
  /**
   * How many tracks the state was evaluated against.
   * @zh 评估菜单状态所针对的目标曲目数量
   */
  selectionCount: Int;
}

interface ExecuteContextMenuCommandParams {
  /**
   * Command GUID in `{...}` form, as `discovery.getContextMenuCommands` reports it.
   * @zh {...} 格式的命令 GUID（取自 discovery.getContextMenuCommands）
   * @minLength 1
   */
  guid: string;
  /**
   * Dispatch even a command the host would never draw.
   * @zh 是否强制派发宿主永不绘制的隐藏命令（FORCE_OFF）
   * @default false
   */
  force?: boolean;
}

interface ExecuteContextMenuCommandResult {
  /**
   * The GUID that was executed.
   * @zh 执行的目标命令 GUID
   */
  guid: string;
  /**
   * Label of the command.
   * @zh 执行的命令标签
   */
  name: string;
  /**
   * Whether the host would never draw the command (`FORCE_OFF`).
   * @zh 宿主界面是否永不绘制该命令（FORCE_OFF）
   */
  hidden: boolean;
  /**
   * Whether a registered item owns the GUID.
   * @zh 是否成功匹配到拥有该 GUID 的注册命令
   */
  resolved: boolean;
  /**
   * The `force` that applied.
   * @zh 实际生效的 force 参数值
   */
  force: boolean;
  /**
   * How many tracks the command ran against.
   * @zh 该命令实际作用的目标曲目数量
   */
  itemCount: Int;
}

interface ExecuteContextMenuByPathParams {
  /**
   * Slash-separated label path of the command, such as `Playback Statistics/Rating/5`.
   * @zh 斜杠分隔的命令标签路径（例如 Playback Statistics/Rating/5）
   * @minLength 1
   */
  path: string;
  /**
   * Run against this track instead of the current target. A `|subsong:N` suffix selects a
   * subsong; without one the first subsong of the file is used.
   * @zh 显式指定执行的目标曲目路径（省略时取当前播放或选中的曲目；支持 |subsong:N 子曲目后缀，缺省取首个子曲目）
   * @security MediaRead
   */
  trackPath?: string;
}

interface ExecuteContextMenuByPathResult {
  /**
   * The path as given.
   * @zh 调用的输入命令路径
   */
  path: string;
  /**
   * Full name of the command that ran.
   * @zh 实际匹配并执行的完整命令路径名称
   */
  foundName: string;
  /**
   * Always `unique` on success: exactly one command matched.
   * @zh 路径匹配模式（成功执行时恒为 unique，表示唯一精确匹配）
   */
  match: 'unique';
  /**
   * Number of commands the path matched; `1` on success.
   * @zh 路径匹配命中的候选命令数量（成功时恒为 1）
   */
  candidateCount: Int;
  /**
   * How many tracks the command ran against.
   * @zh 该命令实际作用的目标曲目数量
   */
  itemCount: Int;
}

interface GetContextMenuTreeResult {
  /**
   * The root node.
   * @zh 右键菜单树的根节点
   */
  tree: DiscoveryContextMenuTreeNode;
  /**
   * Whether anything in the tree was clipped.
   * @zh 菜单树是否存在截断裁剪
   */
  truncated: boolean;
  /**
   * Whether the depth limit clipped something.
   * @zh 是否因超出最大树深度限额而被截断
   */
  depthExceeded: boolean;
  /**
   * Whether the per-node children limit clipped something.
   * @zh 是否因超出单节点子项限额而被截断
   */
  childrenExceeded: boolean;
  /**
   * The depth limit that applied.
   * @zh 实际生效的最大树深度限额
   */
  maxDepth: Int;
  /**
   * The per-node children limit that applied.
   * @zh 实际生效的单节点子项数量限额
   */
  maxChildrenPerNode: Int;
  /**
   * How many tracks the menu was built for.
   * @zh 构建菜单树所针对的目标曲目数量
   */
  itemCount: Int;
}

interface GetInputFormatsResult {
  /**
   * The file types.
   * @zh 可播放音频输入格式列表
   */
  fileTypes: DiscoveryInputFormatType[];
  /**
   * Number of entries in `fileTypes`.
   * @zh fileTypes 列表条目总数
   */
  count: Int;
}

interface GetComponentsResult {
  /**
   * The components.
   * @zh 已安装组件列表
   */
  components: DiscoveryComponentInfo[];
  /**
   * Number of entries in `components`.
   * @zh components 列表条目总数
   */
  count: Int;
}

interface GetUIElementsResult {
  /**
   * The elements.
   * @zh 已注册 UI 元素列表
   */
  elements: DiscoveryUIElementInfo[];
  /**
   * Number of entries in `elements`.
   * @zh elements 列表条目总数
   */
  count: Int;
}

interface GetDspEntriesResult {
  /**
   * The DSPs.
   * @zh 已注册 DSP 条目列表
   */
  entries: DiscoveryDspEntryInfo[];
  /**
   * Number of entries in `entries`.
   * @zh entries 列表条目总数
   */
  count: Int;
}

interface GetOutputDevicesResult {
  /**
   * The backends.
   * @zh 已注册音频输出后端设备列表
   */
  devices: DiscoveryOutputDeviceEntry[];
  /**
   * Number of entries in `devices`.
   * @zh devices 列表条目总数
   */
  count: Int;
}

interface GetPreferencePagesResult {
  /**
   * The pages.
   * @zh 首选项设置页面层级列表
   */
  pages: DiscoveryPreferencePageInfo[];
  /**
   * Number of entries in `pages`.
   * @zh pages 列表条目总数
   */
  count: Int;
}

interface SearchCommandsParams {
  /**
   * Text to look for in names, descriptions and paths; ASCII case is ignored.
   * @zh 搜索关键词文本（在名称、描述与路径中匹配，忽略 ASCII 大小写）
   * @minLength 1
   */
  query: string;
  /**
   * Also search commands expanded from dynamic submenus.
   * @zh 是否同时搜索从动态子菜单中展开的子命令
   * @default true
   */
  expandDynamic?: boolean;
  /**
   * Which menu family to search.
   * @zh 搜索的目标菜单族范围
   * @default "all"
   */
  scope?: 'all' | 'mainmenu' | 'contextmenu';
  /**
   * Also search entries the host would not show.
   * @zh 是否同时搜索宿主界面通常不绘制的隐藏菜单项
   * @default false
   */
  includeHidden?: boolean;
}

interface SearchCommandsResult {
  /**
   * The query as given.
   * @zh 搜索查询关键词
   */
  query: string;
  /**
   * The hits, main menu first.
   * @zh 命令搜索命中列表（主菜单命令优先排列）
   */
  results: DiscoverySearchResult[];
  /**
   * Number of entries in `results`.
   * @zh results 列表命中总数
   */
  count: Int;
  /**
   * The `expandDynamic` that applied.
   * @zh 实际生效的 expandDynamic 参数值
   */
  expandDynamic: boolean;
  /**
   * The `scope` that applied.
   * @zh 实际生效的 scope 搜索范围
   */
  scope: 'all' | 'mainmenu' | 'contextmenu';
  /**
   * The `includeHidden` that applied.
   * @zh 实际生效的 includeHidden 参数值
   */
  includeHidden: boolean;
  /**
   * Hits from the main menu.
   * @zh 主菜单命令命中数量
   */
  mainMenuHits: Int;
  /**
   * Hits from the context menu.
   * @zh 右键菜单命令命中数量
   */
  contextMenuHits: Int;
  /**
   * Whether context-menu state was observed. False when the context family was searched with
   * nothing selected or playing; the hits' `enabled` and `checked` must not be filtered on then.
   * @zh 右键菜单状态是否可观测（无选中或未播放曲目时为 false；此时命中项的 enabled 与 checked 不可用于条件过滤）
   */
  stateKnown: boolean;
}

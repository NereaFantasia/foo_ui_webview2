import type { Int, Json, OperationStatus } from './common.js';

export interface Api {
  /**
   * Run a main-menu command addressed by GUID, leaf name or slash-separated path. A disabled
   * command fails with `MENU_ITEM_DISABLED`. Paths match every ancestor; a wrong prefix never
   * falls back to a leaf name. Ambiguous identities fail with `MENU_MATCH_AMBIGUOUS` and
   * `details.candidates`; a missing name/path is `MENU_COMMAND_NOT_FOUND`, a missing GUID is
   * `NOT_FOUND`. Addressing a dynamic leaf by GUID requires both GUIDs. Unreadable state fails
   * with `OPERATION_FAILED`. The call answers when the command returns: one that shows a modal dialog, such as renaming a
   * playlist, answers only after the dialog closes.
   * @zh 执行主菜单命令（支持按 GUID、叶子命令名或斜杠分隔的完整菜单路径寻址）；处于禁用状态的命令以 `MENU_ITEM_DISABLED` 失败；路径匹配必须严格匹配每一级祖先（前缀不匹配时不回退至叶子名）；存在重名歧义时以 `MENU_MATCH_AMBIGUOUS` 失败并在 `details.candidates` 列出候选列表；名称或路径未查得为 `MENU_COMMAND_NOT_FOUND`，指定 GUID 不存在为 `NOT_FOUND`；按 GUID 寻址动态叶子命令时必须同时提供主命令与动态子命令的两个 GUID；菜单状态不可读以 `OPERATION_FAILED` 失败；调用在底层命令执行返回后应答（若命令弹出模态对话框如重命名播放列表，则等待对话框关闭后应答）
   */
  runMainMenuCommand(params: RunMainMenuCommandParams): RunMainMenuCommandResult;

  /**
   * Run a context-menu command on the playing track, or on the active playlist selection when
   * nothing is playing; fails with `NO_ACTIVE_ITEM` when there is neither. A name that matches
   * no command fails with `MENU_COMMAND_NOT_FOUND`, a GUID that no command owns with `NOT_FOUND`.
   * The call answers when the command returns: one that shows a modal dialog answers only after
   * the dialog closes.
   * @zh 执行上下文右键菜单命令；优先作用于当前正在播放的曲目，未在播放时作用于活动播放列表当前选中的曲目（两者均无目标曲目时以 `NO_ACTIVE_ITEM` 失败）；名称未匹配到有效命令以 `MENU_COMMAND_NOT_FOUND` 失败，GUID 未查得对应命令以 `NOT_FOUND` 失败；调用在底层命令执行返回后应答（若命令弹出模态对话框则等待对话框关闭后应答）
   */
  runContextCommand(params: RunContextCommandParams): RunContextCommandResult;

  /**
   * Get the main menu as a tree. The host builds it from its menu tree, falls back to the Win32
   * menus (`source: "v1-hmenu"`), and as a last resort returns a flat command list
   * (`fallback: "flat-mainmenu-commands"`). A source that cannot build a tree permits fallback;
   * unreadable state in a built tree fails with `OPERATION_FAILED` instead of returning guessed
   * availability. Root filtering keeps full command paths. Addressability is independent of enabled.
   * @zh 获取完整主菜单树形结构；宿主优先通过内部菜单树构建，无法构建时回退至 Win32 菜单（`source: "v1-hmenu"`），最终兜底回退至扁平命令列表（`fallback: "flat-mainmenu-commands"`）；构建过程中状态不可读以 `OPERATION_FAILED` 失败且不猜测可用状态；指定 `root` 过滤时各节点依然保留完整全局路径；命令是否可寻址（`executable`）与当前是否启用（`enabled`）相互独立
   */
  getMainMenu(params: GetMainMenuParams): GetMainMenuResult;

  /**
   * Get the context menu for the tracks `mode` selects, as a tree.
   * @zh 获取指定 `mode` 寻址的目标曲目上下文右键菜单树形结构
   */
  getContextMenu(params: GetContextMenuParams): GetContextMenuResult;

  /**
   * Run the context-menu item whose `commandId` a `menu.getContextMenu` call reported. The menu
   * is rebuilt from `mode` and `handles`, so pass the `handles` that call used and the `mode` it
   * reported, not `auto` again. An id is a position in the rebuilt menu, not a name: an id past
   * its end fails with `NOT_FOUND`, but when the tracks, the selection, the playing track or the
   * installed components changed since, the same id can run another item without failing.
   * Pass `expectPath` and `expectTarget` together to rule that out: the host then runs the item
   * only when the rebuilt menu matches what that call reported, and otherwise fails without
   * running anything, with `details.reason` set: `NOT_FOUND` (`targetChanged`, `commandChanged`),
   * `MENU_MATCH_AMBIGUOUS` (`ambiguousPath`), `MENU_ITEM_DISABLED` (`disabled`), or
   * `NOT_SUPPORTED` (`unsupportedTargetMode`) for `mode: "playlist"`. Reopen the menu after such
   * a failure. The call answers when the command returns: one that shows a modal dialog answers
   * only after the dialog closes.
   * @zh 执行由 `menu.getContextMenu` 返回的 `commandId` 所对应的右键菜单项；执行时会依据传入的 `mode` 与 `handles` 重新构建菜单（必须传入此前获取菜单时所用的具体 `handles` 及其实际解析出的 `mode`，不可再次传入 `auto`）；`id` 代表菜单重建后的条目索引而非命令名称（索引超出范围以 `NOT_FOUND` 失败；若在此期间曲目集合、选择状态、播放曲目或已安装组件发生变动，相同索引可能指向其他命令条目）；建议同时传入 `expectPath` 与 `expectTarget` 进行安全校验（校验通过方才执行，若检测到不一致则拒绝执行且返回明确原因，并在 `details.reason` 中标记为 `targetChanged`、`commandChanged`、`ambiguousPath`、`disabled` 或 `unsupportedTargetMode`）；调用在底层命令执行返回后应答（若命令弹出模态对话框则等待对话框关闭后应答）
   */
  runContextCommandById(params: RunContextCommandByIdParams): RunContextCommandByIdResult;

  /**
   * Show the host's native context menu at the mouse pointer for the tracks `mode` selects. The
   * menu opens just after the call returns.
   * @zh 在鼠标光标位置为 `mode` 所选曲目弹出宿主的原生右键上下文菜单（菜单在调用返回后异步唤起）
   */
  showNativePopup(params: ShowNativePopupParams): void;

  /**
   * Show a self-drawn menu. The choice arrives as `menu:select` and a dismissal as
   * `menu:dismiss`; changing a rating, slider or segmented row reports `menu:valueChanged` and
   * leaves the menu open. Too many rows, too deep nesting, too many segments or too much style
   * text fail with `INVALID_PARAMS` and `details`.
   * @zh 弹出自定义自绘菜单窗口；用户选中菜单项时触发 `menu:select` 事件，菜单关闭时触发 `menu:dismiss` 事件；交互调整评分、滑块或分段控件时派发 `menu:valueChanged` 事件且菜单保持打开；条目数量超出上限、嵌套层级过深、分段数量超标或自定义 CSS 文本过长时以 `INVALID_PARAMS` 失败（错误详情附带 `details`）
   */
  show(params: ShowParams): ShowResult;

  /**
   * Close the self-drawn menu if one is open; its `menu:dismiss` carries `reason`.
   * @zh 主动关闭当前处于打开状态的自绘菜单（并在随后派发的 `menu:dismiss` 事件中携带传入的 `reason`）
   */
  close(params: CloseParams): void;
}

export interface Events {
  /**
   * A row of a menu opened with `menu.show` was chosen; `menu:dismiss` with reason `select`
   * follows as the menu closes. Sent to the page that called `menu.show`, or to a page under the
   * same top-level window when that one cannot be found, or else to the main window's page.
   * @zh 自绘菜单项选中事件；菜单项被选中后派发，随后伴随原因为 "select" 的 `menu:dismiss` 事件关闭菜单；定向投递给发起调用的页面（若该页面已失效则退回同顶层窗口其他页面或主窗口页面）
   * @delivery caller
   */
  select: SelectPayload;

  /**
   * A menu opened with `menu.show` closed, whether or not a row was chosen. Delivered like
   * `menu:select`, also when another page's `menu.show` or `menu.close` closed the menu.
   * @zh 自绘菜单关闭事件（无论是否选中条目均会触发，包含被外部调用或点击外部关闭情形；投递目标同 `menu:select`）
   * @delivery caller
   */
  dismiss: DismissPayload;

  /**
   * A rating, slider or segmented control in a menu opened with `menu.show` changed value; the
   * menu stays open. Delivered like `menu:select`.
   * @zh 自绘菜单内部控件值变更事件（评分、滑块或分段控件交互修改时触发且菜单保持展示，投递目标同 `menu:select`）
   * @delivery caller
   */
  valueChanged: ValueChangedPayload;
}

interface SelectPayload {
  /**
   * Id of the menu, as `menu.show` returned it.
   * @zh 自绘菜单唯一标识符（对应 `menu.show` 返回的 menuId）
   */
  menuId: string;
  /**
   * `id` of the chosen row.
   * @zh 被选中的菜单项条目标识符 itemId
   */
  itemId: string;
}

interface DismissPayload {
  /**
   * Id of the menu, as `menu.show` returned it.
   * @zh 自绘菜单唯一标识符（对应 `menu.show` 返回的 menuId）
   */
  menuId: string;
  /**
   * Why the menu closed: `select` after a row was chosen, `outside` for a click outside it,
   * `escape`, `blur` when another window took the focus, `replaced` when another menu opened,
   * `timeout` when the menu page did not answer in time, or the `reason` given to `menu.close`.
   * @zh 菜单关闭原因标识：条目选中为 "select"、点击外部区域为 "outside"、按下 Esc 为 "escape"、窗口失焦为 "blur"、新菜单顶替为 "replaced"、渲染超时为 "timeout"，或主动关闭时指定的 reason
   */
  reason: string;
}

interface ValueChangedPayload {
  /**
   * Id of the menu, as `menu.show` returned it.
   * @zh 自绘菜单唯一标识符（对应 `menu.show` 返回的 menuId）
   */
  menuId: string;
  /**
   * `id` of the control's row.
   * @zh 发生数值变动的控件所在菜单项 itemId
   */
  itemId: string;
  /**
   * The control's new value.
   * @zh 控件更新后的当前数值
   */
  value: Int;
}

/**
 * Where a menu entry came from.
 * @zh 菜单节点数据源分类标识
 */
type MenuNodeSource =
  | 'mainmenu_static'
  | 'mainmenu_dynamic'
  | 'contextmenu_static'
  | 'contextmenu_dynamic'
  | 'hmenu_fallback';

/**
 * Command counts below a submenu.
 * @zh 子菜单层级下所包含的命令数量与可用性统计信息
 */
interface MenuAvailability {
  /**
   * Commands anywhere below the submenu.
   * @zh 子菜单层级下包含的命令总数
   */
  totalCommands: Int;
  /**
   * Commands that are enabled.
   * @zh 子菜单层级下当前处于可用状态的命令总数
   */
  availableCommands: Int;
  /**
   * Commands that are disabled.
   * @zh 子菜单层级下当前处于禁用状态的命令总数
   */
  disabledCommands: Int;
  /**
   * Whether every command is enabled; also true when there are none.
   * @zh 是否包含的全部命令均处于可用状态（未包含任何命令时亦为 true）
   */
  allAvailable: boolean;
}

/**
 * An observed command identity used to diagnose an ambiguous menu path.
 * @zh 路径歧义诊断候选项的已观测命令身份标识
 */
interface MenuAddressCandidate {
  /**
   * Full observed label path.
   * @zh 观测到的完整菜单标签层级路径
   */
  path: string;
  /**
   * Owning command GUID.
   * @zh 所属命令的全局唯一标识符 GUID
   */
  guid: string;
  /**
   * Dynamic leaf GUID, when present.
   * @zh 动态叶子子命令的 GUID 标识符（存在时返回）
   */
  subGuid?: string;
  /**
   * Immediate registered parent group GUID; empty when unavailable.
   * @zh 直接父分组的注册 GUID（无法获取时为空字符串）
   */
  groupGuid: string;
}

/**
 * A separator, submenu with children, or command with state and address fields.
 * @zh 菜单树节点数据结构（可表示分隔符、子菜单或包含状态与执行地址的命令节点）
 */
interface MenuTreeNode {
  /**
   * Node kind.
   * @zh 菜单节点类型：separator（分隔线）、submenu（子菜单）或 command（具体命令）
   */
  type: 'separator' | 'submenu' | 'command';
  /**
   * Label as the host reports it (localized on a translated build).
   * @zh 宿主报告的原始菜单标签文本（已汉化宿主为对应中文译名）
   */
  label?: string;
  /**
   * `label` translated for `locale`; the same as `label` when translation is off or has no entry
   * for it.
   * @zh 依据 `locale` 参数本地化翻译后的显示标签（未开启翻译或无对应译名时与 `label` 保持一致）
   */
  displayLabel?: string;
  /**
   * Slash-separated labels from the top of the menu.
   * @zh 从顶层菜单起的斜杠分隔层级路径字符串
   */
  path?: string;
  /**
   * `path` made of display labels.
   * @zh 由本地化显示标签组成的层级路径字符串
   */
  displayPath?: string;
  /**
   * Raw display flags. Their bits depend on where the node came from: SDK menu flags from the
   * menu tree and the flat list, Win32 menu state from the Win32 menus. Absent on a submenu of the
   * Win32 menus.
   * @zh 原始显示标志位（位定义取决于节点数据源：菜单树与扁平列表对应 SDK 菜单标志，Win32 菜单对应 Win32 菜单状态；Win32 子菜单节点缺省本字段）
   */
  flags?: Int;
  /**
   * The children, in menu order; only on a submenu.
   * @zh 子菜单所包含的下级子节点列表（按菜单展现顺序排列，仅在 submenu 节点提供）
   */
  children?: MenuTreeNode[];
  /**
   * Command counts below this submenu; only with `withAvailability`, and not on submenus nested
   * inside the Win32 menus.
   * @zh 子菜单层级下的命令可用性统计（仅在 `withAvailability` 为 true 时返回，Win32 菜单下级子菜单不提供本字段）
   */
  availability?: MenuAvailability;
  /**
   * Whether the command can run; only on a command.
   * @zh 命令当前是否处于可用可执行状态（仅在 command 节点提供）
   */
  enabled?: boolean;
  /**
   * The same as `enabled`; only on a command.
   * @zh 命令可用状态别名（数值与 `enabled` 相同，仅在 command 节点提供）
   */
  available?: boolean;
  /**
   * Whether the command shows a check mark (a radio mark counts too); only on a command.
   * @zh 命令当前是否处于勾选状态（包含单选标记样式，仅在 command 节点提供）
   */
  checked?: boolean;
  /**
   * Whether the check mark is a radio mark; only on a command.
   * @zh 勾选标记是否呈现为圆点单选样式（仅在 command 节点提供）
   */
  radioChecked?: boolean;
  /**
   * Whether the host would not draw the command; only on a command.
   * @zh 宿主在渲染时是否隐藏该命令（仅在 command 节点提供）
   */
  hidden?: boolean;
  /**
   * Menu item id. For a context-menu command, `menu.runContextCommandById` runs it. The id is
   * valid only for the menu it came from, so do not store it. Absent on the flat list.
   * @zh 临时菜单项索引标识符（供 `menu.runContextCommandById` 执行使用；仅对当前次菜单树有效且禁止持久化保存，扁平命令列表不提供）
   */
  commandId?: Int;
  /**
   * Command GUID, the stable address to pass to `menu.runMainMenuCommand` or
   * `menu.runContextCommand`; absent when it could not be resolved.
   * @zh 跨语言稳定的命令 GUID 标识符（供 `menu.runMainMenuCommand` 或 `menu.runContextCommand` 寻址使用，未解析时缺省）
   */
  guid?: string;
  /**
   * GUID of a dynamic child command, passed together with `guid`.
   * @zh 动态子命令的全局唯一标识符 subGuid（须与主命令 guid 搭配使用）
   */
  subGuid?: string;
  /**
   * Main-menu sources reflect the provider's node kind, even when a dynamic leaf has no usable
   * subGuid. Context-menu sources use subGuid presence. Win32 menu commands use `hmenu_fallback`.
   * @zh 菜单节点数据源分类：主菜单按提供者节点定义区分（动态叶子即便缺失 subGuid 仍标记为动态）；上下文菜单按是否存在 subGuid 区分；Win32 菜单命令标记为 `hmenu_fallback`
   */
  source?: MenuNodeSource;
  /**
   * For main menus, whether the command has a unique complete execution address, independently
   * of enabled. For context menus, whether a command GUID is present.
   * @zh 命令是否具备确定性的唯一执行地址（主菜单表示是否存在完整寻址凭据，上下文菜单表示是否存在命令 GUID）
   */
  executable?: boolean;
  /**
   * Why the command cannot be run; present when `executable` is false.
   * @zh 命令不可寻址执行的具体原因分类（仅在 `executable` 为 false 时提供）
   */
  unaddressableReason?: 'noStableIdentifier' | 'ambiguousMatch' | 'missingDynamicIdentifier' | 'dynamicParent' | 'incompleteHierarchy';
  /**
   * Diagnostic candidates for an ambiguous main-menu node; never select the first automatically.
   * @zh 主菜单节点遭遇路径歧义时的所有匹配候选列表（供调用方诊断或展示，禁止前端自动盲选首项）
   */
  addressCandidates?: MenuAddressCandidate[];
  /**
   * `true` on a command of the flat list.
   * @zh 是否属于扁平命令兜底列表项（为 true 时表示节点源自兜底扁平列表）
   */
  fallback?: boolean;
}

interface RunMainMenuCommandParams {
  /**
   * The command: a GUID such as `{11213A01-...}`, a leaf name, or a slash-separated path. Prefer
   * the GUID, the one form that does not depend on the host's language.
   * @zh 目标命令标识：支持 GUID（如 `{11213A01-...}`）、叶子命令名称或斜杠分隔的完整菜单路径（推荐优先使用 GUID 形式以规避宿主语言差异）
   * @minLength 1
   */
  command: string;
  /**
   * GUID of a dynamic child of the command; only with the GUID form. Empty is the same as absent.
   * @zh 目标动态子命令的 GUID 标识符（仅在 command 传入 GUID 形式时生效，空字符串等同缺省）
   */
  subGuid?: string;
}

interface RunMainMenuCommandResult {
  /**
   * GUID of the command that ran, including calls resolved from names or paths. Older hosts
   * can omit it for those forms.
   * @zh 实际执行命令的 GUID 标识符（由名称或路径成功解析时亦返回，旧版本宿主可能在非 GUID 调用时缺省）
   */
  guid?: string;
  /**
   * Whether it was a dynamic child command.
   * @zh 所执行的命令是否属于动态子命令
   */
  dynamic?: boolean;
  /**
   * GUID of the dynamic child that ran.
   * @zh 实际执行的动态子命令 GUID 标识符
   */
  subGuid?: string;
  /**
   * How a name or path was resolved: `v2-tree` by the menu tree, `index` by the command list.
   * Absent for the GUID form.
   * @zh 寻址名称或路径的底层解析源：`v2-tree` 表示基于菜单树解析，`index` 表示基于命令列表索引（以 GUID 形式调用时不提供）
   */
  source?: 'v2-tree' | 'index';
  /**
   * Always `unknown`: foobar2000 does not report whether the command finished or whether a
   * dialog it opened was cancelled.
   * @zh 底层操作完成状态（foobar2000 宿主不反馈命令实际执行结果或模态对话框取消状态，因此固定为 "unknown"）
   */
  completion: OperationStatus;
}

interface RunContextCommandParams {
  /**
   * The command: a GUID or a command name.
   * @zh 目标命令标识：支持 GUID 标识符或命令名称
   * @minLength 1
   */
  command: string;
  /**
   * GUID of a dynamic child such as a rating value or a converter preset; without it the owning
   * container is targeted, which does nothing. Empty is the same as absent.
   * @zh 目标动态子项的 GUID 标识符（如指定评分星级或格式转换预设；未提供时默认作用于父容器而不触发动作，空字符串等同缺省）
   */
  subGuid?: string;
}

interface RunContextCommandResult {
  /**
   * GUID of the command that ran; absent when it was reached by name and has no GUID.
   * @zh 实际执行命令的 GUID 标识符（按名称查找且底层无 GUID 时缺省）
   */
  guid?: string;
  /**
   * How many tracks the command ran on.
   * @zh 本次命令实际作用的目标曲目句柄总数
   */
  itemCount: Int;
  /**
   * `false` when the command has no GUID and was handed to a host entry point that reports
   * nothing back, so whether it ran is unknown.
   * @zh 命令执行是否获得明确调度确认（若命令缺失 GUID 仅能派发至单向宿主入口时为 false，此时无法确认底层是否实际执行）
   */
  executionConfirmed: boolean;
  /**
   * Always `unknown`: foobar2000 does not report whether the command finished or whether a
   * dialog it opened was cancelled.
   * @zh 底层操作完成状态（foobar2000 宿主不反馈命令实际执行结果或模态对话框取消状态，因此固定为 "unknown"）
   */
  completion: OperationStatus;
}

interface GetMainMenuParams {
  /**
   * Label or slash-separated path of a submenu to return instead of the whole menu. When nothing
   * matches, the whole menu is returned with `rootMatched: false`.
   * @zh 需定向提取的子菜单标签或斜杠层级路径（缺省时返回整棵主菜单树；若未匹配到目标子菜单则回退返回整棵菜单树且标记 `rootMatched` 为 false）
   * @default ""
   */
  root?: string;
  /**
   * Locale for `displayLabel`: `auto` keeps the host's labels; a `zh` or `en` tag translates
   * common labels.
   * @zh `displayLabel` 显示标签的目标区域代码（`auto` 保留宿主原始标签，以 `zh` 或 `en` 开头时自动翻译常见菜单命令）
   * @default "auto"
   */
  locale?: string;
  /**
   * `false` turns the label translation off.
   * @zh 是否启用菜单标签本地化翻译（设置为 false 时关闭翻译）
   * @default true
   */
  i18n?: boolean;
  /**
   * Add command counts to submenus.
   * @zh 是否在各子菜单节点中附加命令数量与可用性统计信息 `availability`
   * @default true
   */
  withAvailability?: boolean;
}

interface GetMainMenuResult {
  /**
   * Label of the submenu returned; empty for the whole menu.
   * @zh 实际返回的根子菜单显示标签（返回整棵主菜单树时为空字符串）
   */
  root: string;
  /**
   * The `root` that was asked for.
   * @zh 请求时传入的原始 `root` 参数
   */
  requestedRoot: string;
  /**
   * Whether `root` was found; true when none was asked for, except on the flat list, which is
   * always false.
   * @zh 请求的 `root` 是否成功匹配（未指定 `root` 时为 true；触发扁平兜底列表时恒为 false）
   */
  rootMatched: boolean;
  /**
   * The `locale` used.
   * @zh 实际采用的区域语言标识
   */
  locale: string;
  /**
   * The `i18n` used.
   * @zh 实际采用的本地化翻译开关状态
   */
  i18n: boolean;
  /**
   * The `withAvailability` used.
   * @zh 实际采用的命令统计包含开关状态
   */
  withAvailability: boolean;
  /**
   * The top-level nodes.
   * @zh 菜单树顶层节点列表
   */
  items: MenuTreeNode[];
  /**
   * `v1-hmenu` when the tree was built from the Win32 menus.
   * @zh 菜单树数据源标记（当且仅当回退采用 Win32 原生菜单构建时固定为 `"v1-hmenu"`）
   */
  source?: 'v1-hmenu';
  /**
   * `flat-mainmenu-commands` when only the flat command list could be built.
   * @zh 兜底构建模式标记（当且仅当仅能构建出扁平命令列表时固定为 `"flat-mainmenu-commands"`）
   */
  fallback?: 'flat-mainmenu-commands';
}

interface GetContextMenuParams {
  /**
   * Tracks to build the menu for: `handles` uses `handles`, `playlist` the active playlist,
   * `nowPlaying` the playing track and `selection` the active playlist selection. Any other
   * value, `auto` included, takes the first of `handles`, the playing track, the selection and the
   * active playlist that is available. Given `handles` of which none is usable fail with
   * `INVALID_PARAMS` in every mode, `auto` included, rather than falling back to other tracks;
   * `nowPlaying` with nothing playing and `selection` with nothing selected fail with
   * `NO_ACTIVE_ITEM`.
   * @zh 菜单目标曲目解析模式：`handles`（显式传入曲目）、`playlist`（活动播放列表全量曲目）、`nowPlaying`（当前播放曲目）、`selection`（活动播放列表选中项）；传入 `auto` 或其他值时按 handles -> nowPlaying -> selection -> playlist 优先级自动选择首个可用目标；若显式指定 `handles` 但无有效曲目句柄，任何模式（含 auto）均以 `INVALID_PARAMS` 失败且不回退；若指定 `nowPlaying` 但未在播放，或指定 `selection` 但无选中项，均以 `NO_ACTIVE_ITEM` 失败
   * @default "auto"
   */
  mode?: string;
  /**
   * Locale for `displayLabel`: `auto` keeps the host's labels; a `zh` or `en` tag translates
   * common labels.
   * @zh `displayLabel` 显示标签的目标区域代码（`auto` 保留宿主原始标签，以 `zh` 或 `en` 开头时自动翻译常见菜单命令）
   * @default "auto"
   */
  locale?: string;
  /**
   * `false` turns the label translation off.
   * @zh 是否启用菜单标签本地化翻译（设置为 false 时关闭翻译）
   * @default true
   */
  i18n?: boolean;
  /**
   * Add command counts to submenus.
   * @zh 是否在各子菜单节点中附加命令数量与可用性统计信息 `availability`
   * @default true
   */
  withAvailability?: boolean;
  /**
   * Tracks for `mode: "handles"` and `auto`: each a path, optionally ending in `|subsong:N`, or an
   * object `{ path, subsong }`. Paths the security policy refuses and entries of other types are
   * skipped. Each path is checked on disk, so prefer `selection`, `playlist` or `nowPlaying` for
   * tracks the host already lists.
   * @zh 供 `mode: "handles"` 及 `auto` 使用的目标曲目列表（数组项支持文件路径字符串如带 `|subsong:N` 后缀，或 `{ path, subsong }` 对象；受安全策略拦截的路径或非法类型条目自动忽略；因物理路径需执行磁盘检查，宿主已有曲目推荐优先使用 `selection`、`playlist` 或 `nowPlaying`）
   */
  handles?: Json[];
}

interface GetContextMenuResult {
  /**
   * The mode used; `auto` resolves to one of the others.
   * @zh 最终实际采用的目标曲目模式（`auto` 自动解析为 `handles`、`playlist`、`nowPlaying` 或 `selection` 之一）
   */
  mode: 'handles' | 'playlist' | 'nowPlaying' | 'selection';
  /**
   * The `locale` used.
   * @zh 实际采用的区域语言标识
   */
  locale: string;
  /**
   * The `i18n` used.
   * @zh 实际采用的本地化翻译开关状态
   */
  i18n: boolean;
  /**
   * The `withAvailability` used.
   * @zh 实际采用的命令统计包含开关状态
   */
  withAvailability: boolean;
  /**
   * The top-level nodes.
   * @zh 菜单树顶层节点列表
   */
  items: MenuTreeNode[];
  /**
   * A fingerprint of the tracks the menu was built for, to pass back as `expectTarget`. Absent
   * for `mode: "playlist"`. Opaque, and valid only while the host process runs.
   * @zh 上下文菜单目标曲目集合的不透明指纹凭据（供 `runContextCommandById` 的 `expectTarget` 校验使用；`mode` 为 `playlist` 时缺省，仅在当前宿主进程生命周期内有效）
   */
  targetKey?: string;
}

interface RunContextCommandByIdResult {
  /**
   * Always `unknown`: foobar2000 does not report whether the command finished or whether a
   * dialog it opened was cancelled.
   * @zh 底层操作完成状态（foobar2000 宿主不反馈命令实际执行结果或模态对话框取消状态，因此固定为 "unknown"）
   */
  completion: OperationStatus;
}

interface RunContextCommandByIdParams {
  /**
   * The `commandId` of a context-menu node.
   * @zh 目标右键菜单节点的 `commandId` 索引
   * @minimum 0
   * @maximum 4294967295
   */
  id: Int;
  /**
   * The `mode` the menu was built with; see `menu.getContextMenu`.
   * @zh 获取菜单树时所使用的具体 `mode` 参数（禁止传入 auto，须沿用解析后的实际模式）
   * @default "auto"
   */
  mode?: string;
  /**
   * The `handles` the menu was built with; see `menu.getContextMenu`.
   * @zh 获取菜单树时传入的 `handles` 曲目列表
   */
  handles?: Json[];
  /**
   * The node's `path` as `menu.getContextMenu` reported it. Give it with `expectTarget`: the item
   * at `id` must have this path, and no other node may share it.
   * @zh 预期执行节点的层级路径 `path`（须与 `expectTarget` 搭配使用；指定 `id` 的菜单项必须完全匹配该路径且整棵树中不得存在重名路径）
   * @minLength 1
   */
  expectPath?: string;
  /**
   * The `targetKey` that `menu.getContextMenu` reported. Give it with `expectPath`: the tracks
   * resolved now must give the same key.
   * @zh 预期目标曲目指纹凭据 `targetKey`（须与 `expectPath` 搭配使用；当前重新解析出的曲目指纹必须与之严格相等）
   * @minLength 1
   */
  expectTarget?: string;
}

interface ShowNativePopupParams {
  /**
   * Tracks to build the menu for; see `menu.getContextMenu`.
   * @zh 弹出原生菜单的目标曲目模式（取值同 `menu.getContextMenu`）
   * @default "auto"
   */
  mode?: string;
  /**
   * Tracks for `mode: "handles"` and `auto`; see `menu.getContextMenu`.
   * @zh 供 `mode: "handles"` 及 `auto` 使用的目标曲目列表（定义同 `menu.getContextMenu`）
   */
  handles?: Json[];
  /**
   * Accepted for compatibility and ignored: the menu always opens at the mouse pointer.
   * @zh 兼容保留参数（无实际效果，原生菜单总在当前鼠标光标位置弹出）
   */
  x?: Int;
  /**
   * Accepted for compatibility and ignored: the menu always opens at the mouse pointer.
   * @zh 兼容保留参数（无实际效果，原生菜单总在当前鼠标光标位置弹出）
   */
  y?: Int;
}

interface ShowParams {
  /**
   * Menu rows; the SDK's `MenuPopupItem` describes their fields. A row icon over 32 KiB is dropped.
   * @zh 自绘菜单条目定义列表（单项字段结构详见 SDK 的 `MenuPopupItem`；超出 32 KiB 的条目图标将自动忽略）
   */
  items?: Json[];
  /**
   * Screen x of the anchor in physical pixels; absent or negative uses the mouse pointer.
   * @zh 弹出锚点屏幕横坐标 X（物理像素；缺省或传入负数时使用当前鼠标光标位置）
   */
  x?: Int;
  /**
   * Screen y of the anchor in physical pixels; absent or negative uses the mouse pointer.
   * @zh 弹出锚点屏幕纵坐标 Y（物理像素；缺省或传入负数时使用当前鼠标光标位置）
   */
  y?: Int;
  /**
   * `contentSized` draws each panel in a window sized to its content; any other value uses one
   * full-screen overlay window.
   * @zh 弹窗窗口渲染模型：传入 `contentSized` 各级面板在独立自适应窗口中渲染，其他值采用全屏透明遮罩覆盖窗口
   */
  windowModel?: string;
  /**
   * Style sheet applied to the menu on top of the built-in styles, at most 256 KiB.
   * @zh 叠加于内置样式之上的自定义 CSS 样式表字符串（最大限制 256 KiB）
   */
  css?: string;
  /**
   * Replace the built-in styles with `css` instead of adding to them.
   * @zh 是否全量替换内置菜单样式（为 true 时完全使用 `css` 参数而不叠加内置样式）
   */
  cssReplace?: boolean;
  /**
   * Window backdrop: `acrylic` (the default), `mica`, `mica-alt` or `none`. Other values keep
   * the default.
   * @zh 菜单窗口背景材质类型：`acrylic`（默认）、`mica`、`mica-alt` 或 `none`（其他值回退至默认材质）
   */
  backdrop?: string;
  /**
   * Dark tint for the backdrop; the default is `true`.
   * @zh 背景材质是否采用深色色调（默认 true）
   */
  backdropDarkMode?: boolean;
  /**
   * Length of the closing animation in milliseconds, clamped to 0–1000; the default `0` closes
   * at once.
   * @zh 菜单关闭淡出动画时长（毫秒，限制在 0 至 1000 毫秒之间，默认 `0` 表示立即关闭）
   */
  closeAnimationMs?: Int;
}

interface ShowResult {
  /**
   * Id of the menu; `menu:select`, `menu:dismiss` and `menu:valueChanged` carry it.
   * @zh 自绘菜单唯一标识符（随附于 `menu:select`、`menu:dismiss` 与 `menu:valueChanged` 事件中）
   */
  menuId: string;
}

interface CloseParams {
  /**
   * Reason reported in `menu:dismiss`.
   * @zh 菜单关闭原因文本（将在随后的 `menu:dismiss` 事件中携带）
   * @default "api"
   */
  reason?: string;
}

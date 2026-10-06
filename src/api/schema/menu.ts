import type { Int, Json } from './common.js';

export interface Api {
  /**
   * Run a main-menu command addressed by GUID, leaf name or slash-separated path. A disabled
   * command fails with `MENU_ITEM_DISABLED`; a name that matches several commands fails with
   * `MENU_MATCH_AMBIGUOUS` and lists them in `candidates`; a name that matches none fails with
   * `MENU_COMMAND_NOT_FOUND`, and a GUID that no command owns with `NOT_FOUND`.
   * @zh 运行主菜单命令，按 GUID、叶子名或斜杠分隔的路径寻址。命令被禁用时以 `MENU_ITEM_DISABLED` 失败；名称匹配到多条命令时以 `MENU_MATCH_AMBIGUOUS` 失败并在 `candidates` 里列出；名称一条都没匹配到时以 `MENU_COMMAND_NOT_FOUND` 失败，没有命令拥有该 GUID 时以 `NOT_FOUND` 失败。
   */
  runMainMenuCommand(params: RunMainMenuCommandParams): RunMainMenuCommandResult;

  /**
   * Run a context-menu command on the playing track, or on the active playlist selection when
   * nothing is playing; fails with `NO_ACTIVE_ITEM` when there is neither. A name that matches
   * no command fails with `MENU_COMMAND_NOT_FOUND`, a GUID that no command owns with `NOT_FOUND`.
   * @zh 对正在播放的曲目运行右键菜单命令，没有播放时对活动播放列表的选中项运行；两者都没有时以 `NO_ACTIVE_ITEM` 失败。名称没匹配到命令时以 `MENU_COMMAND_NOT_FOUND` 失败，没有命令拥有该 GUID 时以 `NOT_FOUND` 失败。
   */
  runContextCommand(params: RunContextCommandParams): RunContextCommandResult;

  /**
   * Get the main menu as a tree. The host builds it from its menu tree, falls back to the Win32
   * menus (`source: "v1-hmenu"`), and as a last resort returns a flat command list
   * (`fallback: "flat-mainmenu-commands"`); fails with `OPERATION_FAILED` when none can be built.
   * @zh 以树的形式获取主菜单。宿主先按菜单树构建，失败时退到 Win32 菜单（`source: "v1-hmenu"`），最后退到扁平命令列表（`fallback: "flat-mainmenu-commands"`）；都建不出时以 `OPERATION_FAILED` 失败。
   */
  getMainMenu(params: GetMainMenuParams): GetMainMenuResult;

  /**
   * Get the context menu for the tracks `mode` selects, as a tree.
   * @zh 以树的形式获取 `mode` 所选曲目的右键菜单。
   */
  getContextMenu(params: GetContextMenuParams): GetContextMenuResult;

  /**
   * Run the context-menu item whose `commandId` a `menu.getContextMenu` call reported. The menu
   * is rebuilt from `mode` and `handles`, so pass the `handles` that call used and the `mode` it
   * reported, not `auto` again. An id is a position in the rebuilt menu, not a name: an id past
   * its end fails with `NOT_FOUND`, but when the tracks, the selection, the playing track or the
   * installed components changed since, the same id can run another item without failing.
   * @zh 运行 `menu.getContextMenu` 报告的 `commandId` 对应的右键菜单项。菜单会按 `mode` 与 `handles` 重建，所以要传那次调用用的 `handles` 和它报告的 `mode`，不要再传 `auto`。id 是重建后菜单里的位置而不是名称：超出菜单末尾的 id 以 `NOT_FOUND` 失败；但曲目、选择、正在播放的曲目或已装组件在此期间变了时，同一个 id 可能不报错地运行另一项。
   */
  runContextCommandById(params: RunContextCommandByIdParams): void;

  /**
   * Show the host's native context menu at the mouse pointer for the tracks `mode` selects. The
   * menu opens just after the call returns.
   * @zh 在鼠标指针处为 `mode` 所选曲目弹出宿主的原生右键菜单。菜单在调用返回后随即打开。
   */
  showNativePopup(params: ShowNativePopupParams): void;

  /**
   * Show a self-drawn menu. The choice arrives as `menu:select` and a dismissal as
   * `menu:dismiss`; changing a rating, slider or segmented row reports `menu:valueChanged` and
   * leaves the menu open. Too many rows, too deep nesting, too many segments or too much style
   * text fail with `INVALID_PARAMS` and `details`.
   * @zh 显示自绘菜单。选中结果以 `menu:select` 事件到达，关闭以 `menu:dismiss` 到达；调整评分、滑块或分段行时报告 `menu:valueChanged`，菜单保持打开。行数过多、嵌套过深、分段过多或样式文本过长时以 `INVALID_PARAMS` 失败并带 `details`。
   */
  show(params: ShowParams): ShowResult;

  /**
   * Close the self-drawn menu if one is open; its `menu:dismiss` carries `reason`.
   * @zh 关闭打开着的自绘菜单；它的 `menu:dismiss` 事件带上 `reason`。
   */
  close(params: CloseParams): void;
}

export interface Events {
  /**
   * A row of a menu opened with `menu.show` was chosen; `menu:dismiss` with reason `select`
   * follows as the menu closes. Sent to the page that called `menu.show`, or to a page under the
   * same top-level window when that one cannot be found, or else to the main window's page.
   * @zh `menu.show` 打开的菜单里某一行被选中；菜单关闭时随后还有一个原因为 `select` 的 `menu:dismiss`。发给调用 `menu.show` 的页面；找不到它时发给同一顶层窗口下的页面，再没有就发给主窗口的页面。
   * @delivery caller
   */
  select: SelectPayload;

  /**
   * A menu opened with `menu.show` closed, whether or not a row was chosen. Delivered like
   * `menu:select`, also when another page's `menu.show` or `menu.close` closed the menu.
   * @zh `menu.show` 打开的菜单关闭了，不论有没有选中某一行。投递范围同 `menu:select`，菜单被别的页面的 `menu.show` 或 `menu.close` 关掉时也一样。
   * @delivery caller
   */
  dismiss: DismissPayload;

  /**
   * A rating, slider or segmented control in a menu opened with `menu.show` changed value; the
   * menu stays open. Delivered like `menu:select`.
   * @zh `menu.show` 打开的菜单里的评分、滑块或分段控件改了值，菜单保持打开。投递范围同 `menu:select`。
   * @delivery caller
   */
  valueChanged: ValueChangedPayload;
}

interface SelectPayload {
  /**
   * Id of the menu, as `menu.show` returned it.
   * @zh 菜单的 id，同 `menu.show` 的返回值。
   */
  menuId: string;
  /**
   * `id` of the chosen row.
   * @zh 被选中的那一行的 `id`。
   */
  itemId: string;
}

interface DismissPayload {
  /**
   * Id of the menu, as `menu.show` returned it.
   * @zh 菜单的 id，同 `menu.show` 的返回值。
   */
  menuId: string;
  /**
   * Why the menu closed: `select` after a row was chosen, `outside` for a click outside it,
   * `escape`, `blur` when another window took the focus, `replaced` when another menu opened,
   * `timeout` when the menu page did not answer in time, or the `reason` given to `menu.close`.
   * @zh 菜单关闭的原因：选中某一行后为 `select`，在菜单外点击为 `outside`，`escape`，别的窗口抢走焦点为 `blur`，另一个菜单打开为 `replaced`，菜单页面没按时响应为 `timeout`，或调用 `menu.close` 时给的 `reason`。
   */
  reason: string;
}

interface ValueChangedPayload {
  /**
   * Id of the menu, as `menu.show` returned it.
   * @zh 菜单的 id，同 `menu.show` 的返回值。
   */
  menuId: string;
  /**
   * `id` of the control's row.
   * @zh 控件所在那一行的 `id`。
   */
  itemId: string;
  /**
   * The control's new value.
   * @zh 控件的新值。
   */
  value: Int;
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
 * Command counts below a submenu.
 * @zh 子菜单之下的命令计数。
 */
interface MenuAvailability {
  /**
   * Commands anywhere below the submenu.
   * @zh 子菜单之下的全部命令数。
   */
  totalCommands: Int;
  /**
   * Commands that are enabled.
   * @zh 可用的命令数。
   */
  availableCommands: Int;
  /**
   * Commands that are disabled.
   * @zh 被禁用的命令数。
   */
  disabledCommands: Int;
  /**
   * Whether every command is enabled; also true when there are none.
   * @zh 是否全部命令都可用；一条命令都没有时也为 true。
   */
  allAvailable: boolean;
}

/**
 * One node of a menu tree. `type` tells the kind: a separator has no other fields, a submenu has
 * `children`, and a command has the state and address fields.
 * @zh 菜单树的一个节点。`type` 表示种类：分隔符没有其他字段，子菜单有 `children`，命令有状态与地址字段。
 */
interface MenuTreeNode {
  /**
   * Node kind.
   * @zh 节点种类。
   */
  type: 'separator' | 'submenu' | 'command';
  /**
   * Label as the host reports it (localized on a translated build).
   * @zh 宿主报告的标签（汉化版上是译名）。
   */
  label?: string;
  /**
   * `label` translated for `locale`; the same as `label` when translation is off or has no entry
   * for it.
   * @zh 按 `locale` 翻译后的 `label`；不翻译或没有对应译名时与 `label` 相同。
   */
  displayLabel?: string;
  /**
   * Slash-separated labels from the top of the menu.
   * @zh 从菜单顶层起的斜杠分隔标签。
   */
  path?: string;
  /**
   * `path` made of display labels.
   * @zh 由显示标签组成的 `path`。
   */
  displayPath?: string;
  /**
   * Raw display flags. Their bits depend on where the node came from: SDK menu flags from the
   * menu tree and the flat list, Win32 menu state from the Win32 menus. Absent on a submenu of the
   * Win32 menus.
   * @zh 原始显示位。位的含义取决于节点来源：菜单树与扁平列表是 SDK 菜单标志，Win32 菜单是 Win32 菜单状态。Win32 菜单的子菜单没有。
   */
  flags?: Int;
  /**
   * The children, in menu order; only on a submenu.
   * @zh 子节点，按菜单顺序；只有子菜单有。
   */
  children?: MenuTreeNode[];
  /**
   * Command counts below this submenu; only with `withAvailability`, and not on submenus nested
   * inside the Win32 menus.
   * @zh 该子菜单之下的命令计数；只在 `withAvailability` 时出现，嵌在 Win32 菜单里的子菜单没有。
   */
  availability?: MenuAvailability;
  /**
   * Whether the command can run; only on a command.
   * @zh 命令能否运行；只有命令有。
   */
  enabled?: boolean;
  /**
   * The same as `enabled`; only on a command.
   * @zh 与 `enabled` 相同；只有命令有。
   */
  available?: boolean;
  /**
   * Whether the command shows a check mark (a radio mark counts too); only on a command.
   * @zh 命令是否带勾选标记（单选标记也算）；只有命令有。
   */
  checked?: boolean;
  /**
   * Whether the check mark is a radio mark; only on a command.
   * @zh 勾选标记是否为单选样式；只有命令有。
   */
  radioChecked?: boolean;
  /**
   * Whether the host would not draw the command; only on a command.
   * @zh 宿主是否不会绘制该命令；只有命令有。
   */
  hidden?: boolean;
  /**
   * Menu item id. For a context-menu command, `menu.runContextCommandById` runs it. The id is
   * valid only for the menu it came from, so do not store it. Absent on the flat list.
   * @zh 菜单项 id。右键菜单命令可以交给 `menu.runContextCommandById` 运行。id 只对产生它的那份菜单有效，不要保存。扁平列表里没有。
   */
  commandId?: Int;
  /**
   * Command GUID, the stable address to pass to `menu.runMainMenuCommand` or
   * `menu.runContextCommand`; absent when it could not be resolved.
   * @zh 命令 GUID，是交给 `menu.runMainMenuCommand` 或 `menu.runContextCommand` 的稳定地址；解析不到时没有。
   */
  guid?: string;
  /**
   * GUID of a dynamic child command, passed together with `guid`.
   * @zh 动态子命令的 GUID，与 `guid` 一起传。
   */
  subGuid?: string;
  /**
   * Where the command came from: the dynamic value when it carries `subGuid`, the static value
   * otherwise, and `hmenu_fallback` for every command of the Win32 menus.
   * @zh 命令的来源：带 `subGuid` 时为动态值，否则为静态值；Win32 菜单的命令一律为 `hmenu_fallback`。
   */
  source?: MenuNodeSource;
  /**
   * Whether the command has a `guid` to run it by.
   * @zh 命令是否有可用来运行它的 `guid`。
   */
  executable?: boolean;
  /**
   * Why the command cannot be run; present when `executable` is false.
   * @zh 命令不能运行的原因；`executable` 为 false 时出现。
   */
  unaddressableReason?: 'noStableIdentifier';
  /**
   * `true` on a command of the flat list.
   * @zh 扁平列表里的命令为 `true`。
   */
  fallback?: boolean;
}

interface RunMainMenuCommandParams {
  /**
   * The command: a GUID such as `{11213A01-...}`, a leaf name, or a slash-separated path. Prefer
   * the GUID, the one form that does not depend on the host's language.
   * @zh 命令：`{11213A01-...}` 这样的 GUID、叶子名或斜杠分隔的路径。优先用 GUID，只有它不受宿主语言影响。
   * @minLength 1
   */
  command: string;
  /**
   * GUID of a dynamic child of the command; only with the GUID form. Empty is the same as absent.
   * @zh 该命令的某个动态子命令的 GUID；只配合 GUID 形式使用。空串等同没传。
   */
  subGuid?: string;
}

interface RunMainMenuCommandResult {
  /**
   * GUID of the command that ran; absent when the menu tree ran it by name or path.
   * @zh 已运行命令的 GUID；菜单树按名称或路径运行时没有。
   */
  guid?: string;
  /**
   * Whether it was a dynamic child command.
   * @zh 是否为动态子命令。
   */
  dynamic?: boolean;
  /**
   * GUID of the dynamic child that ran.
   * @zh 已运行的动态子命令的 GUID。
   */
  subGuid?: string;
  /**
   * How a name or path was resolved: `v2-tree` by the menu tree, `index` by the command list.
   * Absent for the GUID form.
   * @zh 名称或路径的解析方式：`v2-tree` 为菜单树，`index` 为命令列表。GUID 形式没有。
   */
  source?: 'v2-tree' | 'index';
}

interface RunContextCommandParams {
  /**
   * The command: a GUID or a command name.
   * @zh 命令：GUID 或命令名。
   * @minLength 1
   */
  command: string;
  /**
   * GUID of a dynamic child such as a rating value or a converter preset; without it the owning
   * container is targeted, which does nothing. Empty is the same as absent.
   * @zh 动态子项的 GUID，例如某个评分值或转换预设；不传时目标是它所属的容器，什么也不会发生。空串等同没传。
   */
  subGuid?: string;
}

interface RunContextCommandResult {
  /**
   * GUID of the command that ran; absent when it was reached by name and has no GUID.
   * @zh 已运行命令的 GUID；按名称找到且没有 GUID 时没有。
   */
  guid?: string;
  /**
   * How many tracks the command ran on.
   * @zh 命令作用的曲目数。
   */
  itemCount: Int;
  /**
   * `false` when the command has no GUID and was handed to a host entry point that reports
   * nothing back, so whether it ran is unknown.
   * @zh 命令没有 GUID、只能交给不回报结果的宿主入口时为 `false`，此时不知道它是否真的运行了。
   */
  executionConfirmed: boolean;
}

interface GetMainMenuParams {
  /**
   * Label or slash-separated path of a submenu to return instead of the whole menu. When nothing
   * matches, the whole menu is returned with `rootMatched: false`.
   * @zh 要返回的子菜单的标签或斜杠分隔路径，不传则返回整个菜单。没有匹配时返回整个菜单，`rootMatched` 为 false。
   * @default ""
   */
  root?: string;
  /**
   * Locale for `displayLabel`: `auto` keeps the host's labels; a `zh` or `en` tag translates
   * common labels.
   * @zh `displayLabel` 用的语言：`auto` 保留宿主的标签；`zh` 或 `en` 开头的标签会翻译常见标签。
   * @default "auto"
   */
  locale?: string;
  /**
   * `false` turns the label translation off.
   * @zh 为 `false` 时关闭标签翻译。
   * @default true
   */
  i18n?: boolean;
  /**
   * Add command counts to submenus.
   * @zh 给子菜单加上命令计数。
   * @default true
   */
  withAvailability?: boolean;
}

interface GetMainMenuResult {
  /**
   * Label of the submenu returned; empty for the whole menu.
   * @zh 返回的子菜单的标签；返回整个菜单时为空。
   */
  root: string;
  /**
   * The `root` that was asked for.
   * @zh 请求的 `root`。
   */
  requestedRoot: string;
  /**
   * Whether `root` was found; true when none was asked for, except on the flat list, which is
   * always false.
   * @zh 是否找到了 `root`；没有请求时为 true，扁平列表除外，它恒为 false。
   */
  rootMatched: boolean;
  /**
   * The `locale` used.
   * @zh 使用的 `locale`。
   */
  locale: string;
  /**
   * The `i18n` used.
   * @zh 使用的 `i18n`。
   */
  i18n: boolean;
  /**
   * The `withAvailability` used.
   * @zh 使用的 `withAvailability`。
   */
  withAvailability: boolean;
  /**
   * The top-level nodes.
   * @zh 顶层节点。
   */
  items: MenuTreeNode[];
  /**
   * `v1-hmenu` when the tree was built from the Win32 menus.
   * @zh 树由 Win32 菜单构建时为 `v1-hmenu`。
   */
  source?: 'v1-hmenu';
  /**
   * `flat-mainmenu-commands` when only the flat command list could be built.
   * @zh 只能建出扁平命令列表时为 `flat-mainmenu-commands`。
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
   * @zh 为哪些曲目构建菜单：`handles` 用 `handles`，`playlist` 用活动播放列表，`nowPlaying` 用正在播放的曲目，`selection` 用活动播放列表的选中项。其他值（包括 `auto`）按 `handles`、正在播放的曲目、选中项、活动播放列表的顺序取第一个可用的。给了 `handles` 却没有一条可用时，任何模式（包括 `auto`）都以 `INVALID_PARAMS` 失败，不会落到别的曲目上；`nowPlaying` 没有播放、`selection` 没有选中时以 `NO_ACTIVE_ITEM` 失败。
   * @default "auto"
   */
  mode?: string;
  /**
   * Locale for `displayLabel`: `auto` keeps the host's labels; a `zh` or `en` tag translates
   * common labels.
   * @zh `displayLabel` 用的语言：`auto` 保留宿主的标签；`zh` 或 `en` 开头的标签会翻译常见标签。
   * @default "auto"
   */
  locale?: string;
  /**
   * `false` turns the label translation off.
   * @zh 为 `false` 时关闭标签翻译。
   * @default true
   */
  i18n?: boolean;
  /**
   * Add command counts to submenus.
   * @zh 给子菜单加上命令计数。
   * @default true
   */
  withAvailability?: boolean;
  /**
   * Tracks for `mode: "handles"` and `auto`: each a path, optionally ending in `|subsong:N`, or an
   * object `{ path, subsong }`. Paths the security policy refuses and entries of other types are
   * skipped. Each path is checked on disk, so prefer `selection`, `playlist` or `nowPlaying` for
   * tracks the host already lists.
   * @zh `mode: "handles"` 与 `auto` 用的曲目：每项是路径（可带 `|subsong:N` 后缀）或 `{ path, subsong }` 对象。安全策略拒绝的路径与其他类型的条目会被跳过。每个路径都要在磁盘上检查，所以宿主列表里已有的曲目优先用 `selection`、`playlist` 或 `nowPlaying`。
   */
  handles?: Json[];
}

interface GetContextMenuResult {
  /**
   * The mode used; `auto` resolves to one of the others.
   * @zh 实际使用的模式；`auto` 会落到其余某一个。
   */
  mode: 'handles' | 'playlist' | 'nowPlaying' | 'selection';
  /**
   * The `locale` used.
   * @zh 使用的 `locale`。
   */
  locale: string;
  /**
   * The `i18n` used.
   * @zh 使用的 `i18n`。
   */
  i18n: boolean;
  /**
   * The `withAvailability` used.
   * @zh 使用的 `withAvailability`。
   */
  withAvailability: boolean;
  /**
   * The top-level nodes.
   * @zh 顶层节点。
   */
  items: MenuTreeNode[];
}

interface RunContextCommandByIdParams {
  /**
   * The `commandId` of a context-menu node.
   * @zh 右键菜单节点的 `commandId`。
   * @minimum 0
   * @maximum 4294967295
   */
  id: Int;
  /**
   * The `mode` the menu was built with; see `menu.getContextMenu`.
   * @zh 构建菜单时用的 `mode`；见 `menu.getContextMenu`。
   * @default "auto"
   */
  mode?: string;
  /**
   * The `handles` the menu was built with; see `menu.getContextMenu`.
   * @zh 构建菜单时用的 `handles`；见 `menu.getContextMenu`。
   */
  handles?: Json[];
}

interface ShowNativePopupParams {
  /**
   * Tracks to build the menu for; see `menu.getContextMenu`.
   * @zh 为哪些曲目构建菜单；见 `menu.getContextMenu`。
   * @default "auto"
   */
  mode?: string;
  /**
   * Tracks for `mode: "handles"` and `auto`; see `menu.getContextMenu`.
   * @zh `mode: "handles"` 与 `auto` 用的曲目；见 `menu.getContextMenu`。
   */
  handles?: Json[];
  /**
   * Accepted for compatibility and ignored: the menu always opens at the mouse pointer.
   * @zh 为兼容而接受，没有作用：菜单总在鼠标指针处打开。
   */
  x?: Int;
  /**
   * Accepted for compatibility and ignored: the menu always opens at the mouse pointer.
   * @zh 为兼容而接受，没有作用：菜单总在鼠标指针处打开。
   */
  y?: Int;
}

interface ShowParams {
  /**
   * Menu rows; the SDK's `MenuPopupItem` describes their fields. A row icon over 32 KiB is dropped.
   * @zh 菜单行；各字段见 SDK 的 `MenuPopupItem`。超过 32 KiB 的行图标会被丢弃。
   */
  items?: Json[];
  /**
   * Screen x of the anchor in physical pixels; absent or negative uses the mouse pointer.
   * @zh 锚点的屏幕 x，单位为物理像素；不传或为负时用鼠标指针位置。
   */
  x?: Int;
  /**
   * Screen y of the anchor in physical pixels; absent or negative uses the mouse pointer.
   * @zh 锚点的屏幕 y，单位为物理像素；不传或为负时用鼠标指针位置。
   */
  y?: Int;
  /**
   * `contentSized` draws each panel in a window sized to its content; any other value uses one
   * full-screen overlay window.
   * @zh `contentSized` 让每个面板画在按内容定尺寸的窗口里；其他值使用一个全屏覆盖窗口。
   */
  windowModel?: string;
  /**
   * Style sheet applied to the menu on top of the built-in styles, at most 256 KiB.
   * @zh 叠加在内置样式之上的菜单样式表，最多 256 KiB。
   */
  css?: string;
  /**
   * Replace the built-in styles with `css` instead of adding to them.
   * @zh 用 `css` 取代内置样式，而不是叠加在上面。
   */
  cssReplace?: boolean;
  /**
   * Window backdrop: `acrylic` (the default), `mica`, `mica-alt` or `none`. Other values keep
   * the default.
   * @zh 窗口背景材质：`acrylic`（缺省）、`mica`、`mica-alt` 或 `none`。其他值保持缺省。
   */
  backdrop?: string;
  /**
   * Dark tint for the backdrop; the default is `true`.
   * @zh 背景材质用深色调；缺省为 `true`。
   */
  backdropDarkMode?: boolean;
  /**
   * Length of the closing animation in milliseconds, clamped to 0–1000; the default `0` closes
   * at once.
   * @zh 关闭动画的时长，单位毫秒，夹在 0 到 1000 之间；缺省 `0` 表示立即关闭。
   */
  closeAnimationMs?: Int;
}

interface ShowResult {
  /**
   * Id of the menu; `menu:select`, `menu:dismiss` and `menu:valueChanged` carry it.
   * @zh 菜单的 id；`menu:select`、`menu:dismiss` 与 `menu:valueChanged` 都带着它。
   */
  menuId: string;
}

interface CloseParams {
  /**
   * Reason reported in `menu:dismiss`.
   * @zh 在 `menu:dismiss` 里报告的原因。
   * @default "api"
   */
  reason?: string;
}

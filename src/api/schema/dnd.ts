import type { Int } from './common.js';

export interface Api {
  /**
   * Read the real filesystem paths of a drag session from host memory, so the answer does not
   * depend on the delivery order of `dnd:*` messages and stays valid after an `await`. Fails
   * with `NOT_FOUND` when the calling window has no drag-drop registration.
   * @zh 从宿主内存读取一次拖放会话的真实路径，不依赖 `dnd:*` 消息的到达顺序，`await` 之后照样可用。调用窗口没有拖放注册时以 `NOT_FOUND` 失败。
   */
  getPathsAsync(params: GetPathsAsyncParams): GetPathsAsyncResult;

  /**
   * Report what this window's drag-drop integration can currently deliver. Fails with
   * `NOT_FOUND` when the calling window has no drag-drop registration.
   * @zh 报告本窗口的拖放集成当前能提供什么。调用窗口没有拖放注册时以 `NOT_FOUND` 失败。
   */
  getCapabilities(): GetCapabilitiesResult;

  /**
   * Exchange paths for a one-shot token that lets the next drag out of this window carry those
   * files. Parameter shape errors are reported first; then the document origin is checked
   * (`ORIGIN_DENIED`) before any path is resolved or validated, then the window's `dragOut`
   * capability (`NOT_SUPPORTED`), then each path must resolve to a local file (`INVALID_PATH`)
   * and pass path security (`PERMISSION_DENIED`).
   * @zh 用一批路径换一枚一次性 token，供本窗口下一次拖出携带这些文件。参数形状错误最先报告；然后在解析或校验任何路径之前先判定文档 origin（`ORIGIN_DENIED`），再看窗口的 `dragOut` 能力（`NOT_SUPPORTED`），最后每条路径都要能解析成本地文件（`INVALID_PATH`）并通过路径安全检查（`PERMISSION_DENIED`）。
   */
  prepareDrag(params: PrepareDragParams): PrepareDragResult;

  /**
   * Not implemented: always fails with `NOT_SUPPORTED`. Use `dnd.prepareDrag` instead.
   * @zh 未实现：总是以 `NOT_SUPPORTED` 失败。请改用 `dnd.prepareDrag`。
   */
  startDrag(): void;
}

export interface Events {
  /**
   * A drag entered the window. Only the window under the cursor gets it, never another one,
   * because real filesystem paths are sensitive. `paths` is empty when the drag carries no
   * `CF_HDROP` file list (browser links, virtual shell objects, archive entries) or when the
   * document origin is not trusted with real paths.
   * @zh 拖动进入了窗口。只发给光标下的窗口，不发给别的窗口，因为真实路径属于敏感信息。拖动不带 `CF_HDROP` 文件列表（浏览器链接、虚拟 shell 对象、压缩包内条目）或文档 origin 不受信时，`paths` 为空。
   * @delivery window
   */
  enter: EnterPayload;

  /**
   * The drag left the window without dropping.
   * @zh 拖动没有放下就离开了窗口。
   * @delivery window
   */
  leave: LeavePayload;

  /**
   * The drag was dropped on the window. A drop lands only where the page accepts it, by calling
   * `preventDefault()` in its HTML5 `dragover` handler, and elsewhere only when released within
   * half a second of entering, before the page has answered. `paths` is the final list, which the
   * drag source may have changed since `dnd:enter`. The event can arrive before or after the
   * page's own HTML5 `drop` handler runs; call `dnd.getPathsAsync` from that handler when the paths
   * are needed together with the drop.
   * @zh 拖动在窗口上放下。只有页面接受的地方才能放下，即页面在 HTML5 `dragover` 处理函数里调了 `preventDefault()`；别处只有在进入后半秒内、页面还没答复时松手才会放下。`paths` 是最终的列表，拖动源可能在 `dnd:enter` 之后改过它。此事件可能早于也可能晚于页面自己的 HTML5 `drop` 处理函数到达；需要与放下同时拿到路径时，在那个处理函数里调 `dnd.getPathsAsync`。
   * @delivery window
   */
  drop: DropPayload;

  /**
   * What the window's drag-drop integration can deliver changed after the page loaded, for
   * instance when a navigation changes the document origin. The payload has the same fields as
   * `dnd.getCapabilities` returns.
   * @zh 页面加载之后，窗口的拖放集成能提供的能力变了，例如导航改变了文档 origin。载荷与 `dnd.getCapabilities` 的返回字段相同。
   * @delivery window
   */
  capabilitiesChanged: GetCapabilitiesResult;

  /**
   * The host attached no files to a drag out of the window, decided at the moment the drag
   * started. The drag itself goes on as an ordinary page drag without files, so a drop inside the
   * page still works; the token text is blanked first, and in the rare case that it cannot be the
   * drag is cancelled instead. It is not a completion notice: when the host accepts the token and
   * hands the files over, nothing follows, and the page's own `dragend` reports the outcome
   * through `dataTransfer.dropEffect` (`copy` when a target took the files, `none` when the drag
   * was cancelled or refused). Only a drag that carried a drag token can cause it; a page
   * dragging its own text, image or link never does.
   * @zh 宿主在拖出开始时决定不给这次拖动附带文件。拖动本身照常进行，成为不带文件的普通页面拖动，所以在页面内放下仍然有效；宿主会先抹掉 token 文本，极少数抹不掉的情况下改为取消这次拖动。它不是完成通知：宿主接受 token 并交出文件时不会有后续事件，结果由页面自己的 `dragend` 经 `dataTransfer.dropEffect` 给出（目标收下文件为 `copy`，拖动被取消或拒绝为 `none`）。只有带 drag token 的拖动才会引发它；页面拖自己的文字、图片或链接永远不会。
   * @delivery window
   */
  dragEnded: DragEndedPayload;
}

interface EnterPayload {
  /**
   * Ties the `dnd:enter`, `dnd:leave` and `dnd:drop` of one drag together. Unique across the host
   * process, so it also tells which window the drag belongs to.
   * @zh 把同一次拖动的 `dnd:enter`、`dnd:leave` 与 `dnd:drop` 关联起来。在整个宿主进程内唯一，因此也能看出拖动属于哪个窗口。
   */
  sessionId: string;
  /**
   * Absolute filesystem paths, in `DataTransfer.files` order; empty when the drag carries no file
   * list or the document origin is not trusted with paths.
   * @zh 绝对路径，顺序同 `DataTransfer.files`；拖动不带文件列表或文档 origin 不受信时为空。
   */
  paths: string[];
  /**
   * Target of the `.lnk` shortcut at the same index of `paths`, or `null`; always as long as
   * `paths`, including when both are withheld and empty. Only `.lnk` is resolved: a shortcut to a
   * shell object, a target too long to read back intact (Windows caps it at `MAX_PATH`), an
   * unavailable COM apartment and an entry skipped to keep the drop responsive all report
   * `null`, never an empty string. A broken shortcut reports the path it recorded, so a non-null
   * entry says where the shortcut points, not that a file is there.
   * @zh `paths` 同下标处 `.lnk` 快捷方式的目标，或 `null`；长度恒等于 `paths`，两者都被隐去而为空时也一样。只解析 `.lnk`：指向 shell 对象的快捷方式、目标太长读不完整（Windows 以 `MAX_PATH` 为上限）、COM 套间不可用、为保持放下响应而跳过的条目，都报 `null`，从不为空串。失效的快捷方式报它记录的路径，所以非 null 只说明快捷方式指向哪里，不保证那里有文件。
   */
  resolvedPaths: (string | null)[];
  /**
   * Whether the drag carries a `CF_HDROP` file list; reported truthfully even when `paths` is
   * withheld, since it reveals nothing by itself.
   * @zh 拖动是否带 `CF_HDROP` 文件列表；即使 `paths` 被隐去也如实报告，因为它本身不泄露任何信息。
   */
  hasFiles: boolean;
  source: DragSource;
  /**
   * Cursor x in client-area physical pixels; divide by `devicePixelRatio` for CSS pixels.
   * @zh 光标在客户区内的横坐标，单位物理像素；除以 `devicePixelRatio` 得到 CSS 像素。
   */
  x: Int;
  /**
   * Cursor y in client-area physical pixels; divide by `devicePixelRatio` for CSS pixels.
   * @zh 光标在客户区内的纵坐标，单位物理像素；除以 `devicePixelRatio` 得到 CSS 像素。
   */
  y: Int;
}

/**
 * Where the drag came from: `self` when it started in this window's page, `other-window` when it
 * started in another window of the same foobar2000 (the main window or a popup), `external` for
 * everything else, such as Explorer, another application, another foobar2000 process or a drag
 * that started in a Default UI or Columns UI panel. The host reads it from a marker it adds to
 * every drag that starts in a window it hosts; any program can imitate that marker, so treat the
 * value as a hint about the drag's origin, not as a security check.
 * @zh 拖动从哪里来：从本窗口的页面拖出为 `self`；从同一个 foobar2000 的另一个窗口（主窗口或 popup）拖出为 `other-window`；其余都是 `external`，例如资源管理器、别的程序、另一个 foobar2000 进程，以及从 Default UI 或 Columns UI 面板拖出的拖动。宿主给它承载的窗口里开始的每次拖动加一个标记，据此判断；任何程序都能仿造这个标记，所以只把它当作来源提示，不要当作安全检查。
 */
type DragSource = 'self' | 'other-window' | 'external';

interface LeavePayload {
  /**
   * The drag's session, as in `dnd:enter`.
   * @zh 这次拖动的会话，同 `dnd:enter`。
   */
  sessionId: string;
}

interface DropPayload {
  /**
   * The drag's session, as in `dnd:enter`.
   * @zh 这次拖动的会话，同 `dnd:enter`。
   */
  sessionId: string;
  /**
   * Absolute filesystem paths, in `DataTransfer.files` order; empty when the drag carries no file
   * list or the document origin is not trusted with paths.
   * @zh 绝对路径，顺序同 `DataTransfer.files`；拖动不带文件列表或文档 origin 不受信时为空。
   */
  paths: string[];
  /**
   * Target of the `.lnk` shortcut at the same index of `paths`, or `null`; same length and same
   * rules as the field of this name in `dnd:enter`.
   * @zh `paths` 同下标处 `.lnk` 快捷方式的目标，或 `null`；长度与规则同 `dnd:enter` 的同名字段。
   */
  resolvedPaths: (string | null)[];
  /**
   * Cursor x in client-area physical pixels.
   * @zh 光标在客户区内的横坐标，单位物理像素。
   */
  x: Int;
  /**
   * Cursor y in client-area physical pixels.
   * @zh 光标在客户区内的纵坐标，单位物理像素。
   */
  y: Int;
  /**
   * Win32 modifier and mouse-button mask at drop time (`MK_*` flags), for a page that wants
   * modifier-dependent behaviour. It does not change the drop effect reported to the drag source,
   * which is never move or link.
   * @zh 放下时的 Win32 修饰键与鼠标按键掩码（`MK_*` 标志），供需要按修饰键区分行为的页面使用。它不影响报告给拖动源的放置效果，那个从不是移动或链接。
   */
  keyState: Int;
  source: DragSource;
}

interface DragEndedPayload {
  /**
   * Always `failed`: the host reports only refusals.
   * @zh 恒为 `failed`：宿主只报告拒绝。
   */
  result: 'failed';
  /**
   * Why no files were attached: `PERMISSION_DENIED` when the token was unknown, expired, already
   * spent, superseded by a later `prepareDrag` or minted for another window; `INVALID_PARAMS`
   * when `dataTransfer.effectAllowed` was not exactly `copy`, which `dnd.applyDragToken` sets
   * correctly; `OPERATION_FAILED` when the file list could not be attached to the drag.
   * @zh 没有附带文件的原因：token 未知、过期、已用过、被之后的 `prepareDrag` 取代或属于别的窗口时为 `PERMISSION_DENIED`；`dataTransfer.effectAllowed` 不恰好是 `copy` 时为 `INVALID_PARAMS`（`dnd.applyDragToken` 会正确设置它）；文件列表无法附到拖动上时为 `OPERATION_FAILED`。
   */
  code: 'PERMISSION_DENIED' | 'INVALID_PARAMS' | 'OPERATION_FAILED';
  /**
   * Human-readable reason; never contains a filesystem path.
   * @zh 可读的原因；从不包含文件路径。
   */
  error: string;
}

interface GetPathsAsyncParams {
  /**
   * Session to query, from a `dnd:*` payload. Omit it, or pass an empty string, for the session
   * that is active or most recently ended for this window.
   * @zh 要查询的会话，取自 `dnd:*` 载荷。省略或传空串则查本窗口当前活动或最近结束的会话。
   */
  sessionId?: string;
}

interface GetPathsAsyncResult {
  /**
   * Session the paths belong to; an empty string when no session was found, so a caller can
   * tell "nothing to report" from "a session with no files".
   * @zh 路径所属的会话；找不到会话时为空串，调用方由此区分「没有可报的」与「有会话但没有文件」。
   */
  sessionId: string;
  /**
   * Real paths in `DataTransfer.files` order; empty when the session expired, carried no file
   * list, or the document origin is not trusted with paths.
   * @zh 真实路径，顺序同 `DataTransfer.files`；会话过期、没有文件列表或文档 origin 不受信时为空。
   */
  paths: string[];
  /**
   * Target of the `.lnk` shortcut at the same index of `paths`, or `null` when the entry is not
   * a shortcut, points at a shell object rather than a file, has a target too long to read
   * back intact, or could not be resolved; never an empty string. A broken shortcut still
   * reports the path it recorded. Always the same length as `paths`.
   * @zh `paths` 同下标处 `.lnk` 快捷方式的目标；不是快捷方式、指向 shell 对象而非文件、目标太长读不完整或解析不了时为 `null`，从不为空串。失效的快捷方式仍报它记录的路径。长度恒等于 `paths`。
   */
  resolvedPaths: (string | null)[];
  /**
   * Where the session's drag came from, as in `dnd:enter`; absent when no session was found.
   * Reported even when the paths are withheld, since it reveals no path.
   * @zh 该会话的拖动从哪里来，同 `dnd:enter`；找不到会话时没有这个字段。路径被隐去时也照常报告，因为它不泄露任何路径。
   */
  source?: DragSource;
}

interface GetCapabilitiesResult {
  /**
   * The page receives standard HTML5 drag events; `false` means the window has no drag-drop
   * support at all.
   * @zh 页面收得到标准的 HTML5 拖放事件；`false` 表示窗口完全没有拖放支持。
   */
  html5: boolean;
  /**
   * Real filesystem paths are obtainable through `dnd.getPathsAsync` and the `dnd:*` events. Not
   * fixed for the window's lifetime: a navigation or Chromium re-registering its own drop
   * target can withdraw it, announced by `dnd:capabilitiesChanged`.
   * @zh 能经 `dnd.getPathsAsync` 与 `dnd:*` 事件拿到真实路径。在窗口生命周期内并非恒定：导航或 Chromium 重新注册自己的放置目标都会收回它，由 `dnd:capabilitiesChanged` 通知。
   */
  paths: boolean;
  /**
   * How the window hosts its WebView: `visual` for the main and popup windows, `standard` for a
   * DUI or CUI panel.
   * @zh 窗口承载 WebView 的方式：主窗口与弹出窗口为 `visual`，DUI / CUI 面板为 `standard`。
   */
  hosting: 'visual' | 'standard';
  /**
   * Why `paths` is `false`; its presence always means `paths` is `false`.
   * @zh `paths` 为 `false` 的原因；出现即表示 `paths` 为 `false`。
   */
  pathsUnavailableReason?:
    | 'register-failed'
    | 'forward-unavailable'
    | 'inner-target-not-found'
    | 'chain-failed'
    | 'displaced'
    | 'origin-untrusted';
  /**
   * Files can be dragged out of the window through `dnd.prepareDrag`. Independent of `paths`.
   * @zh 能经 `dnd.prepareDrag` 把文件拖出窗口。与 `paths` 无关。
   */
  dragOut: boolean;
  /**
   * Why `dragOut` is `false`; its presence always means `dragOut` is `false`.
   * @zh `dragOut` 为 `false` 的原因；出现即表示 `dragOut` 为 `false`。
   */
  dragOutUnavailableReason?: 'not-visual-hosting' | 'runtime-too-old' | 'register-failed';
}

// `paths` deliberately carries no @security: the MediaRead check must run after the origin
// decision and on the normalised paths, which DndPrepareDrag does by calling ValidatePathParam
// itself. RegisterDndApi asserts that this declaration stays free of path parameters.
interface PrepareDragParams {
  /**
   * Locations to drag out: native paths, `file://` and `file-relative://` URLs, `archive://` /
   * `unpack://` entries, and `path|subsong:N`. File existence is not checked.
   * @zh 要拖出的位置：原生路径、`file://` 与 `file-relative://` URL、`archive://` / `unpack://` 项，以及 `路径|subsong:N`。不检查文件是否存在。
   * @minItems 1
   */
  paths: string[];
}

interface PrepareDragResult {
  /**
   * Opaque one-shot token of 32 lowercase hex characters, valid for 30 seconds, bound to the
   * requesting window and superseded by the next successful call.
   * @zh 不透明的一次性 token，32 个小写十六进制字符，30 秒内有效，绑定请求它的窗口，下一次成功调用会顶掉它。
   */
  token: string;
}

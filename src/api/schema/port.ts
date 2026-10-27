import type { Int, Json } from './common.js';

export interface Api {
  /**
   * Open a port on a named channel for the calling window and announce it with `port:connected`.
   * Any number of ports, from one window or several, can share a channel. A port belongs to the
   * page that opened it and stays open until `port.disconnect`, until that page starts a top-level
   * navigation (a reload or another address; same-document navigation such as a hash change does
   * not count, and a navigation that ends in a download still does), or until its WebView goes
   * away: the popup closes, the panel is removed, or the WebView is rebuilt or crashes beyond
   * recovery. Each of these closes
   * the port with `port:disconnected` before the next page can open ports.
   * @zh 为调用窗口在指定的命名频道上创建并打开通信端口（向所有窗口广播 port:connected 事件；同一频道允许多个窗口的端口共存；端口生命周期绑定至打开它的页面上下文，持续生效直至显式调用 port.disconnect、页面发生顶层导航或宿主 WebView 销毁重构；端口关闭时自动广播 port:disconnected 事件并在后续新页面打开端口前完成清理）
   */
  connect(params: ConnectParams): ConnectResult;

  /**
   * Close a port and announce it with `port:disconnected`. Only the window that opened the port
   * may close it; another window fails with `PERMISSION_DENIED`, and an id no open port has with
   * `PORT_NOT_FOUND`.
   * @zh 关闭指定的通信端口并广播 port:disconnected 事件（仅允许打开该端口的拥有者窗口调用；跨窗口调用将以 PERMISSION_DENIED 拒绝；端口不存在时返回 PORT_NOT_FOUND）
   */
  disconnect(params: DisconnectParams): void;

  /**
   * Send a message to every other port on the sender's channel, as a `port:message` event to each
   * port's window. The sending port must belong to the calling window, otherwise the call fails with
   * `PERMISSION_DENIED`; an unknown sending port fails with `PORT_NOT_FOUND`.
   * @zh 向指定发送端口所在频道内的其他全部对等端口广播消息（以 port:message 事件定向派发给各对等端口所属的窗口；发送端口必须归属于调用窗口，否则返回 PERMISSION_DENIED；端口不存在时返回 PORT_NOT_FOUND）
   */
  postMessage(params: PostMessageParams): PostMessageResult;

  /**
   * Send a message to one port, as a `port:message` event to that port's window. An unknown sending
   * port fails with `PORT_NOT_FOUND`, a sending port of another window with `PERMISSION_DENIED`, an
   * unknown target port with `TARGET_NOT_FOUND`, and a target window that did not take the event
   * with `OPERATION_FAILED`.
   * @zh 向指定目标端口单播发送点对点消息（以 port:message 事件定向送达目标端口所属窗口；发送端口须属于调用窗口，否则返回 PERMISSION_DENIED；发送端口不存在返回 PORT_NOT_FOUND；目标端口不存在返回 TARGET_NOT_FOUND；目标窗口未能接收事件时返回 OPERATION_FAILED）
   */
  postMessageTo(params: PostMessageToParams): void;

  /**
   * List the open ports, of every channel or of one.
   * @zh 获取当前所有活跃端口列表（可指定频道名称仅查询该频道内的端口）
   */
  getPorts(params: GetPortsParams): GetPortsResult;
}

export interface Events {
  /**
   * `port.connect` opened a port. Every window receives it, the opener's included.
   * @zh 端口建立连接广播事件（由 port.connect 触发，向当前所有窗口广播派发）
   * @delivery broadcast
   */
  connected: ConnectedPayload;

  /**
   * A port closed: `port.disconnect` closed it, or the page that opened it started a top-level
   * navigation or went away with its WebView (popup closed, panel removed, WebView rebuilt), which
   * closes all of that page's ports. Every window receives it; a page that closes by navigating
   * receives its own ports' events only if they arrive before it unloads.
   * @zh 端口断开连接广播事件（由 port.disconnect 显式关闭、页面导航或宿主 WebView 销毁触发，向当前所有窗口广播派发）
   * @delivery broadcast
   */
  disconnected: DisconnectedPayload;

  /**
   * A message reached a port. Each receiving port gets its own event, sent to the window that
   * opened it: `port.postMessage` reaches every other port on the sender's channel, the sender's
   * own window included, and `port.postMessageTo` reaches one port.
   * @zh 端口消息到达事件（定向派发给接收端口所属的宿主窗口；通过 port.postMessage 广播时频道内所有对等端口均会收到，通过 port.postMessageTo 单播时仅目标端口收到）
   * @delivery owner
   */
  message: MessagePayload;
}

interface ConnectedPayload {
  /**
   * Id of the new port, as `port.connect` returned it.
   * @zh 新建端口的唯一标识符 portId（与 port.connect 返回一致）
   */
  portId: string;
  /**
   * The channel name.
   * @zh 端口所属的频道名称
   */
  name: string;
  /**
   * Id of the window that opened the port; `main` when the caller cannot be matched to a window.
   * @zh 打开该端口的源窗口标识符 windowId（若无法匹配到具体窗口则固定为 main）
   */
  windowId: string;
}

interface DisconnectedPayload {
  /**
   * Id of the port that closed.
   * @zh 已关闭端口的唯一标识符 portId
   */
  portId: string;
  /**
   * The channel name the port was on.
   * @zh 该端口原本所属的频道名称
   */
  name: string;
  /**
   * Id of the window that opened the port.
   * @zh 打开该端口的源窗口标识符 windowId
   */
  windowId: string;
}

interface MessagePayload {
  /**
   * Id of the receiving port, one this window opened.
   * @zh 接收该消息的本窗口端口标识符 portId
   */
  portId: string;
  /**
   * Id of the sending port.
   * @zh 发送该消息的源端口标识符 sourcePortId
   */
  sourcePortId: string;
  /**
   * Id of the sending page's window; `main` when the sender cannot be matched to a window.
   * @zh 发送方页面所在的源窗口标识符 sourceWindowId（若无法匹配窗口则固定为 main）
   */
  sourceWindowId: string;
  /**
   * The message as the sender passed it; never `null`.
   * @zh 发送方传入的消息正文数据（原样透传，非 null）
   */
  message: Json;
}

/**
 * An open port.
 * @zh 活跃端口信息数据结构
 */
interface PortInfo {
  /**
   * Id of the port; the other port methods take it.
   * @zh 端口唯一标识符 portId
   */
  portId: string;
  /**
   * Id of the window the port belongs to, such as `main`.
   * @zh 端口所属的窗口标识符 windowId（如 main）
   */
  windowId: string;
  /**
   * The channel name.
   * @zh 端口所属的频道名称
   */
  name: string;
}

interface ConnectParams {
  /**
   * Channel name; ports with the same name exchange messages.
   * @zh 待连接的频道名称（同名频道内的端口之间可互相通信）
   * @minLength 1
   */
  name: string;
}

interface ConnectResult {
  /**
   * Id of the new port; the other port methods take it.
   * @zh 新分配的端口唯一标识符 portId
   */
  portId: string;
  /**
   * The channel name.
   * @zh 端口连接的频道名称
   */
  name: string;
  /**
   * Id of the calling window, which the port belongs to.
   * @zh 拥有该端口的调用窗口标识符 windowId
   */
  windowId: string;
}

interface DisconnectParams {
  /**
   * Id of the port to close.
   * @zh 待关闭的端口唯一标识符 portId
   * @minLength 1
   */
  portId: string;
}

interface PostMessageParams {
  /**
   * Id of the sending port.
   * @zh 负责发送消息的源端口标识符 portId
   * @minLength 1
   */
  portId: string;
  /**
   * The message, delivered as `message` of the event. `null` counts as missing.
   * @zh 待发送的消息正文（将作为事件的 message 载荷送达；传入 null 按缺失处理）
   */
  message: Json;
}

interface PostMessageResult {
  /**
   * How many of the other ports' windows took the event.
   * @zh 成功接收并收下该消息事件的对等端口所属窗口总数
   */
  recipients: Int;
}

interface PostMessageToParams {
  /**
   * Id of the sending port.
   * @zh 负责发送消息的源端口标识符 portId
   * @minLength 1
   */
  portId: string;
  /**
   * Id of the port to deliver to.
   * @zh 目标接收端口的唯一标识符 targetPortId
   * @minLength 1
   */
  targetPortId: string;
  /**
   * The message, delivered as `message` of the event. `null` counts as missing.
   * @zh 待发送的消息正文（将作为事件的 message 载荷送达；传入 null 按缺失处理）
   */
  message: Json;
}

interface GetPortsParams {
  /**
   * List only the ports of this channel.
   * @zh 待筛选的目标频道名称（缺省时查询全部频道的端口）
   */
  name?: string;
}

interface GetPortsResult {
  /**
   * The ports, in no particular order.
   * @zh 活跃端口列表（顺序不固定）
   */
  ports: PortInfo[];
}

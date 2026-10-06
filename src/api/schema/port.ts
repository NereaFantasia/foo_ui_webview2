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
   * @zh 为调用窗口在一个命名频道上打开端口，并以 `port:connected` 事件通告。同一频道可以有任意多个端口，来自一个或多个窗口。端口属于打开它的页面，一直开着，直到 `port.disconnect`，或者该页面开始顶层导航（重载或跳到别的地址；锚点变化这类同文档导航不算，最后变成下载的导航也算），或者它的 WebView 消失：popup 关闭、面板移除、WebView 重建或崩溃后无法恢复。这几种情况都会以 `port:disconnected` 关闭端口，且在下一个页面能打开端口之前完成。
   */
  connect(params: ConnectParams): ConnectResult;

  /**
   * Close a port and announce it with `port:disconnected`. Only the window that opened the port
   * may close it; another window fails with `PERMISSION_DENIED`, and an id no open port has with
   * `PORT_NOT_FOUND`.
   * @zh 关闭端口并以 `port:disconnected` 事件通告。只有打开端口的窗口能关闭它；别的窗口以 `PERMISSION_DENIED` 失败，没有打开的端口是该 id 时以 `PORT_NOT_FOUND` 失败。
   */
  disconnect(params: DisconnectParams): void;

  /**
   * Send a message to every other port on the sender's channel, as a `port:message` event to each
   * port's window. The sending port must belong to the calling window, otherwise the call fails with
   * `PERMISSION_DENIED`; an unknown sending port fails with `PORT_NOT_FOUND`.
   * @zh 向发送端口所在频道的其他每个端口发送消息，以 `port:message` 事件送到各端口的窗口。发送端口必须属于调用窗口，否则以 `PERMISSION_DENIED` 失败；发送端口不存在时以 `PORT_NOT_FOUND` 失败。
   */
  postMessage(params: PostMessageParams): PostMessageResult;

  /**
   * Send a message to one port, as a `port:message` event to that port's window. An unknown sending
   * port fails with `PORT_NOT_FOUND`, a sending port of another window with `PERMISSION_DENIED`, an
   * unknown target port with `TARGET_NOT_FOUND`, and a target window that did not take the event
   * with `OPERATION_FAILED`.
   * @zh 向一个端口发送消息，以 `port:message` 事件送到该端口的窗口。发送端口不存在时以 `PORT_NOT_FOUND` 失败，发送端口属于别的窗口时以 `PERMISSION_DENIED` 失败，目标端口不存在时以 `TARGET_NOT_FOUND` 失败，目标窗口没有收下事件时以 `OPERATION_FAILED` 失败。
   */
  postMessageTo(params: PostMessageToParams): void;

  /**
   * List the open ports, of every channel or of one.
   * @zh 列出打开着的端口，全部频道或某一个频道。
   */
  getPorts(params: GetPortsParams): GetPortsResult;
}

export interface Events {
  /**
   * `port.connect` opened a port. Every window receives it, the opener's included.
   * @zh `port.connect` 打开了一个端口。所有窗口都会收到，包括打开它的窗口。
   * @delivery broadcast
   */
  connected: ConnectedPayload;

  /**
   * A port closed: `port.disconnect` closed it, or the page that opened it started a top-level
   * navigation or went away with its WebView (popup closed, panel removed, WebView rebuilt), which
   * closes all of that page's ports. Every window receives it; a page that closes by navigating
   * receives its own ports' events only if they arrive before it unloads.
   * @zh 一个端口关闭了：`port.disconnect` 关闭了它，或者打开它的页面开始顶层导航、随 WebView 一起消失（popup 关闭、面板移除、WebView 重建），这时该页面的端口全部关闭。所有窗口都会收到；因导航而关闭端口的页面，只有在卸载之前到达的事件才收得到。
   * @delivery broadcast
   */
  disconnected: DisconnectedPayload;

  /**
   * A message reached a port. Each receiving port gets its own event, sent to the window that
   * opened it: `port.postMessage` reaches every other port on the sender's channel, the sender's
   * own window included, and `port.postMessageTo` reaches one port.
   * @zh 一条消息到达了某个端口。每个接收端口各收到一个事件，发给打开它的窗口：`port.postMessage` 送到发送端口所在频道的其他每个端口（发送方自己窗口里的也算），`port.postMessageTo` 只送到一个端口。
   * @delivery owner
   */
  message: MessagePayload;
}

interface ConnectedPayload {
  /**
   * Id of the new port, as `port.connect` returned it.
   * @zh 新端口的 id，与 `port.connect` 返回的相同。
   */
  portId: string;
  /**
   * The channel name.
   * @zh 频道名。
   */
  name: string;
  /**
   * Id of the window that opened the port; `main` when the caller cannot be matched to a window.
   * @zh 打开端口的窗口 id；对不上窗口时为 `main`。
   */
  windowId: string;
}

interface DisconnectedPayload {
  /**
   * Id of the port that closed.
   * @zh 关闭的端口 id。
   */
  portId: string;
  /**
   * The channel name the port was on.
   * @zh 端口所在的频道名。
   */
  name: string;
  /**
   * Id of the window that opened the port.
   * @zh 打开该端口的窗口 id。
   */
  windowId: string;
}

interface MessagePayload {
  /**
   * Id of the receiving port, one this window opened.
   * @zh 接收端口的 id，是本窗口打开的端口。
   */
  portId: string;
  /**
   * Id of the sending port.
   * @zh 发送端口的 id。
   */
  sourcePortId: string;
  /**
   * Id of the sending page's window; `main` when the sender cannot be matched to a window.
   * @zh 发送方页面所在窗口的 id；对不上窗口时为 `main`。
   */
  sourceWindowId: string;
  /**
   * The message as the sender passed it; never `null`.
   * @zh 发送方传入的消息，原样送达；不会是 `null`。
   */
  message: Json;
}

/**
 * An open port.
 * @zh 一个打开着的端口。
 */
interface PortInfo {
  /**
   * Id of the port; the other port methods take it.
   * @zh 端口的 id；其他端口方法都用它。
   */
  portId: string;
  /**
   * Id of the window the port belongs to, such as `main`.
   * @zh 端口所属窗口的 id，例如 `main`。
   */
  windowId: string;
  /**
   * The channel name.
   * @zh 频道名。
   */
  name: string;
}

interface ConnectParams {
  /**
   * Channel name; ports with the same name exchange messages.
   * @zh 频道名；同名的端口之间互通消息。
   * @minLength 1
   */
  name: string;
}

interface ConnectResult {
  /**
   * Id of the new port; the other port methods take it.
   * @zh 新端口的 id；其他端口方法都用它。
   */
  portId: string;
  /**
   * The channel name.
   * @zh 频道名。
   */
  name: string;
  /**
   * Id of the calling window, which the port belongs to.
   * @zh 调用窗口的 id，端口归它所有。
   */
  windowId: string;
}

interface DisconnectParams {
  /**
   * Id of the port to close.
   * @zh 要关闭的端口的 id。
   * @minLength 1
   */
  portId: string;
}

interface PostMessageParams {
  /**
   * Id of the sending port.
   * @zh 发送端口的 id。
   * @minLength 1
   */
  portId: string;
  /**
   * The message, delivered as `message` of the event. `null` counts as missing.
   * @zh 消息，作为事件的 `message` 送达。`null` 按没传处理。
   */
  message: Json;
}

interface PostMessageResult {
  /**
   * How many of the other ports' windows took the event.
   * @zh 其他端口的窗口里有多少个收下了事件。
   */
  recipients: Int;
}

interface PostMessageToParams {
  /**
   * Id of the sending port.
   * @zh 发送端口的 id。
   * @minLength 1
   */
  portId: string;
  /**
   * Id of the port to deliver to.
   * @zh 接收端口的 id。
   * @minLength 1
   */
  targetPortId: string;
  /**
   * The message, delivered as `message` of the event. `null` counts as missing.
   * @zh 消息，作为事件的 `message` 送达。`null` 按没传处理。
   */
  message: Json;
}

interface GetPortsParams {
  /**
   * List only the ports of this channel.
   * @zh 只列出该频道的端口。
   */
  name?: string;
}

interface GetPortsResult {
  /**
   * The ports, in no particular order.
   * @zh 端口，不保证顺序。
   */
  ports: PortInfo[];
}

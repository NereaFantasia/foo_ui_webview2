import type { Int, Json } from './common.js';

export interface Api {
  /**
   * Broadcast an event to every window. Receivers get an event named `event` whose data is
   * `{ payload, sourceWindowId }`.
   * @zh 向每个窗口广播事件。接收方收到名为 `event` 的事件，数据为 `{ payload, sourceWindowId }`。
   */
  emit(params: EmitParams): EmitResult;

  /**
   * Send an event to one window. It receives an event named `event` whose data is
   * `{ payload, sourceWindowId }`. A window id no open window has fails with `NOT_FOUND`.
   * @zh 向一个窗口发送事件。它收到名为 `event` 的事件，数据为 `{ payload, sourceWindowId }`。没有打开的窗口是该 id 时以 `NOT_FOUND` 失败。
   */
  emitTo(params: EmitToParams): void;
}

interface EmitParams {
  /**
   * Event name the receivers subscribe to.
   * @zh 接收方订阅的事件名。
   * @minLength 1
   */
  event: string;
  /**
   * Data delivered as `payload`; absent or `null` sends an empty object.
   * @zh 作为 `payload` 送达的数据；不传或为 `null` 时发送空对象。
   */
  payload?: Json;
  /**
   * Leave out the calling window.
   * @zh 不发给调用窗口自己。
   * @default false
   */
  excludeSelf?: boolean;
}

interface EmitResult {
  /**
   * How many windows the event was sent to, counted from the open windows.
   * @zh 事件发往的窗口数，按打开着的窗口计。
   */
  recipients: Int;
}

interface EmitToParams {
  /**
   * Event name the receiver subscribes to.
   * @zh 接收方订阅的事件名。
   * @minLength 1
   */
  event: string;
  /**
   * Id of the window to send to, such as `main` or the `windowId` a port reported.
   * @zh 接收窗口的 id，例如 `main` 或端口报告的 `windowId`。
   * @minLength 1
   */
  targetWindowId: string;
  /**
   * Data delivered as `payload`; absent or `null` sends an empty object.
   * @zh 作为 `payload` 送达的数据；不传或为 `null` 时发送空对象。
   */
  payload?: Json;
}

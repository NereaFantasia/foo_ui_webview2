import type { Int, Json } from './common.js';

export interface Api {
  /**
   * Broadcast an event to every window. Receivers get an event named `event` whose data is
   * `{ payload, sourceWindowId }`.
   * @zh 向所有打开的窗口广播自定义事件（接收方收到以参数 event 为名的事件通知，负载数据为 { payload, sourceWindowId }；可通过 excludeSelf 排除调用方窗口）
   */
  emit(params: EmitParams): EmitResult;

  /**
   * Send an event to one window. It receives an event named `event` whose data is
   * `{ payload, sourceWindowId }`. A window id no open window has fails with `NOT_FOUND`.
   * @zh 向指定窗口定向发送自定义事件（目标窗口收到以参数 event 为名的事件通知，负载数据为 { payload, sourceWindowId }；目标窗口不存在时返回 NOT_FOUND）
   */
  emitTo(params: EmitToParams): void;
}

interface EmitParams {
  /**
   * Event name the receivers subscribe to.
   * @zh 接收方订阅的事件名
   * @minLength 1
   */
  event: string;
  /**
   * Data delivered as `payload`; absent or `null` sends an empty object.
   * @zh 作为 payload 送达的业务数据（不传或为 null 时发送空对象 {}）
   */
  payload?: Json;
  /**
   * Leave out the calling window.
   * @zh 是否排除当前调用方窗口
   * @default false
   */
  excludeSelf?: boolean;
}

interface EmitResult {
  /**
   * How many windows the event was sent to, counted from the open windows.
   * @zh 实际接收事件的打开窗口总数
   */
  recipients: Int;
}

interface EmitToParams {
  /**
   * Event name the receiver subscribes to.
   * @zh 接收方订阅的事件名
   * @minLength 1
   */
  event: string;
  /**
   * Id of the window to send to, such as `main` or the `windowId` a port reported.
   * @zh 目标窗口标识（例如 main 或端口报告的 windowId）
   * @minLength 1
   */
  targetWindowId: string;
  /**
   * Data delivered as `payload`; absent or `null` sends an empty object.
   * @zh 作为 payload 送达的业务数据（不传或为 null 时发送空对象 {}）
   */
  payload?: Json;
}

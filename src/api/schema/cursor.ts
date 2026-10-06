export interface Api {
  /**
   * Hide or restore the client-area cursor of the calling window. Only a call that changes the
   * state announces `cursor:hiddenChanged` to that window; repeating the same value reports
   * `changed: false`. Each window keeps its own state.
   * @zh 隐藏或恢复调用窗口客户区的光标。只有真正改变了状态的调用才向该窗口发 `cursor:hiddenChanged`；重复同一个值报 `changed: false`。每个窗口各自维护状态。
   */
  setHidden(params: SetHiddenParams): SetHiddenResult;

  /**
   * Report whether the calling window's cursor is hidden; `false` when the calling window
   * cannot be resolved.
   * @zh 报告调用窗口的光标是否隐藏；解析不到调用窗口时为 `false`。
   */
  isHidden(): IsHiddenResult;
}

export interface Events {
  /**
   * The calling window's cursor was hidden or restored by `cursor.setHidden`; a call that leaves
   * the state as it was sends nothing.
   * @zh `cursor.setHidden` 隐藏或恢复了调用窗口的光标；没有改变状态的调用不发。
   * @delivery caller
   */
  hiddenChanged: HiddenChangedPayload;
}

interface HiddenChangedPayload {
  /**
   * Whether the cursor is hidden now.
   * @zh 光标现在是否隐藏。
   */
  hidden: boolean;
}

interface SetHiddenParams {
  /**
   * `true` hides the cursor, `false` restores it.
   * @zh `true` 隐藏光标，`false` 恢复。
   */
  hidden: boolean;
}

interface SetHiddenResult {
  /**
   * Whether the state changed; `false` when it was already as requested.
   * @zh 状态是否改变；已经是请求的状态时为 `false`。
   */
  changed: boolean;
}

interface IsHiddenResult {
  /**
   * Whether the cursor is hidden.
   * @zh 光标是否隐藏。
   */
  hidden: boolean;
}

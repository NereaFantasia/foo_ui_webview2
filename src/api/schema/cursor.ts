export interface Api {
  /**
   * Hide or restore the client-area cursor of the calling window. Only a call that changes the
   * state announces `cursor:hiddenChanged` to that window; repeating the same value reports
   * `changed: false`. Each window keeps its own state.
   * @zh 隐藏或恢复调用窗口客户区的光标（仅在实际改变状态时向该窗口派发 cursor:hiddenChanged 事件；重复请求相同状态时返回 changed: false；各窗口独立维护光标状态）
   */
  setHidden(params: SetHiddenParams): SetHiddenResult;

  /**
   * Report whether the calling window's cursor is hidden; `false` when the calling window
   * cannot be resolved.
   * @zh 查询调用窗口的光标当前是否处于隐藏状态（无法解析调用窗口时返回 false）
   */
  isHidden(): IsHiddenResult;
}

export interface Events {
  /**
   * The calling window's cursor was hidden or restored by `cursor.setHidden`; a call that leaves
   * the state as it was sends nothing.
   * @zh 光标隐藏状态变更事件（由 cursor.setHidden 触发状态翻转时派发给调用窗口；未改变实际状态的调用不触发）
   * @delivery caller
   */
  hiddenChanged: HiddenChangedPayload;
}

interface HiddenChangedPayload {
  /**
   * Whether the cursor is hidden now.
   * @zh 翻转后的光标当前是否处于隐藏状态
   */
  hidden: boolean;
}

interface SetHiddenParams {
  /**
   * `true` hides the cursor, `false` restores it.
   * @zh 是否隐藏光标（true 为隐藏，false 为恢复显示）
   */
  hidden: boolean;
}

interface SetHiddenResult {
  /**
   * Whether the state changed; `false` when it was already as requested.
   * @zh 光标隐藏状态是否实际发生改变（若当前已处于目标状态则为 false）
   */
  changed: boolean;
}

interface IsHiddenResult {
  /**
   * Whether the cursor is hidden.
   * @zh 光标当前是否处于隐藏状态
   */
  hidden: boolean;
}

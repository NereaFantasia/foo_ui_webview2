import type { SystemApiInfo } from './common.js';

export interface Events {
  /**
   * An external plugin registered a method, or replaced one it had registered before.
   * `isExternal` is always `true`.
   * @zh 外部插件注册了一个方法，或替换了自己先前注册的方法。`isExternal` 恒为 `true`。
   * @delivery broadcast
   */
  registered: SystemApiInfo;

  /**
   * An external plugin removed one of its methods. Unregistering the whole plugin sends
   * `plugin:unregistered` instead.
   * @zh 外部插件移除了自己的一个方法。注销整个插件时发的是 `plugin:unregistered`。
   * @delivery broadcast
   */
  unregistered: SystemApiInfo;
}

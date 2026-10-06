import type { SystemPluginInfo } from './common.js';

export interface Events {
  /**
   * An external plugin registered its namespace through the C++ `PluginRegistry`. Registering
   * the same namespace again only updates its details and sends nothing. The plugin has no
   * methods yet at this point, so `apis` is empty and `apiCount` is `0`; each method follows as
   * `api:registered`.
   * @zh 外部插件通过 C++ 的 `PluginRegistry` 注册了自己的命名空间。重复注册同一命名空间只更新信息，不发此事件。此时插件还没有方法，`apis` 为空、`apiCount` 为 `0`；之后每个方法各发一次 `api:registered`。
   * @delivery broadcast
   */
  registered: SystemPluginInfo;

  /**
   * An external plugin was unregistered. `apis` lists the methods removed with it; they are not
   * announced one by one with `api:unregistered`.
   * @zh 外部插件被注销。`apis` 列出随它一起移除的方法；这些方法不会逐个发 `api:unregistered`。
   * @delivery broadcast
   */
  unregistered: SystemPluginInfo;
}

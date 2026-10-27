import type { Int, Json } from './common.js';

export interface Api {
  /**
   * Read a value from the state shared by all windows. Expired values are removed first, each
   * announced with `state:deleted` and reason `expired`.
   * @zh 读取所有窗口共享状态中的指定键值；读取前先行清理已过期键，并针对每个清理项广播 `reason` 为 `expired` 的 `state:deleted` 事件
   */
  get(params: GetParams): GetResult;

  /**
   * Store a value in the shared state and announce it with `state:changed`, unless `silent`.
   * @zh 在跨窗口共享状态中存入指定键值；默认广播 `state:changed` 事件通告所有窗口，传入 `silent: true` 时静默写入
   */
  set(params: SetParams): SetResult;

  /**
   * Remove a value from the shared state; when it existed, announce it with `state:deleted` and
   * reason `deleted`.
   * @zh 从跨窗口共享状态中移除指定键；若目标键此前存在，广播 `reason` 为 `deleted` 的 `state:deleted` 事件通告所有窗口
   */
  delete(params: DeleteParams): DeleteResult;

  /**
   * List the keys of the shared state. Expired values are removed first, as in `state.get`.
   * @zh 列出跨窗口共享状态中所有匹配的键名；与 `state.get` 相同，返回前先行清理已过期键
   */
  keys(params: KeysParams): KeysResult;
}

export interface Events {
  /**
   * `state.set` stored a value without `silent`. When the key held a value that had expired but
   * was not swept yet, `previousValue` is that old value, and its expiry is never announced.
   * @zh 调用 `state.set` 写入共享键值（且未设置 `silent: true`）时触发；若写入前目标键已过期但尚未被清理，`previousValue` 仍记录该旧值且不再广播过期事件
   * @delivery broadcast
   */
  changed: ChangedPayload;

  /**
   * A value left the shared state: `state.delete` removed it, or its lifetime ran out. Expired
   * values are swept only when `state.get` or `state.keys` runs, so the event for an expiry
   * comes then rather than at the expiry time; deleting a value that expired but was not swept
   * yet reports `deleted`.
   * @zh 共享状态中的键被显式删除或存活期耗尽过期时触发；过期键仅在调用 `state.get` 或 `state.keys` 触发延迟清理时广播事件（事件时间为清理触发时刻而非实际到期时刻），若主动删除已过期但尚未清理的键，事件原因记为 `deleted`
   * @delivery broadcast
   */
  deleted: DeletedPayload;
}

interface ChangedPayload {
  /**
   * The key.
   * @zh 发生变更的存储键名
   */
  key: string;
  /**
   * The stored value; never `null`, which `state.set` treats as missing.
   * @zh 存入的键值；不会为 `null`（`state.set` 将 `null` 视作参数缺失）
   */
  value: Json;
  /**
   * The value the key held before; `null` when the key is new.
   * @zh 写入前该键原有的旧值；若此前不存在则为 `null`
   */
  previousValue: Json;
  /**
   * Id of the window that stored it; `main` when the caller cannot be matched to a window.
   * @zh 写入该键值的源窗口 ID；无法匹配调用方窗口时为 `main`
   */
  sourceWindowId: string;
  /**
   * When the value expires, in milliseconds since the Unix epoch; present only for a value stored
   * with a positive `ttlMs`.
   * @zh 键值的过期时间戳（自 Unix 纪元起的毫秒数）；仅在写入时指定了正数 `ttlMs` 时提供
   */
  expiresAt?: Int;
}

interface DeletedPayload {
  /**
   * The key.
   * @zh 被删除或过期的存储键名
   */
  key: string;
  /**
   * Id of the window that deleted it, `main` when the caller cannot be matched to a window; `""`
   * for an expiry.
   * @zh 执行删除操作的源窗口 ID；无法匹配调用方窗口时为 `main`，因超时过期被清理时为空字符串 `""`
   */
  sourceWindowId: string;
  /**
   * `deleted` for `state.delete`, `expired` when its lifetime ran out.
   * @zh 移除原因：`deleted` 表示由 `state.delete` 显式删除，`expired` 表示存活期满过期清理
   */
  reason: 'deleted' | 'expired';
}

interface GetParams {
  /**
   * The key.
   * @zh 待读取的存储键名
   * @minLength 1
   */
  key: string;
}

interface GetResult {
  /**
   * Whether the key exists.
   * @zh 目标键是否存在于共享状态中
   */
  exists: boolean;
  /**
   * The value; `null` when the key does not exist.
   * @zh 存储的键值；目标键不存在时为 `null`
   */
  value: Json;
  /**
   * The key; present when it exists.
   * @zh 目标键名；仅在键存在时返回
   */
  key?: string;
  /**
   * When the value expires, in milliseconds since the Unix epoch; present for a value stored with
   * `ttlMs`.
   * @zh 键值的过期时间戳（自 Unix 纪元起的毫秒数）；仅在写入时指定了 `ttlMs` 且尚未过期时返回
   */
  expiresAt?: Int;
}

interface SetParams {
  /**
   * The key.
   * @zh 待存储的键名
   * @minLength 1
   */
  key: string;
  /**
   * The value to store. `null` counts as missing; remove a key with `state.delete`.
   * @zh 待存储的键值；`null` 会被视作参数缺失报错拒绝，如需清除键请调用 `state.delete`
   */
  value: Json;
  /**
   * Store without announcing `state:changed`.
   * @zh 是否静默写入而不广播 `state:changed` 事件
   * @default false
   */
  silent?: boolean;
  /**
   * Lifetime in milliseconds, after which the value is removed; `0` or less stores it without
   * expiry.
   * @zh 键值存活时长（毫秒），超时后将被自动移除；`0` 或负数表示永久保存不过期
   */
  ttlMs?: Int;
}

interface SetResult {
  /**
   * When the value expires, in milliseconds since the Unix epoch; present when `ttlMs` is
   * positive.
   * @zh 键值计算得出的过期时间戳（自 Unix 纪元起的毫秒数）；仅在传入正数 `ttlMs` 时返回
   */
  expiresAt?: Int;
}

interface DeleteParams {
  /**
   * The key.
   * @zh 待删除的存储键名
   * @minLength 1
   */
  key: string;
}

interface DeleteResult {
  /**
   * Whether the key existed before the call.
   * @zh 本次删除操作前该键是否存在于共享状态中
   */
  existed: boolean;
}

interface KeysParams {
  /**
   * `*` for every key, a prefix followed by `*` such as `lyrics:*`, or an exact key.
   * @zh 匹配模式：`*` 匹配所有键，`前缀:*`（如 `lyrics:*`）匹配指定前缀，不带通配符时按完整键名精确匹配
   * @default "*"
   */
  pattern?: string;
}

interface KeysResult {
  /**
   * The matching keys, in no particular order.
   * @zh 匹配到的所有键名列表，不保证特定顺序
   */
  keys: string[];
}

import type { Int, Json } from './common.js';

export interface Api {
  /**
   * Read a value from the state shared by all windows. Expired values are removed first, each
   * announced with `state:deleted` and reason `expired`.
   * @zh 读取所有窗口共享的状态里的一个值。会先移除过期的值，每个都以原因为 `expired` 的 `state:deleted` 事件通告。
   */
  get(params: GetParams): GetResult;

  /**
   * Store a value in the shared state and announce it with `state:changed`, unless `silent`.
   * @zh 在共享状态里存一个值，并以 `state:changed` 事件通告，除非 `silent`。
   */
  set(params: SetParams): SetResult;

  /**
   * Remove a value from the shared state; when it existed, announce it with `state:deleted` and
   * reason `deleted`.
   * @zh 从共享状态里移除一个值；该值存在时以原因为 `deleted` 的 `state:deleted` 事件通告。
   */
  delete(params: DeleteParams): DeleteResult;

  /**
   * List the keys of the shared state. Expired values are removed first, as in `state.get`.
   * @zh 列出共享状态的键。与 `state.get` 一样，会先移除过期的值。
   */
  keys(params: KeysParams): KeysResult;
}

export interface Events {
  /**
   * `state.set` stored a value without `silent`. When the key held a value that had expired but
   * was not swept yet, `previousValue` is that old value, and its expiry is never announced.
   * @zh `state.set` 存了一个值且没带 `silent`。该键原来的值已过期但还没被清扫时，`previousValue` 就是那个旧值，它的过期也不会再通告。
   * @delivery broadcast
   */
  changed: ChangedPayload;

  /**
   * A value left the shared state: `state.delete` removed it, or its lifetime ran out. Expired
   * values are swept only when `state.get` or `state.keys` runs, so the event for an expiry
   * comes then rather than at the expiry time; deleting a value that expired but was not swept
   * yet reports `deleted`.
   * @zh 共享状态里的一个值被移除：`state.delete` 删掉了它，或它的存活期到了。过期的值只在 `state.get` 或 `state.keys` 运行时才清扫，所以过期事件在那时发出，而不是在到期的那一刻；删除一个已过期但还没清扫的值报 `deleted`。
   * @delivery broadcast
   */
  deleted: DeletedPayload;
}

interface ChangedPayload {
  /**
   * The key.
   * @zh 键。
   */
  key: string;
  /**
   * The stored value; never `null`, which `state.set` treats as missing.
   * @zh 存入的值；不会是 `null`，`state.set` 把 null 当作没传。
   */
  value: Json;
  /**
   * The value the key held before; `null` when the key is new.
   * @zh 该键原来的值；新键为 `null`。
   */
  previousValue: Json;
  /**
   * Id of the window that stored it; `main` when the caller cannot be matched to a window.
   * @zh 写入它的窗口 id；对不上窗口时为 `main`。
   */
  sourceWindowId: string;
  /**
   * When the value expires, in milliseconds since the Unix epoch; present only for a value stored
   * with a positive `ttlMs`.
   * @zh 值的过期时间，自 Unix 纪元起的毫秒数；只有用正数 `ttlMs` 存的值才有。
   */
  expiresAt?: Int;
}

interface DeletedPayload {
  /**
   * The key.
   * @zh 键。
   */
  key: string;
  /**
   * Id of the window that deleted it, `main` when the caller cannot be matched to a window; `""`
   * for an expiry.
   * @zh 删除它的窗口 id，对不上窗口时为 `main`；过期时为 `""`。
   */
  sourceWindowId: string;
  /**
   * `deleted` for `state.delete`, `expired` when its lifetime ran out.
   * @zh `state.delete` 删除为 `deleted`，存活期到了为 `expired`。
   */
  reason: 'deleted' | 'expired';
}

interface GetParams {
  /**
   * The key.
   * @zh 键。
   * @minLength 1
   */
  key: string;
}

interface GetResult {
  /**
   * Whether the key exists.
   * @zh 键是否存在。
   */
  exists: boolean;
  /**
   * The value; `null` when the key does not exist.
   * @zh 值；键不存在时为 `null`。
   */
  value: Json;
  /**
   * The key; present when it exists.
   * @zh 键；存在时出现。
   */
  key?: string;
  /**
   * When the value expires, in milliseconds since the Unix epoch; present for a value stored with
   * `ttlMs`.
   * @zh 值的过期时间，自 Unix 纪元起的毫秒数；用 `ttlMs` 存的值才有。
   */
  expiresAt?: Int;
}

interface SetParams {
  /**
   * The key.
   * @zh 键。
   * @minLength 1
   */
  key: string;
  /**
   * The value to store. `null` counts as missing; remove a key with `state.delete`.
   * @zh 要存的值。`null` 按没传处理；移除键用 `state.delete`。
   */
  value: Json;
  /**
   * Store without announcing `state:changed`.
   * @zh 存值但不通告 `state:changed`。
   * @default false
   */
  silent?: boolean;
  /**
   * Lifetime in milliseconds, after which the value is removed; `0` or less stores it without
   * expiry.
   * @zh 存活时长，单位毫秒，过后值被移除；`0` 或负数表示不过期。
   */
  ttlMs?: Int;
}

interface SetResult {
  /**
   * When the value expires, in milliseconds since the Unix epoch; present when `ttlMs` is
   * positive.
   * @zh 值的过期时间，自 Unix 纪元起的毫秒数；`ttlMs` 为正数时出现。
   */
  expiresAt?: Int;
}

interface DeleteParams {
  /**
   * The key.
   * @zh 键。
   * @minLength 1
   */
  key: string;
}

interface DeleteResult {
  /**
   * Whether the key existed before the call.
   * @zh 调用前键是否存在。
   */
  existed: boolean;
}

interface KeysParams {
  /**
   * `*` for every key, a prefix followed by `*` such as `lyrics:*`, or an exact key.
   * @zh `*` 表示全部键，前缀加 `*`（例如 `lyrics:*`）表示该前缀下的键，否则按完整键精确匹配。
   * @default "*"
   */
  pattern?: string;
}

interface KeysResult {
  /**
   * The matching keys, in no particular order.
   * @zh 匹配的键，不保证顺序。
   */
  keys: string[];
}

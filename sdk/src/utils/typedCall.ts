/**
 * Typing for host calls by declared method name, without a runtime
 * dependency on the bridge, so the separately bundled SMP compatibility
 * layer and components can use it around their own invoke function.
 *
 * The method name selects the declared parameter and response types from
 * {@link ApiMethodMap}, so a call site cannot pair a method with the wrong
 * shapes, name a method the host does not declare, or pass an undeclared key
 * in an object literal.
 */

import type { ApiMethodMap } from '../types/generated/index.js';

/** Methods whose declared parameters are all optional may omit the params argument. */
export type CallArgs<M extends keyof ApiMethodMap> =
    Record<never, never> extends ApiMethodMap[M][0]
        ? [params?: ApiMethodMap[M][0]]
        : [params: ApiMethodMap[M][0]];

/** A call function typed by {@link ApiMethodMap}. */
export type TypedCall = <M extends keyof ApiMethodMap>(
    method: M,
    ...args: CallArgs<M>
) => Promise<ApiMethodMap[M][1]>;

/**
 * Wrap a loosely typed invoke function so its calls are checked against
 * {@link ApiMethodMap}. Arity is preserved: a call without params reaches
 * `invoke` with one argument.
 */
export function typedCall<R>(invoke: (method: string, params?: object) => Promise<R>): TypedCall {
    return <M extends keyof ApiMethodMap>(method: M, ...args: CallArgs<M>) =>
        (args.length === 0 ? invoke(method) : invoke(method, args[0])) as Promise<ApiMethodMap[M][1]>;
}

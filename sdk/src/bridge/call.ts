/**
 * Typed host call used by the namespace facades.
 *
 * The method name selects the declared parameter and response types (see
 * `typedCall.ts`). The public `bridge.invoke` stays loosely typed for
 * dynamic dispatch and for methods that are not declared.
 */

import { bridge } from './Bridge.js';
import { typedCall, type TypedCall } from '../utils/typedCall.js';

/**
 * Call a declared host method through the shared {@link bridge}. Reads
 * `bridge.invoke` on every call, so test spies installed on it apply.
 */
export const call: TypedCall = typedCall((...args: [method: string, params?: object]) =>
    bridge.invoke(...args),
);

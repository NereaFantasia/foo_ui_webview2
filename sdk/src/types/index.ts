/**
 * `foo-webview-sdk` — Aggregated type re-export.
 *
 * Use this as the single import point for all SDK shared types:
 *
 * ```ts
 * import type { TrackInfo, FBEventName, FBEventPayloadMap } from
 *     'foo-webview-sdk/types';
 * ```
 *
 * Sub-modules:
 *
 * - {@link "./responses"} — domain models, response shapes, error envelope.
 * - {@link "./events"}    — event names, per-event payload shapes, the
 *                           master {@link "./events".FBEventPayloadMap}.
 * - {@link "./native"}    — `window.fb2k` ambient declaration. Imported here
 *                           for its global side effect (it augments
 *                           `interface Window`); no value re-export.
 *
 * SMP-compatibility types (`SmpEventName`, `SmpCompatApi`, the `declare
 * global` SMP classes) live in `src/smp/types.ts` and are not
 * re-exported here.
 */

// Side-effect import — installs `window.fb2k` typing onto the global
// `Window` interface for every module that depends on this barrel.
import './native.js';

export type * from './responses.js';
export type * from './events.js';
export type { NativeFb2k } from './native.js';

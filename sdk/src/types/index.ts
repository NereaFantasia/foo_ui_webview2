/**
 * `foo-webview-sdk` — Aggregated type re-export.
 *
 * The package root re-exports everything here, so consumers import the types
 * from `foo-webview-sdk` itself:
 *
 * ```ts
 * import type { TrackInfo, FBEventName, FBEventPayloadMap } from
 *     'foo-webview-sdk';
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
// Every declared method's `XxxParams`, `ApiParamsMap` (method name to params) and
// `ApiMethodMap` (method name to `[params, response]`).
export type * from './generated/params.js';
export type { ApiMethodMap } from './generated/index.js';
export type { NativeFb2k } from './native.js';
// The track row shared by every declared endpoint that returns whole tracks, from
// src/api/schema/common.ts.
export type { Track, TrackPartial } from './generated/schema-types.js';

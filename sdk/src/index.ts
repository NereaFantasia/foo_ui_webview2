/**
 * `foo-webview-sdk` — package root entry.
 *
 * Re-exports the full runtime surface — the aggregate `fb` object (also
 * the default export), the singleton `bridge`, the reactive `state`
 * mirror and every namespace proxy — from `./bridge`, alongside the
 * shared public type surface from `./types`. The bare package specifier
 * therefore resolves for both runtime and types:
 *
 * ```ts
 * import fb from 'foo-webview-sdk';
 * import { player, playlist } from 'foo-webview-sdk';
 * import type { TrackInfo, FBEventName } from 'foo-webview-sdk';
 * ```
 *
 * The same runtime bridge stays available under the `./bridge` sub-path;
 * web components and the SMP compatibility layer live under
 * `./components` and `./smp-compat`.
 */

export * from './bridge/index.js';
export { default } from './bridge/index.js';
export type * from './types/index.js';

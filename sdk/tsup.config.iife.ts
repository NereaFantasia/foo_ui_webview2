/**
 * tsup configuration — IIFE bundles for `<script>`-tag consumers.
 *
 * Each IIFE entry requires its own `globalName` so the bundles can be
 * loaded side-by-side without colliding. tsup processes the `array`
 * form of `defineConfig` as a series of independent builds;
 * `clean: false` is essential so each pass keeps the previous pass's
 * `dist/` artefacts.
 *
 * | globalName        | Entry                        | Window slot              |
 * |-------------------|------------------------------|--------------------------|
 * | `fb`              | `src/bridge/iife.ts`         | `window.fb`              |
 * | `__fbComponents`  | `src/components/iife.ts`     | `window.__fbComponents`  |
 * | `__fbSmpCompat`   | `src/smp/iife.ts`            | `window.__fbSmpCompat`   |
 *
 * Components are auto-registered via `customElements.define` as a
 * side effect of loading `dist/components.global.js`; the
 * `__fbComponents` / `__fbSmpCompat` globals are placeholder slots
 * tsup demands and are not part of the public surface — the SMP
 * compat IIFE installs `window.smp` / `window.plman` / `window.utils`
 * / `window.smpUtils` plus the wrapper class globals as side effects
 * during script load.
 */

import { defineConfig, type Options } from 'tsup';

const COMMON: Pick<
    Options,
    'format' | 'clean' | 'sourcemap' | 'target' | 'outDir' | 'dts' | 'splitting' | 'treeshake'
> = {
    format: ['iife'],
    clean: false,
    sourcemap: true,
    target: 'es2020',
    outDir: 'dist',
    // .d.ts is emitted by the ESM config; suppress here to avoid duplicate
    // generation passes per IIFE entry.
    dts: false,
    splitting: false,
    treeshake: true,
};

// Note: tsup automatically appends a `.global.<ext>` suffix to every
// IIFE entry, so the bare entry name (e.g. `bridge`) becomes
// `dist/bridge.global.js` — never include `.global` in the entry key,
// otherwise the suffix doubles up (`bridge.global.global.js`).
export default defineConfig([
    {
        ...COMMON,
        entry: { bridge: 'src/bridge/iife.ts' },
        globalName: 'fb',
    },
    {
        ...COMMON,
        entry: { components: 'src/components/iife.ts' },
        globalName: '__fbComponents',
    },
    {
        ...COMMON,
        entry: { 'smp-compat': 'src/smp/iife.ts' },
        globalName: '__fbSmpCompat',
    },
]);

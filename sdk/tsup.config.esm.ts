/**
 * tsup configuration — ESM + .d.ts output for npm consumers.
 *
 * Each entry is built independently so that `rollup-plugin-dts`
 * operates on a single entry at a time — this eliminates the
 * content-hashed DTS chunk files (e.g. `Bridge-BskUU0nE.d.ts`)
 * that appear when multiple entries share types.  The resulting
 * `.d.ts` files are fully self-contained with stable filenames.
 *
 * IIFE bundles (each requiring its own `globalName`) are produced from
 * the sibling {@link "./tsup.config.iife"} configuration.
 */

import { readFileSync } from 'node:fs';
import { defineConfig, type Options } from 'tsup';

const COMMON: Pick<
    Options,
    'format' | 'dts' | 'sourcemap' | 'target' | 'outDir' | 'splitting' | 'treeshake'
> = {
    format: ['esm'],
    dts: true,
    sourcemap: true,
    target: 'es2020',
    outDir: 'dist',
    splitting: false,
    treeshake: true,
};

/**
 * Extract the `declare global { ... }` augmentation
 * (HTMLElementTagNameMap / HTMLElementEventMap) from the generated
 * `src/components/generated/global.d.ts` for re-injection into
 * `dist/components.d.ts`.
 *
 * Why: the ESM entry activates the augmentation via
 * `import type {} from './generated/global.js'`, but rollup-plugin-dts
 * tree-shakes that empty type-only import, silently dropping the whole
 * `declare global` block from the published bundle — npm consumers lost
 * `document.createElement('fb-...')` typing. The import block of
 * global.d.ts is intentionally stripped here: every referenced Fb* name
 * is re-exported by the components entry itself, so the block resolves
 * against the bundled declarations.
 *
 * Read at config-load time so the footer always tracks the codegen
 * output (`npm run gen:components-global`) without manual sync.
 */
function componentsGlobalFooter(): string {
    const raw = readFileSync(
        new URL('./src/components/generated/global.d.ts', import.meta.url),
        'utf8',
    );
    const start = raw.indexOf('declare global');
    if (start < 0) {
        throw new Error(
            'tsup.config.esm.ts: no `declare global` block found in src/components/generated/global.d.ts — '
            + 'regenerate it via `npm run gen:components-global`',
        );
    }
    // Drop the trailing `export {};` module marker — dist/components.d.ts
    // already has exports, and a duplicate marker is just noise.
    const block = raw.slice(start).replace(/\nexport \{\};?\s*$/, '\n');
    return `\n${block}`;
}

export default defineConfig([
    {
        ...COMMON,
        entry: { index: 'src/index.ts' },
        clean: true,
    },
    {
        ...COMMON,
        entry: { bridge: 'src/bridge/index.ts' },
        clean: false,
    },
    {
        ...COMMON,
        entry: { components: 'src/components/index.ts' },
        dts: { footer: componentsGlobalFooter() },
        clean: false,
    },
    {
        ...COMMON,
        entry: { 'smp-compat': 'src/smp/index.ts' },
        clean: false,
    },
]);

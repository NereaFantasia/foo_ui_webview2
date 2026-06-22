// sdk/vitest.config.ts
//
// Phase 2 test-gate harness. Plan §8.7.1 anchors `sdk/src/**/*.test.ts`
// next to the source files; vitest is the natural fit because we already
// run TypeScript through esbuild (via tsup) and want zero extra
// transpile config.
//
// We deliberately stay on the `node` environment and stub `window` per
// test with `vi.stubGlobal()`, so we don't pull in the heavier
// `jsdom` / `happy-dom` packages just to satisfy a few `typeof window`
// guards inside Bridge.ts.

import { defineConfig } from 'vitest/config';

export default defineConfig({
    test: {
        include: ['src/**/*.test.ts'],
        environment: 'node',
        globals: false,
        clearMocks: true,
        restoreMocks: true,
    },
    esbuild: {
        target: 'es2020',
    },
});

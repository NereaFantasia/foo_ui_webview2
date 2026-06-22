// sdk/src/components/__tests__/FbVolumeControl-rerender.test.ts
//
// Regression guard for AUDIT_REPORT_2026-05-09 Bug #5
// (`docs/report/sdk-source-audit/AUDIT_REPORT_2026-05-09.md` MINOR
// section).
//
// Symptom — `<fb-volume-control>` declares `['vertical', 'no-icon']`
// in `observedAttributes`, but for a long time it had no
// `attributeChangedCallback`, so toggling `vertical` at runtime left
// the previously-orientated fill / thumb inline styles intact:
//
//   const el = document.querySelector('fb-volume-control')!;
//   el.setAttribute('vertical', '');
//   // CSS flips the layout, but `_fillEl.style.width` still holds
//   // the horizontal pct value and `_fillEl.style.height` is empty,
//   // so the vertical fill renders incorrectly until the next
//   // volume change calls `_updateVisual()`.
//
// Fix — implement `attributeChangedCallback()` that calls
// `_updateVisual()` once `_domReady`. This test pins the contract
// statically so any future refactor that drops the callback fails CI
// before the regression ships.
//
// Static analysis (no DOM) — same approach as `no-visual-style.test.ts`.

/// <reference types="vite/client" />

import { describe, expect, it } from 'vitest';

const sourceMap = import.meta.glob('../FbVolumeControl.ts', {
    query: '?raw',
    import: 'default',
    eager: true,
}) as Record<string, string>;

const SOURCE = Object.values(sourceMap)[0];

function stripComments(src: string): string {
    return src
        .replace(/\/\*[\s\S]*?\*\//g, '')
        .replace(/(^|\s)\/\/[^\n]*/g, '$1');
}

describe('FbVolumeControl · attributeChangedCallback re-render contract', () => {
    it('source file is loadable', () => {
        expect(SOURCE).toBeTruthy();
        expect(SOURCE.length).toBeGreaterThan(500);
    });

    it("observes the 'vertical' and 'no-icon' attributes", () => {
        const stripped = stripComments(SOURCE);
        expect(stripped).toMatch(
            /observedAttributes\s*\(\s*\)[\s\S]*?return\s*\[[^\]]*'vertical'/,
        );
        expect(stripped).toMatch(
            /observedAttributes\s*\(\s*\)[\s\S]*?return\s*\[[^\]]*'no-icon'/,
        );
    });

    it('declares an attributeChangedCallback method', () => {
        const stripped = stripComments(SOURCE);
        expect(stripped).toMatch(/attributeChangedCallback\s*\(/);
    });

    it('attributeChangedCallback body invokes _updateVisual()', () => {
        const stripped = stripComments(SOURCE);
        // Match `attributeChangedCallback(...): void { ... }`.
        const m = stripped.match(
            /attributeChangedCallback\s*\([^)]*\)\s*:\s*void\s*\{([\s\S]*?)\n\s*\}/,
        );
        expect(
            m,
            'attributeChangedCallback method body not located — ' +
                'has the signature changed?',
        ).toBeTruthy();
        expect(m![1]).toContain('_updateVisual()');
    });

    it('guards the callback with `_domReady` (no early reflow)', () => {
        const stripped = stripComments(SOURCE);
        const m = stripped.match(
            /attributeChangedCallback\s*\([^)]*\)\s*:\s*void\s*\{([\s\S]*?)\n\s*\}/,
        );
        expect(m).toBeTruthy();
        // The base class (`FbBaseElement`) only flips `_domReady` after
        // `_buildDOM()` runs, so re-painting earlier would touch
        // unitialised refs (`this._fillEl` is `!`-asserted).
        expect(m![1]).toMatch(/if\s*\(\s*!\s*this\._domReady\s*\)\s*return/);
    });
});

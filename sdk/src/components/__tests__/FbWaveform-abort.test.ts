// sdk/src/components/__tests__/FbWaveform-abort.test.ts
//
// The host decodes two full-track waveforms at a time. An element that loads
// again (the playing track changed) or is removed must cancel the host request
// it no longer wants, or that request keeps a decode slot busy and every other
// waveform waits behind it.
//
// Static analysis (no DOM), same approach as `no-visual-style.test.ts`.

/// <reference types="vite/client" />

import { describe, expect, it } from 'vitest';

const sourceMap = import.meta.glob('../FbWaveform.ts', {
    query: '?raw',
    import: 'default',
    eager: true,
}) as Record<string, string>;

const SOURCE = Object.values(sourceMap)[0];

function stripComments(src: string): string {
    return src.replace(/\/\*[\s\S]*?\*\//g, '').replace(/(^|\s)\/\/[^\n]*/g, '$1');
}

function methodBody(name: string): string {
    const stripped = stripComments(SOURCE);
    const start = stripped.search(new RegExp(`\\n\\s{4}(?:override\\s+|private\\s+async\\s+)?${name}\\(`));
    expect(start, `${name} not located`).toBeGreaterThan(-1);
    const next = stripped.slice(start + 1).search(/\n\s{4}(?:override|private|protected|public|static|get|set)\s/);
    return next < 0 ? stripped.slice(start) : stripped.slice(start, start + 1 + next);
}

describe('FbWaveform · cancels superseded host requests', () => {
    it('aborts the previous request before starting a new load and passes the new signal', () => {
        const body = methodBody('_loadWaveform');
        const abortPrevious = body.search(/this\._loadAbort\?\.abort\(\)/);
        const create = body.search(/new AbortController\(\)/);
        const request = body.search(/fb\.audio\.generateFullWaveform\(/);
        expect(abortPrevious, 'previous request is not aborted').toBeGreaterThan(-1);
        expect(create).toBeGreaterThan(abortPrevious);
        expect(request).toBeGreaterThan(create);
        expect(body).toMatch(/this\._loadAbort\s*=\s*abort;/);
        expect(body).toMatch(/signal:\s*abort\.signal/);
    });

    it('aborts the request in flight on disconnect', () => {
        const body = methodBody('disconnectedCallback');
        expect(body).toMatch(/this\._loadAbort\?\.abort\(\)/);
    });
});

// sdk/src/components/__tests__/FbSpectrumVisualizer-frames.test.ts
//
// Every spectrum subscription on `audio:spectrum` receives its own frames,
// so an element that drew every frame on that event would draw another
// subscription's band count (a 64-band element next to a 256-band
// subscription would flip between 64 and 256 bars). The element must drop
// frames tagged with another subscription's id and keep drawing untagged
// frames from hosts that merge subscriptions.
//
// Static analysis (no DOM), same approach as `no-visual-style.test.ts`.

/// <reference types="vite/client" />

import { describe, expect, it } from 'vitest';

const sourceMap = import.meta.glob('../FbSpectrumVisualizer.ts', {
    query: '?raw',
    import: 'default',
    eager: true,
}) as Record<string, string>;

const SOURCE = Object.values(sourceMap)[0];

function stripComments(src: string): string {
    return src.replace(/\/\*[\s\S]*?\*\//g, '').replace(/(^|\s)\/\/[^\n]*/g, '$1');
}

function spectrumHandlerBody(): string {
    const stripped = stripComments(SOURCE);
    const m = stripped.match(/this\._sub\(\s*'audio:spectrum'[\s\S]*?\n\s{8}\}\);/);
    expect(m, "`this._sub('audio:spectrum', ...)` handler not located").toBeTruthy();
    return m![0];
}

describe('FbSpectrumVisualizer · per-subscription frames', () => {
    it('subscribes and unsubscribes with its own subscriptionId', () => {
        const stripped = stripComments(SOURCE);
        expect(stripped).toMatch(/'audio\.subscribeSpectrum'[\s\S]*?subscriptionId:\s*this\._subscriptionId/);
        expect(stripped).toMatch(/'audio\.unsubscribeSpectrum'[\s\S]*?subscriptionId:\s*this\._subscriptionId/);
    });

    it("drops frames tagged with another subscription's id before drawing", () => {
        const body = spectrumHandlerBody();
        const guard = body.search(/subscriptionId\s*!==\s*this\._subscriptionId/);
        const draw = body.search(/this\._spectrum\s*=/);
        expect(guard, 'foreign-frame guard missing').toBeGreaterThan(-1);
        expect(draw).toBeGreaterThan(guard);
        expect(body.slice(guard, draw)).toMatch(/return;/);
    });

    it('keeps untagged frames: the guard only applies when subscriptionId is a string', () => {
        const body = spectrumHandlerBody();
        expect(body).toMatch(/typeof\s+payload\?\.subscriptionId\s*===\s*'string'/);
    });
});

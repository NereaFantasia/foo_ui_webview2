// sdk/src/smp/classes/FbProfiler.test.ts
//
// Phase 5.1 test gate — FbProfiler stopwatch contract.

import { describe, expect, it, vi } from 'vitest';

import { FbProfiler } from './FbProfiler.js';

describe('FbProfiler', () => {
    it('starts at zero immediately after construction', () => {
        const p = new FbProfiler('test');
        expect(p.Time).toBeGreaterThanOrEqual(0);
        expect(p.Time).toBeLessThan(50);
    });

    it('Reset rewinds the start baseline', async () => {
        const p = new FbProfiler();
        await new Promise((r) => setTimeout(r, 20));
        p.Reset();
        expect(p.Time).toBeLessThan(20);
    });

    it('Print logs through console.log without throwing', () => {
        const p = new FbProfiler('demo');
        const spy = vi.spyOn(console, 'log').mockImplementation(() => {});
        p.Print();
        expect(spy).toHaveBeenCalledTimes(1);
        const msg = spy.mock.calls[0]?.[0];
        expect(typeof msg).toBe('string');
        expect(msg).toContain('demo');
        spy.mockRestore();
    });
});

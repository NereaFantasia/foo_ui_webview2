import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

describe('discovery main-menu execution', () => {
    beforeEach(() => vi.resetModules());
    afterEach(() => vi.unstubAllGlobals());

    it.each([undefined, '{CHILD}'])('keeps the GUID pair and result shape: %s', async (subGuid) => {
        const result = subGuid
            ? { success: true, guid: '{OWNER}', dynamic: true, subGuid }
            : { success: true, guid: '{OWNER}', dynamic: false };
        const invoke = vi.fn().mockResolvedValue(result);
        vi.stubGlobal('window', { fb2k: { invoke, on: vi.fn(), off: vi.fn() } });
        const { discovery } = await import('./discovery.js');
        expect(await discovery.executeMainMenuCommand('{OWNER}', subGuid)).toBe(result);
        expect(invoke).toHaveBeenCalledWith('discovery.executeMainMenuCommand',
            subGuid ? { guid: '{OWNER}', subGuid } : { guid: '{OWNER}' });
        expect(result).not.toHaveProperty('force');
    });
});

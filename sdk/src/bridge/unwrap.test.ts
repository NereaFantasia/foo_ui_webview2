// sdk/src/bridge/unwrap.test.ts
//
// unwrap(): the success branch or one of its fields for a success envelope,
// an ApiCallError carrying code and details for a failure envelope.

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { ApiCallError, unwrap } from './unwrap.js';

describe('unwrap', () => {
    it('returns the success branch, or one field of it', () => {
        const res = { success: true as const, playlists: [{ name: 'A' }], count: 1 };
        expect(unwrap(res)).toBe(res);
        expect(unwrap(res, 'playlists')).toEqual([{ name: 'A' }]);
    });

    it('throws an ApiCallError with the failure code, message and details', () => {
        const res = {
            success: false as const,
            error: 'Playlist is locked',
            code: 'LOCKED',
            details: { playlist: 2, isLocked: true },
        };
        let thrown: unknown;
        try {
            unwrap(res);
        } catch (e) {
            thrown = e;
        }
        expect(thrown).toBeInstanceOf(ApiCallError);
        expect(thrown).toBeInstanceOf(Error);
        const err = thrown as ApiCallError;
        expect(err.message).toBe('Playlist is locked');
        expect(err.code).toBe('LOCKED');
        expect(err.details).toEqual({ playlist: 2, isLocked: true });
        expect(err.name).toBe('ApiCallError');
    });
});

describe('unwrap with a namespace call', () => {
    beforeEach(() => vi.resetModules());
    afterEach(() => vi.unstubAllGlobals());

    it('turns the no-host failure into NOT_SUPPORTED', async () => {
        vi.useFakeTimers();
        vi.stubGlobal('window', {});
        const { playlist } = await import('./namespaces/playlist.js');
        const pending = playlist.getAll();
        await vi.advanceTimersByTimeAsync(200);
        const res = await pending;
        vi.useRealTimers();
        expect(() => unwrap(res, 'playlists')).toThrow(ApiCallError);
        try {
            unwrap(res, 'playlists');
        } catch (e) {
            expect((e as ApiCallError).code).toBe('NOT_SUPPORTED');
        }
    });
});

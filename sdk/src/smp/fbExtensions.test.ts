// sdk/src/smp/fbExtensions.test.ts
//
// `fb.GetQueryItems` / `fb.GetLibraryItems` read contracts.
//
// GetQueryItems issues at most two `library.search` calls regardless of
// hit count: a one-row probe for the total, then a single projected
// fetch. GetLibraryItems keeps paging `library.getAll` in 500-row
// chunks because that pipeline has no projection and a whole-library
// single shot would materialise it all at once.

import { describe, expect, it, vi } from 'vitest';

import { createInitialCache } from './cache.js';
import { attachFbExtensions } from './fbExtensions.js';
import type { SmpBridgeShape } from './bridgeShape.js';
import type { FbMetadbHandleList } from './classes/FbMetadbHandleList.js';
import type { SMPPlman } from './types.js';

interface InvokeCall {
    method: string;
    params: Record<string, unknown>;
}

interface FbExtSurface {
    GetQueryItems: (
        handlesLike: unknown,
        query: string,
    ) => Promise<FbMetadbHandleList>;
    GetLibraryItems: () => Promise<FbMetadbHandleList>;
}

/** Build a track row shaped like the projected `library.search` output. */
function row(path: string, subsong = 0, duration = 1, fileSize = 2): object {
    return { absolutePath: path, path, subsong, duration, fileSize };
}

/**
 * Attach the extensions onto a bridge whose `invoke` is recorded, and
 * return both the extension surface and the call log.
 */
function setup(
    responder: (call: InvokeCall) => unknown,
): { ext: FbExtSurface; calls: InvokeCall[] } {
    const calls: InvokeCall[] = [];
    const fb = {
        invoke: vi.fn(async (method: string, params?: object) => {
            const call = {
                method,
                params: (params ?? {}) as Record<string, unknown>,
            };
            calls.push(call);
            return responder(call);
        }),
        on: vi.fn(() => vi.fn()),
        off: vi.fn(),
        once: vi.fn(),
    } as unknown as SmpBridgeShape;

    attachFbExtensions(
        fb,
        {} as unknown as SMPPlman,
        createInitialCache(),
        () => {},
    );

    return { ext: fb as unknown as FbExtSurface, calls };
}

/** Call log filtered to one method. */
function callsTo(calls: InvokeCall[], method: string): InvokeCall[] {
    return calls.filter((c) => c.method === method);
}

describe('fb.GetQueryItems', () => {
    it('makes no bridge call for an empty query', async () => {
        const { ext, calls } = setup(() => ({}));

        const list = await ext.GetQueryItems(null, '');

        expect(list.Count).toBe(0);
        expect(calls).toHaveLength(0);
    });

    it('probes with limit=1 then fetches every hit in one call', async () => {
        const { ext, calls } = setup((call) => {
            if (call.params.limit === 1) return { total: 3 };
            return {
                tracks: [row('C:\\a.flac'), row('C:\\b.flac'), row('C:\\c.flac')],
                total: 3,
            };
        });

        const list = await ext.GetQueryItems(null, '%genre% IS Jazz');
        const searches = callsTo(calls, 'library.search');

        expect(searches).toHaveLength(2);
        expect(searches[0].params).toEqual({
            query: '%genre% IS Jazz',
            offset: 0,
            limit: 1,
        });
        expect(searches[1].params).toEqual({
            query: '%genre% IS Jazz',
            offset: 0,
            limit: 3,
            fields: ['absolutePath', 'path', 'subsong', 'duration', 'fileSize'],
        });
        expect(list.Count).toBe(3);
    });

    it('stays at two calls when the hit count is large', async () => {
        const hits = 8000;
        const { ext, calls } = setup((call) => {
            if (call.params.limit === 1) return { total: hits };
            const tracks = Array.from({ length: hits }, (_v, i) =>
                row(`C:\\track-${i}.flac`),
            );
            return { tracks, total: hits };
        });

        const list = await ext.GetQueryItems(null, 'lossless');

        expect(callsTo(calls, 'library.search')).toHaveLength(2);
        expect(list.Count).toBe(hits);
    });

    it('stops after the probe when nothing matches', async () => {
        const { ext, calls } = setup(() => ({
            tracks: [],
            total: 0,
        }));

        const list = await ext.GetQueryItems(null, 'no such thing');

        expect(list.Count).toBe(0);
        expect(callsTo(calls, 'library.search')).toHaveLength(1);
    });

    it('stops after the probe when the response carries no total', async () => {
        const { ext, calls } = setup(() => ({ success: false, error: 'boom' }));

        const list = await ext.GetQueryItems(null, 'bad ( query');

        expect(list.Count).toBe(0);
        expect(callsTo(calls, 'library.search')).toHaveLength(1);
    });

    it('prefers tracks over items on pre-1.13 hosts that still send both', async () => {
        const { ext } = setup((call) => {
            if (call.params.limit === 1) return { total: 2 };
            return {
                tracks: [row('C:\\x.flac'), row('C:\\y.flac')],
                items: [row('C:\\wrong.flac'), row('C:\\wrong2.flac')],
                total: 2,
            };
        });

        const list = await ext.GetQueryItems(null, 'anything');

        expect(list.Count).toBe(2);
        expect(list[0].Path).toBe('C:\\x.flac');
        expect(list[1].Path).toBe('C:\\y.flac');
    });

    it('keeps hit order and every handle property the projection backs', async () => {
        const { ext } = setup((call) => {
            if (call.params.limit === 1) return { total: 2 };
            const tracks = [
                row('C:\\first.flac', 0, 120.5, 4096),
                row('C:\\second.flac', 3, 61.25, 2048),
            ];
            return { tracks, total: 2 };
        });

        const list = await ext.GetQueryItems(null, 'ordered');

        expect(list.Count).toBe(2);
        expect(list[0].Path).toBe('C:\\first.flac');
        expect(list[0].SubSong).toBe(0);
        expect(list[0].Length).toBe(120.5);
        expect(list[0].FileSize).toBe(4096);
        expect(list[1].Path).toBe('C:\\second.flac');
        expect(list[1].SubSong).toBe(3);
        expect(list.CalcTotalDuration()).toBeCloseTo(181.75, 5);
        expect(list.CalcTotalSize()).toBe(6144);
    });
});

describe('fb.GetLibraryItems', () => {
    it('keeps paging library.getAll in 500-row chunks', async () => {
        const total = 1200;
        const { ext, calls } = setup((call) => {
            const offset = Number(call.params.offset ?? 0);
            const limit = Number(call.params.limit ?? 0);
            const count = Math.max(0, Math.min(limit, total - offset));
            const tracks = Array.from({ length: count }, (_v, i) =>
                row(`C:\\lib-${offset + i}.flac`),
            );
            return { tracks, total };
        });

        const list = await ext.GetLibraryItems();
        const pages = callsTo(calls, 'library.getAll');

        expect(pages).toHaveLength(3);
        expect(pages.map((p) => p.params.limit)).toEqual([500, 500, 500]);
        expect(pages.map((p) => p.params.offset)).toEqual([0, 500, 1000]);
        expect(list.Count).toBe(total);
        expect(pages.every((p) => !('fields' in p.params))).toBe(true);
    });
});

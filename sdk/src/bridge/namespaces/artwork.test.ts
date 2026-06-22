// sdk/src/bridge/namespaces/artwork.test.ts
//
// Phase 2 test gate — `artwork.withMaxSize` is the simplest of the seven
// "complex" methods (plan §588 / §596 — pure URL utility) and the one
// most likely to silently regress because it is just string manipulation
// without a host round-trip.
//
// Phase 5 §5.4 — also lock `artwork.getFb2kUrlByPathBatch` payload shape
// so the C++ contract (`{ paths | items, type, maxSize }`) cannot drift.

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import { artwork } from './artwork.js';

interface MockNative {
    invoke: ReturnType<typeof vi.fn>;
    on: ReturnType<typeof vi.fn>;
    off: ReturnType<typeof vi.fn>;
    _handleResponse: () => void;
}

function makeNative(): MockNative {
    return {
        invoke: vi.fn(),
        on: vi.fn(),
        off: vi.fn(),
        _handleResponse: () => {
            /* dummy */
        },
    };
}

describe('artwork.withMaxSize', () => {
    it('returns the original URL untouched on missing or zero maxSize', () => {
        expect(artwork.withMaxSize('fb2k://art', 0)).toBe('fb2k://art');
        expect(artwork.withMaxSize('fb2k://art', undefined)).toBe('fb2k://art');
        expect(artwork.withMaxSize('', 256)).toBe('');
    });

    it('appends ?maxSize for URLs without an existing query string', () => {
        expect(artwork.withMaxSize('fb2k://art', 256)).toBe(
            'fb2k://art?maxSize=256',
        );
    });

    it('appends &maxSize for URLs that already carry a query string', () => {
        expect(artwork.withMaxSize('fb2k://art?type=front', 256)).toBe(
            'fb2k://art?type=front&maxSize=256',
        );
    });

    it('preserves URL fragments', () => {
        // The legacy bridge.js implementation appends maxSize to the
        // *path* portion, not after the fragment, so the fragment stays
        // at the end. This regression-locks that ordering.
        expect(artwork.withMaxSize('fb2k://art#fragment', 256)).toBe(
            'fb2k://art#fragment?maxSize=256',
        );
    });
});

describe('artwork.getFb2kUrlByPathBatch (§5.4)', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('forwards bare-string paths under the `paths` key (§5.4 BLOCKER)', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, artworks: [] });
        vi.stubGlobal('window', { fb2k: native });
        const { artwork: aw } = await import('./artwork.js');

        await aw.getFb2kUrlByPathBatch(['/a.flac', '/b.flac']);

        expect(native.invoke).toHaveBeenCalledWith(
            'artwork.getFb2kUrlByPathBatch',
            { paths: ['/a.flac', '/b.flac'] },
        );
        // Regression guard: must not also send `items` (the C++ handler
        // accepts either, never both).
        const call = native.invoke.mock.calls[0];
        expect(call[1]).not.toHaveProperty('items');
    });

    it('forwards object items under the `items` key', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, artworks: [] });
        vi.stubGlobal('window', { fb2k: native });
        const { artwork: aw } = await import('./artwork.js');

        await aw.getFb2kUrlByPathBatch([
            { path: '/a.flac', type: 'back' },
            { path: '/b.flac', maxSize: 512 },
        ]);

        expect(native.invoke.mock.calls[0][1]).toMatchObject({
            items: [
                { path: '/a.flac', type: 'back' },
                { path: '/b.flac', maxSize: 512 },
            ],
        });
    });

    it('threads batch-wide type and maxSize options', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, artworks: [] });
        vi.stubGlobal('window', { fb2k: native });
        const { artwork: aw } = await import('./artwork.js');

        await aw.getFb2kUrlByPathBatch(['/a.flac'], {
            type: 'front',
            maxSize: 256,
        });

        expect(native.invoke.mock.calls[0][1]).toEqual({
            paths: ['/a.flac'],
            type: 'front',
            maxSize: 256,
        });
    });

    it('returns the `{ artworks }` envelope verbatim', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: true,
            artworks: [
                {
                    path: '/a.flac',
                    available: true,
                    type: 'front',
                    dataUrl: 'fb2k://artwork/?path=/a.flac&type=front',
                },
                { available: false, error: 'invalid path' },
            ],
        });
        vi.stubGlobal('window', { fb2k: native });
        const { artwork: aw } = await import('./artwork.js');

        const result = await aw.getFb2kUrlByPathBatch(['/a.flac', '']);
        expect(result?.artworks).toHaveLength(2);
        expect(result?.artworks[0]).toMatchObject({
            available: true,
            dataUrl: expect.stringContaining('fb2k://artwork'),
        });
        expect(result?.artworks[1].available).toBe(false);
    });
});

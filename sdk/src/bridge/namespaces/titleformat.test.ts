// sdk/src/bridge/namespaces/titleformat.test.ts
//
// Regression guards — `titleformat.evalFields` and
// `titleformat.evalFieldsBatch` previously sent `fields: string[]` to
// the host, but the C++ handler expects `fields: { fieldName: pattern,
// ... }`. This test gate locks the new contract:
//
//   1. evalFields/evalFieldsBatch send `{ path, fields }` (or
//      `{ paths, fields }`) where `fields` is a plain object map.
//   2. The wrappers return the flat envelope verbatim — fieldName keys
//      sit at the top level (or per-row) alongside the `path`/`success`
//      reply metadata.
//   3. The `infoAvailable` readiness flag survives passthrough: `false`
//      stays `false` (not `undefined`), and a failed row keeps it absent
//      rather than coerced to `false`.

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

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

describe('titleformat namespace — §5.4 fields contract', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('evalFields sends fields as a `{ name: pattern }` map', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: true,
            path: '/track.flac',
            artist: 'Band',
            year: '2026',
        });
        vi.stubGlobal('window', { fb2k: native });
        const { titleformat } = await import('./titleformat.js');

        const result = await titleformat.evalFields('/track.flac', {
            artist: '%artist%',
            year: '$year(%date%)',
        });

        expect(native.invoke).toHaveBeenCalledWith('titleformat.evalFields', {
            path: '/track.flac',
            fields: {
                artist: '%artist%',
                year: '$year(%date%)',
            },
        });
        // Field map must NOT be flattened to an array (§5.4 contract).
        const call = native.invoke.mock.calls[0];
        expect(Array.isArray(call[1].fields)).toBe(false);
        expect(typeof call[1].fields).toBe('object');

        // Envelope keys round-trip verbatim (TitleformatFieldsResult).
        expect(result?.success).toBe(true);
        expect(result?.path).toBe('/track.flac');
        expect((result as Record<string, unknown>).artist).toBe('Band');
        expect((result as Record<string, unknown>).year).toBe('2026');
    });

    it('evalFieldsBatch sends `{ paths, fields }` and returns the per-row envelope', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: true,
            total: 2,
            successCount: 2,
            errorCount: 0,
            results: [
                {
                    path: '/a.flac',
                    success: true,
                    artist: 'Band A',
                    year: '2026',
                },
                {
                    path: '/b.flac',
                    success: true,
                    artist: 'Band B',
                    year: '2025',
                },
            ],
        });
        vi.stubGlobal('window', { fb2k: native });
        const { titleformat } = await import('./titleformat.js');

        const result = await titleformat.evalFieldsBatch(
            ['/a.flac', '/b.flac'],
            { artist: '%artist%', year: '$year(%date%)' },
        );

        expect(native.invoke).toHaveBeenCalledWith(
            'titleformat.evalFieldsBatch',
            {
                paths: ['/a.flac', '/b.flac'],
                fields: {
                    artist: '%artist%',
                    year: '$year(%date%)',
                },
            },
        );
        expect(result?.results).toHaveLength(2);
        expect(
            (result?.results[0] as Record<string, unknown>).artist,
        ).toBe('Band A');
        expect(result?.successCount).toBe(2);
        expect(result?.errorCount).toBe(0);
    });

    it('passes the infoAvailable flag through verbatim (top level)', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: true,
            path: '/unindexed.flac',
            bitrate: '',
            infoAvailable: false,
        });
        vi.stubGlobal('window', { fb2k: native });
        const { titleformat } = await import('./titleformat.js');

        const result = await titleformat.evalFields('/unindexed.flac', {
            bitrate: '%bitrate%',
        });

        // The wrapper must not normalise, default or drop the flag —
        // `false` has to survive as `false`, not become `undefined`.
        expect(result?.infoAvailable).toBe(false);
        expect('infoAvailable' in (result as object)).toBe(true);
    });

    it('passes the infoAvailable flag through verbatim (per row)', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: true,
            total: 3,
            successCount: 2,
            errorCount: 1,
            results: [
                { path: '/indexed.flac', success: true, infoAvailable: true },
                { path: '/unindexed.flac', success: true, infoAvailable: false },
                // Failed rows carry no flag at all.
                { path: '/missing.flac', success: false, error: 'Failed to open file' },
            ],
        });
        vi.stubGlobal('window', { fb2k: native });
        const { titleformat } = await import('./titleformat.js');

        const result = await titleformat.evalFieldsBatch(
            ['/indexed.flac', '/unindexed.flac', '/missing.flac'],
            { bitrate: '%bitrate%' },
        );

        const rows = result?.results ?? [];
        expect(rows[0]?.infoAvailable).toBe(true);
        expect(rows[1]?.infoAvailable).toBe(false);
        expect('infoAvailable' in (rows[1] as object)).toBe(true);
        // Absent on a failed row, not coerced to `false`.
        expect(rows[2]?.infoAvailable).toBeUndefined();
        expect('infoAvailable' in (rows[2] as object)).toBe(false);
    });
});

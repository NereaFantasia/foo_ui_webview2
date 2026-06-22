// sdk/src/bridge/namespaces/titleformat.test.ts
//
// Phase 5 §5.4 regression guards — `titleformat.evalFields` and
// `titleformat.evalFieldsBatch` previously sent `fields: string[]` to
// the host, but the C++ handler expects `fields: { fieldName: pattern,
// ... }`. This test gate locks the new contract:
//
//   1. evalFields/evalFieldsBatch send `{ path, fields }` (or
//      `{ paths, fields }`) where `fields` is a plain object map.
//   2. The wrappers return the flat envelope verbatim — fieldName keys
//      sit at the top level (or per-row) alongside the `path`/`success`
//      reply metadata.

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
});

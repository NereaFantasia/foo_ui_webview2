import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

interface MockNative {
    invoke: ReturnType<typeof vi.fn>;
    on: ReturnType<typeof vi.fn>;
    off: ReturnType<typeof vi.fn>;
    _handleResponse: () => void;
}

function makeNative(): MockNative {
    return {
        invoke: vi.fn().mockResolvedValue({ success: true, albums: [] }),
        on: vi.fn(),
        off: vi.fn(),
        _handleResponse: () => {
            /* dummy */
        },
    };
}

describe('library.getArtistAlbums', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('forwards `sort` and `match` from the trailing options argument', async () => {
        const native = makeNative();
        vi.stubGlobal('window', { fb2k: native });
        const { library } = await import('./library.js');

        await library.getArtistAlbums('A', 20, {
            sort: 'year',
            match: 'substring',
        });

        expect(native.invoke).toHaveBeenCalledWith('library.getArtistAlbums', {
            artist: 'A',
            limit: 20,
            sort: 'year',
            match: 'substring',
        });
    });

    // The host refuses an unrecognised `match` with a bare
    // `{ success: false, error }`, so the wrapper must not silently drop or
    // rewrite the value — a caller has to be able to see what it sent.
    it('passes an unrecognised `match` through untouched', async () => {
        const native = makeNative();
        vi.stubGlobal('window', { fb2k: native });
        const { library } = await import('./library.js');

        await library.getArtistAlbums('A', undefined, {
            match: 'exakt' as 'exact',
        });

        expect(native.invoke).toHaveBeenCalledWith('library.getArtistAlbums', {
            artist: 'A',
            match: 'exakt',
        });
    });

    // `options` is typed optional, but JS callers reach this with an explicit
    // null; spreading null throws, so the wrapper guards on typeof.
    it('tolerates a null `options` and omits an absent `limit`', async () => {
        const native = makeNative();
        vi.stubGlobal('window', { fb2k: native });
        const { library } = await import('./library.js');

        await library.getArtistAlbums(
            'A',
            undefined,
            null as unknown as undefined,
        );

        expect(native.invoke).toHaveBeenCalledWith('library.getArtistAlbums', {
            artist: 'A',
        });
        const params = native.invoke.mock.calls[0][1] as Record<
            string,
            unknown
        >;
        expect(params).not.toHaveProperty('limit');
    });

    // `limit: 0` is a meaningful request (host returns `total` with an empty
    // page), so it must survive the `!= null` guard rather than being elided
    // as falsy.
    it('forwards an explicit `limit` of 0', async () => {
        const native = makeNative();
        vi.stubGlobal('window', { fb2k: native });
        const { library } = await import('./library.js');

        await library.getArtistAlbums('A', 0);

        expect(native.invoke).toHaveBeenCalledWith('library.getArtistAlbums', {
            artist: 'A',
            limit: 0,
        });
    });
});

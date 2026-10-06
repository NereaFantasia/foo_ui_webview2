import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

function makeNative() {
    return {
        invoke: vi.fn().mockResolvedValue({ success: true, queueCount: 0 }),
        on: vi.fn(),
        off: vi.fn(),
        _handleResponse: () => {
            /* dummy */
        },
    };
}

describe('queue.remove', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('sends a single position as `index`', async () => {
        const native = makeNative();
        vi.stubGlobal('window', { fb2k: native });
        const { queue } = await import('./queue.js');

        await queue.remove(2);

        expect(native.invoke).toHaveBeenCalledWith('queue.remove', { index: 2 });
    });

    it('sends an array of positions as `indices` and never as `index`', async () => {
        const native = makeNative();
        vi.stubGlobal('window', { fb2k: native });
        const { queue } = await import('./queue.js');

        await queue.remove([3, 0, 3]);
        await queue.remove([]);

        expect(native.invoke.mock.calls).toEqual([
            ['queue.remove', { indices: [3, 0, 3] }],
            ['queue.remove', { indices: [] }],
        ]);
    });
});

describe('queue playlist-row references', () => {
    const GUID = '{12345678-1111-2222-3031-323334353637}';

    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('setContents passes rows named by index or by GUID through as given', async () => {
        const native = makeNative();
        vi.stubGlobal('window', { fb2k: native });
        const { queue } = await import('./queue.js');

        await queue.setContents([{ queueIndex: 1 }, { playlistGuid: GUID, item: 4 }, { playlist: 0, item: 2 }]);

        expect(native.invoke).toHaveBeenCalledWith('queue.setContents', {
            items: [{ queueIndex: 1 }, { playlistGuid: GUID, item: 4 }, { playlist: 0, item: 2 }],
        });
    });

    it('insertNext keeps GUID rows in `items` and paths in `paths`', async () => {
        const native = makeNative();
        vi.stubGlobal('window', { fb2k: native });
        const { queue } = await import('./queue.js');

        await queue.insertNext(['a.flac', { playlistGuid: GUID, item: 0 }], 1);

        expect(native.invoke).toHaveBeenCalledWith('queue.insertNext', {
            paths: ['a.flac'],
            items: [{ playlistGuid: GUID, item: 0 }],
            position: 1,
        });
    });
});

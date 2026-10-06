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

    // The host refuses an unrecognised `match` as INVALID_PARAMS, so the
    // wrapper must not silently drop or rewrite the value — a caller has to be
    // able to see what it sent.
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

describe('library.getAll', () => {
    type Handler = (data: unknown) => void;

    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.useRealTimers();
        vi.unstubAllGlobals();
    });

    // A native whose `library:getAllResult` listeners the test can fire.
    function withEvents(answer: unknown) {
        const listeners = new Set<Handler>();
        const native = makeNative();
        native.invoke.mockResolvedValue(answer);
        native.on.mockImplementation((_event: string, handler: Handler) => listeners.add(handler));
        native.off.mockImplementation((_event: string, handler: Handler) => listeners.delete(handler));
        const emit = (payload: unknown): void => {
            for (const handler of [...listeners]) handler(payload);
        };
        return { native, emit, listeners };
    }

    const page = {
        tracks: [],
        items: [],
        total: 0,
        offset: 0,
        limit: 0,
        fromCache: false,
    };

    it('resolves with a synchronous page or failure as the host sent it', async () => {
        const native = makeNative();
        const failure = { success: false, error: 'Library not enabled', code: 'LIBRARY_DISABLED' };
        native.invoke
            .mockResolvedValueOnce({ success: true, ...page })
            .mockResolvedValueOnce(failure);
        vi.stubGlobal('window', { fb2k: native });
        const { library } = await import('./library.js');

        await expect(library.getAll(0, 10)).resolves.toEqual({ success: true, ...page });
        await expect(library.getAll(0, 10)).resolves.toEqual(failure);
    });

    it('resolves a pending answer with the page the event delivers', async () => {
        const host = withEvents({ success: true, pending: true, requestId: 'r1' });
        vi.stubGlobal('window', { fb2k: host.native });
        const { library } = await import('./library.js');

        const result = library.getAll();
        // The listener is in place by the time the call goes out.
        await vi.waitFor(() => expect(host.native.invoke).toHaveBeenCalled());
        expect(host.listeners.size).toBe(1);
        host.emit({ ...page, requestId: 'other' });
        host.emit({ ...page, total: 3, requestId: 'r1' });

        await expect(result).resolves.toMatchObject({ success: true, total: 3, requestId: 'r1' });
        expect(host.listeners.size).toBe(0);
    });

    it('resolves a failed or timed-out pending answer with OPERATION_FAILED instead of rejecting', async () => {
        const failed = withEvents({ success: true, pending: true, requestId: 'r2' });
        vi.stubGlobal('window', { fb2k: failed.native });
        let { library } = await import('./library.js');

        const failure = library.getAll();
        await vi.waitFor(() => expect(failed.native.invoke).toHaveBeenCalled());
        expect(failed.listeners.size).toBe(1);
        failed.emit({ ...page, requestId: 'r2', error: 'worker failed' });
        await expect(failure).resolves.toEqual({
            success: false,
            error: 'worker failed',
            code: 'OPERATION_FAILED',
            details: { requestId: 'r2' },
        });
        expect(failed.listeners.size).toBe(0);

        vi.resetModules();
        vi.useFakeTimers();
        const silent = withEvents({ success: true, pending: true, requestId: 'r3' });
        vi.stubGlobal('window', { fb2k: silent.native });
        ({ library } = await import('./library.js'));

        const timedOut = library.getAll(undefined, undefined, { timeout: 500 });
        await vi.advanceTimersByTimeAsync(600);
        await expect(timedOut).resolves.toMatchObject({
            success: false,
            code: 'OPERATION_FAILED',
            details: { requestId: 'r3', timeoutMs: 500 },
        });
        expect(silent.listeners.size).toBe(0);
    });
});

describe('library.getAll options and result event', () => {
    const PAGE = {
        requestId: 'r1',
        tracks: [],
        items: [],
        total: 0,
        offset: 0,
        limit: 100,
        fromCache: false,
    };

    function nativeWithHandler() {
        const native = makeNative();
        const handlers: Array<(payload: unknown) => void> = [];
        native.on.mockImplementation((event: string, h: (payload: unknown) => void) => {
            if (event === 'library:getAllResult') handlers.push(h);
        });
        native.off.mockImplementation((event: string, h: (payload: unknown) => void) => {
            const i = handlers.indexOf(h);
            if (event === 'library:getAllResult' && i >= 0) handlers.splice(i, 1);
        });
        const emit = (payload: unknown): void => {
            for (const h of [...handlers]) h(payload);
        };
        const offCount = (): number =>
            native.off.mock.calls.filter((c) => c[0] === 'library:getAllResult').length;
        return { native, emit, offCount };
    }

    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.useRealTimers();
        vi.unstubAllGlobals();
    });

    it('defaults to asyncResult and listens before sending the call', async () => {
        const { native, offCount } = nativeWithHandler();
        native.invoke.mockResolvedValue({ success: true, ...PAGE, pending: undefined });
        vi.stubGlobal('window', { fb2k: native });
        const { library } = await import('./library.js');

        await library.getAll(0, 50);

        expect(native.invoke).toHaveBeenCalledWith('library.getAll', {
            offset: 0,
            limit: 50,
            asyncResult: true,
        });
        expect(native.on.mock.invocationCallOrder[0]).toBeLessThan(
            native.invoke.mock.invocationCallOrder[0],
        );
        // Answered at once, so the listener goes away without waiting.
        expect(offCount()).toBe(1);
    });

    it('forwards useCache and asyncResult: false without listening', async () => {
        const { native } = nativeWithHandler();
        native.invoke
            .mockResolvedValueOnce({ success: true, ...PAGE })
            .mockResolvedValueOnce({ success: false, error: 'boom', code: 'INTERNAL' });
        vi.stubGlobal('window', { fb2k: native });
        const { library } = await import('./library.js');

        const r = await library.getAll(undefined, undefined, {
            useCache: false,
            asyncResult: false,
        });

        expect(native.invoke).toHaveBeenCalledWith('library.getAll', {
            useCache: false,
            asyncResult: false,
        });
        expect(native.on).not.toHaveBeenCalled();
        expect(r).toMatchObject({ success: true, total: 0 });
        // A failure on the main-thread path resolves as the host sent it.
        await expect(library.getAll(0, 10, { asyncResult: false })).resolves.toEqual({
            success: false,
            error: 'boom',
            code: 'INTERNAL',
        });
        expect(native.on).not.toHaveBeenCalled();
    });

    it('resolves with a page delivered before the pending answer is handled', async () => {
        const { native, emit, offCount } = nativeWithHandler();
        native.invoke.mockImplementation(() => {
            emit({ ...PAGE, requestId: 'other' });
            emit({ ...PAGE, total: 7 });
            return Promise.resolve({ success: true, pending: true, requestId: 'r1' });
        });
        vi.stubGlobal('window', { fb2k: native });
        const { library } = await import('./library.js');

        const r = await library.getAll(0, 1_000_000, { useCache: true });

        expect(r).toMatchObject({ success: true, requestId: 'r1', total: 7 });
        expect(offCount()).toBe(1);
    });

    it('ignores other request ids and resolves on the matching event', async () => {
        const { native, emit, offCount } = nativeWithHandler();
        native.invoke.mockResolvedValue({ success: true, pending: true, requestId: 'r1' });
        vi.stubGlobal('window', { fb2k: native });
        const { library } = await import('./library.js');

        const p = library.getAll();
        await vi.waitFor(() => expect(native.invoke).toHaveBeenCalled());
        await Promise.resolve();
        emit({ ...PAGE, requestId: 'r2', total: 99 });
        emit({ ...PAGE, total: 3 });

        await expect(p).resolves.toMatchObject({ success: true, requestId: 'r1', total: 3 });
        expect(offCount()).toBe(1);
    });

    it('resolves an event carrying error with OPERATION_FAILED and stops listening', async () => {
        const { native, emit, offCount } = nativeWithHandler();
        native.invoke.mockImplementation(() => {
            emit({ ...PAGE, error: 'library unavailable' });
            return Promise.resolve({ success: true, pending: true, requestId: 'r1' });
        });
        vi.stubGlobal('window', { fb2k: native });
        const { library } = await import('./library.js');

        await expect(library.getAll()).resolves.toEqual({
            success: false,
            error: 'library unavailable',
            code: 'OPERATION_FAILED',
            details: { requestId: 'r1' },
        });
        expect(offCount()).toBe(1);
    });

    it('stops listening when the call fails or rejects', async () => {
        const { native, offCount } = nativeWithHandler();
        native.invoke
            .mockResolvedValueOnce({ success: false, error: 'boom', code: 'INTERNAL' })
            .mockRejectedValueOnce(new Error('bridge down'));
        vi.stubGlobal('window', { fb2k: native });
        const { library } = await import('./library.js');

        await expect(library.getAll()).resolves.toEqual({
            success: false,
            error: 'boom',
            code: 'INTERNAL',
        });
        // Only a rejected call rejects; the wrapper adds no rejection of its own.
        await expect(library.getAll()).rejects.toThrow('bridge down');
        expect(offCount()).toBe(2);
    });

    it('resolves with OPERATION_FAILED on timeout and stops listening when no event arrives', async () => {
        vi.useFakeTimers();
        const { native, offCount } = nativeWithHandler();
        native.invoke.mockResolvedValue({ success: true, pending: true, requestId: 'r1' });
        vi.stubGlobal('window', { fb2k: native });
        const { library } = await import('./library.js');

        const p = library.getAll(0, undefined, { timeout: 500 });
        const settled = expect(p).resolves.toEqual({
            success: false,
            error: 'library.getAll timed out after 500 ms',
            code: 'OPERATION_FAILED',
            details: { requestId: 'r1', timeoutMs: 500 },
        });
        await vi.advanceTimersByTimeAsync(500);
        await settled;
        expect(offCount()).toBe(1);
        expect(native.on).toHaveBeenCalledTimes(1);
    });
});

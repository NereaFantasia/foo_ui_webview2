// sdk/src/bridge/namespaces/metadata.test.ts
//
// Contract for the default `metadata:writeComplete` subscriber.
//
// The `metadata.ts` module installs a module-level `bridge.on(
// 'metadata:writeComplete', ...)` logger so async-write failures surface
// in the JS console. This test locks in three properties:
//
//   1. importing the module registers exactly one listener for the
//      `metadata:writeComplete` event;
//   2. calling `metadata.disableDefaultLogger()` detaches that listener
//      and is idempotent;
//   3. the surface keys reflect the public API including the new
//      `disableDefaultLogger` opt-out helper.
//
// Module state is reset between tests so the `bridge` singleton in
// Bridge.ts is freshly constructed against the stubbed `window`.

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

describe('metadata namespace (§5.6 default logger)', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('importing the module registers a metadata:writeComplete listener', async () => {
        const native = makeNative();
        vi.stubGlobal('window', { fb2k: native });
        await import('./metadata.js');

        const calls = native.on.mock.calls.filter(
            (c) => c[0] === 'metadata:writeComplete',
        );
        expect(calls.length).toBe(1);
        expect(typeof calls[0][1]).toBe('function');
    });

    it('disableDefaultLogger detaches the listener exactly once', async () => {
        const native = makeNative();
        vi.stubGlobal('window', { fb2k: native });
        const { metadata } = await import('./metadata.js');

        metadata.disableDefaultLogger();
        expect(native.off).toHaveBeenCalledWith(
            'metadata:writeComplete',
            expect.any(Function),
        );

        const offCallsAfterFirst = native.off.mock.calls.length;
        // Second call is a no-op — the underscore-prefixed cache is null
        // after the first call so off() must not fire again.
        metadata.disableDefaultLogger();
        expect(native.off.mock.calls.length).toBe(offCallsAfterFirst);
    });

    it('exposes the registered methods plus the §5.6 opt-out helper', async () => {
        vi.stubGlobal('window', { fb2k: makeNative() });
        const { metadata } = await import('./metadata.js');

        const expectedKeys = [
            'read',
            'readBatch',
            'readByPath',
            'readRaw',
            'probeBatchAsync',
            'cancelProbe',
            'write',
            'writeBatch',
            'embedArtwork',
            'embedArtworkBytes',
            'embedArtworkFromDataUrl',
            'removeEmbeddedArt',
            'removeField',
            'removeTag',
            'disableDefaultLogger',
        ].sort();

        expect(Object.keys(metadata).sort()).toEqual(expectedKeys);
    });

    // ── Envelope typing guards ───────────────────────────────────────
    //
    // Previously, `metadata.read` was typed as
    // `bridge.invoke<TrackInfo>('metadata.read', …)` but the C++ handler
    // always returns `{ success, path, tags, info }`. Consumer code had
    // to cast through `unknown` to reach `.tags` / `.info`. These tests
    // pin the new contract so the envelope keys survive any future
    // refactor.

    it('read(path) returns the `{ success, path, tags, info }` envelope verbatim (§5.3)', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: true,
            path: '/music/track.flac',
            tags: {
                TITLE: 'Song',
                ARTIST: 'Band',
                ALBUM: 'Record',
            },
            info: {
                duration: 201.3,
                bitrate: 1000,
                sampleRate: 44100,
                channels: 2,
                codec: 'FLAC',
            },
        });
        vi.stubGlobal('window', { fb2k: native });
        const { metadata } = await import('./metadata.js');

        const result = await metadata.read('/music/track.flac');

        expect(native.invoke).toHaveBeenCalledWith('metadata.read', {
            path: '/music/track.flac',
        });
        expect(result?.success).toBe(true);
        expect(result?.path).toBe('/music/track.flac');
        // tags preserves upstream casing — no auto-flattening.
        expect(result?.tags?.TITLE).toBe('Song');
        expect(result?.info?.codec).toBe('FLAC');
        expect(result?.info?.channels).toBe(2);
    });

    it('readBatch(paths) returns the `results[]` envelope verbatim (§5.3)', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: true,
            total: 2,
            successCount: 1,
            errorCount: 1,
            results: [
                {
                    path: '/ok.flac',
                    success: true,
                    tags: { TITLE: 'Song', ARTIST: 'Band' },
                },
                {
                    path: '/missing.flac',
                    success: false,
                    error: 'Failed to open file',
                },
            ],
        });
        vi.stubGlobal('window', { fb2k: native });
        const { metadata } = await import('./metadata.js');

        const result = await metadata.readBatch([
            '/ok.flac',
            '/missing.flac',
        ]);

        expect(native.invoke).toHaveBeenCalledWith('metadata.readBatch', {
            paths: ['/ok.flac', '/missing.flac'],
        });
        expect(result?.results).toHaveLength(2);
        expect(result?.results[0]).toMatchObject({
            path: '/ok.flac',
            success: true,
        });
        expect(result?.results[1]).toMatchObject({
            path: '/missing.flac',
            success: false,
        });
    });

    // ── Cancellable batch probe ──────────────────────────────────────
    //
    // probeBatchAsync returns a dispatch receipt, not results; the results
    // arrive on metadata:probeProgress / metadata:probeComplete. These tests
    // pin the invoke payload and the receipt keys, since a wrapper that
    // silently dropped `includeTags` or renamed `operationId` would leave the
    // caller unable to correlate or cancel anything.

    it('probeBatchAsync(paths) forwards paths and returns the dispatch receipt', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: true,
            operationId: 'probe_deadbeef00000001',
            totalCount: 2,
        });
        vi.stubGlobal('window', { fb2k: native });
        const { metadata } = await import('./metadata.js');

        const receipt = await metadata.probeBatchAsync([
            '/a.flac',
            '/album.cue|subsong:2',
        ]);

        expect(native.invoke).toHaveBeenCalledWith(
            'metadata.probeBatchAsync',
            { paths: ['/a.flac', '/album.cue|subsong:2'] },
        );
        expect(receipt?.operationId).toBe('probe_deadbeef00000001');
        expect(receipt?.totalCount).toBe(2);
    });

    it('probeBatchAsync forwards includeTags when supplied', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: true,
            operationId: 'probe_1',
            totalCount: 1,
        });
        vi.stubGlobal('window', { fb2k: native });
        const { metadata } = await import('./metadata.js');

        await metadata.probeBatchAsync(['/a.flac'], { includeTags: false });

        expect(native.invoke).toHaveBeenCalledWith(
            'metadata.probeBatchAsync',
            { paths: ['/a.flac'], includeTags: false },
        );
    });

    it('cancelProbe(operationId) reports whether the run was still live', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, cancelled: false });
        vi.stubGlobal('window', { fb2k: native });
        const { metadata } = await import('./metadata.js');

        const result = await metadata.cancelProbe('probe_gone');

        expect(native.invoke).toHaveBeenCalledWith('metadata.cancelProbe', {
            operationId: 'probe_gone',
        });
        // false covers both "already finished" and "never existed"; the
        // wrapper must not turn that into a rejection.
        expect(result?.success).toBe(true);
        expect(result?.cancelled).toBe(false);
    });

    it('readByPath(path) returns the flat record shape (§5.3)', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: true,
            path: '/music/track.flac',
            canonicalPath: 'C:\\music\\track.flac',
            TITLE: 'Song',
            ARTIST: 'Band',
            ALBUM: 'Record',
            TRACKNUMBER: '7',
        });
        vi.stubGlobal('window', { fb2k: native });
        const { metadata } = await import('./metadata.js');

        const result = (await metadata.readByPath('/music/track.flac')) as
            | Record<string, unknown>
            | null;

        expect(native.invoke).toHaveBeenCalledWith('metadata.readByPath', {
            path: '/music/track.flac',
        });
        // Top-level flat tags — no nested `.tags` key here.
        expect(result?.TITLE).toBe('Song');
        expect(result?.TRACKNUMBER).toBe('7');
        expect(result?.canonicalPath).toBe('C:\\music\\track.flac');
        expect(result).not.toHaveProperty('tags');
    });

    // ── Container sub-track addressing ───────────────────────────────
    //
    // `read` / `readByPath` accept an explicit track index for CUE
    // sheets, ISO images and other multi-track containers. The host
    // gives `cueIndex` precedence over a `|subsong:N` path suffix, so
    // the wrapper must forward it untouched and must not synthesize a
    // default when the caller omits it.

    it('read forwards cueIndex when supplied', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, tags: {}, info: {} });
        vi.stubGlobal('window', { fb2k: native });
        const { metadata } = await import('./metadata.js');

        await metadata.read('/music/album.cue', { cueIndex: 2 });

        expect(native.invoke).toHaveBeenCalledWith('metadata.read', {
            path: '/music/album.cue',
            cueIndex: 2,
        });
    });

    it('read omits cueIndex entirely when not supplied', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, tags: {}, info: {} });
        vi.stubGlobal('window', { fb2k: native });
        const { metadata } = await import('./metadata.js');

        await metadata.read('/music/album.cue|subsong:2');

        expect(native.invoke).toHaveBeenCalledWith('metadata.read', {
            path: '/music/album.cue|subsong:2',
        });
    });

    it('readByPath forwards cueIndex when supplied', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, TITLE: 'Track 3' });
        vi.stubGlobal('window', { fb2k: native });
        const { metadata } = await import('./metadata.js');

        await metadata.readByPath('/music/album.cue', { cueIndex: 3 });

        expect(native.invoke).toHaveBeenCalledWith('metadata.readByPath', {
            path: '/music/album.cue',
            cueIndex: 3,
        });
    });

    it('embedArtworkBytes sends raw Base64 imageData to the existing endpoint', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true });
        vi.stubGlobal('window', { fb2k: native });
        const { metadata } = await import('./metadata.js');

        await metadata.embedArtworkBytes(
            '/music/track.flac',
            new Uint8Array([0xff, 0xd8, 0xff, 0xe0]),
            {
                type: 'front',
                target: 'all',
                path: '/wrong.flac',
                imageData: 'wrong',
            } as never,
        );

        expect(native.invoke).toHaveBeenCalledWith('metadata.embedArtwork', {
            path: '/music/track.flac',
            imageData: '/9j/4A==',
            type: 'front',
            target: 'all',
        });
    });

    it('embedArtworkFromDataUrl strips the image data URL header', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true });
        vi.stubGlobal('window', { fb2k: native });
        const { metadata } = await import('./metadata.js');

        await metadata.embedArtworkFromDataUrl(
            '/music/track.flac',
            'data:image/png;base64,iVBORw==',
            {
                target: ['embedded', 'file'],
                path: '/wrong.flac',
                imageData: 'wrong',
            } as never,
        );

        expect(native.invoke).toHaveBeenCalledWith('metadata.embedArtwork', {
            path: '/music/track.flac',
            imageData: 'iVBORw==',
            target: ['embedded', 'file'],
        });
    });

    it.each([
        'data:text/plain;base64,SGVsbG8=',
        'data:image/+;base64,iVBORw==',
        'data:image/png,iVBORw==',
        'data:image/png;charset=utf-8;base64,iVBORw==',
        'data:image/png;base64,not-base64',
    ])('embedArtworkFromDataUrl rejects invalid image input: %s', async (input) => {
        const native = makeNative();
        vi.stubGlobal('window', { fb2k: native });
        const { metadata } = await import('./metadata.js');

        await expect(
            metadata.embedArtworkFromDataUrl('/music/track.flac', input),
        ).rejects.toThrow(TypeError);
        expect(native.invoke).not.toHaveBeenCalled();
    });
});

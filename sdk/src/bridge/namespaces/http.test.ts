// sdk/src/bridge/namespaces/http.test.ts
//
// Phase 2 test gate — covers BLOCKER 2 + CRITICAL 3 contracts:
//
//   - the eight verbs map 1:1 to registered cpp_api_handler names
//     (http.{get,post,put,delete,patch,head,download,abort})
//   - http.request implements the legacy event-wrapped behaviour:
//       * synchronous host response (`async: false`) → resolves immediately
//       * asynchronous host response (`async: true`) → resolves on the
//         matching `http:response` event
//   - hallucinated APIs (cancel / clearCache / getCacheStats) are absent

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

describe('http namespace', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('http.get invokes the registered http.get handler with url + opts', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ status: 200, body: 'ok' });
        vi.stubGlobal('window', { fb2k: native });
        const { http } = await import('./http.js');

        await http.get('https://example.com', { timeout: 5000 });

        expect(native.invoke).toHaveBeenCalledWith('http.get', {
            url: 'https://example.com',
            timeout: 5000,
        });
    });

    it('http verbs cover exactly the registered surface (no hallucinated keys)', async () => {
        vi.stubGlobal('window', { fb2k: makeNative() });
        const { http } = await import('./http.js');

        const expectedKeys = [
            'get',
            'post',
            'put',
            'delete',
            'patch',
            'head',
            'download',
            'abort',
            'request',
            // Phase 5 §5.6: opt-out for the default `http:downloadComplete`
            // failure logger installed at module load.
            'disableDefaultDownloadLogger',
        ].sort();

        expect(Object.keys(http).sort()).toEqual(expectedKeys);
        // BLOCKER 2 regression guard
        expect(http).not.toHaveProperty('cancel');
        expect(http).not.toHaveProperty('clearCache');
        expect(http).not.toHaveProperty('getCacheStats');
    });

    it('http.request resolves synchronously on async: false', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: true,
            async: false,
            status: 200,
            body: 'sync-body',
        });
        vi.stubGlobal('window', { fb2k: native });
        const { http } = await import('./http.js');

        // Module load installs the default `http:downloadComplete` logger
        // (Phase 5 §5.6). We snapshot how many native.on calls existed
        // before http.request runs so the assertion below targets only the
        // request-induced listener.
        const onCallsBefore = native.on.mock.calls.length;
        expect(
            native.on.mock.calls.some((c) => c[0] === 'http:downloadComplete'),
        ).toBe(true);

        const r = await http.request('https://example.com');

        expect(native.invoke).toHaveBeenCalledTimes(1);
        expect(native.invoke).toHaveBeenCalledWith(
            'http.get',
            expect.objectContaining({ url: 'https://example.com' }),
        );
        expect((r as { body?: string }).body).toBe('sync-body');
        // No additional listener (e.g. for `http:response`) must be
        // installed when the host returns synchronously.
        expect(native.on.mock.calls.length).toBe(onCallsBefore);
    });

    it('http.request resolves on matching http:response when async: true', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: true,
            async: true,
            requestId: 'req-42',
        });

        let registeredHandler: ((payload: unknown) => void) | null = null;
        native.on.mockImplementation(
            (event: string, h: (payload: unknown) => void) => {
                if (event === 'http:response') registeredHandler = h;
            },
        );

        vi.stubGlobal('window', { fb2k: native });
        const { http } = await import('./http.js');

        const promise = http.request('https://example.com');

        // _httpRequest chains: native.invoke -> .then(init) -> bridge.on.
        // That's two awaits inside the SDK plus the test's own await,
        // which microtask flushing alone isn't guaranteed to drain. Wait
        // for a macrotask so every queued microtask has settled.
        await vi.waitFor(() => {
            expect(registeredHandler).not.toBeNull();
        });

        expect(native.on).toHaveBeenCalledWith(
            'http:response',
            expect.any(Function),
        );

        // Stray response with a different requestId — must be ignored.
        registeredHandler!({
            requestId: 'unrelated-99',
            success: true,
            body: 'stray',
        });

        // Real response — must resolve the original promise.
        registeredHandler!({
            requestId: 'req-42',
            success: true,
            body: 'async-body',
        });

        const r = await promise;
        expect((r as { body?: string }).body).toBe('async-body');
        // listener must be detached after delivery (cleanup contract)
        expect(native.off).toHaveBeenCalled();
    });

    it('http.request rejects when async response carries success: false', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: true,
            async: true,
            requestId: 'req-err',
        });

        let registeredHandler: ((payload: unknown) => void) | null = null;
        native.on.mockImplementation(
            (event: string, h: (payload: unknown) => void) => {
                if (event === 'http:response') registeredHandler = h;
            },
        );

        vi.stubGlobal('window', { fb2k: native });
        const { http } = await import('./http.js');

        const promise = http.request('https://example.com');
        await vi.waitFor(() => {
            expect(registeredHandler).not.toBeNull();
        });

        registeredHandler!({
            requestId: 'req-err',
            success: false,
            error: 'remote 500',
        });

        await expect(promise).rejects.toThrow('remote 500');
    });

    // Regression guard for the binary-response decoding path. The host
    // transports binary payloads as base64 and the SDK decodes them
    // transparently into an ArrayBuffer when the caller passes
    // `responseType: 'arraybuffer'` or `'binary'`.
    it('http.get with responseType: arraybuffer decodes base64 body into an ArrayBuffer', async () => {
        const native = makeNative();
        // 0x00 0x61 0x73 0x6D 0x01 0x00 0x00 0x00 — the WASM magic header
        // including the leading null byte that breaks UTF-8 strict mode.
        const wasmBase64 = 'AGFzbQEAAAA=';
        native.invoke.mockResolvedValue({
            success: true,
            status: 200,
            body: wasmBase64,
            responseType: 'base64',
            headers: { 'content-type': 'application/wasm' },
        });
        vi.stubGlobal('window', { fb2k: native });
        const { http } = await import('./http.js');

        const r = await http.get('https://example.com/m.wasm', {
            responseType: 'arraybuffer',
        });

        expect(native.invoke).toHaveBeenCalledWith('http.get', {
            url: 'https://example.com/m.wasm',
            responseType: 'arraybuffer',
        });
        expect(r.body).toBeInstanceOf(ArrayBuffer);
        const bytes = new Uint8Array(r.body as ArrayBuffer);
        expect(Array.from(bytes)).toEqual([
            0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        ]);
        // status / headers must survive the body-only rewrite.
        expect(r.status).toBe(200);
        expect(r.headers).toEqual({ 'content-type': 'application/wasm' });
    });

    it('http.get with responseType: binary is treated as an arraybuffer alias', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: true,
            status: 200,
            body: '/v7+',  // 0xFE 0xFE 0xFE — illegal in UTF-8 strict mode.
            responseType: 'base64',
        });
        vi.stubGlobal('window', { fb2k: native });
        const { http } = await import('./http.js');

        const r = await http.get('https://example.com/blob', {
            responseType: 'binary',
        });

        expect(r.body).toBeInstanceOf(ArrayBuffer);
        const bytes = new Uint8Array(r.body as ArrayBuffer);
        expect(Array.from(bytes)).toEqual([0xfe, 0xfe, 0xfe]);
    });

    it('http.get with responseType: text leaves body untouched', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: true,
            status: 200,
            body: 'hello world',
            responseType: 'text',
        });
        vi.stubGlobal('window', { fb2k: native });
        const { http } = await import('./http.js');

        const r = await http.get('https://example.com/page', {
            responseType: 'text',
        });

        expect(typeof r.body).toBe('string');
        expect(r.body).toBe('hello world');
    });

    it('http.get with responseType: base64 keeps the body as a base64 string', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: true,
            status: 200,
            body: 'AGFzbQEAAAA=',
            responseType: 'base64',
        });
        vi.stubGlobal('window', { fb2k: native });
        const { http } = await import('./http.js');

        const r = await http.get('https://example.com/m.wasm', {
            responseType: 'base64',
        });

        // 'base64' callers opted out of auto-decoding; body stays a string
        // so existing call sites keep working.
        expect(typeof r.body).toBe('string');
        expect(r.body).toBe('AGFzbQEAAAA=');
    });

    // Per-request TLS opt-in: callers can ask the host to skip TLS
    // validation by passing `insecureTls: true`. The host applies the
    // skip only when its global advanced setting is also enabled; from
    // the SDK side we just verify the flag is forwarded verbatim.
    it('http.get forwards insecureTls: true verbatim to the host handler', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, status: 200, body: 'ok' });
        vi.stubGlobal('window', { fb2k: native });
        const { http } = await import('./http.js');

        await http.get('https://self-signed.example/', {
            insecureTls: true,
            timeout: 5000,
        });

        expect(native.invoke).toHaveBeenCalledWith('http.get', {
            url: 'https://self-signed.example/',
            insecureTls: true,
            timeout: 5000,
        });
    });

    it('http.post forwards insecureTls along with body and headers', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, status: 201, body: '{}' });
        vi.stubGlobal('window', { fb2k: native });
        const { http } = await import('./http.js');

        await http.post('https://nas.local/api', { id: 1 }, {
            insecureTls: true,
            headers: { 'X-Token': 'abc' },
        });

        expect(native.invoke).toHaveBeenCalledWith('http.post', {
            url: 'https://nas.local/api',
            body: { id: 1 },
            insecureTls: true,
            headers: { 'X-Token': 'abc' },
        });
    });

    it('http.download forwards insecureTls when downloading from self-signed host', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, savedTo: 'C:\\out\\f.bin' });
        vi.stubGlobal('window', { fb2k: native });
        const { http } = await import('./http.js');

        await http.download('https://192.168.1.10/file.bin', 'C:\\out\\f.bin', {
            insecureTls: true,
        });

        expect(native.invoke).toHaveBeenCalledWith('http.download', {
            url: 'https://192.168.1.10/file.bin',
            saveTo: 'C:\\out\\f.bin',
            insecureTls: true,
        });
    });

    it('http.get omits insecureTls from the wire when not specified', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, status: 200, body: 'ok' });
        vi.stubGlobal('window', { fb2k: native });
        const { http } = await import('./http.js');

        await http.get('https://example.com/');

        // Strict equality — must not leak an undefined `insecureTls` key.
        expect(native.invoke).toHaveBeenCalledWith('http.get', {
            url: 'https://example.com/',
        });
    });

    it('http.request with responseType: arraybuffer decodes the async http:response payload', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: true,
            async: true,
            requestId: 'req-bin',
        });

        let registeredHandler: ((payload: unknown) => void) | null = null;
        native.on.mockImplementation(
            (event: string, h: (payload: unknown) => void) => {
                if (event === 'http:response') registeredHandler = h;
            },
        );

        vi.stubGlobal('window', { fb2k: native });
        const { http } = await import('./http.js');

        const promise = http.request('https://example.com/m.wasm', {
            responseType: 'arraybuffer',
        });
        await vi.waitFor(() => {
            expect(registeredHandler).not.toBeNull();
        });

        registeredHandler!({
            requestId: 'req-bin',
            success: true,
            status: 200,
            body: 'AGFzbQEAAAA=',
            responseType: 'base64',
            headers: {},
        });

        const r = await promise;
        expect(r.body).toBeInstanceOf(ArrayBuffer);
        const bytes = new Uint8Array(r.body as ArrayBuffer);
        expect(Array.from(bytes)).toEqual([
            0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        ]);
    });
});

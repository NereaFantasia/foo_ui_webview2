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

describe('file binary helpers', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('readBinary decodes the host Base64 payload into exact bytes', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: true,
            content: 'AAEC/f7/',
            encoding: 'base64',
        });
        vi.stubGlobal('window', { fb2k: native });
        const { file } = await import('./file.js');

        const bytes = await file.readBinary('/music/cover.bin');

        expect(native.invoke).toHaveBeenCalledWith('file.read', {
            path: '/music/cover.bin',
            encoding: 'binary',
        });
        expect(Array.from(bytes)).toEqual([0, 1, 2, 253, 254, 255]);
    });

    it('readBinary rejects malformed Base64 returned by the host', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, content: 'not-base64' });
        vi.stubGlobal('window', { fb2k: native });
        const { file } = await import('./file.js');

        await expect(file.readBinary('/music/bad.bin')).rejects.toThrow(
            'Expected canonical padded Base64 data.',
        );
    });

    it('readBinary rejects a successful response without content', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true });
        vi.stubGlobal('window', { fb2k: native });
        const { file } = await import('./file.js');

        await expect(file.readBinary('/music/empty.bin')).rejects.toThrow(
            'file.read did not return binary content.',
        );
    });

    it('readBinary rejects the Host error envelope', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: false,
            error: 'File not found',
        });
        vi.stubGlobal('window', { fb2k: native });
        const { file } = await import('./file.js');

        await expect(file.readBinary('/music/missing.bin')).rejects.toThrow(
            'File not found',
        );
    });

    it('writeBinary emits the legacy binary wire without changing write()', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, bytesWritten: 4 });
        vi.stubGlobal('window', { fb2k: native });
        const { file } = await import('./file.js');

        await file.writeBinary(
            '/music/cover.bin',
            new Uint8Array([0, 1, 254, 255]).buffer,
            {
                append: true,
                path: '/wrong/path.bin',
                content: 'wrong',
                encoding: 'utf-8',
            } as never,
        );

        expect(native.invoke).toHaveBeenCalledWith('file.write', {
            path: '/music/cover.bin',
            content: 'base64:AAH+/w==',
            encoding: 'binary',
            append: true,
        });
    });

    it('writeDataUrl strips the header and preserves the Base64 payload', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, bytesWritten: 4 });
        vi.stubGlobal('window', { fb2k: native });
        const { file } = await import('./file.js');

        await file.writeDataUrl(
            '/music/cover.png',
            'data:image/png;base64,iVBORw==',
            {
                append: true,
                path: '/wrong/path.png',
                content: 'wrong',
                encoding: 'utf-8',
            } as never,
        );

        expect(native.invoke).toHaveBeenCalledWith('file.write', {
            path: '/music/cover.png',
            content: 'base64:iVBORw==',
            encoding: 'binary',
            append: true,
        });
    });

    it.each([
        'iVBORw==',
        'data:image/png,iVBORw==',
        'data:image/png;base64,not-base64',
        'data:;base64,iVBORw==',
        'data:!/png;base64,iVBORw==',
        'data:image/+;base64,iVBORw==',
        'data:image/png;charset;base64,iVBORw==',
        'data:image/png;charset=utf-8;base64,iVBORw==',
        'data:image/png;base64,',
        'data:image/png;base64,AB==',
        'data:image/png;base64,AAB=',
    ])('writeDataUrl rejects invalid input before invoking the host: %s', async (input) => {
        const native = makeNative();
        vi.stubGlobal('window', { fb2k: native });
        const { file } = await import('./file.js');

        await expect(
            file.writeDataUrl('/music/cover.png', input),
        ).rejects.toThrow(TypeError);
        expect(native.invoke).not.toHaveBeenCalled();
    });
});

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

describe('lyrics namespace', () => {
    const invoke = vi.fn();

    beforeEach(() => {
        vi.resetModules();
        invoke.mockReset().mockResolvedValue({ success: true });
        vi.stubGlobal('window', { fb2k: { invoke, on: vi.fn(), off: vi.fn() } });
    });

    afterEach(() => vi.unstubAllGlobals());

    it('forwards an explicit filename in exists options', async () => {
        const { lyrics } = await import('./lyrics.js');
        await lyrics.exists('C:\\music\\album.cue|subsong:7', { filename: 'track.seven' });
        expect(invoke).toHaveBeenCalledWith('lyrics.exists', {
            path: 'C:\\music\\album.cue|subsong:7', filename: 'track.seven',
        });
    });

    it('keeps the single-argument exists call unchanged', async () => {
        const { lyrics } = await import('./lyrics.js');
        await lyrics.exists('C:\\music\\track.wav');
        expect(invoke).toHaveBeenCalledWith('lyrics.exists', { path: 'C:\\music\\track.wav' });
    });

    it('forwards filename and independent get filters', async () => {
        const { lyrics } = await import('./lyrics.js');
        await lyrics.get('C:\\music\\track.wav', {
            filename: 'alternate.words', source: 'file', type: 'synced', format: 'txt',
        });
        expect(invoke).toHaveBeenCalledWith('lyrics.get', {
            path: 'C:\\music\\track.wav', filename: 'alternate.words',
            source: 'file', type: 'synced', format: 'txt',
        });
    });

    it('does not synthesize a path for the playing track', async () => {
        const { lyrics } = await import('./lyrics.js');
        await lyrics.get(undefined, { filename: 'current.lrc' });
        expect(invoke).toHaveBeenCalledWith('lyrics.get', { filename: 'current.lrc' });
    });

    it('forwards save filename and normalizes a single target', async () => {
        const { lyrics } = await import('./lyrics.js');
        await lyrics.save('C:\\music\\track.wav', '[00:01]line', {
            filename: 'alternate.words', format: 'txt', target: 'file',
        });
        expect(invoke).toHaveBeenCalledWith('lyrics.save', {
            path: 'C:\\music\\track.wav', lyrics: '[00:01]line',
            filename: 'alternate.words', format: 'txt', target: ['file'],
        });
    });

    it('preserves an empty filename for host default naming', async () => {
        const { lyrics } = await import('./lyrics.js');
        await lyrics.exists('C:\\music\\track.wav', { filename: '' });
        expect(invoke).toHaveBeenCalledWith('lyrics.exists', {
            path: 'C:\\music\\track.wav', filename: '',
        });
    });
});

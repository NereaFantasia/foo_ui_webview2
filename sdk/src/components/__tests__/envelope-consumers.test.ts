// sdk/src/components/__tests__/envelope-consumers.test.ts
//
// The list wrappers these components call resolve with `XxxSuccess | ApiFailure`.
// A cast of the awaited answer to an array still compiles and then reads an
// object as a list, so each consumer is pinned to test `success` and read the
// list out of the envelope, with no cast on the answer itself.
//
// Static analysis (no DOM), same approach as `no-visual-style.test.ts`.
/// <reference types="vite/client" />
import { describe, expect, it } from 'vitest';

const sources = import.meta.glob(
    [
        '../FbPlaylistSelector.ts',
        '../FbPlaylistTabs.ts',
        '../FbOutputSelector.ts',
        '../FbDspPresetSelector.ts',
        '../FbPlaylistView.ts',
        '../FbLibraryTree.ts',
    ],
    { query: '?raw', import: 'default', eager: true },
) as Record<string, string>;

function source(file: string): string {
    const text = sources[`../${file}`];
    expect(text, `${file} not loaded`).toBeTypeOf('string');
    return (text as string).replace(/\/\*[\s\S]*?\*\//g, '').replace(/(^|\s)\/\/[^\n]*/g, '$1');
}

const consumers = [
    { file: 'FbPlaylistSelector.ts', call: 'playlist.getAll(', field: '.playlists' },
    { file: 'FbPlaylistTabs.ts', call: 'playlist.getAll(', field: '.playlists' },
    { file: 'FbOutputSelector.ts', call: 'config.getOutputDevices(', field: '.devices' },
    { file: 'FbDspPresetSelector.ts', call: 'config.getDspPresets(', field: '.presets' },
    { file: 'FbPlaylistView.ts', call: 'playlist.getTracks(', field: '.tracks' },
    { file: 'FbLibraryTree.ts', call: 'library.addToPlaylist(', field: '.added' },
] as const;

describe('components read list wrappers through the envelope', () => {
    it.each(consumers)('$file checks success and reads $field', ({ file, call, field }) => {
        const src = source(file);
        expect(src).toContain(call);
        expect(src).toMatch(/\.success === false/);
        expect(src).toContain(field);
    });

    it.each(consumers)('$file does not cast the answer of $call', ({ file, call }) => {
        const src = source(file);
        const escaped = call.replace(/[.()]/g, (c) => `\\${c}`);
        // `(await fb.x.y(...)) as T` or `(await getFb().x.y(...)) as T`
        const cast = new RegExp(`\\(await [^;]*?${escaped}[^;]*?\\)\\)\\s*as\\b`, 's');
        expect(src).not.toMatch(cast);
    });
});

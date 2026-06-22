// sdk/src/smp/classes/FbMetadbHandle.test.ts
//
// Phase 5.1 test gate (plan §8.7) — FbMetadbHandle constructor variants
// and getter contract. The legacy SMP API allowed string handle-ids,
// raw track-info objects, and copy-construction; this test defends
// each variant.

import { describe, expect, it } from 'vitest';

import { FbMetadbHandle } from './FbMetadbHandle.js';

describe('FbMetadbHandle', () => {
    it('constructs from a plain string handle-id', () => {
        const h = new FbMetadbHandle('C:\\song.flac');
        expect(h.Path).toBe('C:\\song.flac');
        expect(h.RawPath).toBe('C:\\song.flac');
        expect(h.SubSong).toBe(0);
        expect(h.HandleId).toBe('C:\\song.flac');
        expect(h.toString()).toBe('C:\\song.flac');
    });

    it('parses |subsong:N suffix from string input', () => {
        const h = new FbMetadbHandle('C:\\song.flac|subsong:4');
        expect(h.Path).toBe('C:\\song.flac');
        expect(h.SubSong).toBe(4);
        expect(h.HandleId).toBe('C:\\song.flac|subsong:4');
    });

    it('constructs from a track-info object with absolutePath', () => {
        const h = new FbMetadbHandle({
            absolutePath: 'D:\\test.mp3',
            duration: 180,
            fileSize: 4_000_000,
        });
        expect(h.Path).toBe('D:\\test.mp3');
        expect(h.SubSong).toBe(0);
        expect(h.Length).toBe(180);
        expect(h.FileSize).toBe(4_000_000);
    });

    it('constructs from a track-info object with path field', () => {
        const h = new FbMetadbHandle({ path: 'D:\\song.ogg' });
        expect(h.Path).toBe('D:\\song.ogg');
    });

    it('honours an explicit subsong number in the input object', () => {
        const h = new FbMetadbHandle({
            absolutePath: 'D:\\album.cue',
            subsong: 3,
        });
        expect(h.Path).toBe('D:\\album.cue');
        expect(h.SubSong).toBe(3);
        expect(h.HandleId).toBe('D:\\album.cue|subsong:3');
    });

    it('prefers the |subsong: tail in pathLike over the explicit field', () => {
        // Legacy parity: parsed-from-path subsong wins; explicit
        // `subsong` field is only consulted when parsed is 0.
        const h = new FbMetadbHandle({
            absolutePath: 'D:\\album.cue|subsong:2',
            subsong: 99, // ignored — pathLike already provides 2
        });
        expect(h.SubSong).toBe(2);
    });

    it('falls back to the explicit subsong field when path has none', () => {
        const h = new FbMetadbHandle({
            absolutePath: 'D:\\album.cue',
            subsong: 7,
        });
        expect(h.SubSong).toBe(7);
    });

    it('supports copy construction', () => {
        const a = new FbMetadbHandle({ absolutePath: 'D:\\a.mp3', subsong: 1 });
        const b = new FbMetadbHandle(a);
        expect(b.Path).toBe('D:\\a.mp3');
        expect(b.SubSong).toBe(1);
    });

    it('returns 0 for Length / FileSize when track info missing', () => {
        const h = new FbMetadbHandle('C:\\bare.flac');
        expect(h.Length).toBe(0);
        expect(h.FileSize).toBe(0);
    });

    it('Compare returns true for identical path + subsong', () => {
        const a = new FbMetadbHandle('C:\\song.flac|subsong:2');
        const b = new FbMetadbHandle({
            absolutePath: 'C:\\song.flac',
            subsong: 2,
        });
        expect(a.Compare(b)).toBe(true);
    });

    it('Compare returns false for different paths', () => {
        const a = new FbMetadbHandle('C:\\song1.flac');
        const b = new FbMetadbHandle('C:\\song2.flac');
        expect(a.Compare(b)).toBe(false);
    });

    it('Compare returns false when subsong differs', () => {
        const a = new FbMetadbHandle('C:\\song.flac|subsong:1');
        const b = new FbMetadbHandle('C:\\song.flac|subsong:2');
        expect(a.Compare(b)).toBe(false);
    });

    it('Compare returns false for null / undefined other', () => {
        const a = new FbMetadbHandle('C:\\song.flac');
        expect(a.Compare(null)).toBe(false);
        expect(a.Compare(undefined)).toBe(false);
    });

    it('Compare coerces a string other into a fresh handle', () => {
        const a = new FbMetadbHandle('C:\\song.flac|subsong:1');
        expect(a.Compare('C:\\song.flac|subsong:1')).toBe(true);
    });

    it('returns null from GetFileInfo when the bridge is unavailable', async () => {
        // No `globalThis.smp` is configured in this test, so getInvoke()
        // returns null and GetFileInfo short-circuits to null.
        const h = new FbMetadbHandle('C:\\song.flac');
        await expect(h.GetFileInfo()).resolves.toBeNull();
    });
});

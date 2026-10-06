// sdk/src/components/__tests__/trackKey.test.ts
//
// A CUE sheet entry is one subsong of a file. A component that sends the
// bare `path` of such a track to the host reaches subsong 0, i.e. the first
// track of the file; the key must keep the `|subsong:N` suffix.
/// <reference types="vite/client" />
import { describe, expect, it } from 'vitest';
import { trackKeyOf } from '../trackKey.js';

describe('trackKeyOf', () => {
    it('prefers handle over path and subsong', () => {
        expect(
            trackKeyOf({
                handle: 'C:\\a.cue|subsong:3',
                path: 'file://C:\\a.cue',
                absolutePath: 'C:\\a.cue',
                subsong: 3,
            }),
        ).toBe('C:\\a.cue|subsong:3');
    });

    it('joins path and a positive subsong when handle is missing or empty', () => {
        expect(trackKeyOf({ path: 'file://C:\\a.cue', subsong: 2 })).toBe(
            'file://C:\\a.cue|subsong:2',
        );
        expect(trackKeyOf({ handle: '', path: 'file://C:\\a.cue', subsong: 2 })).toBe(
            'file://C:\\a.cue|subsong:2',
        );
    });

    it('falls back to absolutePath when path is empty', () => {
        expect(trackKeyOf({ path: '', absolutePath: 'C:\\a.cue', subsong: 1 })).toBe(
            'C:\\a.cue|subsong:1',
        );
    });

    it('adds no suffix for subsong 0, a missing subsong, or a non-positive-integer subsong', () => {
        expect(trackKeyOf({ path: 'C:\\a.flac', subsong: 0 })).toBe('C:\\a.flac');
        expect(trackKeyOf({ path: 'C:\\a.flac' })).toBe('C:\\a.flac');
        expect(trackKeyOf({ path: 'C:\\a.flac', subsong: -1 })).toBe('C:\\a.flac');
        expect(trackKeyOf({ path: 'C:\\a.flac', subsong: 1.5 })).toBe('C:\\a.flac');
        expect(trackKeyOf({ path: 'C:\\a.flac', subsong: Number.NaN })).toBe('C:\\a.flac');
    });

    it('returns an empty key without a track or a path, even with a subsong', () => {
        expect(trackKeyOf(null)).toBe('');
        expect(trackKeyOf(undefined)).toBe('');
        expect(trackKeyOf({ subsong: 2 })).toBe('');
        expect(trackKeyOf({ path: '', absolutePath: '', subsong: 2 })).toBe('');
    });
});

// FbRating extends HTMLElement and cannot load in the node test environment,
// so its use of the key is pinned statically, as in `no-visual-style.test.ts`.
const sourceMap = import.meta.glob('../FbRating.ts', {
    query: '?raw',
    import: 'default',
    eager: true,
}) as Record<string, string>;
const SOURCE = Object.values(sourceMap)[0];

function stripComments(src: string): string {
    return src.replace(/\/\*[\s\S]*?\*\//g, '').replace(/(^|\s)\/\/[^\n]*/g, '$1');
}

describe('FbRating · addresses the track by its subsong-aware key', () => {
    it('derives the current track key with trackKeyOf from both sources', () => {
        const stripped = stripComments(SOURCE);
        const assignments = stripped.match(/this\._currentPath\s*=[^;]*;/g) ?? [];
        const fromTrack = assignments.filter((a) => a.includes('trackKeyOf('));
        // One from `playback:trackChanged`, one from `player.getCurrentTrack()`.
        expect(fromTrack.length).toBe(2);
        expect(stripped).not.toMatch(/\.path\s*\|\|/);
    });

    it('sends that key to rating.get and rating.set', () => {
        const stripped = stripComments(SOURCE);
        expect(stripped).toMatch(/rating\.get\(\s*this\._currentPath\s*\)/);
        expect(stripped).toMatch(/rating\.set\(\s*this\._currentPath\s*,/);
    });
});

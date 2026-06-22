// sdk/src/smp/classes/FbFileInfo.test.ts
//
// Phase 5.1 test gate — FbFileInfo case-insensitive lookup contract.

import { describe, expect, it } from 'vitest';

import { FbFileInfo } from './FbFileInfo.js';

describe('FbFileInfo', () => {
    it('reports zero counts for empty input', () => {
        const fi = new FbFileInfo();
        expect(fi.MetaCount).toBe(0);
        expect(fi.InfoCount).toBe(0);
    });

    it('parses tags from `tags` field', () => {
        const fi = new FbFileInfo({
            tags: {
                Artist: 'A',
                Title: ['T1', 'T2'],
            },
        });
        expect(fi.MetaCount).toBe(2);
        expect(fi.MetaName(0)).toBe('Artist');
        expect(fi.MetaValue(0, 0)).toBe('A');
        expect(fi.MetaValueCount(1)).toBe(2);
        expect(fi.MetaValue(1, 0)).toBe('T1');
        expect(fi.MetaValue(1, 1)).toBe('T2');
    });

    it('also accepts the `meta` alias for the tags object', () => {
        const fi = new FbFileInfo({ meta: { Artist: 'A' } });
        expect(fi.MetaCount).toBe(1);
        expect(fi.MetaFind('artist')).toBe(0);
    });

    it('parses info from `info` field', () => {
        const fi = new FbFileInfo({
            info: { codec: 'flac', sample_rate: 44100 },
        });
        expect(fi.InfoCount).toBe(2);
        expect(fi.InfoFind('CODEC')).toBe(0);
        expect(fi.InfoValue(0)).toBe('flac');
        expect(fi.InfoValue(1)).toBe('44100');
    });

    it('MetaFind / InfoFind are case-insensitive', () => {
        const fi = new FbFileInfo({
            tags: { Artist: 'A' },
            info: { codec: 'flac' },
        });
        expect(fi.MetaFind('ARTIST')).toBe(0);
        expect(fi.MetaFind('artist')).toBe(0);
        expect(fi.MetaFind('missing')).toBe(-1);
        expect(fi.InfoFind('CODEC')).toBe(0);
    });

    it('returns empty strings for out-of-range indices', () => {
        const fi = new FbFileInfo({ tags: { Artist: 'A' } });
        expect(fi.MetaName(99)).toBe('');
        expect(fi.MetaValue(99, 0)).toBe('');
        expect(fi.InfoName(99)).toBe('');
        expect(fi.InfoValue(99)).toBe('');
    });

    it('normalises null tag values to empty strings', () => {
        const fi = new FbFileInfo({ tags: { Comment: null } });
        expect(fi.MetaCount).toBe(1);
        expect(fi.MetaValue(0, 0)).toBe('');
    });
});

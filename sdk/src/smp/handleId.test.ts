// sdk/src/smp/handleId.test.ts
//
// Phase 5.1 test gate (plan §8.7) — handle-id round-trip semantics.
// These contract-level tests defend the legacy SMP `path|subsong:N`
// vocabulary used across selection.set / metadata.read / etc.

import { describe, expect, it } from 'vitest';

import {
    HANDLE_TOKEN,
    formatHandleId,
    parseHandleId,
    stripSubsongSuffix,
} from './handleId.js';

describe('handleId', () => {
    describe('formatHandleId', () => {
        it('returns the bare path when subsong is 0 / undefined', () => {
            expect(formatHandleId('C:\\song.flac')).toBe('C:\\song.flac');
            expect(formatHandleId('C:\\song.flac', 0)).toBe('C:\\song.flac');
        });

        it('appends |subsong:N for positive subsong indices', () => {
            expect(formatHandleId('C:\\song.flac', 2)).toBe(
                'C:\\song.flac' + HANDLE_TOKEN + '2',
            );
            expect(formatHandleId('C:\\song.flac', 10)).toBe(
                'C:\\song.flac|subsong:10',
            );
        });

        it('returns the empty string for empty / falsy paths', () => {
            expect(formatHandleId('')).toBe('');
            // @ts-expect-error — runtime guard against null path
            expect(formatHandleId(null)).toBe('');
        });

        it('coerces non-numeric subsong to 0 (collapse to bare path)', () => {
            // @ts-expect-error — runtime guard against non-numeric subsong
            expect(formatHandleId('C:\\song.flac', 'invalid')).toBe('C:\\song.flac');
        });
    });

    describe('parseHandleId', () => {
        it('returns the canonical empty result for non-string input', () => {
            // parseHandleId formally accepts `unknown`; guard intent is
            // exercised by the runtime branch.
            expect(parseHandleId(null)).toEqual({ path: '', subsong: 0, id: '' });
            expect(parseHandleId(undefined)).toEqual({ path: '', subsong: 0, id: '' });
            expect(parseHandleId(42)).toEqual({ path: '', subsong: 0, id: '' });
        });

        it('treats inputs without |subsong: as plain paths', () => {
            expect(parseHandleId('C:\\song.flac')).toEqual({
                path: 'C:\\song.flac',
                subsong: 0,
                id: 'C:\\song.flac',
            });
        });

        it('parses path|subsong:N with subsong > 0', () => {
            expect(parseHandleId('C:\\song.flac|subsong:3')).toEqual({
                path: 'C:\\song.flac',
                subsong: 3,
                id: 'C:\\song.flac|subsong:3',
            });
        });

        it('treats invalid subsong tail as subsong 0 (collapse)', () => {
            const r = parseHandleId('C:\\song.flac|subsong:abc');
            expect(r.path).toBe('C:\\song.flac');
            expect(r.subsong).toBe(0);
            expect(r.id).toBe('C:\\song.flac');
        });

        it('round-trips formatHandleId / parseHandleId', () => {
            const id = formatHandleId('D:\\album\\07 - track.flac', 5);
            const parsed = parseHandleId(id);
            expect(parsed.path).toBe('D:\\album\\07 - track.flac');
            expect(parsed.subsong).toBe(5);
            expect(parsed.id).toBe(id);
        });
    });

    describe('stripSubsongSuffix', () => {
        it('strips |subsong:N suffix', () => {
            expect(stripSubsongSuffix('C:\\song.flac|subsong:7')).toBe('C:\\song.flac');
        });

        it('returns plain paths unchanged', () => {
            expect(stripSubsongSuffix('C:\\song.flac')).toBe('C:\\song.flac');
        });

        it('returns empty string for non-string input', () => {
            // stripSubsongSuffix formally accepts `unknown`.
            expect(stripSubsongSuffix(null)).toBe('');
            expect(stripSubsongSuffix(undefined)).toBe('');
        });
    });
});

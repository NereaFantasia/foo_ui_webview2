// sdk/src/smp/classes/FbMetadbHandleList.test.ts
//
// Phase 5.1 test gate (plan §8.7) — FbMetadbHandleList Proxy / mutation
// contract. Themes treat the list as a hybrid array-like so the
// numeric-index access path and the `Count` getter must both work.

import { describe, expect, it } from 'vitest';

import { FbMetadbHandle } from './FbMetadbHandle.js';
import { FbMetadbHandleList } from './FbMetadbHandleList.js';

describe('FbMetadbHandleList', () => {
    it('starts empty by default', () => {
        const l = new FbMetadbHandleList();
        expect(l.Count).toBe(0);
    });

    it('Add accepts string handle-ids and FbMetadbHandle instances', () => {
        const l = new FbMetadbHandleList();
        l.Add('C:\\a.flac');
        l.Add(new FbMetadbHandle('C:\\b.flac|subsong:2'));
        expect(l.Count).toBe(2);
        expect(l[0]?.Path).toBe('C:\\a.flac');
        expect(l[1]?.SubSong).toBe(2);
    });

    it('AddRange consumes plain arrays', () => {
        const l = new FbMetadbHandleList();
        l.AddRange(['C:\\a.flac', 'C:\\b.flac']);
        expect(l.Count).toBe(2);
        expect(l[0]?.Path).toBe('C:\\a.flac');
        expect(l[1]?.Path).toBe('C:\\b.flac');
    });

    it('AddRange consumes another FbMetadbHandleList (clone semantics)', () => {
        const a = new FbMetadbHandleList(['C:\\a.flac', 'C:\\b.flac']);
        const b = new FbMetadbHandleList();
        b.AddRange(a);
        expect(b.Count).toBe(2);
        expect(b[0]?.Path).toBe('C:\\a.flac');
    });

    it('numeric index assignment replaces the slot', () => {
        const l = new FbMetadbHandleList(['C:\\a.flac']);
        // The Proxy `set` trap coerces the value through `_toHandle`,
        // so a string id is accepted at runtime even though the index
        // signature types `FbMetadbHandle`. Cast around the static
        // signature to exercise the legacy contract.
        (l as unknown as { [k: number]: unknown })[0] = 'C:\\replaced.flac';
        expect(l[0]?.Path).toBe('C:\\replaced.flac');
    });

    it('numeric index assignment is rejected for out-of-range indices', () => {
        const l = new FbMetadbHandleList(['C:\\a.flac']);
        // The Proxy `set` trap returns false for out-of-range indices,
        // which raises a TypeError in strict mode (test files compile
        // under tsc strict per tsconfig.json).
        const indexed = l as unknown as { [k: number]: unknown };
        expect(() => {
            indexed[5] = 'C:\\nope.flac';
        }).toThrow();
    });

    it('Find returns the index of an equivalent handle', () => {
        const l = new FbMetadbHandleList([
            'C:\\a.flac',
            'C:\\b.flac|subsong:1',
            'C:\\c.flac',
        ]);
        expect(l.Find('C:\\b.flac|subsong:1')).toBe(1);
        expect(l.Find('C:\\missing.flac')).toBe(-1);
    });

    it('Remove deletes the first matching handle', () => {
        const l = new FbMetadbHandleList(['C:\\a.flac', 'C:\\b.flac']);
        expect(l.Remove('C:\\a.flac')).toBe(true);
        expect(l.Count).toBe(1);
        expect(l[0]?.Path).toBe('C:\\b.flac');
    });

    it('Remove returns false when no match is present', () => {
        const l = new FbMetadbHandleList(['C:\\a.flac']);
        expect(l.Remove('C:\\unknown.flac')).toBe(false);
        expect(l.Count).toBe(1);
    });

    it('RemoveById removes by absolute index', () => {
        const l = new FbMetadbHandleList(['C:\\a.flac', 'C:\\b.flac', 'C:\\c.flac']);
        expect(l.RemoveById(1)).toBe(true);
        expect(l.Count).toBe(2);
        expect(l[1]?.Path).toBe('C:\\c.flac');
    });

    it('RemoveById rejects out-of-range indices', () => {
        const l = new FbMetadbHandleList(['C:\\a.flac']);
        expect(l.RemoveById(-1)).toBe(false);
        expect(l.RemoveById(5)).toBe(false);
    });

    it('RemoveAll clears the list', () => {
        const l = new FbMetadbHandleList(['C:\\a.flac', 'C:\\b.flac']);
        l.RemoveAll();
        expect(l.Count).toBe(0);
    });

    it('Convert returns a plain array snapshot', () => {
        const l = new FbMetadbHandleList(['C:\\a.flac', 'C:\\b.flac']);
        const arr = l.Convert();
        expect(arr.length).toBe(2);
        expect(arr[0]?.Path).toBe('C:\\a.flac');
        // Snapshot — mutations do not bleed back into the list.
        arr.length = 0;
        expect(l.Count).toBe(2);
    });

    it('Clone produces an independent copy', () => {
        const a = new FbMetadbHandleList(['C:\\a.flac', 'C:\\b.flac']);
        const b = a.Clone();
        b.RemoveAll();
        expect(b.Count).toBe(0);
        expect(a.Count).toBe(2);
    });

    it('CalcTotalDuration sums the Length values', () => {
        const l = new FbMetadbHandleList([
            { absolutePath: 'C:\\a.flac', duration: 100 },
            { absolutePath: 'C:\\b.flac', duration: 200 },
        ]);
        expect(l.CalcTotalDuration()).toBe(300);
    });

    it('CalcTotalSize sums the FileSize values', () => {
        const l = new FbMetadbHandleList([
            { absolutePath: 'C:\\a.flac', fileSize: 1_000_000 },
            { absolutePath: 'C:\\b.flac', fileSize: 2_500_000 },
        ]);
        expect(l.CalcTotalSize()).toBe(3_500_000);
    });

    it('is iterable via for ... of', () => {
        const l = new FbMetadbHandleList(['C:\\a.flac', 'C:\\b.flac']);
        const collected: string[] = [];
        for (const h of l) collected.push(h.Path);
        expect(collected).toEqual(['C:\\a.flac', 'C:\\b.flac']);
    });
});

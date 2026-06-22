// sdk/src/smp/utils.test.ts
//
// Phase 5.1 test gate — smpUtils helpers (toHandleId / normalizeHandleList /
// toHandleIdArray / clamp / sleep / buildMenuItems) contract.

import { describe, expect, it, vi } from 'vitest';

import { FbMetadbHandle } from './classes/FbMetadbHandle.js';
import { FbMetadbHandleList } from './classes/FbMetadbHandleList.js';
import {
    MENU_FLAGS,
    buildMenuItems,
    clamp,
    getInvoke,
    normalizeHandleList,
    sleep,
    toHandleId,
    toHandleIdArray,
} from './utils.js';
import type { SmpMenuBuildState, SmpRawMenuItem } from './types.js';

describe('utils.toHandleId', () => {
    it('returns string handles unchanged', () => {
        expect(toHandleId('C:\\song.flac|subsong:2')).toBe('C:\\song.flac|subsong:2');
    });

    it('reads HandleId from FbMetadbHandle', () => {
        const h = new FbMetadbHandle('C:\\song.flac|subsong:3');
        expect(toHandleId(h)).toBe('C:\\song.flac|subsong:3');
    });

    it('falls back to absolutePath / path / Path fields', () => {
        expect(toHandleId({ Path: 'A.flac', SubSong: 2 })).toBe('A.flac|subsong:2');
        expect(toHandleId({ absolutePath: 'B.flac' })).toBe('B.flac');
        expect(toHandleId({ path: 'C.flac' })).toBe('C.flac');
    });

    it('returns empty string for falsy / unknown shapes', () => {
        expect(toHandleId(null)).toBe('');
        expect(toHandleId(undefined)).toBe('');
        expect(toHandleId({} as unknown)).toBe('');
    });
});

describe('utils.normalizeHandleList', () => {
    it('honours Convert() before toArray() and Array.isArray()', () => {
        const fakeFromConvert = ['x', 'y'];
        const fakeFromToArray = ['ignored'];
        const obj = {
            Convert: () => fakeFromConvert,
            toArray: () => fakeFromToArray,
        };
        expect(normalizeHandleList(obj)).toBe(fakeFromConvert);
    });

    it('honours toArray() when Convert() is missing', () => {
        const fake = ['only-toArray'];
        const obj = { toArray: () => fake };
        expect(normalizeHandleList(obj)).toBe(fake);
    });

    it('returns the input directly for plain arrays', () => {
        const arr = ['a', 'b'];
        expect(normalizeHandleList(arr)).toBe(arr);
    });

    it('handles `length`-based array-likes', () => {
        const al = { 0: 'a', 1: 'b', length: 2 };
        expect(normalizeHandleList(al)).toEqual(['a', 'b']);
    });

    it('returns [] for falsy / unrecognised inputs', () => {
        expect(normalizeHandleList(null)).toEqual([]);
        expect(normalizeHandleList(undefined)).toEqual([]);
        expect(normalizeHandleList(42)).toEqual([]);
    });
});

describe('utils.toHandleIdArray', () => {
    it('flattens an FbMetadbHandleList → string ids', () => {
        const list = new FbMetadbHandleList(['A.flac', 'B.flac|subsong:1']);
        expect(toHandleIdArray(list)).toEqual(['A.flac', 'B.flac|subsong:1']);
    });

    it('drops empty / unparseable entries', () => {
        expect(
            toHandleIdArray([{ unknown: true }, { Path: 'X' }, '', 'A.flac']),
        ).toEqual(['X', 'A.flac']);
    });
});

describe('utils.clamp', () => {
    it('respects lower bound', () => {
        expect(clamp(-5, 0, 10)).toBe(0);
    });

    it('respects upper bound', () => {
        expect(clamp(20, 0, 10)).toBe(10);
    });

    it('passes-through values within range', () => {
        expect(clamp(5, 0, 10)).toBe(5);
    });
});

describe('utils.sleep', () => {
    it('resolves after the requested delay', async () => {
        const before = Date.now();
        await sleep(10);
        expect(Date.now() - before).toBeGreaterThanOrEqual(5);
    });
});

describe('utils.getInvoke', () => {
    it('returns null when globalThis.smp is missing', () => {
        const g = globalThis as { smp?: unknown };
        const saved = g.smp;
        try {
            delete g.smp;
            expect(getInvoke()).toBeNull();
        } finally {
            if (saved !== undefined) g.smp = saved;
        }
    });

    it('returns the function when globalThis.smp.invoke is a function', () => {
        const g = globalThis as { smp?: { invoke?: unknown } };
        const saved = g.smp;
        try {
            const fn = vi.fn();
            g.smp = { invoke: fn };
            expect(getInvoke()).toBe(fn);
        } finally {
            if (saved === undefined) delete g.smp;
            else g.smp = saved;
        }
    });
});

describe('utils.buildMenuItems', () => {
    function makeState(baseId = 1): SmpMenuBuildState {
        return { nextId: baseId, limit: null, idMap: new Map() };
    }

    it('expands command items with auto-incrementing ids', () => {
        const items: SmpRawMenuItem[] = [
            { type: 'command', label: 'Play', commandId: 100 },
            { type: 'command', label: 'Stop', commandId: 200 },
        ];
        const state = makeState();
        const out = buildMenuItems(items, state);
        expect(out).toHaveLength(2);
        expect(state.idMap.get(1)).toBe(100);
        expect(state.idMap.get(2)).toBe(200);
    });

    it('preserves separators', () => {
        const items: SmpRawMenuItem[] = [
            { type: 'command', label: 'A', commandId: 1 },
            { type: 'separator' },
            { type: 'command', label: 'B', commandId: 2 },
        ];
        const state = makeState();
        const out = buildMenuItems(items, state);
        expect(out[1]).toEqual({ type: 'separator' });
    });

    it('recurses into submenu children', () => {
        const items: SmpRawMenuItem[] = [
            {
                type: 'submenu',
                label: 'Group',
                children: [
                    { type: 'command', label: 'A', commandId: 10 },
                    { type: 'command', label: 'B', commandId: 11 },
                ],
            },
        ];
        const state = makeState();
        const out = buildMenuItems(items, state);
        expect(out[0]).toMatchObject({ label: 'Group' });
        if ('submenu' in out[0]) {
            expect(out[0].submenu).toHaveLength(2);
        }
    });

    it('drops empty submenus', () => {
        const items: SmpRawMenuItem[] = [
            { type: 'submenu', label: 'Empty', children: [] },
        ];
        const out = buildMenuItems(items, makeState());
        expect(out).toEqual([]);
    });

    it('reflects the disabled / checked flags', () => {
        const items: SmpRawMenuItem[] = [
            {
                type: 'command',
                label: 'A',
                commandId: 1,
                flags: MENU_FLAGS.disabled | MENU_FLAGS.checked,
            },
        ];
        const out = buildMenuItems(items, makeState());
        const first = out[0];
        if ('id' in first) {
            expect(first.enabled).toBe(false);
            expect(first.checked).toBe(true);
        }
    });

    it('honours the limit cap', () => {
        const items: SmpRawMenuItem[] = [
            { type: 'command', label: 'A', commandId: 1 },
            { type: 'command', label: 'B', commandId: 2 },
            { type: 'command', label: 'C', commandId: 3 },
        ];
        const state: SmpMenuBuildState = {
            nextId: 1,
            limit: 3, // exclusive — only 2 items fit (1, 2 → next would be 3)
            idMap: new Map(),
        };
        const out = buildMenuItems(items, state);
        expect(out).toHaveLength(2);
    });

    it('maps path / guid into idMap when commandId missing', () => {
        const items: SmpRawMenuItem[] = [
            { type: 'command', label: 'P', path: 'main/Playback/Play' },
            { type: 'command', label: 'G', guid: 'abcd-1234' },
        ];
        const state = makeState();
        buildMenuItems(items, state);
        expect(state.idMap.get(1)).toBe('main/Playback/Play');
        expect(state.idMap.get(2)).toBe('abcd-1234');
    });
});

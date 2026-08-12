// sdk/src/smp/utils.test.ts
//
// smpUtils helpers (toHandleId / normalizeHandleList /
// toHandleIdArray / clamp / sleep / buildMenuItems / splitMenuAddress) contract.

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
    splitMenuAddress,
    toHandleId,
    toHandleIdArray,
} from './utils.js';
import type {
    SmpMenuBuildState,
    SmpMenuFamily,
    SmpRawMenuItem,
} from './types.js';

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
    // Defaults to the context family because most cases below exercise
    // `commandId`, which only that family dispatches.
    function makeState(
        baseId = 1,
        family: SmpMenuFamily = 'contextmenu',
    ): SmpMenuBuildState {
        return { nextId: baseId, limit: null, idMap: new Map(), family };
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

    it('prefers normalized booleans over the raw flag word', () => {
        // The host has already decoded state; trusting flags over it would
        // re-derive the same answer at best and disagree at worst.
        const items: SmpRawMenuItem[] = [
            {
                type: 'command',
                label: 'A',
                commandId: 1,
                enabled: false,
                checked: true,
                flags: 0,
            },
        ];
        const out = buildMenuItems(items, makeState());
        const first = out[0];
        if ('id' in first) {
            expect(first.enabled).toBe(false);
            expect(first.checked).toBe(true);
        }
    });

    it('offers an item as enabled when its state was never observed', () => {
        // flags == 0 is bit-identical to "enabled, unchecked", so without
        // stateKnown a never-evaluated row is indistinguishable from a live one.
        // Greying it out would hide a command the host would happily run.
        const items: SmpRawMenuItem[] = [
            {
                type: 'command',
                label: 'A',
                commandId: 1,
                enabled: false,
                checked: true,
                stateKnown: false,
            },
        ];
        const out = buildMenuItems(items, makeState());
        const first = out[0];
        if ('id' in first) {
            expect(first.enabled).toBe(true);
        }
    });

    it('falls back to flags when no booleans are present', () => {
        // An older host sends only the flag word; that path must keep working.
        const items: SmpRawMenuItem[] = [
            {
                type: 'command',
                label: 'A',
                commandId: 1,
                flags: MENU_FLAGS.radiochecked,
            },
        ];
        const out = buildMenuItems(items, makeState());
        const first = out[0];
        if ('id' in first) {
            expect(first.enabled).toBe(true);
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
            family: 'contextmenu',
        };
        const out = buildMenuItems(items, state);
        expect(out).toHaveLength(2);
    });

    it('maps path / guid into idMap for the main menu', () => {
        const items: SmpRawMenuItem[] = [
            { type: 'command', label: 'P', path: 'main/Playback/Play' },
            { type: 'command', label: 'G', guid: 'abcd-1234' },
        ];
        const state = makeState(1, 'mainmenu');
        buildMenuItems(items, state);
        expect(state.idMap.get(1)).toBe('main/Playback/Play');
        expect(state.idMap.get(2)).toBe('abcd-1234');
    });

    it('prefers guid over path when both are present', () => {
        // The host resolves a GUID directly, while a path has to be matched
        // against a generated menu tree — a lookup that fails outright on
        // localized hosts. Preferring path there left main-menu dispatch broken.
        const items: SmpRawMenuItem[] = [
            {
                type: 'command',
                label: 'Both',
                path: 'main/Playback/Play',
                guid: 'abcd-1234',
            },
        ];
        const state = makeState(1, 'mainmenu');
        buildMenuItems(items, state);
        expect(state.idMap.get(1)).toBe('abcd-1234');
    });

    it('still prefers commandId over guid for context-menu sessions', () => {
        const items: SmpRawMenuItem[] = [
            { type: 'command', label: 'C', commandId: 42, guid: 'abcd-1234' },
        ];
        const state = makeState();
        buildMenuItems(items, state);
        expect(state.idMap.get(1)).toBe(42);
    });

    it('never maps a main-menu row to its commandId', () => {
        // Both main-menu tiers emit a commandId unconditionally, so preferring
        // it meant every allocated id held a number. `menu.runMainMenuCommand`
        // declares `command` as a string and rejects one on type, which made
        // main-menu dispatch fail for every row rather than for an odd few.
        const items: SmpRawMenuItem[] = [
            {
                type: 'command',
                label: 'Always on top',
                commandId: 1,
                guid: 'abcd-1234',
                path: 'View/Always on top',
            },
        ];
        const state = makeState(1, 'mainmenu');
        buildMenuItems(items, state);
        expect(state.idMap.get(1)).toBe('abcd-1234');
    });

    it('encodes a dynamic main-menu child as guid + subGuid', () => {
        // Dispatching the owning GUID alone would run the container slot, which
        // the host documents as undefined behaviour.
        const items: SmpRawMenuItem[] = [
            {
                type: 'command',
                label: 'Show',
                commandId: 7,
                guid: 'owner-guid',
                subGuid: 'node-guid',
            },
        ];
        const state = makeState(1, 'mainmenu');
        buildMenuItems(items, state);
        expect(state.idMap.get(1)).toBe('owner-guid|node-guid');
    });

    it('leaves a row unmapped when the family cannot dispatch it', () => {
        // Storing an identifier the target endpoint rejects on type turns a
        // clean "unmapped id" into a host exception, so neither family falls
        // back to the other's identifier space.
        const mainOnlyCommandId = makeState(1, 'mainmenu');
        buildMenuItems(
            [{ type: 'command', label: 'A', commandId: 3 }],
            mainOnlyCommandId,
        );
        expect(mainOnlyCommandId.idMap.has(1)).toBe(false);

        const contextOnlyGuid = makeState(1, 'contextmenu');
        buildMenuItems(
            [{ type: 'command', label: 'B', guid: 'abcd-1234' }],
            contextOnlyGuid,
        );
        expect(contextOnlyGuid.idMap.has(1)).toBe(false);
    });

    it('falls back to the main menu when family is absent', () => {
        // Untyped callers reach this function through `window.smpUtils`; the
        // main menu is the safer default because it cannot dispatch a number
        // under any circumstance.
        const state = {
            nextId: 1,
            limit: null,
            idMap: new Map<number, number | string>(),
        } as SmpMenuBuildState;
        buildMenuItems(
            [{ type: 'command', label: 'A', commandId: 5, guid: 'abcd-1234' }],
            state,
        );
        expect(state.idMap.get(1)).toBe('abcd-1234');
    });
});

describe('utils.splitMenuAddress', () => {
    it('returns a bare guid unchanged', () => {
        expect(splitMenuAddress('{ABCD-1234}')).toEqual({
            command: '{ABCD-1234}',
        });
    });

    it('splits an owner/node pair', () => {
        expect(splitMenuAddress('owner-guid|node-guid')).toEqual({
            command: 'owner-guid',
            subGuid: 'node-guid',
        });
    });

    it('passes a path through untouched', () => {
        expect(splitMenuAddress('View/Always on top')).toEqual({
            command: 'View/Always on top',
        });
    });
});

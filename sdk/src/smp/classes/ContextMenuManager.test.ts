// sdk/src/smp/classes/ContextMenuManager.test.ts
//
// Dispatch contract: BuildMenu maps each allocated id to the numeric
// `commandId` the host reported, and ExecuteByID sends only that number to
// `menu.runContextCommandById`, together with the `mode` and `handles` the
// menu was built from. The id map is typed `number | string` because the
// builder is shared with the main menu, whose rows are addressed by GUID or
// path; a context-menu row never maps to a string, so a string can only reach
// ExecuteByID from the caller, and SMP's ExecuteByID takes a number. A raw
// host id is accepted only outside the ids BuildMenu handed out.

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import { ContextMenuManager } from './ContextMenuManager.js';
import { FbMetadbHandle } from './FbMetadbHandle.js';
import { FbMetadbHandleList } from './FbMetadbHandleList.js';
import type { SmpRawMenuItem } from '../types.js';

type InvokeMock = ReturnType<typeof vi.fn>;

const globalWithSmp = globalThis as { smp?: { invoke?: unknown } };
let savedSmp: unknown;
let invoke: InvokeMock;

/** Serve `menu.getContextMenu` from `items`; report success for everything else. */
function installHost(items: SmpRawMenuItem[]): void {
    invoke = vi.fn(async (method: string) =>
        method === 'menu.getContextMenu'
            ? { success: true, mode: 'handles', items }
            : { success: true },
    );
    globalWithSmp.smp = { invoke };
}

/** Methods invoked so far, in order. */
function methods(): string[] {
    return invoke.mock.calls.map((args) => String(args[0]));
}

/** A manager whose menu is built for one CUE subsong and one plain file. */
async function builtManager(items: SmpRawMenuItem[]): Promise<ContextMenuManager> {
    installHost(items);
    const cm = new ContextMenuManager();
    cm.InitContext(
        new FbMetadbHandleList([
            new FbMetadbHandle('C:\\album.cue|subsong:2'),
            new FbMetadbHandle('C:\\a.flac'),
        ]),
    );
    await cm.BuildMenu(undefined, 100);
    return cm;
}

beforeEach(() => {
    savedSmp = globalWithSmp.smp;
});

afterEach(() => {
    if (savedSmp === undefined) delete globalWithSmp.smp;
    else globalWithSmp.smp = savedSmp as { invoke?: unknown };
});

describe('ContextMenuManager.ExecuteByID', () => {
    it('runs the host commandId with the mode and handles the menu was built from', async () => {
        const cm = await builtManager([
            { type: 'command', label: 'Properties', path: 'Properties', commandId: 42 },
        ]);

        expect(await cm.ExecuteByID(100)).toBe(true);

        expect(invoke.mock.calls[0]).toEqual([
            'menu.getContextMenu',
            { mode: 'handles', handles: ['C:\\album.cue|subsong:2', 'C:\\a.flac'] },
        ]);
        expect(invoke.mock.calls[1]).toEqual([
            'menu.runContextCommandById',
            { id: 42, mode: 'handles', handles: ['C:\\album.cue|subsong:2', 'C:\\a.flac'] },
        ]);
    });

    it('resolves a numeric string through the same map', async () => {
        const cm = await builtManager([
            { type: 'command', label: 'Properties', path: 'Properties', commandId: 42 },
        ]);

        expect(await cm.ExecuteByID('100')).toBe(true);
        expect(invoke.mock.calls[1][1]).toMatchObject({ id: 42 });
    });

    it('leaves a row without commandId unmapped instead of sending its guid or path', async () => {
        const cm = await builtManager([
            {
                type: 'command',
                label: 'Rating',
                path: 'Playback Statistics/Rating',
                guid: '{11111111-2222-3333-4444-555555555555}',
            },
        ]);

        // Id 100 was allocated to this row; reading it as a raw host id would
        // run whichever command the host numbered 100.
        expect(await cm.ExecuteByID(100)).toBe(false);
        expect(methods()).toEqual(['menu.getContextMenu']);
    });

    it('still takes a raw host id outside the ids BuildMenu allocated', async () => {
        const cm = await builtManager([
            { type: 'command', label: 'Properties', path: 'Properties', commandId: 42 },
        ]);

        expect(await cm.ExecuteByID(7)).toBe(true);
        expect(invoke.mock.calls[1]).toEqual([
            'menu.runContextCommandById',
            { id: 7, mode: 'handles', handles: ['C:\\album.cue|subsong:2', 'C:\\a.flac'] },
        ]);
    });

    it('reports false for a non-numeric string without calling the host', async () => {
        const cm = await builtManager([
            { type: 'command', label: 'Properties', path: 'Properties', commandId: 42 },
        ]);

        expect(await cm.ExecuteByID('Properties')).toBe(false);
        expect(methods()).toEqual(['menu.getContextMenu']);
    });
});

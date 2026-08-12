// sdk/src/smp/classes/MainMenuManager.test.ts
//
// End-to-end dispatch contract: BuildMenu allocates ids, ExecuteByID turns one
// back into the payload `menu.runMainMenuCommand` accepts. Covered here rather
// than only at the buildMenuItems level because the identifier crosses a
// boundary — the builder chooses it, the manager sends it — and a mismatch is
// only observable once both halves run together.

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import { MainMenuManager } from './MainMenuManager.js';
import type { SmpRawMenuItem } from '../types.js';

type InvokeMock = ReturnType<typeof vi.fn>;

const globalWithSmp = globalThis as { smp?: { invoke?: unknown } };
let savedSmp: unknown;
let invoke: InvokeMock;

/** Serve `menu.getMainMenu` from `items`; report success for everything else. */
function installHost(items: SmpRawMenuItem[]): void {
    invoke = vi.fn(async (method: string) =>
        method === 'menu.getMainMenu' ? { success: true, items } : { success: true },
    );
    globalWithSmp.smp = { invoke };
}

/** Arguments of the single `menu.runMainMenuCommand` call. */
function dispatchedPayload(): Record<string, unknown> {
    const call = invoke.mock.calls.find(
        (args) => args[0] === 'menu.runMainMenuCommand',
    );
    expect(call, 'expected a menu.runMainMenuCommand invocation').toBeDefined();
    return call![1] as Record<string, unknown>;
}

beforeEach(() => {
    savedSmp = globalWithSmp.smp;
});

afterEach(() => {
    if (savedSmp === undefined) delete globalWithSmp.smp;
    else globalWithSmp.smp = savedSmp as { invoke?: unknown };
});

describe('MainMenuManager.ExecuteByID', () => {
    it('dispatches a string command even when the row carries a commandId', async () => {
        // Both main-menu tiers emit a commandId unconditionally. Sending it
        // made the host reject every row on type, so main-menu dispatch failed
        // wholesale rather than in edge cases.
        installHost([
            {
                type: 'command',
                label: 'Always on top',
                commandId: 1,
                guid: '{77CFBCD0-0000-0000-0000-000000000000}',
                path: 'View/Always on top',
            },
        ]);

        const mgr = new MainMenuManager();
        mgr.Init('View');
        await mgr.BuildMenu(undefined, 1, 100);

        expect(await mgr.ExecuteByID(1)).toBe(true);
        const payload = dispatchedPayload();
        expect(typeof payload.command).toBe('string');
        expect(payload.command).toBe('{77CFBCD0-0000-0000-0000-000000000000}');
        expect(payload).not.toHaveProperty('subGuid');
    });

    it('splits a dynamic child into command + subGuid', async () => {
        installHost([
            {
                type: 'command',
                label: 'Show',
                commandId: 3,
                guid: '{OWNER}',
                subGuid: '{NODE}',
            },
        ]);

        const mgr = new MainMenuManager();
        mgr.Init('View');
        await mgr.BuildMenu(undefined, 1, 100);

        expect(await mgr.ExecuteByID(1)).toBe(true);
        expect(dispatchedPayload()).toEqual({
            command: '{OWNER}',
            subGuid: '{NODE}',
        });
    });

    it('falls back to the path when the row has no guid', async () => {
        installHost([
            { type: 'command', label: 'Play', commandId: 9, path: 'Playback/Play' },
        ]);

        const mgr = new MainMenuManager();
        mgr.Init('Playback');
        await mgr.BuildMenu(undefined, 1, 100);

        expect(await mgr.ExecuteByID(1)).toBe(true);
        expect(dispatchedPayload()).toEqual({ command: 'Playback/Play' });
    });

    it('refuses an unmapped id without reaching the host', async () => {
        installHost([{ type: 'command', label: 'Orphan', commandId: 4 }]);

        const mgr = new MainMenuManager();
        mgr.Init('View');
        await mgr.BuildMenu(undefined, 1, 100);

        expect(await mgr.ExecuteByID(1)).toBe(false);
        expect(
            invoke.mock.calls.some((args) => args[0] === 'menu.runMainMenuCommand'),
        ).toBe(false);
    });

    it('passes a raw command string through unchanged', async () => {
        installHost([]);

        const mgr = new MainMenuManager();
        expect(await mgr.ExecuteByID('File/Preferences')).toBe(true);
        expect(dispatchedPayload()).toEqual({ command: 'File/Preferences' });
    });
});

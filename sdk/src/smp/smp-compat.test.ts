// sdk/src/smp/smp-compat.test.ts
//
// Phase 5.1 test gate (plan §8.7) — bootstrap orchestrator end-to-end
// contract. Verifies that the SMP compat layer:
//
// 1. Surfaces every public field of `SmpCompatApi` (cache, ready,
//    invoke, parseHandleId, formatHandleId, stripSubsongSuffix,
//    refreshCache, dispose).
// 2. Wires `fb.on(...)` subscriptions whose count matches the legacy
//    bootstrap (cache + 9 playlist refresh handlers = 21 subscriptions).
// 3. Builds the `plman` object with the documented surface (every
//    method from `SMPPlman` is reachable).
// 4. Installs `fb.onSMP` and the SMP-style fb extensions (Play, Pause,
//    Volume, IsPlaying, ...).
// 5. dispose() unsubscribes every event listener and the count drops
//    to zero.
// 6. The reload guard (`installSmpGlobals` re-call) does nothing on
//    second invocation.

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import { bootstrapSmpCompat, installSmpGlobals } from './bootstrap.js';
import type { SmpBridgeShape } from './bridgeShape.js';

interface MockBridge extends SmpBridgeShape {
    invoke: ReturnType<typeof vi.fn>;
    on: ReturnType<typeof vi.fn>;
    off: ReturnType<typeof vi.fn>;
    once: ReturnType<typeof vi.fn>;
}

function makeMockBridge(): {
    fb: MockBridge;
    subscriptions: Array<{ event: string; off: ReturnType<typeof vi.fn> }>;
} {
    const subscriptions: Array<{
        event: string;
        off: ReturnType<typeof vi.fn>;
    }> = [];

    const on = vi.fn((event: string, _handler: (data: unknown) => void) => {
        const off = vi.fn();
        subscriptions.push({ event, off });
        return off;
    });

    // The bridge `on()` returns the unsubscribe directly; mock bridge
    // mirrors that contract so the bootstrap can collect cleanup callbacks.
    const fb = {
        invoke: vi.fn().mockImplementation(async (method: string) => {
            // Provide minimal fixtures for the populateCache pass —
            // every legacy invoke target gets an empty-shape response.
            if (method === 'playlist.getAll') return [];
            if (method === 'playback.getState') return { state: 'stopped' };
            if (method === 'playback.getVolume') return { volumeDb: -20, muted: false };
            if (method === 'playback.getPosition') return { position: 0, duration: 0 };
            if (method === 'playback.getCurrentTrack') return { found: false };
            if (method === 'playback.getPlaybackOrder') return { orderIndex: 0 };
            return {};
        }),
        on,
        off: vi.fn(),
        once: vi.fn(),
    } as unknown as MockBridge;

    return { fb, subscriptions };
}

describe('bootstrapSmpCompat', () => {
    afterEach(() => {
        vi.clearAllMocks();
    });

    it('returns the documented SmpCompatApi shape', () => {
        const { fb } = makeMockBridge();
        const { smp } = bootstrapSmpCompat(fb);

        expect(typeof smp.invoke).toBe('function');
        expect(typeof smp.parseHandleId).toBe('function');
        expect(typeof smp.formatHandleId).toBe('function');
        expect(typeof smp.stripSubsongSuffix).toBe('function');
        expect(typeof smp.refreshCache).toBe('function');
        expect(typeof smp.dispose).toBe('function');
        expect(smp.cache).toBeDefined();
        expect(smp.ready).toBeInstanceOf(Promise);
        expect(smp.__smpCompatLoaded).toBe(true);
    });

    it('subscribes to every cache-relevant event (cache + playlist refresh)', () => {
        const { fb, subscriptions } = makeMockBridge();
        bootstrapSmpCompat(fb);

        // 12 cache-driving events:
        //   playback:stateChanged / paused / volumeChanged / time /
        //   trackChanged / stopped / orderChanged / stopAfterCurrentChanged
        //   window:alwaysOnTopChanged
        //   playback:cursorFollowChanged / followCursorChanged
        //   audio:replaygainModeChanged
        // + 9 playlist-refresh events (activated, created, removed,
        //   reordered, renamed, lockChanged, itemsAdded, itemsRemoved,
        //   itemsReordered)
        // = 21 subscriptions.
        expect(subscriptions.length).toBe(21);

        // Spot-check a few canonical names.
        const events = subscriptions.map((s) => s.event);
        expect(events).toContain('playback:stateChanged');
        expect(events).toContain('playback:trackChanged');
        expect(events).toContain('playback:stopped');
        expect(events).toContain('audio:replaygainModeChanged');
        expect(events).toContain('playlist:activated');
    });

    it('builds plman with every documented method', () => {
        const { fb } = makeMockBridge();
        const { plman } = bootstrapSmpCompat(fb);

        // Sync helpers
        expect(typeof plman.GetPlaylistName).toBe('function');
        expect(typeof plman.PlaylistItemCount).toBe('function');
        expect(typeof plman.FindPlaylist).toBe('function');
        expect(typeof plman.IsAutoPlaylist).toBe('function');
        expect(typeof plman.IsPlaylistLocked).toBe('function');

        // Async methods
        expect(typeof plman.RenamePlaylist).toBe('function');
        expect(typeof plman.CreatePlaylist).toBe('function');
        expect(typeof plman.RemovePlaylist).toBe('function');
        expect(typeof plman.ClearPlaylist).toBe('function');
        expect(typeof plman.GetPlaylistItems).toBe('function');
        expect(typeof plman.GetPlaylistSelectedItems).toBe('function');
        expect(typeof plman.GetPlaylistFocusItemIndex).toBe('function');
        expect(typeof plman.SetPlaylistFocusItem).toBe('function');
        expect(typeof plman.SetPlaylistSelection).toBe('function');
        expect(typeof plman.ClearPlaylistSelection).toBe('function');
        expect(typeof plman.InsertPlaylistItems).toBe('function');
        expect(typeof plman.RemovePlaylistSelection).toBe('function');
        expect(typeof plman.SortByFormat).toBe('function');
        expect(typeof plman.UndoBackup).toBe('function');
        expect(typeof plman.MovePlaylistSelection).toBe('function');
        expect(typeof plman.CreateAutoPlaylist).toBe('function');
        expect(typeof plman.DuplicatePlaylist).toBe('function');
        expect(typeof plman.AddLocations).toBe('function');
        expect(typeof plman.AddItemToPlaybackQueue).toBe('function');
        expect(typeof plman.FlushPlaybackQueue).toBe('function');
        expect(typeof plman.GetPlaybackQueueContents).toBe('function');
    });

    it('attaches SMP-style methods and properties to fb', () => {
        const { fb } = makeMockBridge();
        bootstrapSmpCompat(fb);

        // Sample of base methods (Play, Pause, Stop, Volume, PlaybackTime).
        const fbMut = fb as unknown as {
            Play?: () => unknown;
            Pause?: () => unknown;
            Stop?: () => unknown;
            VolumeUp?: () => unknown;
            VolumeMute?: () => unknown;
            Volume?: number;
            PlaybackTime?: number;
            IsPlaying?: boolean;
            IsPaused?: boolean;
            ComponentPath?: string;
            FoobarPath?: string;
            ProfilePath?: string;
            TitleFormat?: (expr: string) => unknown;
            CreateHandleList?: () => unknown;
            CreateProfiler?: () => unknown;
            GetSelection?: () => unknown;
            CreateContextMenuManager?: () => unknown;
            CreateMainMenuManager?: () => unknown;
            onSMP?: (event: string, cb: () => void) => () => void;
        };

        expect(typeof fbMut.Play).toBe('function');
        expect(typeof fbMut.Pause).toBe('function');
        expect(typeof fbMut.Stop).toBe('function');
        expect(typeof fbMut.VolumeUp).toBe('function');
        expect(typeof fbMut.VolumeMute).toBe('function');
        expect(typeof fbMut.Volume).toBe('number'); // getter resolves
        expect(typeof fbMut.PlaybackTime).toBe('number');
        expect(typeof fbMut.IsPlaying).toBe('boolean');
        expect(typeof fbMut.IsPaused).toBe('boolean');
        expect(typeof fbMut.ComponentPath).toBe('string');
        expect(typeof fbMut.FoobarPath).toBe('string');
        expect(typeof fbMut.ProfilePath).toBe('string');

        expect(typeof fbMut.TitleFormat).toBe('function');
        expect(typeof fbMut.CreateHandleList).toBe('function');
        expect(typeof fbMut.CreateProfiler).toBe('function');
        expect(typeof fbMut.GetSelection).toBe('function');
        expect(typeof fbMut.CreateContextMenuManager).toBe('function');
        expect(typeof fbMut.CreateMainMenuManager).toBe('function');
        expect(typeof fbMut.onSMP).toBe('function');
    });

    it('dispose() unsubscribes every event listener', () => {
        const { fb, subscriptions } = makeMockBridge();
        const { smp } = bootstrapSmpCompat(fb);

        const callsBefore = subscriptions.reduce(
            (n, s) => n + s.off.mock.calls.length,
            0,
        );
        expect(callsBefore).toBe(0);

        smp.dispose();

        for (const s of subscriptions) {
            expect(s.off).toHaveBeenCalledTimes(1);
        }
    });

    it('cache reflects the populateCache pass once ready resolves', async () => {
        const { fb } = makeMockBridge();
        const { smp } = bootstrapSmpCompat(fb);

        await smp.ready;

        // Mock playback.getState returns { state: 'stopped' }, which keeps
        // isPlaying/isPaused at their defaults (false/false).
        expect(smp.cache.isPlaying).toBe(false);
        expect(smp.cache.isPaused).toBe(false);
        // volumeDb populated from getVolume mock.
        expect(smp.cache.volumeDb).toBe(-20);
    });

    it('parseHandleId / formatHandleId / stripSubsongSuffix are wired through', () => {
        const { fb } = makeMockBridge();
        const { smp } = bootstrapSmpCompat(fb);

        expect(smp.formatHandleId('C:\\a.flac', 2)).toBe('C:\\a.flac|subsong:2');
        expect(smp.parseHandleId('C:\\a.flac|subsong:5')).toMatchObject({
            path: 'C:\\a.flac',
            subsong: 5,
        });
        expect(smp.stripSubsongSuffix('C:\\a.flac|subsong:9')).toBe('C:\\a.flac');
    });
});

describe('installSmpGlobals', () => {
    let savedSmp: unknown;
    let savedPlman: unknown;
    let savedSmpUtils: unknown;
    let savedUtils: unknown;

    beforeEach(() => {
        const g = globalThis as Record<string, unknown>;
        savedSmp = g.smp;
        savedPlman = g.plman;
        savedSmpUtils = g.smpUtils;
        savedUtils = g.utils;
        delete g.smp;
        delete g.plman;
        delete g.smpUtils;
        delete g.utils;
    });

    afterEach(() => {
        const g = globalThis as Record<string, unknown>;
        if (savedSmp === undefined) delete g.smp;
        else g.smp = savedSmp;
        if (savedPlman === undefined) delete g.plman;
        else g.plman = savedPlman;
        if (savedSmpUtils === undefined) delete g.smpUtils;
        else g.smpUtils = savedSmpUtils;
        if (savedUtils === undefined) delete g.utils;
        else g.utils = savedUtils;
    });

    it('hangs window.smp / plman / smpUtils / utils onto globalThis', () => {
        const { fb } = makeMockBridge();
        const result = bootstrapSmpCompat(fb);
        installSmpGlobals(result);

        const g = globalThis as Record<string, unknown>;
        expect(g.smp).toBe(result.smp);
        expect(g.plman).toBe(result.plman);
        expect(g.smpUtils).toBe(result.smpUtils);
        expect(g.utils).toBe(result.utils);
    });

    it('installs every wrapper class onto globalThis', () => {
        const { fb } = makeMockBridge();
        const result = bootstrapSmpCompat(fb);
        installSmpGlobals(result);

        const g = globalThis as Record<string, unknown>;
        expect(g.FbMetadbHandle).toBe(result.classes.FbMetadbHandle);
        expect(g.FbMetadbHandleList).toBe(result.classes.FbMetadbHandleList);
        expect(g.FbFileInfo).toBe(result.classes.FbFileInfo);
        expect(g.FbProfiler).toBe(result.classes.FbProfiler);
        expect(g.FbTitleFormat).toBe(result.classes.FbTitleFormat);
        expect(g.FbUiSelectionHolder).toBe(result.classes.FbUiSelectionHolder);
        expect(g.ContextMenuManager).toBe(result.classes.ContextMenuManager);
        expect(g.MainMenuManager).toBe(result.classes.MainMenuManager);
    });

    it('is idempotent — second invocation is a no-op', () => {
        const { fb } = makeMockBridge();
        const result = bootstrapSmpCompat(fb);
        installSmpGlobals(result);

        const g = globalThis as Record<string, unknown>;
        const firstSmp = g.smp;

        // Re-call with a fresh result; legacy parity demands no overwrite.
        const { fb: fb2 } = makeMockBridge();
        const result2 = bootstrapSmpCompat(fb2);
        installSmpGlobals(result2);

        expect(g.smp).toBe(firstSmp); // unchanged
        // The fresh smp is NOT installed because the loaded flag is set.
        expect(g.smp).not.toBe(result2.smp);
    });
});

// sdk/src/bridge/namespaces/tray.test.ts
//
// Locks the tray facade contract for `layoutMode`: the wrapper must
// pass `config.layoutMode` through unchanged and must not invent a default
// when the caller omits it.

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

type Handler = (data: unknown) => void;

interface MockNative {
    invoke: ReturnType<typeof vi.fn>;
    on: ReturnType<typeof vi.fn>;
    off: ReturnType<typeof vi.fn>;
    handlers: Record<string, Handler[]>;
}

function makeNative(): MockNative {
    const handlers: Record<string, Handler[]> = {};
    return {
        handlers,
        invoke: vi.fn(),
        on: vi.fn((event: string, handler: Handler) => {
            (handlers[event] ||= []).push(handler);
        }),
        off: vi.fn((event: string, handler: Handler) => {
            handlers[event] = (handlers[event] || []).filter((h) => h !== handler);
        }),
    };
}

describe('tray.setContextMenu layoutMode', () => {
    beforeEach(() => vi.resetModules());
    afterEach(() => vi.unstubAllGlobals());

    it('forwards layoutMode zones without rewriting defaults', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true });
        vi.stubGlobal('window', { fb2k: native });
        const { tray } = await import('./tray.js');

        const items = [{ id: 'a', label: 'A' }];
        await tray.setContextMenu(items, {
            render: 'webview',
            layoutMode: 'zones',
        });

        expect(native.invoke).toHaveBeenCalledWith('tray.setContextMenu', {
            items,
            config: { render: 'webview', layoutMode: 'zones' },
        });
    });

    it('omits layoutMode when the caller does not set it', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true });
        vi.stubGlobal('window', { fb2k: native });
        const { tray } = await import('./tray.js');

        const items = [{ id: 'a', label: 'A' }];
        await tray.setContextMenu(items, { render: 'webview' });

        expect(native.invoke).toHaveBeenCalledWith('tray.setContextMenu', {
            items,
            config: { render: 'webview' },
        });
        const payload = native.invoke.mock.calls[0][1] as {
            config?: { layoutMode?: string };
        };
        expect(payload.config).not.toHaveProperty('layoutMode');
    });

    it('forwards layoutMode flat explicitly when provided', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true });
        vi.stubGlobal('window', { fb2k: native });
        const { tray } = await import('./tray.js');

        await tray.setContextMenu([{ id: 'a', label: 'A' }], {
            layoutMode: 'flat',
        });

        expect(native.invoke).toHaveBeenCalledWith('tray.setContextMenu', {
            items: [{ id: 'a', label: 'A' }],
            config: { layoutMode: 'flat' },
        });
    });
});

describe('tray.setContextMenu orientation / checked passthrough', () => {
    beforeEach(() => vi.resetModules());
    afterEach(() => vi.unstubAllGlobals());

    it('forwards slider orientation vertical without injecting defaults', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true });
        vi.stubGlobal('window', { fb2k: native });
        const { tray } = await import('./tray.js');

        const items = [
            { id: 'vol', type: 'slider' as const, min: 0, max: 100, value: 40, orientation: 'vertical' as const },
        ];
        await tray.setContextMenu(items, { render: 'webview' });

        expect(native.invoke).toHaveBeenCalledWith('tray.setContextMenu', {
            items,
            config: { render: 'webview' },
        });
        const payload = native.invoke.mock.calls[0][1] as { items: Array<{ orientation?: string }> };
        expect(payload.items[0].orientation).toBe('vertical');
    });

    it('omits orientation when the caller does not set it', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true });
        vi.stubGlobal('window', { fb2k: native });
        const { tray } = await import('./tray.js');

        const items = [{ id: 'vol', type: 'slider' as const, min: 0, max: 100, value: 40 }];
        await tray.setContextMenu(items, { render: 'webview' });

        const payload = native.invoke.mock.calls[0][1] as { items: Array<Record<string, unknown>> };
        expect(payload.items[0]).not.toHaveProperty('orientation');
    });

    it('forwards checked:false without rewriting it away', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true });
        vi.stubGlobal('window', { fb2k: native });
        const { tray } = await import('./tray.js');

        const items = [{ id: 'c', label: 'Check me', checked: false }];
        await tray.setContextMenu(items);

        const payload = native.invoke.mock.calls[0][1] as { items: Array<{ checked?: boolean }> };
        expect(payload.items[0]).toHaveProperty('checked', false);
    });

    it('setMenuItemState forwards checked:false', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, found: true });
        vi.stubGlobal('window', { fb2k: native });
        const { tray } = await import('./tray.js');

        await tray.setMenuItemState('c', { checked: false });
        expect(native.invoke).toHaveBeenCalledWith('tray.setMenuItemState', {
            id: 'c',
            checked: false,
        });
    });
});

describe('tray.setContextMenu playbackAction passthrough', () => {
    beforeEach(() => vi.resetModules());
    afterEach(() => vi.unstubAllGlobals());

    it('forwards a declared playbackAction unchanged', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true });
        vi.stubGlobal('window', { fb2k: native });
        const { tray } = await import('./tray.js');

        const items = [
            { id: 'bg-next', label: 'Next', playbackAction: 'next' as const },
        ];
        await tray.setContextMenu(items, { showPlaybackControls: false });

        expect(native.invoke).toHaveBeenCalledWith('tray.setContextMenu', {
            items,
            config: { showPlaybackControls: false },
        });
        const payload = native.invoke.mock.calls[0][1] as { items: Array<{ playbackAction?: string }> };
        expect(payload.items[0].playbackAction).toBe('next');
    });

    it('omits playbackAction when the caller does not set it', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true });
        vi.stubGlobal('window', { fb2k: native });
        const { tray } = await import('./tray.js');

        const items = [{ id: 'open-settings', label: 'Settings' }];
        await tray.setContextMenu(items);

        const payload = native.invoke.mock.calls[0][1] as { items: Array<Record<string, unknown>> };
        expect(payload.items[0]).not.toHaveProperty('playbackAction');
    });

    it('forwards each of the four playback tokens', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true });
        vi.stubGlobal('window', { fb2k: native });
        const { tray } = await import('./tray.js');

        const tokens = ['play-pause', 'previous', 'next', 'stop'] as const;
        const items = tokens.map((t, i) => ({ id: `b${i}`, label: t, playbackAction: t }));
        await tray.setContextMenu(items, { showPlaybackControls: false });

        const payload = native.invoke.mock.calls[0][1] as { items: Array<{ playbackAction?: string }> };
        expect(payload.items.map((it) => it.playbackAction)).toEqual([...tokens]);
    });

    it('propagates the plugin INVALID_PARAMS error for a rejected value', async () => {
        const native = makeNative();
        // The plugin validates fail-loud and returns an error envelope; the
        // wrapper surfaces it unchanged (no client-side validation).
        native.invoke.mockResolvedValue({
            success: false,
            error: 'invalid playbackAction',
            code: 'INVALID_PARAMS',
        });
        vi.stubGlobal('window', { fb2k: native });
        const { tray } = await import('./tray.js');

        const items = [{ id: 'x', label: 'X', playbackAction: 'next' as const }];
        const res = (await tray.setContextMenu(items)) as { success: boolean; error?: string };
        expect(res.success).toBe(false);
        expect(res.error).toBe('invalid playbackAction');
    });

    it('round-trips playbackAction through getMenuItems', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: true,
            items: [{ id: 'bg-next', label: 'Next', type: 'normal', playbackAction: 'next' }],
        });
        vi.stubGlobal('window', { fb2k: native });
        const { tray } = await import('./tray.js');

        const res = (await tray.getMenuItems()) as { items: Array<{ playbackAction?: string }> };
        expect(res.items[0].playbackAction).toBe('next');
    });
});

// sdk/src/bridge/namespaces/menu.test.ts
//
// Locks the self-drawn menu facade contract: `show` / `close` invoke the
// dot-form handlers with the expected payloads, and `popup` resolves with the
// selected item id on `menu:select` (or `null` on `menu:dismiss`), matched by
// the menu id returned from `menu.show` so overlapping popups never
// cross-resolve.

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

type Handler = (data: unknown) => void;

interface MockNative {
    invoke: ReturnType<typeof vi.fn>;
    on: ReturnType<typeof vi.fn>;
    off: ReturnType<typeof vi.fn>;
    handlers: Record<string, Handler[]>;
    emit: (event: string, payload: unknown) => void;
}

function makeNative(): MockNative {
    const handlers: Record<string, Handler[]> = {};
    const native: MockNative = {
        handlers,
        invoke: vi.fn(),
        on: vi.fn((event: string, handler: Handler) => {
            (handlers[event] ||= []).push(handler);
        }),
        off: vi.fn((event: string, handler: Handler) => {
            handlers[event] = (handlers[event] || []).filter((h) => h !== handler);
        }),
        emit: (event: string, payload: unknown) => {
            for (const h of handlers[event] || []) h(payload);
        },
    };
    return native;
}

/** Flush pending micro/macrotasks so awaited invokes settle and handlers bind. */
function flush(): Promise<void> {
    return new Promise((resolve) => setTimeout(resolve, 0));
}

describe('menu.show', () => {
    beforeEach(() => vi.resetModules());
    afterEach(() => vi.unstubAllGlobals());

    it('invokes menu.show with items only when no position is given', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, menuId: 'menu-1' });
        vi.stubGlobal('window', { fb2k: native });
        const { menu } = await import('./menu.js');

        const items = [{ id: 'a', label: 'A' }];
        const result = await menu.show(items);

        expect(native.invoke).toHaveBeenCalledWith('menu.show', { items });
        expect(result).toEqual({ success: true, menuId: 'menu-1' });
    });

    it('forwards x/y when a position is given', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, menuId: 'menu-2' });
        vi.stubGlobal('window', { fb2k: native });
        const { menu } = await import('./menu.js');

        const items = [{ id: 'a', label: 'A' }];
        await menu.show(items, { x: 200, y: 150 });

        expect(native.invoke).toHaveBeenCalledWith('menu.show', {
            items,
            x: 200,
            y: 150,
        });
    });
});

describe('menu.close', () => {
    beforeEach(() => vi.resetModules());
    afterEach(() => vi.unstubAllGlobals());

    it('forwards an explicit reason', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true });
        vi.stubGlobal('window', { fb2k: native });
        const { menu } = await import('./menu.js');

        await menu.close('api');

        expect(native.invoke).toHaveBeenCalledWith('menu.close', { reason: 'api' });
    });

    it('omits the reason key when none is given', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true });
        vi.stubGlobal('window', { fb2k: native });
        const { menu } = await import('./menu.js');

        await menu.close();

        expect(native.invoke).toHaveBeenCalledWith('menu.close', {});
    });
});

describe('menu.popup', () => {
    beforeEach(() => vi.resetModules());
    afterEach(() => vi.unstubAllGlobals());

    it('resolves with the selected item id on menu:select', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, menuId: 'menu-1' });
        vi.stubGlobal('window', { fb2k: native });
        const { menu } = await import('./menu.js');

        const p = menu.popup([{ id: 'play', label: 'Play' }]);
        await flush();
        native.emit('menu:select', { menuId: 'menu-1', itemId: 'play' });

        await expect(p).resolves.toBe('play');
    });

    it('resolves with null on menu:dismiss', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, menuId: 'menu-1' });
        vi.stubGlobal('window', { fb2k: native });
        const { menu } = await import('./menu.js');

        const p = menu.popup([{ id: 'play', label: 'Play' }]);
        await flush();
        native.emit('menu:dismiss', { menuId: 'menu-1', reason: 'outside' });

        await expect(p).resolves.toBeNull();
    });

    it('ignores events whose menuId does not match', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, menuId: 'menu-1' });
        vi.stubGlobal('window', { fb2k: native });
        const { menu } = await import('./menu.js');

        const p = menu.popup([{ id: 'play', label: 'Play' }]);
        await flush();
        native.emit('menu:select', { menuId: 'other', itemId: 'nope' });
        native.emit('menu:dismiss', { menuId: 'other', reason: 'x' });
        native.emit('menu:select', { menuId: 'menu-1', itemId: 'play' });

        await expect(p).resolves.toBe('play');
    });

    it('unbinds both listeners after settling', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, menuId: 'menu-1' });
        vi.stubGlobal('window', { fb2k: native });
        const { menu } = await import('./menu.js');

        const p = menu.popup([{ id: 'play', label: 'Play' }]);
        await flush();
        native.emit('menu:select', { menuId: 'menu-1', itemId: 'play' });
        await p;

        expect(native.off).toHaveBeenCalledWith('menu:select', expect.any(Function));
        expect(native.off).toHaveBeenCalledWith('menu:dismiss', expect.any(Function));
        expect(native.handlers['menu:select']).toHaveLength(0);
        expect(native.handlers['menu:dismiss']).toHaveLength(0);
    });

    it('resolves null when menu.show fails', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: false, error: 'no window' });
        vi.stubGlobal('window', { fb2k: native });
        const { menu } = await import('./menu.js');

        const result = await menu.popup([{ id: 'play', label: 'Play' }]);

        expect(result).toBeNull();
    });
});

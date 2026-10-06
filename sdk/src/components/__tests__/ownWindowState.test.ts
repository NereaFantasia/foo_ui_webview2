// sdk/src/components/__tests__/ownWindowState.test.ts
//
// window:stateChanged goes to every window. <fb-titlebar> and
// <fb-window-controls> must follow only their own window: an event that
// names another window is ignored, one without windowId (a host that does
// not send it) is taken as before, and events that arrive before the own
// id is known are held rather than applied or lost.

/// <reference types="vite/client" />

import { beforeAll, describe, expect, it, vi } from 'vitest';
import { OwnWindowStateFilter, learnOwnWindowId } from '../ownWindowState.js';

interface State {
    windowId?: string;
    isMaximized: boolean;
}

function makeFilter() {
    const delivered: State[] = [];
    const filter = new OwnWindowStateFilter<State>((s) => delivered.push(s));
    return { filter, delivered };
}

describe('OwnWindowStateFilter', () => {
    it('passes only the own window once its id is known', () => {
        const { filter, delivered } = makeFilter();
        filter.resolve('popup-1');
        filter.push({ windowId: 'main', isMaximized: true });
        filter.push({ windowId: 'popup-1', isMaximized: true });
        filter.push({ windowId: 'popup-2', isMaximized: false });
        expect(delivered).toEqual([{ windowId: 'popup-1', isMaximized: true }]);
    });

    it('passes events without windowId at once, before and after the id is known', () => {
        const { filter, delivered } = makeFilter();
        filter.push({ isMaximized: true });
        filter.resolve('main');
        filter.push({ isMaximized: false });
        expect(delivered).toEqual([{ isMaximized: true }, { isMaximized: false }]);
    });

    it('holds events until the id is known, then delivers the latest one of the own window', () => {
        const { filter, delivered } = makeFilter();
        filter.push({ windowId: 'main', isMaximized: true });
        filter.push({ windowId: 'main', isMaximized: false });
        filter.push({ windowId: 'popup-1', isMaximized: true });
        expect(delivered).toEqual([]);
        filter.resolve('main');
        expect(delivered).toEqual([{ windowId: 'main', isMaximized: false }]);
        filter.push({ windowId: 'popup-1', isMaximized: false });
        expect(delivered).toHaveLength(1);
    });

    it('drops held events of other windows when the own window has none', () => {
        const { filter, delivered } = makeFilter();
        filter.push({ windowId: 'main', isMaximized: true });
        filter.resolve('popup-1');
        expect(delivered).toEqual([]);
    });

    it('delivers the latest held event and everything after it when the id query fails', () => {
        const { filter, delivered } = makeFilter();
        filter.push({ windowId: 'popup-1', isMaximized: true });
        filter.push({ windowId: 'main', isMaximized: false });
        filter.fail();
        expect(delivered).toEqual([{ windowId: 'main', isMaximized: false }]);
        filter.push({ windowId: 'popup-2', isMaximized: true });
        expect(delivered).toHaveLength(2);
    });

    it('settles once: a later resolve or fail changes nothing', () => {
        const { filter, delivered } = makeFilter();
        filter.resolve('main');
        filter.fail();
        filter.resolve('popup-1');
        filter.push({ windowId: 'popup-1', isMaximized: true });
        filter.push({ windowId: 'main', isMaximized: true });
        expect(delivered).toEqual([{ windowId: 'main', isMaximized: true }]);
    });
});

describe('learnOwnWindowId', () => {
    const getCurrentWindowId = vi.fn();

    beforeAll(() => {
        // on and invoke make window.fb pass for the SDK; without them learnOwnWindowId fails early.
        vi.stubGlobal('window', {
            fb: { ui: { getCurrentWindowId }, on: () => () => undefined, invoke: vi.fn() },
        });
    });

    async function settle(answer: () => Promise<unknown>) {
        getCurrentWindowId.mockImplementation(answer);
        const { filter, delivered } = makeFilter();
        filter.push({ windowId: 'main', isMaximized: true });
        filter.push({ windowId: 'popup-1', isMaximized: true });
        learnOwnWindowId(filter);
        await new Promise((r) => setTimeout(r, 0));
        return { filter, delivered };
    }

    it('resolves with the id the host reports', async () => {
        const { delivered } = await settle(() => Promise.resolve({ windowId: 'popup-1' }));
        expect(delivered).toEqual([{ windowId: 'popup-1', isMaximized: true }]);
    });

    it('fails when the call is rejected', async () => {
        const { delivered } = await settle(() => Promise.reject(new Error('NOT_FOUND')));
        expect(delivered).toEqual([{ windowId: 'popup-1', isMaximized: true }]);
    });

    it('fails when the answer has no usable id', async () => {
        const { filter, delivered } = await settle(() => Promise.resolve({ windowId: '' }));
        filter.push({ windowId: 'main', isMaximized: false });
        expect(delivered).toHaveLength(2);
    });
});

// The two components cannot be mounted without a DOM; pin that both route
// the event through the filter and ask for the own id.
const sources = import.meta.glob(['../FbTitlebar.ts', '../FbWindowControls.ts'], {
    query: '?raw',
    import: 'default',
    eager: true,
}) as Record<string, string>;

it('finds both component sources', () => {
    expect(Object.keys(sources)).toHaveLength(2);
});

describe.each(Object.entries(sources))('%s · follows its own window', (_path, source) => {
    it('subscribes through OwnWindowStateFilter and learns the own id', () => {
        expect(source).toMatch(/new OwnWindowStateFilter<WindowStateChangedPayload>\(/);
        expect(source).toMatch(/_sub\('window:stateChanged', \(data\) => filter\.push\(data\)\)/);
        expect(source).toMatch(/learnOwnWindowId\(filter\)/);
    });
});

// sdk/src/components/__tests__/runtime.test.ts
//
// Which SDK instance the components talk to. Inside foobar2000 the host puts a
// smaller object of its own on `window.fb` (events, artwork and shell only), so
// that name alone does not mean the SDK is there: a bundled theme binds the SDK
// through the ESM `registerComponents()`, a `<script>` theme loads
// `bridge.global.js`, which replaces `window.fb` with the full SDK.

import { beforeEach, describe, expect, it, vi } from 'vitest';

import { FakeElement } from './playlistViewHarness.js';

/** The object the host injects as `window.fb`, before any SDK bundle loads. */
function hostInjectedFb() {
    return {
        artwork: {},
        shell: {},
        on: () => () => undefined,
        off: () => undefined,
        once: () => () => undefined,
        emit: () => undefined,
        isAvailable: () => true,
    };
}

/** A stand-in for the SDK aggregate: the components only need its shape here. */
function sdkLike() {
    return { on: () => () => undefined, invoke: vi.fn(), player: {} };
}

describe('components · SDK lookup', () => {
    beforeEach(() => {
        vi.resetModules();
        vi.unstubAllGlobals();
    });

    it('uses window.fb once it holds the SDK', async () => {
        const fb = sdkLike();
        vi.stubGlobal('window', { fb });
        const { getFb } = await import('../runtime.js');
        expect(getFb()).toBe(fb);
    });

    it('refuses the object the host injects, naming both ways to load the SDK', async () => {
        vi.stubGlobal('window', { fb: hostInjectedFb() });
        const { getFb } = await import('../runtime.js');
        expect(() => getFb()).toThrow(/bridge\.global\.js.*registerComponents\(\)/s);
    });

    it('does not keep the host object, so the SDK loaded after it is found', async () => {
        const win: { fb: unknown } = { fb: hostInjectedFb() };
        vi.stubGlobal('window', win);
        const { getFb } = await import('../runtime.js');
        expect(() => getFb()).toThrow();
        const fb = sdkLike();
        win.fb = fb;
        expect(getFb()).toBe(fb);
    });

    it('prefers a bound instance to window.fb', async () => {
        vi.stubGlobal('window', { fb: sdkLike() });
        const { bindFb, getFb } = await import('../runtime.js');
        const bound = sdkLike();
        bindFb(bound as never);
        expect(getFb()).toBe(bound);
    });

    it('binds the SDK of foo-webview-sdk when the ESM entry registers the elements', async () => {
        vi.stubGlobal('window', { fb: hostInjectedFb() });
        vi.stubGlobal('HTMLElement', FakeElement);
        const { registerComponents } = await import('../index.js');
        const { getFb } = await import('../runtime.js');
        const { fb } = await import('../../bridge/index.js');
        registerComponents({});
        expect(getFb()).toBe(fb);
    });
});

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

function makeNative() {
    return {
        invoke: vi.fn().mockResolvedValue({ success: true }),
        on: vi.fn(),
        off: vi.fn(),
        _handleResponse: () => {
            /* dummy */
        },
    };
}

describe('consoleApi args', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it.each(['log', 'warn', 'error'] as const)(
        '%s sends a lone message as `message`',
        async (level) => {
            const native = makeNative();
            vi.stubGlobal('window', { fb2k: native });
            const { consoleApi } = await import('./consoleApi.js');

            await consoleApi[level]('hello');

            expect(native.invoke).toHaveBeenCalledWith(`console.${level}`, { message: 'hello' });
        },
    );

    // The host reads `args` only when `message` is absent, so extra values
    // must travel with the message inside `args`, not beside it.
    it.each(['log', 'warn', 'error'] as const)(
        '%s sends the message and extra values together as `args`',
        async (level) => {
            const native = makeNative();
            vi.stubGlobal('window', { fb2k: native });
            const { consoleApi } = await import('./consoleApi.js');

            await consoleApi[level]('count', 3, { a: [1] }, null);

            expect(native.invoke).toHaveBeenCalledWith(`console.${level}`, {
                args: ['count', 3, { a: [1] }, null],
            });
            expect(native.invoke.mock.calls[0][1]).not.toHaveProperty('message');
        },
    );
});

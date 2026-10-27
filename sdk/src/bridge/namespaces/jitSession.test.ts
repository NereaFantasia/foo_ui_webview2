import { afterEach, expect, it, vi } from 'vitest';

afterEach(() => { vi.unstubAllGlobals(); vi.resetModules(); });

it('preserves credentials and optional fields when forwarding session commands', async () => {
    const native = { invoke: vi.fn().mockResolvedValue({ success: true }), on: vi.fn(), off: vi.fn() };
    vi.stubGlobal('window', { fb2k: native });
    const { jitSession } = await import('./jitSession.js');
    const guard = { sessionId: 's', controlToken: 'secret', expectedRevision: 4, requestId: 'r' };
    await jitSession.setNext({ ...guard, afterAttemptId: 'a', endOfQueue: true });
    expect(native.invoke).toHaveBeenCalledWith('jitSession.setNext', {
        ...guard, afterAttemptId: 'a', endOfQueue: true,
    });
    await jitSession.claimControl({ sessionId: 's', mode: 'handover', expectedOwnerEpoch: 7, requestId: 'claim' });
    expect(native.invoke).toHaveBeenLastCalledWith('jitSession.claimControl', {
        sessionId: 's', mode: 'handover', expectedOwnerEpoch: 7, requestId: 'claim',
    });
});

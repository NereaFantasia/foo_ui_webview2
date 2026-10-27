import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import type { JitSessionSnapshot, JitSessionOperation } from '../../types/generated/schema-types.js';

const transport = vi.hoisted(() => ({
    call: vi.fn(),
    listeners: new Map<string, (payload: unknown) => void>(),
    order: [] as string[],
}));

vi.mock('../call.js', () => ({ call: transport.call }));
vi.mock('../subscribe.js', () => ({
    subscribe: (name: string, handler: (payload: unknown) => void) => {
        transport.order.push(name);
        transport.listeners.set(name, handler);
        return () => transport.listeners.delete(name);
    },
}));

function snapshot(revision = 1, lastEventSeq = 0): JitSessionSnapshot {
    return {
        sessionId: 's', revision, lastEventSeq, ownerEpoch: 1, ownerState: 'connected',
        phase: 'active', phaseReason: null, current: null, next: null, supplyState: 'needed',
        neededAfterAttemptId: null, neededRequestKey: null, pendingOperations: [],
    };
}

const operation: JitSessionOperation = {
    operationId: 'op', kind: 'play', entryId: 'entry', attemptId: 'attempt',
    status: 'completed', reason: null,
};

function emit(name: string, payload: unknown) { transport.listeners.get(name)?.(payload); }

beforeEach(() => {
    transport.listeners.clear();
    transport.order.length = 0;
    transport.call.mockReset().mockImplementation(async (method: string) => {
        transport.order.push(method);
        if (method === 'jitSession.create') return { success: true, sessionId: 's', controlToken: 'token', ownerEpoch: 1, revision: 1 };
        if (method === 'jitSession.getState') return { success: true, state: snapshot() };
        if (method === 'jitSession.getOperation') return { success: true, operation };
        return { success: true, operationId: 'op', attemptId: 'attempt', revision: 2 };
    });
});

afterEach(() => { vi.useRealTimers(); });

describe('JitSessionClient', () => {
    it('subscribes before creating and fills credentials without exposing the token in state', async () => {
        const { JitSessionClient } = await import('./JitSessionClient.js');
        const client = new JitSessionClient();
        await client.create();
        expect(transport.order.slice(0, 5).every((entry) => entry.startsWith('jitSession:'))).toBe(true);
        await client.play({ entryId: 'entry', location: 'C:/track.flac', requestId: 'retry-key' });
        expect(transport.call).toHaveBeenCalledWith('jitSession.play', {
            sessionId: 's', controlToken: 'token', expectedRevision: 1, requestId: 'retry-key',
            entryId: 'entry', location: 'C:/track.flac',
        });
        expect(client.state).not.toHaveProperty('controlToken');
        client.dispose();
    });

    it('retains a terminal event that arrives before the play response', async () => {
        const { JitSessionClient } = await import('./JitSessionClient.js');
        const client = new JitSessionClient();
        await client.create();
        transport.call.mockImplementation(async (method: string) => {
            if (method === 'jitSession.play') {
                emit('jitSession:operationCompleted', { sessionId: 's', revision: 3, eventSeq: 1, operation });
                return { success: true, operationId: 'op', attemptId: 'attempt', revision: 2 };
            }
            return { success: true, state: snapshot(3, 1) };
        });
        const accepted = await client.play({ entryId: 'entry', location: 'C:/track.flac' });
        expect(await client.waitForOperation(accepted.operationId)).toEqual(operation);
        expect(client.revision).toBe(3);
        client.dispose();
    });

    it('does not discard an unseen operation terminal after a newer state snapshot', async () => {
        const { JitSessionClient } = await import('./JitSessionClient.js');
        const client = new JitSessionClient();
        await client.create();
        emit('jitSession:stateChanged', { sessionId: 's', revision: 8, eventSeq: 2, state: snapshot(8, 2) });
        emit('jitSession:operationCompleted', { sessionId: 's', revision: 3, eventSeq: 1, operation });
        expect(await client.waitForOperation('op')).toEqual(operation);
        expect(client.revision).toBe(8);
        client.dispose();
    });

    it('deduplicates events without sharing sequence numbers between sessions', async () => {
        const { JitSessionClient } = await import('./JitSessionClient.js');
        const client = new JitSessionClient();
        await client.create();
        const listener = vi.fn();
        client.subscribe(listener);
        const event = { sessionId: 's', revision: 2, eventSeq: 1, operation };
        emit('jitSession:operationCompleted', event);
        emit('jitSession:operationCompleted', event);
        emit('jitSession:operationCompleted', { ...event, sessionId: 'unrelated', eventSeq: 2 });
        expect(listener).toHaveBeenCalledTimes(1);
        client.dispose();
    });

    it('serializes commands so the next write uses the preceding response revision', async () => {
        const { JitSessionClient } = await import('./JitSessionClient.js');
        const client = new JitSessionClient();
        await client.create();
        await Promise.all([
            client.play({ entryId: 'entry', location: 'C:/track.flac' }),
            client.stop(),
        ]);
        expect(transport.call.mock.calls.find(([name]) => name === 'jitSession.stop')?.[1]).toMatchObject({ expectedRevision: 2 });
        client.dispose();
    });

    it('revokes local writes on handover and does not replay a rejected command', async () => {
        const { JitSessionClient } = await import('./JitSessionClient.js');
        const client = new JitSessionClient();
        await client.create();
        emit('jitSession:controlChanged', { sessionId: 's', revision: 2, eventSeq: 1, ownerEpoch: 2, reason: 'claimed' });
        await expect(client.stop()).rejects.toThrow();
        expect(transport.call.mock.calls.filter(([name]) => name === 'jitSession.stop')).toHaveLength(0);
        client.dispose();
    });

    it('resumes by claiming and reading state without restarting playback', async () => {
        const { JitSessionClient } = await import('./JitSessionClient.js');
        const client = new JitSessionClient();
        transport.call.mockImplementation(async (method: string) => {
            if (method === 'jitSession.claimControl') return { success: true, controlToken: 'resumed', ownerEpoch: 2, revision: 6 };
            return { success: true, state: { ...snapshot(6), ownerEpoch: 2 } };
        });
        await client.claimControl({ sessionId: 's', mode: 'resume' });
        expect(client.revision).toBe(6);
        expect(transport.call.mock.calls.some(([name]) => name === 'jitSession.play')).toBe(false);
        client.dispose();
    });

    it('queries a missed terminal and removes all subscriptions when disposed', async () => {
        const { JitSessionClient } = await import('./JitSessionClient.js');
        const client = new JitSessionClient();
        await client.create();
        expect(await client.waitForOperation('op')).toEqual(operation);
        expect(transport.call).toHaveBeenCalledWith('jitSession.getOperation', { sessionId: 's', operationId: 'op' });
        client.dispose();
        expect(transport.listeners.size).toBe(0);
        await expect(client.stop()).rejects.toThrow();
    });

    it('rejects a pending wait on disposal and clears its timer', async () => {
        vi.useFakeTimers();
        const { JitSessionClient } = await import('./JitSessionClient.js');
        const client = new JitSessionClient();
        await client.create();
        transport.call.mockResolvedValue({ success: true, operation: { ...operation, status: 'pending' } });
        const waiting = client.waitForOperation('op');
        const assertion = expect(waiting).rejects.toThrow('disposed');
        client.dispose();
        await assertion;
        expect(vi.getTimerCount()).toBe(0);
    });

    it('keeps events delivered before create returns its session id', async () => {
        const { JitSessionClient } = await import('./JitSessionClient.js');
        const client = new JitSessionClient();
        transport.call.mockImplementation(async (method: string) => {
            if (method === 'jitSession.create') {
                emit('jitSession:stateChanged', { sessionId: 's', revision: 3, eventSeq: 1, state: snapshot(3, 1) });
                return { success: true, sessionId: 's', controlToken: 'token', ownerEpoch: 1, revision: 1 };
            }
            return { success: true, state: snapshot(2) };
        });
        expect((await client.create()).revision).toBe(3);
        expect(client.revision).toBe(3);
        client.dispose();
    });

    it('waits for a terminal after a pending query and clears the timeout', async () => {
        vi.useFakeTimers();
        const { JitSessionClient } = await import('./JitSessionClient.js');
        const client = new JitSessionClient();
        await client.create();
        transport.call.mockResolvedValue({ success: true, operation: { ...operation, status: 'pending' } });
        const waiting = client.waitForOperation('op');
        emit('jitSession:operationCompleted', { sessionId: 's', revision: 2, eventSeq: 1, operation });
        expect(await waiting).toEqual(operation);
        expect(vi.getTimerCount()).toBe(0);
        client.dispose();
    });

    it('aborts a local wait without issuing a host stop', async () => {
        vi.useFakeTimers();
        const { JitSessionClient } = await import('./JitSessionClient.js');
        const client = new JitSessionClient();
        await client.create();
        transport.call.mockResolvedValue({ success: true, operation: { ...operation, status: 'pending' } });
        const abort = new AbortController();
        const waiting = client.waitForOperation('op', { signal: abort.signal });
        const assertion = expect(waiting).rejects.toMatchObject({ name: 'AbortError' });
        abort.abort();
        await assertion;
        expect(vi.getTimerCount()).toBe(0);
        expect(transport.call.mock.calls.some(([name]) => name === 'jitSession.stop')).toBe(false);
        client.dispose();
    });

    it('times out without turning a pending operation into a failed host record', async () => {
        vi.useFakeTimers();
        const { JitSessionClient } = await import('./JitSessionClient.js');
        const client = new JitSessionClient();
        await client.create();
        transport.call.mockResolvedValue({ success: true, operation: { ...operation, status: 'pending' } });
        const waiting = client.waitForOperation('op', { timeoutMs: 20 });
        const assertion = expect(waiting).rejects.toThrow('Timed out');
        await vi.advanceTimersByTimeAsync(20);
        await assertion;
        expect(vi.getTimerCount()).toBe(0);
        emit('jitSession:operationCompleted', { sessionId: 's', revision: 3, eventSeq: 1, operation });
        expect(await client.waitForOperation('op')).toEqual(operation);
        client.dispose();
    });

    it('sends the original release shape unless a native disposition is chosen', async () => {
        const { JitSessionClient } = await import('./JitSessionClient.js');
        const client = new JitSessionClient();
        await client.create();
        await client.release('plain');
        expect(transport.call).toHaveBeenLastCalledWith('jitSession.release', {
            sessionId: 's', controlToken: 'token', expectedRevision: 1, requestId: 'plain',
        });
        await client.create();
        await client.release('detach-key', 'detach');
        expect(transport.call).toHaveBeenLastCalledWith('jitSession.release', {
            sessionId: 's', controlToken: 'token', expectedRevision: 1, requestId: 'detach-key', nativeDisposition: 'detach',
        });
        client.dispose();
    });

    it('keeps the credentials and refreshes the state when releasing fails', async () => {
        const { JitSessionClient } = await import('./JitSessionClient.js');
        const client = new JitSessionClient();
        await client.create();
        transport.call.mockImplementation(async (method: string) => {
            transport.order.push(method);
            if (method === 'jitSession.release') return { success: false, error: 'partly given back', code: 'OPERATION_FAILED' };
            if (method === 'jitSession.getState') return { success: true, state: snapshot(3) };
            return { success: true, operationId: 'op', attemptId: 'attempt', revision: 3 };
        });
        await expect(client.release('first')).rejects.toMatchObject({ code: 'OPERATION_FAILED' });
        expect(transport.order.slice(-2)).toEqual(['jitSession.release', 'jitSession.getState']);
        await client.stop('still-owner');
        expect(transport.call).toHaveBeenLastCalledWith('jitSession.stop', expect.objectContaining({ controlToken: 'token' }));
        client.dispose();
    });

    it('does not automatically replay a command rejected with CONFLICT', async () => {
        const { JitSessionClient } = await import('./JitSessionClient.js');
        const client = new JitSessionClient();
        await client.create();
        transport.call.mockResolvedValue({ success: false, code: 'CONFLICT', error: 'Revision changed' });
        await expect(client.stop()).rejects.toMatchObject({ code: 'CONFLICT' });
        expect(transport.call.mock.calls.filter(([name]) => name === 'jitSession.stop')).toHaveLength(1);
        client.dispose();
    });

    it('preserves state and write credentials when skip has no ready next entry', async () => {
        const { JitSessionClient } = await import('./JitSessionClient.js');
        const client = new JitSessionClient();
        await client.create();
        const playing = {
            ...snapshot(4, 1),
            current: { entryId: 'entry', attemptId: 'attempt', title: 'Current', status: 'playing' as const },
        };
        emit('jitSession:stateChanged', { sessionId: 's', revision: 4, eventSeq: 1, state: playing });
        transport.call.mockResolvedValueOnce({ success: false, code: 'NO_ACTIVE_ITEM', error: 'No ready next entry' });

        await expect(client.skip('attempt', 'skip-1')).rejects.toMatchObject({ code: 'NO_ACTIVE_ITEM' });
        expect(client.revision).toBe(4);
        expect(client.state).toEqual(playing);
        expect(transport.call.mock.calls.filter(([name]) => name === 'jitSession.skip')).toHaveLength(1);
        expect(transport.call.mock.calls.some(([name]) => name === 'jitSession.stop')).toBe(false);

        transport.call.mockResolvedValueOnce({ success: true, operationId: 'next-op', revision: 5 });
        await client.setNext({ afterAttemptId: 'attempt', entryId: 'next', location: 'C:/next.flac' });
        expect(transport.call).toHaveBeenLastCalledWith('jitSession.setNext', expect.objectContaining({
            controlToken: 'token', expectedRevision: 4,
        }));
        client.dispose();
    });

    it('delivers a recheck result in sequence without treating it as a gap', async () => {
        const { JitSessionClient } = await import('./JitSessionClient.js');
        const client = new JitSessionClient();
        await client.create();
        const seen: string[] = [];
        client.subscribe((event) => seen.push(event.name));
        const calls = transport.call.mock.calls.length;
        emit('jitSession:attemptRechecked', {
            sessionId: 's', revision: 1, eventSeq: 1, attemptId: 'attempt', entryId: 'entry',
            result: 'reproduced', reason: 'accessDenied', stage: 'open', engineReported: true,
        });
        emit('jitSession:stateChanged', { sessionId: 's', revision: 1, eventSeq: 2, state: snapshot(1, 2) });
        expect(seen).toEqual(['attemptRechecked', 'stateChanged']);
        expect(transport.call.mock.calls.length).toBe(calls);
        client.dispose();
    });

    it('reconciles an existing wait when a sequence gap hides its terminal', async () => {
        vi.useFakeTimers();
        const { JitSessionClient } = await import('./JitSessionClient.js');
        const client = new JitSessionClient();
        await client.create();
        let completed = false;
        transport.call.mockImplementation(async (method: string) => method === 'jitSession.getOperation'
            ? { success: true, operation: { ...operation, status: completed ? 'completed' : 'pending' } }
            : { success: true, state: snapshot(3, 3) });
        const settled = vi.fn();
        const waiting = client.waitForOperation('op').then(settled, settled);
        try {
            await vi.waitFor(() => expect(transport.call.mock.calls.filter(([method]) => method === 'jitSession.getOperation')).toHaveLength(1));
            completed = true;
            emit('jitSession:stateChanged', { sessionId: 's', revision: 3, eventSeq: 3, state: snapshot(3, 3) });
            await vi.waitFor(() => expect(settled).toHaveBeenCalledWith(operation));
            expect(vi.getTimerCount()).toBe(0);
        } finally { client.dispose(); await waiting; }
    });

    it('explicit refresh rechecks each operation once and resolves every waiter with its real terminal', async () => {
        const { JitSessionClient } = await import('./JitSessionClient.js');
        const client = new JitSessionClient();
        await client.create();
        let completed = false;
        const failed = { ...operation, status: 'failed', reason: 'resolutionFailed' };
        transport.call.mockImplementation(async (method: string) => method === 'jitSession.getOperation'
            ? { success: true, operation: completed ? failed : { ...operation, status: 'pending' } }
            : { success: true, state: snapshot(3, 3) });
        const settled = vi.fn();
        const waiting = [client.waitForOperation('op'), client.waitForOperation('op')].map((promise) => promise.then(settled, settled));
        try {
            await vi.waitFor(() => expect(transport.call.mock.calls.filter(([method]) => method === 'jitSession.getOperation')).toHaveLength(2));
            const before = transport.call.mock.calls.length;
            completed = true;
            await client.refresh();
            expect(transport.call.mock.calls.slice(before).filter(([method]) => method === 'jitSession.getOperation')).toHaveLength(1);
            expect(settled).toHaveBeenCalledTimes(2);
            expect(settled).toHaveBeenCalledWith(failed);
        } finally { client.dispose(); await Promise.all(waiting); }
    });

    it('does not infer completion from a snapshot that omits a still-pending operation', async () => {
        const { JitSessionClient } = await import('./JitSessionClient.js');
        const client = new JitSessionClient();
        await client.create();
        transport.call.mockImplementation(async (method: string) => method === 'jitSession.getOperation'
            ? { success: true, operation: { ...operation, status: 'pending' } }
            : { success: true, state: snapshot(3, 3) });
        const settled = vi.fn();
        const waiting = client.waitForOperation('op').then(settled, settled);
        try {
            await client.refresh();
            expect(transport.call.mock.calls.filter(([method]) => method === 'jitSession.getOperation')).toHaveLength(2);
            expect(settled).not.toHaveBeenCalled();
            emit('jitSession:operationCompleted', { sessionId: 's', revision: 4, eventSeq: 4, operation });
            await waiting;
            expect(settled).toHaveBeenCalledWith(operation);
        } finally { client.dispose(); await waiting; }
    });

    it('reconciles another gap that arrives while an operation query is in progress', async () => {
        const { JitSessionClient } = await import('./JitSessionClient.js');
        const client = new JitSessionClient();
        await client.create();
        let queries = 0;
        let release: ((value: unknown) => void) | undefined;
        transport.call.mockImplementation(async (method: string) => {
            if (method !== 'jitSession.getOperation') return { success: true, state: snapshot(5, 5) };
            if (++queries === 1) return { success: true, operation: { ...operation, status: 'pending' } };
            if (queries === 2) return new Promise((resolve) => { release = resolve; });
            return { success: true, operation };
        });
        const settled = vi.fn();
        const waiting = client.waitForOperation('op').then(settled, settled);
        try {
            emit('jitSession:stateChanged', { sessionId: 's', revision: 3, eventSeq: 3, state: snapshot(3, 3) });
            await vi.waitFor(() => expect(release).toBeDefined());
            emit('jitSession:stateChanged', { sessionId: 's', revision: 7, eventSeq: 7, state: snapshot(7, 7) });
            release?.({ success: true, operation: { ...operation, status: 'pending' } });
            await vi.waitFor(() => expect(settled).toHaveBeenCalledWith(operation));
            expect(queries).toBe(3);
        } finally { client.dispose(); await waiting; }
    });

    it.each(['abort', 'dispose'] as const)('does not revive a wait after %s while reconciliation is pending', async (action) => {
        const { JitSessionClient } = await import('./JitSessionClient.js');
        const client = new JitSessionClient();
        await client.create();
        let queries = 0;
        let release: ((value: unknown) => void) | undefined;
        transport.call.mockImplementation(async (method: string) => {
            if (method !== 'jitSession.getOperation') return { success: true, state: snapshot(3, 3) };
            if (++queries === 1) return { success: true, operation: { ...operation, status: 'pending' } };
            return new Promise((resolve) => { release = resolve; });
        });
        const abort = new AbortController();
        const settled = vi.fn();
        const waiting = client.waitForOperation('op', { signal: abort.signal }).then(settled, settled);
        const refresh = client.refresh().then(() => undefined, (error: unknown) => error);
        try {
            await vi.waitFor(() => expect(release).toBeDefined());
            if (action === 'abort') abort.abort(); else client.dispose();
            await waiting;
            release?.({ success: true, operation });
            await refresh;
            expect(settled).toHaveBeenCalledTimes(1);
            expect(settled).toHaveBeenCalledWith(expect.objectContaining(action === 'abort'
                ? { name: 'AbortError' } : { message: 'JIT session client is disposed' }));
        } finally { client.dispose(); await waiting; }
    });

    it('keeps a wait alive after a failed reconciliation query and retries it on refresh', async () => {
        const { JitSessionClient } = await import('./JitSessionClient.js');
        const client = new JitSessionClient();
        await client.create();
        let queries = 0;
        transport.call.mockImplementation(async (method: string) => {
            if (method !== 'jitSession.getOperation') return { success: true, state: snapshot(3, 3) };
            if (++queries === 1) return { success: true, operation: { ...operation, status: 'pending' } };
            if (queries === 2) throw new Error('Temporarily disconnected');
            return { success: true, operation };
        });
        const settled = vi.fn();
        const waiting = client.waitForOperation('op').then(settled, settled);
        try {
            await expect(client.refresh()).rejects.toThrow('Temporarily disconnected');
            expect(settled).not.toHaveBeenCalled();
            await client.refresh();
            await waiting;
            expect(settled).toHaveBeenCalledWith(operation);
        } finally { client.dispose(); await waiting; }
    });

    it('does not apply a late reconciliation result or error to a newly selected session', async () => {
        const { JitSessionClient } = await import('./JitSessionClient.js');
        const client = new JitSessionClient();
        await client.create();
        let queries = 0;
        let release: ((value: unknown) => void) | undefined;
        transport.call.mockImplementation(async (method: string, params: { sessionId?: string } = {}) => {
            if (method === 'jitSession.create') return { success: true, sessionId: 'second', controlToken: 'second-token', ownerEpoch: 1, revision: 1 };
            if (method === 'jitSession.getState') return { success: true, state: { ...snapshot(3, 3), sessionId: params.sessionId } };
            if (params.sessionId === 'second') return { success: true, operation: { ...operation, entryId: 'second-entry' } };
            if (++queries === 1) return { success: true, operation: { ...operation, status: 'pending' } };
            return new Promise((resolve) => { release = resolve; });
        });
        const settled = vi.fn();
        const waiting = client.waitForOperation('op').then(settled, settled);
        try {
            emit('jitSession:stateChanged', { sessionId: 's', revision: 3, eventSeq: 3, state: snapshot(3, 3) });
            await vi.waitFor(() => expect(release).toBeDefined());
            await client.create();
            release?.({ success: true, operation });
            await new Promise((resolve) => setTimeout(resolve, 0));
            expect(settled).toHaveBeenCalledWith(expect.objectContaining({ message: 'JIT session changed' }));
            expect(client.state?.sessionId).toBe('second');
            expect(client.synchronizationError).toBeUndefined();
            expect(await client.waitForOperation('op')).toMatchObject({ entryId: 'second-entry' });
        } finally { client.dispose(); await waiting; }
    });

    it('retains reconciliation errors and clears them after an explicit successful refresh', async () => {
        const { JitSessionClient } = await import('./JitSessionClient.js');
        const client = new JitSessionClient();
        await client.create();
        transport.call.mockRejectedValueOnce(new Error('Disconnected'));
        emit('jitSession:operationCompleted', { sessionId: 's', revision: 3, eventSeq: 3, operation });
        await vi.waitFor(() => expect(client.synchronizationError).toBeInstanceOf(Error));
        transport.call.mockResolvedValueOnce({ success: true, state: snapshot(4, 3) });
        await client.refresh();
        expect(client.synchronizationError).toBeUndefined();
        expect(client.revision).toBe(4);
        client.dispose();
    });
});

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import type { PlaybackClockChange, PlaybackClockOptions } from '../PlaybackClock.js';

type Handler = (data: unknown) => void;

// Stand-in for the injected bridge (window.fb2k) and two clocks the test moves by hand.
function makeHost() {
    const handlers = new Map<string, Set<Handler>>();
    const answers: Record<string, unknown> = {
        'playback.getState': { success: true, state: 'playing', canSeek: true, canPause: true },
        'playback.getPosition': { success: true, position: 0, duration: 0, subsong: 0, path: '', hostTime: 0 },
    };
    const native = {
        invoke: vi.fn(async (method: string) => answers[method] ?? { success: false, error: 'unexpected', code: 'X' }),
        on: vi.fn((event: string, handler: Handler) => {
            const set = handlers.get(event) ?? new Set<Handler>();
            set.add(handler);
            handlers.set(event, set);
        }),
        off: vi.fn((event: string, handler: Handler) => {
            handlers.get(event)?.delete(handler);
        }),
    };
    const clock = { wall: 1_000_000, mono: 5_000 };
    const advance = (ms: number): void => {
        clock.wall += ms;
        clock.mono += ms;
    };
    const emit = (event: string, payload: unknown): void => {
        for (const handler of [...(handlers.get(event) ?? [])]) handler(payload);
    };
    const listenerCount = (): number => [...handlers.values()].reduce((sum, set) => sum + set.size, 0);
    const options: PlaybackClockOptions = { now: () => clock.wall, monotonicNow: () => clock.mono };
    return { native, answers, clock, advance, emit, listenerCount, options };
}

type Host = ReturnType<typeof makeHost>;

async function start(host: Host, overrides: PlaybackClockOptions = {}) {
    vi.stubGlobal('window', { fb2k: host.native });
    const { PlaybackClock } = await import('../PlaybackClock.js');
    const clock = new PlaybackClock({ ...host.options, ...overrides });
    const changes: PlaybackClockChange[] = [];
    clock.onChange((change) => changes.push(change));
    await clock.ready;
    return { clock, changes };
}

// A host position read `ageMs` ago on the wall clock.
const tick = (host: Host, position: number, ageMs = 0) =>
    host.emit('playback:timeHighRes', { position, hostTime: host.clock.wall - ageMs });

function activeReadDisposals(clock: object): number {
    const callbacks: unknown = Reflect.get(clock, 'readDisposers');
    expect(callbacks).toBeInstanceOf(Set);
    return callbacks instanceof Set ? callbacks.size : -1;
}

describe('PlaybackClock', () => {
    beforeEach(() => {
        vi.resetModules();
    });
    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('starts from the first read, counting the time since the host read the position', async () => {
        const host = makeHost();
        host.answers['playback.getPosition'] = {
            success: true, position: 10, duration: 100, subsong: 0, path: '', hostTime: host.clock.wall - 20,
        };
        const { clock, changes } = await start(host);
        expect(clock.state).toBe('playing');
        expect(clock.duration).toBe(100);
        expect(clock.position()).toBeCloseTo(10.02, 6);
        host.advance(500);
        expect(clock.position()).toBeCloseTo(10.52, 6);
        expect(changes.map((change) => change.reason)).toEqual(['start']);
    });

    it('nudges the estimate on a small disagreement and replaces it on a large one', async () => {
        const host = makeHost();
        host.answers['playback.getPosition'] = {
            success: true, position: 10, duration: 100, subsong: 0, path: '', hostTime: host.clock.wall,
        };
        const { clock, changes } = await start(host);
        host.advance(100);
        tick(host, 10.14);
        // Predicted 10.1, error 0.04, a quarter of it applied.
        expect(clock.position()).toBeCloseTo(10.11, 6);
        host.advance(100);
        tick(host, 20);
        expect(clock.position()).toBeCloseTo(20, 6);
        expect(changes.map((change) => change.reason)).toEqual(['start', 'resync']);
    });

    it('ignores updates that are too old, from across a clock change, or older than one already applied', async () => {
        const host = makeHost();
        host.answers['playback.getPosition'] = {
            success: true, position: 10, duration: 100, subsong: 0, path: '', hostTime: host.clock.wall,
        };
        const { clock } = await start(host);
        tick(host, 50, 1500);
        tick(host, 50, -10);
        expect(clock.position()).toBeCloseTo(10, 6);
        host.advance(100);
        tick(host, 10.1);
        host.emit('playback:timeHighRes', { position: 60, hostTime: host.clock.wall - 50 });
        expect(clock.position()).toBeCloseTo(10.1, 6);
    });

    it('jumps to the seek target and ignores a stale position read right after the seek', async () => {
        const host = makeHost();
        host.answers['playback.getPosition'] = {
            success: true, position: 10, duration: 100, subsong: 0, path: '', hostTime: host.clock.wall,
        };
        const { clock, changes } = await start(host);
        host.emit('playback:seeked', { position: 60, hostTime: host.clock.wall });
        tick(host, 10.001);
        expect(clock.position()).toBeCloseTo(60, 6);
        host.advance(300);
        tick(host, 70);
        expect(clock.position()).toBeCloseTo(70, 6);
        expect(changes.map((change) => change.reason)).toEqual(['start', 'seek', 'resync']);
    });

    it('holds still while paused and reads zero once stopped', async () => {
        const host = makeHost();
        host.answers['playback.getPosition'] = {
            success: true, position: 10, duration: 100, subsong: 0, path: '', hostTime: host.clock.wall,
        };
        const { clock, changes } = await start(host);
        host.emit('playback:stateChanged', {
            state: 'paused', position: 12, duration: 100, canSeek: true, hostTime: host.clock.wall,
        });
        host.advance(1000);
        expect(clock.state).toBe('paused');
        expect(clock.position()).toBeCloseTo(12, 6);
        host.emit('playback:stateChanged', {
            state: 'stopped', position: 12, duration: 100, canSeek: false, hostTime: host.clock.wall,
        });
        expect(clock.position()).toBe(0);
        expect(clock.duration).toBe(0);
        expect(changes.map((change) => change.reason)).toEqual(['start', 'state', 'state']);
    });

    it('starts a new track at zero and takes its first update without a resync', async () => {
        const host = makeHost();
        host.answers['playback.getPosition'] = {
            success: true, position: 90, duration: 100, subsong: 0, path: '', hostTime: host.clock.wall,
        };
        const { clock, changes } = await start(host);
        host.emit('playback:trackChanged', { duration: 200 });
        expect(clock.position()).toBe(0);
        expect(clock.duration).toBe(200);
        host.advance(100);
        tick(host, 1.5);
        expect(clock.position()).toBeCloseTo(1.5, 6);
        expect(changes.map((change) => change.reason)).toEqual(['start', 'track']);
    });

    it('stays within the track duration', async () => {
        const host = makeHost();
        host.answers['playback.getPosition'] = {
            success: true, position: 99.9, duration: 100, subsong: 0, path: '', hostTime: host.clock.wall,
        };
        const { clock } = await start(host);
        host.advance(5000);
        expect(clock.position()).toBe(100);
    });

    it('counts updates from a host without hostTime as read on arrival', async () => {
        const host = makeHost();
        host.answers['playback.getPosition'] = { success: true, position: 10, duration: 100, subsong: 0, path: '' };
        const { clock } = await start(host);
        expect(clock.position()).toBeCloseTo(10, 6);
        host.advance(100);
        host.emit('playback:timeHighRes', { position: 30 });
        expect(clock.position()).toBeCloseTo(30, 6);
    });

    it('keeps a state change that arrived while a read was pending', async () => {
        const host = makeHost();
        host.answers['playback.getPosition'] = {
            success: true, position: 10, duration: 100, subsong: 0, path: '', hostTime: host.clock.wall,
        };
        const { clock } = await start(host);
        const pending = clock.resync();
        host.emit('playback:stateChanged', {
            state: 'paused', position: 11, duration: 100, canSeek: true, hostTime: host.clock.wall + 1,
        });
        await pending;
        expect(clock.state).toBe('paused');
        expect(clock.position()).toBeCloseTo(11, 6);
    });

    it('resolves ready without an estimate when the host is missing', async () => {
        const host = makeHost();
        host.native.invoke.mockImplementation(async () => {
            throw new Error('no host');
        });
        const { clock, changes } = await start(host);
        expect(clock.state).toBe('stopped');
        expect(clock.position()).toBe(0);
        expect(changes).toEqual([]);
    });

    it('stops following playback after dispose', async () => {
        const host = makeHost();
        const { clock, changes } = await start(host);
        expect(host.listenerCount()).toBe(4);
        clock.dispose();
        clock.dispose();
        expect(host.listenerCount()).toBe(0);
        host.emit('playback:seeked', { position: 5, hostTime: host.clock.wall });
        expect(changes.map((change) => change.reason)).toEqual(['start']);
    });

    it.each(['playback.getState', 'playback.getPosition'])(
        'strict refresh rejects %s failures without applying the other answer', async (method) => {
            const host = makeHost();
            host.answers['playback.getPosition'] = {
                success: true, position: 10, duration: 100, subsong: 0, path: '', hostTime: host.clock.wall,
            };
            const { clock, changes } = await start(host);
            changes.length = 0;
            host.answers['playback.getState'] = { success: true, state: 'paused', canSeek: true, canPause: true };
            host.answers['playback.getPosition'] = {
                success: true, position: 50, duration: 200, subsong: 0, path: '', hostTime: host.clock.wall,
            };
            host.answers[method] = { success: false, code: 'PERMISSION_DENIED', error: 'denied' };
            const { ApiCallError } = await import('../../unwrap.js');
            await expect(clock.resync({ rejectOnFailure: true })).rejects.toMatchObject({
                name: 'ApiCallError', code: 'PERMISSION_DENIED', message: 'denied',
            });
            await expect(clock.resync({ rejectOnFailure: true })).rejects.toBeInstanceOf(ApiCallError);
            expect(clock.state).toBe('playing');
            expect(clock.position()).toBe(10);
            expect(clock.duration).toBe(100);
            expect(changes).toEqual([]);
            clock.dispose();
        },
    );

    it('strict refresh preserves invoke rejection and leaves the estimate unchanged', async () => {
        const host = makeHost();
        const { clock, changes } = await start(host);
        const failure = new Error('bridge disconnected');
        host.native.invoke.mockRejectedValueOnce(failure);
        changes.length = 0;
        await expect(clock.resync({ rejectOnFailure: true })).rejects.toBe(failure);
        expect(clock.state).toBe('playing');
        expect(clock.position()).toBe(0);
        expect(changes).toEqual([]);
        clock.dispose();
    });

    it('strict refresh applies both successful answers together and emits one change', async () => {
        const host = makeHost();
        const { clock, changes } = await start(host);
        changes.length = 0;
        host.answers['playback.getState'] = { success: true, state: 'paused', canSeek: true, canPause: true };
        host.answers['playback.getPosition'] = {
            success: true, position: 50, duration: 200, subsong: 0, path: '', hostTime: host.clock.wall,
        };
        await clock.resync({ rejectOnFailure: true });
        expect(clock.state).toBe('paused');
        expect(clock.position()).toBe(50);
        expect(clock.duration).toBe(200);
        expect(changes).toEqual([{ reason: 'resync', state: 'paused', position: 50 }]);
        clock.dispose();
    });

    it.each(['envelope', 'invoke'] as const)('keeps ready and default resync resolving on %s failures', async (failure) => {
        const host = makeHost();
        if (failure === 'invoke') host.native.invoke.mockRejectedValue(new Error('offline'));
        else {
            host.answers['playback.getState'] = { success: false, code: 'NOT_SUPPORTED', error: 'offline' };
            host.answers['playback.getPosition'] = { success: false, code: 'NOT_SUPPORTED', error: 'offline' };
        }
        const { clock } = await start(host);
        await expect(clock.ready).resolves.toBeUndefined();
        await expect(clock.resync()).resolves.toBeUndefined();
        await expect(clock.resync({ rejectOnFailure: false })).resolves.toBeUndefined();
        clock.dispose();
    });

    it('resolves strict refresh when newer state has already superseded its successful answers', async () => {
        const host = makeHost();
        const { clock } = await start(host);
        const pending = clock.resync({ rejectOnFailure: true });
        host.emit('playback:stateChanged', {
            state: 'paused', position: 25, duration: 100, canSeek: true, hostTime: host.clock.wall + 1,
        });
        await expect(pending).resolves.toBeUndefined();
        expect(clock.state).toBe('paused');
        expect(clock.position()).toBe(25);
        clock.dispose();
    });

    it.each(['success', 'failure'] as const)('discards an older successful read after a newer %s', async (outcome) => {
        const host = makeHost();
        const { clock, changes } = await start(host);
        changes.length = 0;
        let resolveState!: (value: object) => void;
        let resolvePosition!: (value: object) => void;
        host.native.invoke
            .mockReturnValueOnce(new Promise((resolve) => { resolveState = resolve; }))
            .mockReturnValueOnce(new Promise((resolve) => { resolvePosition = resolve; }));
        const old = clock.resync({ rejectOnFailure: true });
        host.answers['playback.getState'] = outcome === 'success'
            ? { success: true, state: 'paused', canSeek: true, canPause: true }
            : { success: false, code: 'NOT_SUPPORTED', error: 'offline' };
        host.answers['playback.getPosition'] = {
            success: true, position: 70, duration: 100, subsong: 0, path: '', hostTime: host.clock.wall,
        };
        const latest = clock.resync({ rejectOnFailure: true });
        if (outcome === 'success') await latest;
        else await expect(latest).rejects.toMatchObject({ code: 'NOT_SUPPORTED' });
        resolveState({ success: true, state: 'playing', canSeek: true, canPause: true });
        resolvePosition({ success: true, position: 50, duration: 100, subsong: 0, path: '', hostTime: host.clock.wall });
        if (outcome === 'success') await expect(old).resolves.toBeUndefined();
        else await expect(old).rejects.toThrow('superseded');
        expect(clock.state).toBe(outcome === 'success' ? 'paused' : 'playing');
        expect(clock.position()).toBe(outcome === 'success' ? 70 : 0);
        expect(changes).toHaveLength(outcome === 'success' ? 1 : 0);
        clock.dispose();
    });

    it.each([true, false])('waits for a newer complete read only with strict=%s', async (strict) => {
        const host = makeHost();
        const { clock } = await start(host);
        let finishOld!: (value: object) => void;
        let finishNew!: (value: object) => void;
        host.native.invoke.mockReturnValueOnce(new Promise((resolve) => { finishOld = resolve; }));
        const old = clock.resync({ rejectOnFailure: strict });
        host.native.invoke.mockReturnValueOnce(new Promise((resolve) => { finishNew = resolve; }));
        const latest = clock.resync();
        let settled = false;
        const outcome = old.then(() => { settled = true; }, (error: unknown) => { settled = true; return error; });
        finishOld({ success: true, state: 'paused', canSeek: true, canPause: true });
        for (let i = 0; i < 12; i++) await Promise.resolve();
        expect(settled).toBe(!strict);
        expect(clock.state).toBe('playing');
        finishNew({ success: true, state: 'playing', canSeek: true, canPause: true });
        await latest;
        await expect(outcome).resolves.toBeUndefined();
        clock.dispose();
    });

    it('follows the newest pending read without waiting for an abandoned intermediate response', async () => {
        const host = makeHost();
        const { clock } = await start(host);
        let finishOld!: (value: object) => void;
        let finishMiddle!: (value: object) => void;
        host.native.invoke.mockReturnValueOnce(new Promise((resolve) => { finishOld = resolve; }));
        const old = clock.resync({ rejectOnFailure: true });
        const outcome = old.then(() => undefined, (error: unknown) => error);
        host.native.invoke.mockReturnValueOnce(new Promise((resolve) => { finishMiddle = resolve; }));
        const middle = clock.resync({ rejectOnFailure: true });
        const middleOutcome = middle.then(() => undefined, (error: unknown) => error);
        finishOld({ success: true, state: 'playing', canSeek: true, canPause: true });
        for (let i = 0; i < 12; i++) await Promise.resolve();
        host.answers['playback.getPosition'] = {
            success: true, position: 50, duration: 100, subsong: 0, path: '', hostTime: host.clock.wall,
        };
        await clock.resync({ rejectOnFailure: true });
        await expect(outcome).resolves.toBeUndefined();
        expect(clock.position()).toBe(50);
        finishMiddle({ success: true, state: 'paused', canSeek: true, canPause: true });
        await expect(middleOutcome).resolves.toBeUndefined();
        expect(clock.state).toBe('playing');
        clock.dispose();
    });

    it('settles waiting and pending reads on dispose without applying their later answers', async () => {
        const host = makeHost();
        const { clock, changes } = await start(host);
        let finishOld!: (value: object) => void;
        let finishLatest!: (value: object) => void;
        host.native.invoke.mockReturnValueOnce(new Promise((resolve) => { finishOld = resolve; }));
        const old = clock.resync({ rejectOnFailure: true });
        const outcome = old.then(() => undefined, (error: unknown) => error);
        host.native.invoke.mockReturnValueOnce(new Promise((resolve) => { finishLatest = resolve; }));
        const latest = clock.resync({ rejectOnFailure: true });
        finishOld({ success: true, state: 'paused', canSeek: true, canPause: true });
        for (let i = 0; i < 12; i++) await Promise.resolve();
        clock.dispose();
        await expect(Promise.all([outcome, latest])).resolves.toEqual([undefined, undefined]);
        finishLatest({ success: true, state: 'paused', canSeek: true, canPause: true });
        for (let i = 0; i < 12; i++) await Promise.resolve();
        expect(clock.state).toBe('playing');
        expect(changes).toHaveLength(1);
    });

    it('unregisters disposal callbacks after every completed read without accumulating history', async () => {
        const host = makeHost();
        const { clock } = await start(host);
        expect(activeReadDisposals(clock)).toBe(0);
        for (let index = 0; index < 64; index++) {
            const read = clock.resync({ rejectOnFailure: true });
            expect(activeReadDisposals(clock)).toBe(1);
            await read;
            expect(activeReadDisposals(clock)).toBe(0);
        }
        clock.dispose();
        expect(activeReadDisposals(clock)).toBe(0);
    });

    it.each(['envelope', 'invoke'] as const)('unregisters disposal callbacks after a strict %s failure', async (failure) => {
        const host = makeHost();
        const { clock } = await start(host);
        if (failure === 'invoke') host.native.invoke.mockRejectedValueOnce(new Error('offline'));
        else host.answers['playback.getState'] = { success: false, code: 'NOT_SUPPORTED', error: 'offline' };
        const read = clock.resync({ rejectOnFailure: true });
        const outcome = read.catch((error: unknown) => error);
        expect(activeReadDisposals(clock)).toBe(1);
        expect(await outcome).toMatchObject({ message: 'offline' });
        expect(activeReadDisposals(clock)).toBe(0);
        clock.dispose();
    });

    it('disposes every active read and clears registrations while host promises remain pending', async () => {
        const host = makeHost();
        const { clock, changes } = await start(host);
        host.native.invoke.mockImplementation(() => new Promise(() => {}));
        const reads = [clock.resync(), clock.resync({ rejectOnFailure: true }), clock.resync({ rejectOnFailure: true })];
        expect(activeReadDisposals(clock)).toBe(3);
        clock.dispose();
        expect(activeReadDisposals(clock)).toBe(0);
        await expect(Promise.all(reads)).resolves.toEqual([undefined, undefined, undefined]);
        expect(activeReadDisposals(clock)).toBe(0);
        expect(changes).toHaveLength(1);
    });

    it('does not settle a waiting read on an intermediate failure while a newer read is pending', async () => {
        const host = makeHost();
        const { clock } = await start(host);
        let finishOld!: (value: object) => void;
        let finishMiddle!: (value: object) => void;
        let finishLatest!: (value: object) => void;
        host.native.invoke.mockReturnValueOnce(new Promise((resolve) => { finishOld = resolve; }));
        let settled = false;
        const old = clock.resync({ rejectOnFailure: true });
        const outcome = old.then(() => { settled = true; }, (error: unknown) => { settled = true; return error; });
        host.native.invoke.mockReturnValueOnce(new Promise((resolve) => { finishMiddle = resolve; }));
        const middle = clock.resync({ rejectOnFailure: true });
        const middleOutcome = middle.catch((error: unknown) => error);
        finishOld({ success: true, state: 'playing', canSeek: true, canPause: true });
        for (let i = 0; i < 12; i++) await Promise.resolve();
        host.native.invoke.mockReturnValueOnce(new Promise((resolve) => { finishLatest = resolve; }));
        const latest = clock.resync({ rejectOnFailure: true });
        finishMiddle({ success: false, code: 'NOT_SUPPORTED', error: 'middle failed' });
        expect(await middleOutcome).toMatchObject({ code: 'NOT_SUPPORTED' });
        expect(settled).toBe(false);
        finishLatest({ success: true, state: 'playing', canSeek: true, canPause: true });
        await latest;
        await expect(outcome).resolves.toBeUndefined();
        clock.dispose();
    });

    it('rejects waiters on a track change without waiting for a stale pending response', async () => {
        const host = makeHost();
        const { clock } = await start(host);
        let finishOld!: (value: object) => void;
        let finishLatest!: (value: object) => void;
        host.native.invoke.mockReturnValueOnce(new Promise((resolve) => { finishOld = resolve; }));
        const old = clock.resync({ rejectOnFailure: true });
        const outcome = old.catch((error: unknown) => error);
        host.native.invoke.mockReturnValueOnce(new Promise((resolve) => { finishLatest = resolve; }));
        const latest = clock.resync({ rejectOnFailure: true });
        const latestOutcome = latest.catch((error: unknown) => error);
        finishOld({ success: true, state: 'playing', canSeek: true, canPause: true });
        for (let i = 0; i < 12; i++) await Promise.resolve();
        host.emit('playback:trackChanged', { duration: 200 });
        expect(await outcome).toMatchObject({ message: expect.stringContaining('superseded') });
        expect(clock.position()).toBe(0);
        finishLatest({ success: true, state: 'playing', canSeek: true, canPause: true });
        expect(await latestOutcome).toMatchObject({ message: expect.stringContaining('superseded') });
        clock.dispose();
    });

    it('does not count a newer partial default read as a complete snapshot', async () => {
        const host = makeHost();
        const { clock } = await start(host);
        let finishOld!: (value: object) => void;
        host.native.invoke.mockReturnValueOnce(new Promise((resolve) => { finishOld = resolve; }));
        const old = clock.resync({ rejectOnFailure: true });
        host.answers['playback.getState'] = { success: false, code: 'NOT_SUPPORTED', error: 'offline' };
        host.answers['playback.getPosition'] = {
            success: true, position: 40, duration: 100, subsong: 0, path: '', hostTime: host.clock.wall,
        };
        await clock.resync();
        finishOld({ success: true, state: 'playing', canSeek: true, canPause: true });
        await expect(old).rejects.toThrow('superseded');
        expect(clock.position()).toBe(40);
        clock.dispose();
    });

    it.each([true, false])('discards a previous-track read with strict=%s', async (strict) => {
        const host = makeHost();
        const { clock } = await start(host);
        let finishOld!: (value: object) => void;
        host.native.invoke.mockReturnValueOnce(new Promise((resolve) => { finishOld = resolve; }));
        host.answers['playback.getPosition'] = {
            success: true, position: 70, duration: 100, subsong: 0, path: '', hostTime: host.clock.wall,
        };
        const old = clock.resync({ rejectOnFailure: strict });
        host.emit('playback:trackChanged', { duration: 200 });
        finishOld({ success: true, state: 'playing', canSeek: true, canPause: true });
        if (strict) await expect(old).rejects.toThrow('superseded');
        else await expect(old).resolves.toBeUndefined();
        expect(clock.position()).toBe(0);
        expect(clock.duration).toBe(200);
        clock.dispose();
    });

    it('does not let a successful read before a track change validate older reads', async () => {
        const host = makeHost();
        const { clock } = await start(host);
        let finishOld!: (value: object) => void;
        host.native.invoke.mockReturnValueOnce(new Promise((resolve) => { finishOld = resolve; }));
        const old = clock.resync({ rejectOnFailure: true });
        await clock.resync({ rejectOnFailure: true });
        host.emit('playback:trackChanged', { duration: 200 });
        finishOld({ success: true, state: 'playing', canSeek: true, canPause: true });
        await expect(old).rejects.toThrow('superseded');
        expect(clock.position()).toBe(0);
        expect(clock.duration).toBe(200);
        clock.dispose();
    });

    it('lets a complete current-track snapshot satisfy a discarded previous-track read', async () => {
        const host = makeHost();
        const { clock } = await start(host);
        let finishOld!: (value: object) => void;
        host.native.invoke.mockReturnValueOnce(new Promise((resolve) => { finishOld = resolve; }));
        const old = clock.resync({ rejectOnFailure: true });
        host.emit('playback:trackChanged', { duration: 200 });
        host.answers['playback.getPosition'] = {
            success: true, position: 30, duration: 200, subsong: 0, path: '', hostTime: host.clock.wall,
        };
        await clock.resync();
        finishOld({ success: true, state: 'paused', canSeek: true, canPause: true });
        await expect(old).resolves.toBeUndefined();
        expect(clock.state).toBe('playing');
        expect(clock.position()).toBe(30);
        expect(clock.duration).toBe(200);
        clock.dispose();
    });
});

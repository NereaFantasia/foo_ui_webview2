import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { bridge } from '../../Bridge.js';
import type { RawEventHandler } from '../../Bridge.js';
import { MediaElementFollower } from '../MediaElementFollower.js';
import { PlaybackClock, type PlaybackClockChange, type PlaybackClockChangeReason, type PlaybackClockState, type PlaybackClockResyncOptions } from '../PlaybackClock.js';
import type { MediaGetStreamUrlResponse } from '../../../types/generated/responses.js';

function deferred<T>() {
    let resolve!: (value: T) => void;
    let reject!: (reason: unknown) => void;
    const promise = new Promise<T>((yes, no) => { resolve = yes; reject = no; });
    return { promise, resolve, reject };
}

class FakeClock {
    ready = Promise.resolve();
    state: PlaybackClockState = 'playing';
    duration = 100;
    seconds = 10;
    listeners = new Set<(change: PlaybackClockChange) => void>();
    position = () => this.seconds;
    resync = vi.fn(async (_options?: PlaybackClockResyncOptions) => {});
    dispose = vi.fn();
    onChange(listener: (change: PlaybackClockChange) => void) {
        this.listeners.add(listener);
        return () => { this.listeners.delete(listener); };
    }
    emit(reason: PlaybackClockChangeReason) {
        for (const listener of this.listeners) listener({ reason, state: this.state, position: this.seconds });
    }
}

class FakeElement extends EventTarget {
    srcObject: HTMLMediaElement['srcObject'] = null;
    preload: HTMLMediaElement['preload'] = 'none';
    querySelector = vi.fn((_selector: string): Element | null => null);
    crossOrigin: string | null = null;
    muted = false;
    currentTime = 0;
    duration = 100;
    playbackRate = 1;
    paused = true;
    readyState = 0;
    currentSrc = '';
    error: MediaError | null = null;
    assigned: Array<{ source: string; muted: boolean; crossOrigin: string | null }> = [];
    callbacks = new Map<string, Set<EventListenerOrEventListenerObject>>();
    private source = '';
    get src() { return this.source; }
    set src(value: string) {
        this.source = value;
        this.readyState = 0;
        this.error = null;
        this.assigned.push({ source: value, muted: this.muted, crossOrigin: this.crossOrigin });
    }
    load = vi.fn(() => { this.readyState = 0; });
    pause = vi.fn(() => { this.paused = true; });
    play = vi.fn(async () => { this.paused = false; });
    removeAttribute(name: string) { if (name === 'src') { this.source = ''; this.currentSrc = ''; } }
    override addEventListener(type: string, callback: EventListenerOrEventListenerObject | null) {
        super.addEventListener(type, callback);
        if (callback) {
            const set = this.callbacks.get(type) ?? new Set();
            set.add(callback);
            this.callbacks.set(type, set);
        }
    }
    override removeEventListener(type: string, callback: EventListenerOrEventListenerObject | null) {
        super.removeEventListener(type, callback);
        if (callback) this.callbacks.get(type)?.delete(callback);
    }
    metadata() {
        this.currentSrc = this.src;
        this.readyState = 1;
        this.dispatchEvent(new Event('loadedmetadata'));
    }
    fail(code: number) {
        this.currentSrc = this.src;
        this.error = { code, message: 'media failure', MEDIA_ERR_ABORTED: 1, MEDIA_ERR_NETWORK: 2, MEDIA_ERR_DECODE: 3, MEDIA_ERR_SRC_NOT_SUPPORTED: 4 };
        this.dispatchEvent(new Event('error'));
    }
}

const issued = (url: string): MediaGetStreamUrlResponse => ({ success: true, url, size: 1000, mimeType: 'video/mp4' });
const flush = async () => { for (let i = 0; i < 12; i++) await Promise.resolve(); };
let clock: FakeClock;
let element: FakeElement;
let page: EventTarget & { visibilityState: string };
const followers: MediaElementFollower[] = [];
const realClocks: PlaybackClock[] = [];
function make(offsetSeconds = 0) {
    const follower = new MediaElementFollower(element, { clock, offsetSeconds });
    followers.push(follower);
    return follower;
}
async function playing(offsetSeconds = 0) {
    const follower = make(offsetSeconds);
    await follower.setSource('first.mp4');
    element.metadata();
    await flush();
    clock.resync.mockClear();
    return follower;
}

async function realClockFollower(overrides: Record<string, unknown> = {}, beforeFollower?: () => void) {
    const answers: Record<string, unknown> = {
        'playback.getState': { success: true, state: 'playing', canSeek: true, canPause: true },
        'playback.getPosition': { success: true, position: 10, duration: 100, subsong: 0, path: '', hostTime: 1000 },
        'media.getStreamUrl': issued('https://media.test/real-clock'),
        ...overrides,
    };
    const handlers = new Map<string, Set<RawEventHandler>>();
    vi.mocked(bridge.invoke).mockImplementation(async (method) => answers[method]);
    vi.spyOn(bridge, 'on').mockImplementation((event: string, handler: RawEventHandler) => {
        const listeners = handlers.get(event) ?? new Set<RawEventHandler>();
        listeners.add(handler);
        handlers.set(event, listeners);
        return () => { listeners.delete(handler); };
    });
    const actualClock = new PlaybackClock({ now: () => 1000, monotonicNow: () => 0 });
    realClocks.push(actualClock);
    await actualClock.ready;
    beforeFollower?.();
    const follower = new MediaElementFollower(element, { clock: actualClock });
    followers.push(follower);
    await follower.setSource('first.mp4');
    element.metadata();
    await flush();
    const emit = (event: string, payload: unknown) => {
        for (const handler of handlers.get(event) ?? []) handler(payload);
    };
    return { follower, actualClock, answers, emit };
}

describe('MediaElementFollower', () => {
    beforeEach(() => {
        vi.useFakeTimers();
        clock = new FakeClock();
        element = new FakeElement();
        page = Object.assign(new EventTarget(), { visibilityState: 'visible' });
        vi.stubGlobal('document', page);
        vi.spyOn(bridge, 'invoke').mockImplementation(async (method) => method === 'media.getStreamUrl'
            ? issued('https://media.test/first')
            : { success: false, code: 'NOT_SUPPORTED', error: 'unavailable' });
    });
    afterEach(() => {
        for (const follower of followers.splice(0)) follower.dispose();
        for (const actualClock of realClocks.splice(0)) actualClock.dispose();
        vi.useRealTimers();
        vi.unstubAllGlobals();
    });

    it('sets anonymous CORS and mute before assigning a source, then applies the offset', async () => {
        await playing(2);
        expect(element.assigned).toEqual([{ source: 'https://media.test/first', crossOrigin: 'anonymous', muted: true }]);
        expect(element.currentTime).toBe(12);
        expect(element.play).toHaveBeenCalledTimes(1);
        expect(element.preload).toBe('metadata');
    });

    it('rejects srcObject before taking ownership of the element or clock', () => {
        element.srcObject = new Blob(['media']);
        expect(() => make()).toThrow('srcObject');
        expect(clock.listeners.size).toBe(0);
        expect(element.srcObject).not.toBeNull();
    });

    it('rejects source children without removing or mutating them', () => {
        const source = {} as Element;
        element.querySelector.mockReturnValue(source);
        expect(() => make()).toThrow('source');
        expect(element.querySelector('source')).toBe(source);
        expect(element.load).not.toHaveBeenCalled();
        expect(clock.listeners.size).toBe(0);
    });

    it.each(['playback.getState', 'playback.getPosition', 'invoke'])(
        'stays paused after a real clock %s failure and recovers after a complete read', async (method) => {
            const { follower, answers } = await realClockFollower();
            expect(element.paused).toBe(false);
            const errors: Error[] = [];
            follower.onError((error) => errors.push(error));
            if (method === 'invoke') vi.mocked(bridge.invoke).mockRejectedValueOnce(new Error('offline'));
            else answers[method] = { success: false, code: 'NOT_SUPPORTED', error: 'offline' };
            const failed = follower.resync();
            expect(element.paused).toBe(true);
            await expect(failed).rejects.toThrow('offline');
            expect(errors).toHaveLength(1);
            const playCount = element.play.mock.calls.length;
            await vi.advanceTimersByTimeAsync(300);
            expect(element.paused).toBe(true);
            expect(element.play).toHaveBeenCalledTimes(playCount);
            answers['playback.getState'] = { success: true, state: 'playing', canSeek: true, canPause: true };
            answers['playback.getPosition'] = { success: true, position: 50, duration: 100, subsong: 0, path: '', hostTime: 1000 };
            await follower.resync();
            await flush();
            expect(follower.error).toBeNull();
            expect(element.currentTime).toBe(50);
            expect(element.paused).toBe(false);
        },
    );

    it.each(['paused', 'stopped'] as const)(
        'immediately follows a real host %s event while a refresh is pending', async (state) => {
            const { follower, emit } = await realClockFollower();
            const pending = deferred<unknown>();
            vi.mocked(bridge.invoke).mockReturnValueOnce(pending.promise);
            const refresh = follower.resync();
            element.paused = false;
            element.pause.mockClear();
            const position = element.currentTime;
            emit('playback:stateChanged', { state, position: 25, duration: 100, canSeek: true, hostTime: 1001 });
            expect(element.pause).toHaveBeenCalledTimes(1);
            expect(element.paused).toBe(true);
            expect(element.currentTime).toBe(position);
            pending.resolve({ success: true, state: 'playing', canSeek: true, canPause: true });
            await refresh;
            expect(element.paused).toBe(true);
            expect(element.currentTime).toBe(state === 'paused' ? 25 : 0);
        },
    );

    it('keeps the barrier when an external clock read supersedes its refresh but has not succeeded', async () => {
        const { follower, actualClock, answers } = await realClockFollower();
        const oldState = deferred<unknown>();
        const externalState = deferred<unknown>();
        vi.mocked(bridge.invoke).mockReturnValueOnce(oldState.promise);
        const refresh = follower.resync();
        const outcome = refresh.then(() => null, (error: unknown) => error);
        vi.mocked(bridge.invoke).mockReturnValueOnce(externalState.promise);
        const external = actualClock.resync();
        oldState.resolve({ success: true, state: 'playing', canSeek: true, canPause: true });
        await flush();
        const playCount = element.play.mock.calls.length;
        await vi.advanceTimersByTimeAsync(100);
        expect(element.paused).toBe(true);
        expect(element.play).toHaveBeenCalledTimes(playCount);
        expect(follower.error).toBeNull();
        externalState.resolve({ success: false, code: 'NOT_SUPPORTED', error: 'external read failed' });
        await external;
        expect(await outcome).toMatchObject({ message: expect.stringContaining('superseded') });
        await vi.advanceTimersByTimeAsync(100);
        expect(element.paused).toBe(true);
        expect(follower.error?.message).toContain('superseded');
        answers['playback.getPosition'] = { success: true, position: 50, duration: 100, subsong: 0, path: '', hostTime: 1000 };
        await follower.resync();
        expect(element.currentTime).toBe(50);
        expect(element.paused).toBe(false);
        expect(follower.error).toBeNull();
    });

    it('keeps both followers paused when a shared clock refresh supersedes one and then fails', async () => {
        const { follower, actualClock, answers } = await realClockFollower();
        const secondElement = new FakeElement();
        const second = new MediaElementFollower(secondElement, { clock: actualClock });
        followers.push(second);
        await second.setSource('second.mp4');
        secondElement.metadata();
        await flush();
        expect(secondElement.paused).toBe(false);
        const firstState = deferred<unknown>();
        const secondState = deferred<unknown>();
        vi.mocked(bridge.invoke).mockReturnValueOnce(firstState.promise);
        const firstRefresh = follower.resync();
        const firstOutcome = firstRefresh.then(() => null, (error: unknown) => error);
        vi.mocked(bridge.invoke).mockReturnValueOnce(secondState.promise);
        const secondRefresh = second.resync();
        firstState.resolve({ success: true, state: 'playing', canSeek: true, canPause: true });
        await flush();
        expect(element.paused).toBe(true);
        expect(secondElement.paused).toBe(true);
        secondState.resolve({ success: false, code: 'NOT_SUPPORTED', error: 'shared read failed' });
        await expect(secondRefresh).rejects.toMatchObject({ code: 'NOT_SUPPORTED' });
        expect(await firstOutcome).toMatchObject({ message: expect.stringContaining('superseded') });
        await vi.advanceTimersByTimeAsync(100);
        expect(element.paused).toBe(true);
        expect(secondElement.paused).toBe(true);
        answers['playback.getPosition'] = { success: true, position: 50, duration: 100, subsong: 0, path: '', hostTime: 1000 };
        await follower.resync();
        await second.resync();
        expect(element.currentTime).toBe(50);
        expect(secondElement.currentTime).toBe(50);
        expect(element.paused).toBe(false);
        expect(secondElement.paused).toBe(false);
    });

    it('starts two followers constructed together on an already-ready shared clock', async () => {
        const { follower: previous, actualClock } = await realClockFollower();
        previous.dispose();
        const firstElement = new FakeElement();
        const secondElement = new FakeElement();
        const first = new MediaElementFollower(firstElement, { clock: actualClock });
        const second = new MediaElementFollower(secondElement, { clock: actualClock });
        followers.push(first, second);
        await Promise.all([first.setSource('first.mp4'), second.setSource('second.mp4')]);
        firstElement.metadata();
        secondElement.metadata();
        await flush();
        expect(first.error).toBeNull();
        expect(second.error).toBeNull();
        expect(firstElement.paused).toBe(false);
        expect(secondElement.paused).toBe(false);
        expect(firstElement.currentTime).toBe(10);
        expect(secondElement.currentTime).toBe(10);
    });

    it('resumes both shared followers after the clock and both followers refresh on visibility', async () => {
        const { follower, actualClock, answers } = await realClockFollower();
        const secondElement = new FakeElement();
        const second = new MediaElementFollower(secondElement, { clock: actualClock });
        followers.push(second);
        await second.setSource('second.mp4');
        secondElement.metadata();
        await flush();
        answers['playback.getPosition'] = { success: true, position: 50, duration: 100, subsong: 0, path: '', hostTime: 1000 };
        page.visibilityState = 'hidden';
        page.dispatchEvent(new Event('visibilitychange'));
        page.visibilityState = 'visible';
        page.dispatchEvent(new Event('visibilitychange'));
        await flush();
        expect(follower.error).toBeNull();
        expect(second.error).toBeNull();
        expect(element.paused).toBe(false);
        expect(secondElement.paused).toBe(false);
        expect(element.currentTime).toBe(50);
        expect(secondElement.currentTime).toBe(50);
    });

    it('rejects a previous-track clock read without starting the explicitly selected next source', async () => {
        const { follower, actualClock, answers, emit } = await realClockFollower();
        const oldState = deferred<unknown>();
        vi.mocked(bridge.invoke).mockReturnValueOnce(oldState.promise);
        const refresh = follower.resync();
        emit('playback:trackChanged', { duration: 200 });
        await follower.setSource('next.mp4');
        element.currentTime = 0;
        element.metadata();
        oldState.resolve({ success: true, state: 'playing', canSeek: true, canPause: true });
        await expect(refresh).rejects.toThrow('superseded');
        await vi.advanceTimersByTimeAsync(100);
        expect(actualClock.position()).toBe(0);
        expect(actualClock.duration).toBe(200);
        expect(element.paused).toBe(true);
        expect(element.currentTime).toBe(0);
        expect(follower.error).toBeNull();
        answers['playback.getPosition'] = { success: true, position: 50, duration: 200, subsong: 0, path: '', hostTime: 1000 };
        await follower.resync();
        expect(element.currentTime).toBe(50);
        expect(element.paused).toBe(false);
    });

    it('invalidates a pending play attempt before starting a strict refresh', async () => {
        const follower = await playing();
        const play = deferred<void>();
        element.paused = true;
        element.play.mockReturnValueOnce(play.promise);
        clock.emit('state');
        const read = deferred<void>();
        clock.resync.mockReturnValueOnce(read.promise);
        const refresh = follower.resync();
        expect(clock.resync).toHaveBeenCalledWith({ rejectOnFailure: true });
        expect(element.paused).toBe(true);
        play.reject(new Error('play interrupted by refresh'));
        await flush();
        expect(follower.error).toBeNull();
        read.resolve();
        await refresh;
    });

    it('requires a complete initial read even when ready resolved with partial state', async () => {
        const { follower, answers, actualClock } = await realClockFollower({
            'playback.getPosition': { success: false, code: 'NOT_SUPPORTED', error: 'initial position unavailable' },
        });
        expect(actualClock.state).toBe('playing');
        expect(element.play).not.toHaveBeenCalled();
        expect(element.paused).toBe(true);
        expect(follower.error).toMatchObject({ code: 'NOT_SUPPORTED' });
        answers['playback.getPosition'] = { success: true, position: 50, duration: 100, subsong: 0, path: '', hostTime: 1000 };
        await follower.resync();
        expect(follower.error).toBeNull();
        expect(element.currentTime).toBe(50);
        expect(element.paused).toBe(false);
    });

    it.each(['success', 'failure'] as const)('does not replace an explicit %s when ready settles later', async (outcome) => {
        const ready = deferred<void>();
        clock.ready = ready.promise;
        const follower = make();
        await follower.setSource('first.mp4');
        element.metadata();
        clock.seconds = 50;
        if (outcome === 'failure') clock.resync.mockRejectedValueOnce(new Error('refresh unavailable'));
        const refresh = follower.resync();
        if (outcome === 'success') await refresh;
        else await expect(refresh).rejects.toThrow('refresh unavailable');
        ready.resolve();
        await flush();
        expect(clock.resync).toHaveBeenCalledTimes(1);
        expect(element.paused).toBe(outcome === 'failure');
        expect(element.currentTime).toBe(outcome === 'success' ? 50 : 0);
    });

    it.each(['success', 'failure'] as const)('drops an initial strict read that finishes after a manual %s', async (outcome) => {
        const state = deferred<unknown>();
        const position = deferred<unknown>();
        const { follower, answers } = await realClockFollower({}, () => {
            const invoke = vi.mocked(bridge.invoke);
            invoke.mockImplementationOnce(async () => issued('https://media.test/initial'));
            invoke.mockReturnValueOnce(state.promise).mockReturnValueOnce(position.promise);
        });
        expect(element.paused).toBe(true);
        answers['playback.getState'] = outcome === 'success'
            ? { success: true, state: 'playing', canSeek: true, canPause: true }
            : { success: false, code: 'NOT_SUPPORTED', error: 'manual read failed' };
        answers['playback.getPosition'] = { success: true, position: 50, duration: 100, subsong: 0, path: '', hostTime: 1000 };
        const refresh = follower.resync();
        if (outcome === 'success') await refresh;
        else await expect(refresh).rejects.toThrow('manual read failed');
        state.resolve({ success: true, state: 'playing', canSeek: true, canPause: true });
        position.resolve({ success: true, position: 80, duration: 100, subsong: 0, path: '', hostTime: 1000 });
        await flush();
        expect(element.paused).toBe(outcome === 'failure');
        expect(element.currentTime).toBe(outcome === 'success' ? 50 : 0);
        if (outcome === 'failure') expect(follower.error).toMatchObject({ code: 'NOT_SUPPORTED' });
    });

    it('ignores small drift, adjusts rate for moderate drift, and seeks on large drift', async () => {
        await playing();
        element.currentTime = 9.98;
        await vi.advanceTimersByTimeAsync(100);
        expect(element.currentTime).toBe(9.98);
        expect(element.playbackRate).toBe(1);
        element.currentTime = 9.8;
        await vi.advanceTimersByTimeAsync(100);
        expect(element.currentTime).toBe(9.8);
        expect(element.playbackRate).toBe(1.05);
        element.currentTime = 10.2;
        await vi.advanceTimersByTimeAsync(100);
        expect(element.playbackRate).toBe(0.95);
        element.currentTime = 9.7;
        await vi.advanceTimersByTimeAsync(100);
        expect(element.currentTime).toBe(10);
        expect(element.playbackRate).toBe(1);
    });

    it('hard-aligns seeks and follows pause, resume and stop', async () => {
        await playing(2);
        clock.seconds = 10.01;
        clock.emit('seek');
        expect(element.currentTime).toBe(12.01);
        clock.state = 'paused';
        clock.seconds = 20;
        clock.emit('state');
        expect(element.paused).toBe(true);
        expect(element.currentTime).toBe(22);
        clock.state = 'playing';
        clock.emit('state');
        await flush();
        expect(element.paused).toBe(false);
        clock.state = 'stopped';
        clock.seconds = 0;
        clock.emit('state');
        expect(element.paused).toBe(true);
        expect(element.currentTime).toBe(0);
    });

    it('clamps offset targets to the media timeline and keeps a negative target paused', async () => {
        await playing(-20);
        expect(element.currentTime).toBe(0);
        expect(element.play).not.toHaveBeenCalled();
        clock.seconds = 30;
        clock.emit('seek');
        expect(element.currentTime).toBe(10);
        clock.seconds = 130;
        clock.emit('seek');
        expect(element.currentTime).toBe(100);
        expect(element.paused).toBe(true);
    });

    it('pauses on track changes and keeps the loaded resource until a source is selected', async () => {
        await playing();
        clock.emit('track');
        expect(element.src).toBe('https://media.test/first');
        expect(element.paused).toBe(true);
        clock.seconds = 30;
        await vi.advanceTimersByTimeAsync(500);
        expect(element.paused).toBe(true);
        expect(element.currentTime).toBe(10);
        expect(bridge.invoke).toHaveBeenCalledTimes(1);
    });

    it('reuses the resource for another subsong of the same file at its timeline offset', async () => {
        const follower = make(0.5);
        await follower.setSource('movie.mkv|subsong:1');
        element.metadata();
        await flush();
        expect(element.currentTime).toBe(10.5);
        clock.emit('track');
        clock.seconds = 0;
        element.play.mockClear();
        await follower.setSource('movie.mkv|subsong:2', { timelineOffset: 20 });
        expect(bridge.invoke).toHaveBeenCalledTimes(1);
        expect(element.assigned).toHaveLength(1);
        expect(element.currentTime).toBe(20.5);
        expect(element.play).toHaveBeenCalledTimes(1);
        clock.seconds = 1;
        await vi.advanceTimersByTimeAsync(100);
        expect(element.currentTime).toBe(21.5);
    });

    it('replaces the resource for another file or after the kept resource failed', async () => {
        const follower = await playing();
        clock.emit('track');
        await follower.setSource('second.mp4', { timelineOffset: 5 });
        expect(bridge.invoke).toHaveBeenCalledTimes(2);
        element.metadata();
        await flush();
        expect(element.currentTime).toBe(15);
        clock.emit('track');
        element.fail(3);
        await follower.setSource('second.mp4|subsong:2');
        expect(bridge.invoke).toHaveBeenCalledTimes(3);
        expect(follower.error).toBeNull();
    });

    it('rejects a non-finite timeline offset without changing the source', async () => {
        const follower = await playing();
        await expect(follower.setSource('other.mp4', { timelineOffset: Number.NaN })).rejects.toThrow(RangeError);
        expect(element.src).toBe('https://media.test/first');
        expect(bridge.invoke).toHaveBeenCalledTimes(1);
    });

    it('drops a pending resource on track changes instead of keeping it', async () => {
        const pending = deferred<MediaGetStreamUrlResponse>();
        vi.mocked(bridge.invoke).mockReturnValueOnce(pending.promise);
        const follower = make();
        const request = follower.setSource('movie.mkv|subsong:1');
        clock.emit('track');
        pending.resolve(issued('https://media.test/late'));
        await request;
        await follower.setSource('movie.mkv|subsong:2');
        expect(bridge.invoke).toHaveBeenCalledTimes(2);
    });

    it('waits for a fresh clock reading before aligning after becoming visible', async () => {
        await playing();
        const read = deferred<void>();
        clock.resync.mockReturnValueOnce(read.promise);
        page.visibilityState = 'hidden';
        page.dispatchEvent(new Event('visibilitychange'));
        clock.seconds = 50;
        page.visibilityState = 'visible';
        page.dispatchEvent(new Event('visibilitychange'));
        await vi.advanceTimersByTimeAsync(100);
        expect(clock.resync).toHaveBeenCalledTimes(1);
        expect(element.currentTime).toBe(10);
        read.resolve();
        await flush();
        expect(element.currentTime).toBe(50);
    });

    it.each(['setSource', 'track', 'renewal'] as const)(
        'keeps the visibility refresh barrier across %s until the clock is fresh', async (action) => {
            const follower = await playing();
            const refresh = deferred<void>();
            clock.resync.mockImplementationOnce(async () => {
                await refresh.promise;
                clock.seconds = 50;
            });
            page.visibilityState = 'hidden';
            page.dispatchEvent(new Event('visibilitychange'));
            page.visibilityState = 'visible';
            page.dispatchEvent(new Event('visibilitychange'));
            if (action === 'renewal') {
                element.fail(2);
                await flush();
            } else {
                if (action === 'track') clock.emit('track');
                await follower.setSource('next.mp4');
            }
            element.currentTime = 0;
            element.play.mockClear();
            element.metadata();
            await vi.advanceTimersByTimeAsync(100);
            expect(element.paused).toBe(true);
            expect(element.currentTime).toBe(0);
            expect(element.play).not.toHaveBeenCalled();
            refresh.resolve();
            await flush();
            expect(element.currentTime).toBe(50);
            expect(element.paused).toBe(false);
            expect(element.play).toHaveBeenCalledTimes(1);
        },
    );

    it('waits for the latest refresh when an older read completes after a source change', async () => {
        const follower = await playing();
        const old = deferred<void>();
        const latest = deferred<void>();
        clock.resync.mockReturnValueOnce(old.promise).mockReturnValueOnce(latest.promise);
        const first = follower.resync();
        await follower.setSource('next.mp4');
        const second = follower.resync();
        element.currentTime = 0;
        element.metadata();
        old.resolve();
        await first;
        await vi.advanceTimersByTimeAsync(100);
        expect(element.paused).toBe(true);
        expect(element.currentTime).toBe(0);
        clock.seconds = 50;
        latest.resolve();
        await second;
        expect(element.currentTime).toBe(50);
    });

    it('keeps a new source paused after an old refresh fails until a fresh read succeeds', async () => {
        const follower = await playing();
        const refresh = deferred<void>();
        clock.resync.mockReturnValueOnce(refresh.promise);
        page.dispatchEvent(new Event('visibilitychange'));
        await follower.setSource('next.mp4');
        element.currentTime = 0;
        element.metadata();
        refresh.reject(new Error('old read failed'));
        await vi.advanceTimersByTimeAsync(100);
        expect(follower.error).toBeNull();
        expect(element.paused).toBe(true);
        expect(element.currentTime).toBe(0);
        clock.seconds = 50;
        await follower.resync();
        expect(element.currentTime).toBe(50);
        expect(element.paused).toBe(false);
    });

    it('ignores late source responses and late failures from superseded requests', async () => {
        const first = deferred<MediaGetStreamUrlResponse>();
        const second = deferred<MediaGetStreamUrlResponse>();
        vi.mocked(bridge.invoke).mockReturnValueOnce(first.promise).mockReturnValueOnce(second.promise);
        const follower = make();
        const old = follower.setSource('old.mp4');
        const current = follower.setSource('new.mp4');
        second.resolve(issued('https://media.test/new'));
        await current;
        first.reject(new Error('old failed'));
        await old;
        expect(element.src).toBe('https://media.test/new');
        expect(follower.error).toBeNull();
    });

    it('ignores metadata callbacks belonging to a previous source', async () => {
        const follower = await playing();
        const oldListeners = [...(element.callbacks.get('loadedmetadata') ?? [])];
        await follower.setSource('new.mp4');
        element.currentTime = 3;
        element.readyState = 1;
        for (const listener of oldListeners) {
            if (typeof listener === 'function') listener(new Event('loadedmetadata'));
            else listener.handleEvent(new Event('loadedmetadata'));
        }
        expect(element.currentTime).toBe(3);
        element.metadata();
        expect(element.currentTime).toBe(10);
    });

    it('ignores rejection of play from a previous source', async () => {
        const play = deferred<void>();
        element.play.mockReturnValueOnce(play.promise);
        const follower = await playing();
        await follower.setSource('new.mp4');
        element.metadata();
        play.reject(new Error('old play aborted'));
        await flush();
        expect(follower.error).toBeNull();
    });

    it('reports a current play rejection once and lets resync retry', async () => {
        const denied = new Error('play denied');
        element.play.mockRejectedValueOnce(denied);
        const follower = make();
        const errors: Error[] = [];
        const off = follower.onError((error) => errors.push(error));
        await follower.setSource('first.mp4');
        element.metadata();
        await flush();
        expect(follower.error).toBe(denied);
        expect(errors).toEqual([denied]);
        await vi.advanceTimersByTimeAsync(500);
        expect(element.play).toHaveBeenCalledTimes(1);
        off();
        await follower.resync();
        await flush();
        expect(follower.error).toBeNull();
        expect(element.play).toHaveBeenCalledTimes(2);
    });

    it('renews network failures at most twice and restores the latest clock position', async () => {
        const follower = await playing();
        const renewal = deferred<MediaGetStreamUrlResponse>();
        vi.mocked(bridge.invoke).mockReturnValueOnce(renewal.promise);
        element.fail(2);
        element.fail(2);
        clock.seconds = 40;
        renewal.resolve(issued('https://media.test/renewed'));
        await flush();
        element.metadata();
        expect(element.currentTime).toBe(40);
        element.fail(2);
        await flush();
        element.metadata();
        element.fail(2);
        await flush();
        expect(bridge.invoke).toHaveBeenCalledTimes(3);
        expect(follower.error?.message).toContain('media failure');
        expect(element.paused).toBe(true);
    });

    it('stops renewal when the host denies permission', async () => {
        const follower = await playing();
        vi.mocked(bridge.invoke).mockResolvedValueOnce({ success: false, code: 'PERMISSION_DENIED', error: 'denied' });
        element.fail(2);
        await flush();
        element.fail(2);
        await flush();
        expect(bridge.invoke).toHaveBeenCalledTimes(2);
        expect(follower.error).toMatchObject({ code: 'PERMISSION_DENIED' });
        expect(element.paused).toBe(true);
    });

    it('does not let renewal overwrite an explicitly selected source', async () => {
        const follower = await playing();
        const renewal = deferred<MediaGetStreamUrlResponse>();
        vi.mocked(bridge.invoke).mockReturnValueOnce(renewal.promise);
        element.fail(2);
        await follower.setSource('new.mp4');
        renewal.resolve(issued('https://media.test/stale'));
        await flush();
        expect(element.src).toBe('https://media.test/first');
        expect(follower.error).toBeNull();
    });

    it('reports and rejects an explicit source failure but does not retry decode errors', async () => {
        const follower = make();
        vi.mocked(bridge.invoke).mockResolvedValueOnce({ success: false, code: 'PERMISSION_DENIED', error: 'denied' });
        await expect(follower.setSource('denied.mp4')).rejects.toMatchObject({ code: 'PERMISSION_DENIED' });
        expect(follower.error).toMatchObject({ code: 'PERMISSION_DENIED' });
        await follower.setSource('ok.mp4');
        element.metadata();
        element.fail(3);
        await flush();
        expect(bridge.invoke).toHaveBeenCalledTimes(2);
        expect(follower.error?.message).toContain('media failure');
    });

    it('waits for clock readiness, ignores disposed work and releases only owned resources', async () => {
        const ready = deferred<void>();
        clock.ready = ready.promise;
        const follower = make();
        await follower.setSource('first.mp4');
        element.metadata();
        expect(element.play).not.toHaveBeenCalled();
        ready.resolve();
        await flush();
        expect(element.play).toHaveBeenCalledTimes(1);
        expect(clock.resync).toHaveBeenCalledWith({ rejectOnFailure: true });
        clock.resync.mockClear();
        follower.dispose();
        expect(clock.dispose).not.toHaveBeenCalled();
        expect(clock.listeners.size).toBe(0);
        expect([...element.callbacks.values()].every((set) => set.size === 0)).toBe(true);
        expect(vi.getTimerCount()).toBe(0);
        page.dispatchEvent(new Event('visibilitychange'));
        expect(clock.resync).not.toHaveBeenCalled();
        await expect(follower.setSource('late.mp4')).rejects.toThrow('disposed');
        await expect(follower.resync()).rejects.toThrow('disposed');
        expect(() => follower.onError(() => {})).toThrow('disposed');
        const dispose = vi.spyOn(PlaybackClock.prototype, 'dispose');
        const owned = new MediaElementFollower(new FakeElement());
        owned.dispose();
        expect(dispose).toHaveBeenCalledTimes(1);
    });

    it('clearing a source cancels pending issue requests', async () => {
        const pending = deferred<MediaGetStreamUrlResponse>();
        vi.mocked(bridge.invoke).mockReturnValueOnce(pending.promise);
        const follower = make();
        const request = follower.setSource('first.mp4');
        await follower.setSource(null);
        pending.resolve(issued('https://media.test/late'));
        await request;
        expect(element.src).toBe('');
    });

    it.each(['track', 'dispose'] as const)('ignores a source response after %s', async (action) => {
        const pending = deferred<MediaGetStreamUrlResponse>();
        vi.mocked(bridge.invoke).mockReturnValueOnce(pending.promise);
        const follower = make();
        const request = follower.setSource('first.mp4');
        if (action === 'track') clock.emit('track');
        else follower.dispose();
        pending.resolve(issued('https://media.test/late'));
        await request;
        expect(element.src).toBe('');
        expect(follower.error).toBeNull();
    });

    it('ignores a successful old source response after a new source has metadata', async () => {
        const pending = deferred<MediaGetStreamUrlResponse>();
        vi.mocked(bridge.invoke).mockReturnValueOnce(pending.promise);
        const follower = make();
        const old = follower.setSource('old.mp4');
        await follower.setSource('new.mp4');
        element.metadata();
        pending.resolve(issued('https://media.test/old'));
        await old;
        expect(element.src).toBe('https://media.test/first');
        expect(element.currentTime).toBe(10);
    });

    it('ignores a superseded visibility refresh failure and preserves the new source', async () => {
        const follower = await playing();
        const refresh = deferred<void>();
        clock.resync.mockReturnValueOnce(refresh.promise);
        page.dispatchEvent(new Event('visibilitychange'));
        await follower.setSource('new.mp4');
        element.metadata();
        refresh.reject(new Error('old refresh failed'));
        await flush();
        expect(follower.error).toBeNull();
        expect(element.src).toBe('https://media.test/first');
    });

    it('reports an expired play attempt only for its current transport state', async () => {
        const pending = deferred<void>();
        element.play.mockReturnValueOnce(pending.promise);
        const follower = await playing();
        clock.state = 'paused';
        clock.emit('state');
        pending.reject(new Error('play aborted by pause'));
        await flush();
        expect(follower.error).toBeNull();
    });

    it('renews again after an explicit source choice resets the retry budget', async () => {
        const follower = await playing();
        for (let i = 0; i < 3; i++) {
            element.fail(2);
            await flush();
            element.metadata();
        }
        expect(follower.error).not.toBeNull();
        await follower.setSource('another.mp4');
        element.metadata();
        element.fail(2);
        await flush();
        element.metadata();
        expect(bridge.invoke).toHaveBeenCalledTimes(5);
        expect(follower.error).toBeNull();
    });
});

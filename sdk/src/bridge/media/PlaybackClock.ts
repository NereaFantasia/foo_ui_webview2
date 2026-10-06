/**
 * An estimate of foobar2000's playback position at any moment, for anything
 * that has to move with the audio between the host's position updates: a
 * progress bar, word-timed lyrics, a muted `<video>` that follows playback.
 *
 * The host stamps every position it reports with `hostTime`, the system time
 * it read the position at, on the clock `Date.now()` reads. The clock takes
 * `Date.now() - hostTime` as the age of a position when it arrives and moves
 * forward from there on `performance.now()`.
 */

import { call } from '../call.js';
import { subscribe } from '../subscribe.js';
import { ApiCallError } from '../unwrap.js';
import type {
    PlaybackSeekedPayload,
    PlaybackStateChangedPayload,
    PlaybackTimeHighResPayload,
    PlaybackTrackChangedPayload,
} from '../../types/events.js';

/** Transport state the clock follows. */
export type PlaybackClockState = 'playing' | 'paused' | 'stopped';

/**
 * Why the estimate jumped instead of moving on smoothly: the first read
 * (`start`), a seek, a state change, a new track, or a position update that
 * disagreed with the estimate by more than the resync threshold (`resync`,
 * also sent after {@link PlaybackClock.resync} reads the position again).
 */
export type PlaybackClockChangeReason = 'start' | 'seek' | 'state' | 'track' | 'resync';

/** Passed to {@link PlaybackClock.onChange} listeners. */
export interface PlaybackClockChange {
    reason: PlaybackClockChangeReason;
    state: PlaybackClockState;
    /** Estimated position right after the change, in seconds. */
    position: number;
}

/** Options of {@link PlaybackClock}. */
export interface PlaybackClockOptions {
    /**
     * Oldest position update the clock accepts, in milliseconds between
     * `hostTime` and arrival. Older updates, and those that look older or newer
     * than possible because the system time was changed in between, are
     * ignored. Defaults to 1000.
     */
    maxDeliveryDelayMs?: number;
    /**
     * Difference in seconds between an update and the estimate above which the
     * update replaces the estimate (a `resync` change) instead of nudging it.
     * Defaults to 0.25.
     */
    resyncThresholdSeconds?: number;
    /** Wall clock in Unix milliseconds; defaults to `Date.now`. */
    now?: () => number;
    /** Monotonic clock in milliseconds; defaults to `performance.now`. */
    monotonicNow?: () => number;
}

/** Control whether a host read must succeed before resync can resolve. */
export interface PlaybackClockResyncOptions {
    /**
     * Reject if either host call fails, without applying either answer.
     * Defaults to false; failure envelopes become ApiCallError instances.
     */
    rejectOnFailure?: boolean;
}

const DEFAULT_MAX_DELIVERY_DELAY_MS = 1000;
const DEFAULT_RESYNC_THRESHOLD_SECONDS = 0.25;
// Date.now() has whole milliseconds and hostTime a fraction, so a fresh update
// can look up to a millisecond younger than its arrival; allow a little more.
const CLOCK_TOLERANCE_MS = 2;
// Share of the error a small disagreement moves the estimate by, so the
// position does not stutter on the host's reading jitter.
const CORRECTION_GAIN = 0.25;
// After a seek, updates read this soon after it can still carry the old
// position; they are ignored when they disagree with the seek target.
const SEEK_GUARD_MS = 250;

interface Anchor {
    /** Position in seconds at monotonic time `at`. */
    position: number;
    at: number;
}

const defaultMonotonicNow = (): number =>
    typeof globalThis.performance?.now === 'function' ? globalThis.performance.now() : Date.now();

/**
 * Follows playback through `playback:timeHighRes`, `playback:seeked`,
 * `playback:stateChanged` and `playback:trackChanged`, and reads the state and
 * position once at creation, again when the page becomes visible, and on
 * {@link PlaybackClock.resync}. Call {@link PlaybackClock.dispose} when done.
 *
 * With a host that does not send `hostTime`, updates count as read when they
 * arrive, so the estimate trails the audio by the delivery delay.
 *
 * @example
 *   const clock = new PlaybackClock();
 *   await clock.ready;
 *   const draw = () => {
 *       bar.style.width = `${(clock.position() / clock.duration) * 100}%`;
 *       requestAnimationFrame(draw);
 *   };
 *   requestAnimationFrame(draw);
 */
export class PlaybackClock {
    /** Resolves once the first read of state and position settled; never rejects. */
    readonly ready: Promise<void>;

    private readonly maxDelay: number;
    private readonly resyncThreshold: number;
    private readonly now: () => number;
    private readonly monotonicNow: () => number;
    private readonly listeners = new Set<(change: PlaybackClockChange) => void>();
    private readonly unsubscribers: Array<() => void> = [];
    private currentState: PlaybackClockState = 'stopped';
    private currentDuration = 0;
    private anchor: Anchor;
    // Host time of the newest update applied; older ones arriving late are dropped.
    private newestHostTime = Number.NEGATIVE_INFINITY;
    private seekGuardUntil = Number.NEGATIVE_INFINITY;
    // Set when a new track starts: its first update replaces the estimate.
    private replaceNext = true;
    // Bumped by every stateChanged, so a read that started earlier does not overwrite it.
    private stateGeneration = 0;
    // Only the newest read may apply answers. A strict caller whose read was
    // replaced waits for a complete newer snapshot for the same track.
    private readGeneration = 0;
    private completeReadGeneration = 0;
    private pendingReadGeneration: number | null = null;
    private readonly readWaiters = new Set<() => void>();
    private readonly readDisposers = new Set<() => void>();
    private disposed = false;

    constructor(options: PlaybackClockOptions = {}) {
        this.maxDelay = options.maxDeliveryDelayMs ?? DEFAULT_MAX_DELIVERY_DELAY_MS;
        this.resyncThreshold = options.resyncThresholdSeconds ?? DEFAULT_RESYNC_THRESHOLD_SECONDS;
        this.now = options.now ?? Date.now;
        this.monotonicNow = options.monotonicNow ?? defaultMonotonicNow;
        this.anchor = { position: 0, at: this.monotonicNow() };
        // Subscribe before the first read, so nothing sent while it is pending is missed.
        this.unsubscribers.push(
            subscribe('playback:timeHighRes', (payload) => this.onTick(payload)),
            subscribe('playback:seeked', (payload) => this.onSeeked(payload)),
            subscribe('playback:stateChanged', (payload) => this.onStateChanged(payload)),
            subscribe('playback:trackChanged', (payload) => this.onTrackChanged(payload)),
        );
        if (typeof document !== 'undefined' && typeof document.addEventListener === 'function') {
            const onVisibility = (): void => {
                if (document.visibilityState === 'visible') void this.resync();
            };
            document.addEventListener('visibilitychange', onVisibility);
            this.unsubscribers.push(() => document.removeEventListener('visibilitychange', onVisibility));
        }
        this.ready = this.read('start');
    }

    /** The transport state last reported by the host. */
    get state(): PlaybackClockState {
        return this.currentState;
    }

    /** Length of the playing track in seconds; `0` when unknown or stopped. */
    get duration(): number {
        return this.currentDuration;
    }

    /**
     * Estimated position in seconds at monotonic time `at` (the
     * `performance.now()` scale, now when omitted), kept within the track's
     * duration when it is known. Paused and stopped positions do not move.
     */
    position(at: number = this.monotonicNow()): number {
        const { position, at: anchoredAt } = this.anchor;
        const moved = this.currentState === 'playing' ? position + (at - anchoredAt) / 1000 : position;
        const upper = this.currentDuration > 0 ? this.currentDuration : Number.POSITIVE_INFINITY;
        return Math.min(Math.max(moved, 0), upper);
    }

    /** Call `listener` whenever the estimate jumps; returns a function that removes it. */
    onChange(listener: (change: PlaybackClockChange) => void): () => void {
        this.listeners.add(listener);
        return () => {
            this.listeners.delete(listener);
        };
    }

    /**
     * Refresh the estimate from the host's state and position.
     * By default, failed responses are ignored and successful answers may still
     * apply. With rejectOnFailure, both calls must succeed before either applies;
     * host failure envelopes reject with ApiCallError and transport errors propagate.
     * Events received during the read take precedence over older answers.
     *
     * If a newer read replaces a successful strict read, the older caller waits
     * for a complete newer snapshot for the current track. It rejects if none is
     * available or pending; it does not start another read. Disposing the clock
     * releases pending reads without applying their answers, even in strict mode.
     */
    resync(options: PlaybackClockResyncOptions = {}): Promise<void> {
        return this.read('resync', options.rejectOnFailure ?? false);
    }

    /** Stop following playback and drop the listeners. Safe to call more than once. */
    dispose(): void {
        if (this.disposed) return;
        this.disposed = true;
        this.pendingReadGeneration = null;
        for (const disposeRead of this.readDisposers) disposeRead();
        this.readDisposers.clear();
        this.wakeReadWaiters();
        for (const unsubscribe of this.unsubscribers.splice(0)) unsubscribe();
        this.listeners.clear();
    }

    private async read(reason: PlaybackClockChangeReason, rejectOnFailure = false): Promise<void> {
        if (this.disposed) return;
        const generation = this.stateGeneration;
        const readGeneration = ++this.readGeneration;
        // Give each read its own removable cancellation registration. A shared
        // never-settled promise would retain reactions from every completed read.
        let disposeRead!: () => void;
        const disposedSignal = new Promise<null>((resolve) => {
            disposeRead = () => { resolve(null); };
        });
        this.readDisposers.add(disposeRead);
        this.pendingReadGeneration = readGeneration;
        this.wakeReadWaiters();
        try {
            let stateAnswer: Awaited<ReturnType<typeof readState>> = null;
            let positionAnswer: Awaited<ReturnType<typeof readPosition>> = null;
            try {
                const answers = await Promise.race([
                    Promise.all([readState(rejectOnFailure), readPosition(rejectOnFailure)]),
                    disposedSignal,
                ]);
                if (answers === null) return;
                [stateAnswer, positionAnswer] = answers;
            } catch (error) {
                if (rejectOnFailure) throw error;
                return;
            }
            if (this.disposed) return;
            if (readGeneration !== this.readGeneration) {
                if (rejectOnFailure) await this.waitForCompleteRead(readGeneration);
                return;
            }
            this.applyReadResults(stateAnswer, positionAnswer, generation, readGeneration, reason);
        } finally {
            this.readDisposers.delete(disposeRead);
            if (this.pendingReadGeneration === readGeneration) this.pendingReadGeneration = null;
            this.wakeReadWaiters();
        }
    }

    private applyReadResults(
        state: Awaited<ReturnType<typeof readState>>,
        position: Awaited<ReturnType<typeof readPosition>>,
        stateGeneration: number,
        readGeneration: number,
        reason: PlaybackClockChangeReason,
    ): void {
        if (state && stateGeneration === this.stateGeneration) this.currentState = state.state;
        if (position && this.isNewest(position.hostTime)) {
            this.currentDuration = position.duration;
            this.place(position.position, this.readAt(position.hostTime, true) ?? this.monotonicNow());
        }
        // A partial non-strict read cannot release callers waiting for a full snapshot.
        if (state && position) this.completeReadGeneration = readGeneration;
        this.emit(reason);
    }

    private async waitForCompleteRead(generation: number): Promise<void> {
        // Follow the latest pending read, not one captured promise: an intermediate
        // read may never reply while a later read already has the required snapshot.
        while (!this.disposed && this.completeReadGeneration <= generation) {
            if (this.pendingReadGeneration === null || this.pendingReadGeneration <= generation
                || this.pendingReadGeneration !== this.readGeneration) {
                throw new Error('PlaybackClock refresh was superseded without a complete newer snapshot');
            }
            await new Promise<void>((resolve) => { this.readWaiters.add(resolve); });
        }
    }

    private wakeReadWaiters(): void {
        const waiters = [...this.readWaiters];
        this.readWaiters.clear();
        for (const wake of waiters) wake();
    }

    private onTick(payload: PlaybackTimeHighResPayload): void {
        const at = this.readAt(payload.hostTime, false);
        if (at === null || !this.isNewest(payload.hostTime)) return;
        const error = payload.position - this.position(at);
        const disagrees = Math.abs(error) > this.resyncThreshold;
        if (disagrees && typeof payload.hostTime === 'number' && payload.hostTime < this.seekGuardUntil) return;
        if (this.replaceNext || this.currentState !== 'playing' || disagrees) {
            const jumped = !this.replaceNext && disagrees && this.currentState === 'playing';
            this.place(payload.position, at);
            if (jumped) this.emit('resync');
            return;
        }
        this.anchor = { position: this.position(at) + error * CORRECTION_GAIN, at };
    }

    private onSeeked(payload: PlaybackSeekedPayload): void {
        const at = this.readAt(payload.hostTime, true) ?? this.monotonicNow();
        this.isNewest(payload.hostTime);
        this.seekGuardUntil = (typeof payload.hostTime === 'number' ? payload.hostTime : this.now()) + SEEK_GUARD_MS;
        this.place(payload.position, at);
        this.emit('seek');
    }

    private onStateChanged(payload: PlaybackStateChangedPayload): void {
        this.stateGeneration += 1;
        const at = this.readAt(payload.hostTime, true) ?? this.monotonicNow();
        this.isNewest(payload.hostTime);
        this.currentState = payload.state;
        this.currentDuration = payload.state === 'stopped' ? 0 : payload.duration;
        this.place(payload.state === 'stopped' ? 0 : payload.position, at);
        this.emit('state');
    }

    private onTrackChanged(payload: PlaybackTrackChangedPayload): void {
        // A complete snapshot from the previous track cannot authorize playback of the next.
        this.stateGeneration += 1;
        this.readGeneration += 1;
        this.completeReadGeneration = 0;
        this.pendingReadGeneration = null;
        this.wakeReadWaiters();
        this.currentDuration = typeof payload.duration === 'number' ? payload.duration : 0;
        this.place(0, this.monotonicNow());
        this.replaceNext = true;
        this.emit('track');
    }

    /**
     * Monotonic time the host read a position at, from its `hostTime`. `null`
     * when the update is older than allowed or the system time moved in
     * between, unless `lenient`, which counts such an update as read on
     * arrival. Updates without `hostTime` always count as read on arrival.
     */
    private readAt(hostTime: unknown, lenient: boolean): number | null {
        const arrivedAt = this.monotonicNow();
        if (typeof hostTime !== 'number' || !Number.isFinite(hostTime)) return arrivedAt;
        const delay = this.now() - hostTime;
        if (delay < -CLOCK_TOLERANCE_MS || delay > this.maxDelay) return lenient ? arrivedAt : null;
        return arrivedAt - Math.max(0, delay);
    }

    /** Whether `hostTime` is not older than the newest applied update; records it when so. */
    private isNewest(hostTime: unknown): boolean {
        if (typeof hostTime !== 'number' || !Number.isFinite(hostTime)) return true;
        if (hostTime < this.newestHostTime) return false;
        this.newestHostTime = hostTime;
        return true;
    }

    private place(position: number, at: number): void {
        this.anchor = { position, at };
        this.replaceNext = false;
    }

    private emit(reason: PlaybackClockChangeReason): void {
        if (this.disposed) return;
        const change: PlaybackClockChange = { reason, state: this.currentState, position: this.position() };
        for (const listener of [...this.listeners]) listener(change);
    }
}

async function readState(rejectOnFailure: boolean): Promise<{ state: PlaybackClockState } | null> {
    const answer = await call('playback.getState');
    if (answer.success === false && rejectOnFailure) throw new ApiCallError(answer);
    return answer.success === true ? { state: answer.state } : null;
}

async function readPosition(rejectOnFailure: boolean): Promise<{ position: number; duration: number; hostTime: unknown } | null> {
    const answer = await call('playback.getPosition');
    if (answer.success === false && rejectOnFailure) throw new ApiCallError(answer);
    if (answer.success !== true) return null;
    return { position: answer.position, duration: answer.duration, hostTime: answer.hostTime };
}

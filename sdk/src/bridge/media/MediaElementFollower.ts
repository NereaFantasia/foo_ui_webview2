import { PlaybackClock } from './PlaybackClock.js';
import { media } from '../namespaces/media.js';
import { unwrap } from '../unwrap.js';

/**
 * Supply a clock without transferring ownership to the follower.
 * resync must honor rejectOnFailure: resolving it authorizes playback, so an
 * incomplete or failed refresh must reject. ready must settle even if the initial
 * read fails; the follower performs a strict refresh before starting playback.
 */
export type MediaFollowerClock = Pick<PlaybackClock, 'ready' | 'state' | 'duration' | 'position' | 'onChange' | 'resync'>;

/** Clock ownership and the mapping from the host to the media timeline. */
export interface MediaElementFollowerOptions {
    /** Borrowed when supplied; otherwise the follower creates and disposes a clock. */
    clock?: MediaFollowerClock;
    /** Added to clock.position(), in seconds; defaults to zero. */
    offsetSeconds?: number;
}

/** Where a source's playback starts within its file. */
export interface MediaSourceOptions {
    /**
     * Start of the playing track on the file's timeline, in seconds; defaults to zero.
     * For a chapter that foobar2000 plays as a subsong, pass the chapter's start from
     * `media.getContainerInfo`. Added to offsetSeconds.
     */
    timelineOffset?: number;
}

type MediaElement = Pick<HTMLMediaElement,
    'src' | 'srcObject' | 'preload' | 'currentSrc' | 'crossOrigin' | 'muted' | 'currentTime' | 'duration' |
    'playbackRate' | 'paused' | 'readyState' | 'error' | 'play' | 'pause' | 'load' |
    'removeAttribute' | 'addEventListener' | 'removeEventListener'> & {
        querySelector(selectors: string): Element | null;
    };

const INTERVAL_MS = 100;
const SMALL_DRIFT_SECONDS = 0.05;
const LARGE_DRIFT_SECONDS = 0.25;
const MAX_RENEWALS = 2;

// foobar2000 addresses a subsong as `path|subsong:N`; all subsongs share the file's resource.
const containerOf = (path: string): string => path.replace(/\|subsong:\d+$/, '');

/**
 * Keep a muted media element on foobar2000's timeline using PlaybackClock.
 * Every 100 ms, drift up to 50 ms leaves time unchanged, drift below 250 ms
 * adjusts playbackRate within 0.95–1.05, and larger drift or a seek aligns time.
 * A track change pauses the element until the next setSource call. Selecting the
 * same file again, with or without another `|subsong:N` suffix, keeps a loaded
 * resource and only moves to the new timelineOffset; another file or null replaces
 * it. Without that call the element stays paused on its last frame.
 * Network errors renew the URL at most twice per explicit setSource call.
 * The element must have no srcObject or source children. Do not change its
 * source externally while following. preload is set to 'metadata'.
 * Playback begins only after a complete host read, including at construction.
 * @experimental
 */
export class MediaElementFollower {
    private readonly clock: MediaFollowerClock;
    private readonly ownedClock: PlaybackClock | null;
    private readonly offset: number;
    private readonly unsubscribers: Array<() => void> = [];
    private readonly sourceUnsubscribers: Array<() => void> = [];
    private readonly errorListeners = new Set<(error: Error) => void>();
    private readonly interval: ReturnType<typeof setInterval>;
    private generation = 0;
    private refreshGeneration = 0;
    private playAttempt = 0;
    private path: string | null = null;
    private container: string | null = null;
    private timelineOffset = 0;
    private renewals = 0;
    private disposed = false;
    private clockReady = false;
    private metadataReady = false;
    private refreshing = true;
    private terminal = false;
    private playPending = false;
    private playBlocked = false;
    private currentError: Error | null = null;

    /**
     * Control the source, preload, mute, playback rate and transport.
     * crossOrigin is set to 'anonymous'. These element settings are not restored
     * on disposal; use one follower per element and dispose it before replacing it.
     * @throws Error if srcObject or source children are present.
     * @throws RangeError if offsetSeconds is not finite.
     */
    constructor(private readonly element: MediaElement, options: MediaElementFollowerOptions = {}) {
        this.offset = options.offsetSeconds ?? 0;
        if (!Number.isFinite(this.offset)) throw new RangeError('offsetSeconds must be finite');
        if (element.srcObject !== null) throw new Error('MediaElementFollower requires a null srcObject');
        if (element.querySelector('source') !== null) throw new Error('MediaElementFollower does not accept source children');
        this.ownedClock = options.clock ? null : new PlaybackClock();
        this.clock = options.clock ?? this.ownedClock!;
        element.crossOrigin = 'anonymous';
        element.muted = true;
        element.preload = 'metadata';
        this.unsubscribers.push(this.clock.onChange((change) => {
            if (change.reason === 'track') {
                // A loaded resource may serve the next track of the same file; a pending one is dropped.
                this.path = null;
                if (this.metadataReady) this.pause();
                else this.clearSource();
                return;
            }
            // A pending refresh blocks synchronize(), but pause/stop must still take effect now.
            if (this.refreshing && change.state !== 'playing') this.pause();
            this.synchronize(change.reason === 'seek' || change.reason === 'resync' || change.reason === 'state');
        }));
        if (typeof document !== 'undefined') {
            const page = document;
            const onVisibility = (): void => {
                if (page.visibilityState === 'visible') void this.resync().catch(() => {});
            };
            page.addEventListener('visibilitychange', onVisibility);
            this.unsubscribers.push(() => page.removeEventListener('visibilitychange', onVisibility));
        }
        this.interval = setInterval(() => this.synchronize(false), INTERVAL_MS);
        void this.clock.ready.then(() => this.onClockReady(), () => this.onClockReady());
    }

    /**
     * Latest current-source failure. setSource clears all failures; a successful
     * resync clears play() and clock-read failures. Source failures require setSource.
     */
    get error(): Error | null {
        return this.currentError;
    }

    /** Subscribe to current-source failures; the returned function unsubscribes. */
    onError(listener: (error: Error) => void): () => void {
        this.assertActive();
        this.errorListeners.add(listener);
        return () => { this.errorListeners.delete(listener); };
    }

    /**
     * Issue and assign a local container URL; null pauses and clears the source.
     * Resolves after assignment, before metadata or playback. A superseded call
     * resolves without changing the new source; a current host failure rejects.
     * The same file as the current resource, ignoring a `|subsong:N` suffix, keeps
     * that resource unless it failed, and resolves at once. A new call resets the
     * network retry budget. Decode errors and exhausted retries require an explicit
     * new call; resync alone cannot repair the source.
     * @throws Error if disposed, or ApiCallError for a host failure envelope.
     * @throws RangeError if timelineOffset is not finite.
     */
    async setSource(path: string | null, options: MediaSourceOptions = {}): Promise<void> {
        this.assertActive();
        const timelineOffset = options.timelineOffset ?? 0;
        if (!Number.isFinite(timelineOffset)) throw new RangeError('timelineOffset must be finite');
        const reuse = path !== null && !this.terminal && this.container === containerOf(path) && this.element.error === null;
        this.path = path;
        this.timelineOffset = timelineOffset;
        this.renewals = 0;
        this.playBlocked = false;
        this.currentError = null;
        if (reuse) {
            this.synchronize(true);
            return;
        }
        this.terminal = false;
        this.clearSource();
        if (path !== null) await this.issue(path, this.generation);
    }

    /**
     * Pause, read a complete host snapshot, then align and resume if playing.
     * A failed read rejects and keeps playback paused until a later resync
     * succeeds; it does not consume the source's network retry budget.
     */
    async resync(): Promise<void> {
        this.assertActive();
        const generation = this.generation;
        const refresh = ++this.refreshGeneration;
        this.refreshing = true;
        this.pause();
        try {
            await this.clock.resync({ rejectOnFailure: true });
        } catch (error) {
            if (this.isCurrent(generation) && refresh === this.refreshGeneration) this.report(error);
            throw error;
        }
        if (this.disposed || refresh !== this.refreshGeneration) return;
        this.refreshing = false;
        if (this.isCurrent(generation) && !this.terminal) {
            this.currentError = null;
            this.playBlocked = false;
        }
        this.synchronize(true);
    }

    /**
     * Pause and clear the element, release listeners and dispose an owned clock.
     * Borrowed clocks remain active. Element settings are not restored; repeated
     * disposal is harmless and late source/play results are ignored.
     */
    dispose(): void {
        if (this.disposed) return;
        this.disposed = true;
        this.path = null;
        this.clearSource();
        clearInterval(this.interval);
        for (const unsubscribe of this.unsubscribers.splice(0)) unsubscribe();
        this.errorListeners.clear();
        this.ownedClock?.dispose();
    }

    private assertActive(): void {
        if (this.disposed) throw new Error('MediaElementFollower is disposed');
    }

    private onClockReady(): void {
        if (this.disposed) return;
        // ready settles after failed host reads too; only a strict resync can release playback.
        this.clockReady = true;
        if (this.refreshGeneration === 0) void this.resync().catch(() => {});
        else this.synchronize(true);
    }

    private isCurrent(generation: number): boolean {
        return !this.disposed && generation === this.generation;
    }

    private pause(): void {
        this.playAttempt += 1;
        this.playPending = false;
        this.element.pause();
        this.element.playbackRate = 1;
    }

    private clearSource(): void {
        this.generation += 1;
        // Replacing a URL invalidates resource callbacks, not a pending clock refresh.
        // Keep refreshing set until that read finishes, or the new source could play stale time.
        this.metadataReady = false;
        this.container = null;
        for (const unsubscribe of this.sourceUnsubscribers.splice(0)) unsubscribe();
        this.pause();
        this.element.removeAttribute('src');
        this.element.load();
    }

    private async issue(path: string, generation: number): Promise<void> {
        this.container = containerOf(path);
        try {
            const response = await media.getStreamUrl(path);
            if (!this.isCurrent(generation)) return;
            const { url } = unwrap(response);
            // DOM events carry no source generation. Ignore callbacks whose assigned
            // or selected resource no longer matches this URL.
            const matches = (): boolean => this.isCurrent(generation) && this.element.src === url
                && (!this.element.currentSrc || this.element.currentSrc === url);
            const metadata = (): void => {
                if (!matches() || this.element.readyState < 1) return;
                this.metadataReady = true;
                this.synchronize(true);
            };
            const failed = (): void => {
                if (matches()) this.mediaFailed();
            };
            this.element.addEventListener('loadedmetadata', metadata);
            this.element.addEventListener('error', failed);
            this.sourceUnsubscribers.push(
                () => this.element.removeEventListener('loadedmetadata', metadata),
                () => this.element.removeEventListener('error', failed),
            );
            // Set CORS before src: assigning src may start a request immediately.
            this.element.crossOrigin = 'anonymous';
            this.element.muted = true;
            this.element.preload = 'metadata';
            this.element.src = url;
            this.element.load();
        } catch (error) {
            if (!this.isCurrent(generation)) return;
            this.fail(error);
            throw error;
        }
    }

    private mediaFailed(): void {
        if (this.terminal || !this.path) return;
        const error = this.element.error;
        if (!error) return;
        // HTMLMediaElement hides HTTP status. A network error can mean an invalidated
        // token, so renew within a fixed budget; decode errors do not justify renewal.
        if (error.code === 2 && this.renewals < MAX_RENEWALS) {
            const path = this.path;
            this.renewals += 1;
            this.clearSource();
            void this.issue(path, this.generation).catch(() => {});
            return;
        }
        this.fail(new Error(error.message || `Media playback failed (code ${error.code})`));
    }

    private synchronize(hard: boolean): void {
        if (this.disposed || this.terminal || !this.path || !this.clockReady || !this.metadataReady || this.refreshing) return;
        const rawTarget = this.timelineOffset + (this.clock.state === 'stopped' ? 0 : this.clock.position() + this.offset);
        const duration = this.element.duration;
        const target = Math.max(0, Number.isFinite(duration) ? Math.min(rawTarget, duration) : rawTarget);
        const shouldPlay = this.clock.state === 'playing' && rawTarget >= 0 && (!Number.isFinite(duration) || rawTarget < duration);
        if (!shouldPlay) this.pause();
        const drift = target - this.element.currentTime;
        try {
            if (hard || !shouldPlay || Math.abs(drift) >= LARGE_DRIFT_SECONDS) {
                this.element.currentTime = target;
                this.element.playbackRate = 1;
            } else {
                this.element.playbackRate = Math.abs(drift) <= SMALL_DRIFT_SECONDS
                    ? 1 : Math.min(1.05, Math.max(0.95, 1 + drift * 0.5));
            }
            if (shouldPlay && this.element.paused && !this.playPending && !this.playBlocked) this.play();
        } catch (error) {
            this.fail(error);
        }
    }

    private play(): void {
        const generation = this.generation;
        // pause() invalidates the attempt as well as source changes invalidating generation.
        // A late play() rejection must not block a subsequent resume of the same source.
        const attempt = ++this.playAttempt;
        this.playPending = true;
        void this.element.play().then(() => {
            if (this.isCurrent(generation) && attempt === this.playAttempt) this.playPending = false;
        }).catch((error: unknown) => {
            if (!this.isCurrent(generation) || attempt !== this.playAttempt) return;
            this.playPending = false;
            this.playBlocked = true;
            this.report(error);
        });
    }

    private fail(error: unknown): void {
        this.terminal = true;
        this.pause();
        this.report(error);
    }

    private report(error: unknown): void {
        const failure = error instanceof Error ? error : new Error(String(error));
        this.currentError = failure;
        for (const listener of [...this.errorListeners]) listener(failure);
    }
}

import { jitSession } from '../namespaces/jitSession.js';
import { ApiCallError, unwrap } from '../unwrap.js';
import { SessionEvents } from './SessionEvents.js';
import type { JitSessionClientEvent, JitSessionWaitOptions } from './SessionEvents.js';
import type {
    JitSessionAdoptNativeParams, JitSessionClaimControlParams, JitSessionGetNativeContextParams,
    JitSessionPlayNativeParams, JitSessionPlayParams, JitSessionSetNativeNextParams,
    JitSessionSetNativeQueueParams, JitSessionSetNextParams,
} from '../../types/generated/params.js';
import type { JitSessionOperation, JitSessionSnapshot } from '../../types/generated/schema-types.js';

type GuardKeys = 'sessionId' | 'controlToken' | 'expectedRevision' | 'requestId';

/** Entry to play; reuse requestId only when retrying the same command. */
export type JitSessionPlayOptions = Omit<JitSessionPlayParams, GuardKeys> & { requestId?: string };
/** Next-entry update tied to the current attempt; omit entryId to withdraw it. */
export type JitSessionNextOptions = Omit<JitSessionSetNextParams, GuardKeys> & { requestId?: string };
/** Resume or hand over a session; handover requires the observed expectedOwnerEpoch. */
export type JitSessionClaimOptions = Omit<JitSessionClaimControlParams, 'requestId'> & { requestId?: string };
/** Snapshot from prepareAdoption to commit, with the caller's entry for the track playing. */
export type JitSessionAdoptOptions = Omit<JitSessionAdoptNativeParams, GuardKeys> & { requestId?: string };
/** Adopted source row or queue occurrence to play now. */
export type JitSessionPlayNativeOptions = Omit<JitSessionPlayNativeParams, GuardKeys> & { requestId?: string };
/** Adopted source row or queue occurrence to buffer after the current attempt. */
export type JitSessionNativeNextOptions = Omit<JitSessionSetNativeNextParams, GuardKeys> & { requestId?: string };
/** Queue occurrences to keep, in the order they are given back. */
export type JitSessionNativeQueueOptions = Omit<JitSessionSetNativeQueueParams, GuardKeys> & { requestId?: string };
/** Page of an adopted or prepared native context to read. */
export type JitSessionNativeContextOptions = Omit<JitSessionGetNativeContextParams, 'sessionId'>;
export type { JitSessionClientEvent, JitSessionWaitOptions } from './SessionEvents.js';

/**
 * Keep document-bound JIT credentials, serialize writes and retain operation terminals.
 * Construct before creating or claiming a session. Dispose before unloading the page;
 * disposal removes subscriptions without stopping playback or releasing the host session.
 * Command failures reject with ApiCallError and are never automatically replayed.
 */
export class JitSessionClient {
    private readonly events: SessionEvents;
    private selectedSession?: string;
    private token?: string;
    private ownerEpoch = 0;
    private currentRevision = 0;
    private lastEventSeq = 0;
    private snapshot?: JitSessionSnapshot;
    private disposed = false;
    private commandTail: Promise<void> = Promise.resolve();
    private reconciling = false;
    private reconciliationRequested = false;
    private syncFailure?: unknown;

    constructor() {
        this.events = new SessionEvents((event) => this.observe(event));
    }

    /** Selected session, retained after release so operation results can still be queried. */
    get sessionId(): string | undefined { return this.selectedSession; }
    /** Highest state revision observed in responses, snapshots or events. */
    get revision(): number { return this.currentRevision; }
    /** Latest received snapshot, copied so callers cannot mutate client bookkeeping. */
    get state(): JitSessionSnapshot | undefined { return this.snapshot && structuredClone(this.snapshot); }
    /** Most recent automatic snapshot reconciliation failure; cleared by a successful refresh. */
    get synchronizationError(): unknown { return this.syncFailure; }

    /** Acquire a session and read its initial snapshot, without changing playback. */
    create(requestId: string = crypto.randomUUID()): Promise<JitSessionSnapshot> {
        return this.enqueue(async () => {
            const result = unwrap(await jitSession.create({ requestId }));
            this.bind(result.sessionId, result.controlToken, result.ownerEpoch, result.revision);
            return this.refresh();
        });
    }

    /** Claim an existing session and reconcile its state without restarting its attempt. */
    claimControl(options: JitSessionClaimOptions): Promise<JitSessionSnapshot> {
        return this.enqueue(async () => {
            const result = unwrap(await jitSession.claimControl({ ...options, requestId: options.requestId ?? crypto.randomUUID() }));
            this.bind(options.sessionId, result.controlToken, result.ownerEpoch, result.revision);
            return this.refresh();
        });
    }

    private bind(sessionId: string, token: string, epoch: number, revision: number): void {
        this.assertOpen();
        if (sessionId !== this.selectedSession) {
            this.currentRevision = 0;
            this.lastEventSeq = 0;
            this.snapshot = undefined;
        }
        this.selectedSession = sessionId;
        this.token = token;
        this.ownerEpoch = epoch;
        this.currentRevision = Math.max(this.currentRevision, revision);
        this.events.select(sessionId);
    }

    /** Read state and recheck active operation waits; older responses cannot overwrite a newer snapshot. */
    async refresh(): Promise<JitSessionSnapshot> {
        this.assertOpen();
        const sessionId = this.requireSession();
        const result = unwrap(await jitSession.getState({ sessionId }));
        this.assertOpen();
        if (this.selectedSession !== sessionId) throw new Error('JIT session changed');
        this.applySnapshot(result.state);
        await this.events.reconcile((operationId) => this.getOperation(operationId));
        this.assertOpen();
        if (this.selectedSession !== sessionId) throw new Error('JIT session changed');
        this.syncFailure = undefined;
        return structuredClone(this.snapshot ?? result.state);
    }

    /** Accept a new attempt; waitForOperation observes whether its execution completed. */
    play(options: JitSessionPlayOptions) {
        return this.enqueue(async () => {
            const { requestId, ...entry } = options;
            const result = unwrap(await jitSession.play({ ...entry, ...this.guard(requestId) }));
            this.acceptRevision(result.revision);
            return result;
        });
    }

    /** Modify only the next slot; afterAttemptId prevents an update targeting an old attempt. */
    setNext(options: JitSessionNextOptions) {
        return this.enqueue(async () => {
            const { requestId, ...next } = options;
            const result = unwrap(await jitSession.setNext({ ...next, ...this.guard(requestId) }));
            this.acceptRevision(result.revision);
            return result;
        });
    }

    /**
     * Skip to a prepared next entry without consuming the native queue. Rejects with
     * NO_ACTIVE_ITEM if none is ready; the current attempt and revision are unchanged.
     */
    skip(attemptId: string, requestId?: string) {
        return this.enqueue(async () => {
            const result = unwrap(await jitSession.skip({ attemptId, ...this.guard(requestId) }));
            this.acceptRevision(result.revision);
            return result;
        });
    }

    /** Stop playback while preserving the session and its queryable operation results. */
    stop(requestId?: string) {
        return this.enqueue(async () => {
            const result = unwrap(await jitSession.stop(this.guard(requestId)));
            this.acceptRevision(result.revision);
            return result;
        });
    }

    /**
     * Release the buffer and control while allowing the current resource to finish decoding. For a
     * session that adopted native playback, nativeDisposition chooses between giving the remaining
     * queue occurrences back (restore, the default) and leaving native playback as it is (detach).
     * A failed release keeps the credentials and refreshes the state, so the caller can read the
     * recovery details and retry or detach.
     */
    release(requestId?: string, nativeDisposition?: 'restore' | 'detach'): Promise<void> {
        return this.enqueue(async () => {
            const guard = this.guard(requestId);
            try {
                unwrap(await jitSession.release(nativeDisposition ? { ...guard, nativeDisposition } : guard));
            } catch (error) {
                await this.refresh().catch(() => undefined);
                throw error;
            }
            this.token = undefined;
        });
    }

    /**
     * Snapshot native playback for adoption without changing it; the returned snapshotToken
     * expires after 30 seconds.
     */
    prepareAdoption(requestId?: string) {
        return this.enqueue(async () => {
            const result = unwrap(await jitSession.prepareAdoption(this.guard(requestId)));
            this.acceptRevision(result.revision);
            return result;
        });
    }

    /** Adopt a prepared snapshot without interrupting the track playing. */
    adoptNative(options: JitSessionAdoptOptions) {
        return this.enqueue(async () => {
            const { requestId, ...adoption } = options;
            const result = unwrap(await jitSession.adoptNative({ ...adoption, ...this.guard(requestId) }));
            this.acceptRevision(result.revision);
            return result;
        });
    }

    /** Play an adopted source row or queue occurrence now; starting a queue occurrence consumes it. */
    playNative(options: JitSessionPlayNativeOptions) {
        return this.enqueue(async () => {
            const { requestId, ...entry } = options;
            const result = unwrap(await jitSession.playNative({ ...entry, ...this.guard(requestId) }));
            this.acceptRevision(result.revision);
            return result;
        });
    }

    /** Buffer an adopted source row or queue occurrence after the current attempt. */
    setNativeNext(options: JitSessionNativeNextOptions) {
        return this.enqueue(async () => {
            const { requestId, ...next } = options;
            const result = unwrap(await jitSession.setNativeNext({ ...next, ...this.guard(requestId) }));
            this.acceptRevision(result.revision);
            return result;
        });
    }

    /** Keep and reorder the queue occurrences still to be given back; the ones left out are removed. */
    setNativeQueue(options: JitSessionNativeQueueOptions) {
        return this.enqueue(async () => {
            const { requestId, ...edit } = options;
            const result = unwrap(await jitSession.setNativeQueue({ ...edit, ...this.guard(requestId) }));
            this.acceptRevision(result.revision);
            return result;
        });
    }

    /** Read one page of the native context; reading changes no revision. */
    async getNativeContext(options: JitSessionNativeContextOptions) {
        this.assertOpen();
        const sessionId = this.requireSession();
        return unwrap(await jitSession.getNativeContext({ ...options, sessionId }));
    }

    /** Query an operation; expired host records reject with NOT_FOUND. */
    async getOperation(operationId: string): Promise<JitSessionOperation> {
        this.assertOpen();
        const sessionId = this.requireSession();
        const result = unwrap(await jitSession.getOperation({ sessionId, operationId }));
        this.assertOpen();
        if (sessionId !== this.selectedSession) throw new Error('JIT session changed');
        return result.operation;
    }

    /** Resolve with a terminal, including failed or superseded; timeout or abort affects only this wait. */
    waitForOperation(operationId: string, options: JitSessionWaitOptions = {}): Promise<JitSessionOperation> {
        if (this.disposed) return Promise.reject(new Error('JIT session client is disposed'));
        if (!this.selectedSession) return Promise.reject(new Error('No JIT session is selected'));
        return this.events.wait(operationId, () => this.getOperation(operationId), options);
    }

    /** Subscribe to deduplicated session events, including terminals older than the latest snapshot. */
    subscribe(listener: (event: JitSessionClientEvent) => void): () => void {
        this.assertOpen();
        return this.events.listen(listener);
    }

    private guard(requestId: string = crypto.randomUUID()) {
        const sessionId = this.requireSession();
        if (!this.token) throw new Error('This client does not control the JIT session');
        return { sessionId, controlToken: this.token, expectedRevision: this.currentRevision, requestId };
    }

    private enqueue<T>(action: () => Promise<T>): Promise<T> {
        const result = this.commandTail.then(async () => {
            this.assertOpen();
            try { return await action(); }
            catch (error) {
                if (error instanceof ApiCallError && error.code === 'NOT_OWNER') this.token = undefined;
                throw error;
            }
        });
        this.commandTail = result.then(() => undefined, () => undefined);
        return result;
    }

    private observe(event: JitSessionClientEvent): void {
        const { payload } = event;
        const gap = payload.eventSeq > this.lastEventSeq + 1;
        this.lastEventSeq = Math.max(this.lastEventSeq, payload.eventSeq);
        this.acceptRevision(payload.revision);
        if (event.name === 'stateChanged') this.applySnapshot(event.payload.state);
        if (event.name === 'controlChanged' &&
            (event.payload.reason !== 'claimed' || event.payload.ownerEpoch > this.ownerEpoch)) this.token = undefined;
        if (gap) {
            this.reconciliationRequested = true;
            if (!this.reconciling) void this.reconcileGaps();
        }
    }

    private async reconcileGaps(): Promise<void> {
        this.reconciling = true;
        try {
            do {
                this.reconciliationRequested = false;
                const sessionId = this.selectedSession;
                try { await this.refresh(); }
                catch (error: unknown) {
                    if (!this.disposed && this.selectedSession === sessionId) this.syncFailure = error;
                }
            } while (this.reconciliationRequested && !this.disposed);
        } finally { this.reconciling = false; }
    }

    private applySnapshot(state: JitSessionSnapshot): void {
        this.acceptRevision(state.revision);
        this.lastEventSeq = Math.max(this.lastEventSeq, state.lastEventSeq);
        if (!this.snapshot || state.revision >= this.snapshot.revision) this.snapshot = structuredClone(state);
        if (state.ownerEpoch > this.ownerEpoch || state.phase === 'released') this.token = undefined;
    }

    private acceptRevision(revision: number): void { this.currentRevision = Math.max(this.currentRevision, revision); }
    private assertOpen(): void { if (this.disposed) throw new Error('JIT session client is disposed'); }
    private requireSession(): string {
        if (!this.selectedSession) throw new Error('No JIT session is selected');
        return this.selectedSession;
    }

    /** Remove listeners and reject pending waits without changing host playback. Idempotent. */
    dispose(): void {
        if (this.disposed) return;
        this.disposed = true;
        this.token = undefined;
        this.events.dispose();
    }
}

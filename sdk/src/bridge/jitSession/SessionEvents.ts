import { subscribe } from '../subscribe.js';
import type {
    JitSessionStateChangedPayload, JitSessionNeedNextPayload,
    JitSessionOperationCompletedPayload, JitSessionAttemptFinishedPayload,
    JitSessionAttemptRecheckedPayload, JitSessionControlChangedPayload,
} from '../../types/generated/events.js';
import type { JitSessionOperation } from '../../types/generated/schema-types.js';

/** Session events delivered to the current document, discriminated by name. */
export type JitSessionClientEvent =
    | { name: 'stateChanged'; payload: JitSessionStateChangedPayload }
    | { name: 'needNext'; payload: JitSessionNeedNextPayload }
    | { name: 'operationCompleted'; payload: JitSessionOperationCompletedPayload }
    | { name: 'attemptFinished'; payload: JitSessionAttemptFinishedPayload }
    | { name: 'attemptRechecked'; payload: JitSessionAttemptRecheckedPayload }
    | { name: 'controlChanged'; payload: JitSessionControlChangedPayload };

/** Bound a terminal wait without cancelling the host operation. */
export interface JitSessionWaitOptions {
    /** Milliseconds to wait; defaults to 30000. Must be positive and finite. */
    timeoutMs?: number;
    /** Aborting rejects the local wait and leaves the host operation running. */
    signal?: AbortSignal;
}

interface Waiter {
    resolve: (operation: JitSessionOperation) => void;
    reject: (error: unknown) => void;
}

/** Store document events across response races and reconcile missing operation results. */
export class SessionEvents {
    private sessionId?: string;
    private disposed = false;
    private early: JitSessionClientEvent[] = [];
    private readonly seen = new Set<number>();
    private readonly operations = new Map<string, JitSessionOperation>();
    private readonly waiters = new Map<string, Set<Waiter>>();
    private readonly listeners = new Set<(event: JitSessionClientEvent) => void>();
    private readonly unsubscribe: (() => void)[];

    constructor(private readonly observe: (event: JitSessionClientEvent) => void) {
        this.unsubscribe = [
            subscribe('jitSession:stateChanged', (payload) => this.receive({ name: 'stateChanged', payload })),
            subscribe('jitSession:needNext', (payload) => this.receive({ name: 'needNext', payload })),
            subscribe('jitSession:operationCompleted', (payload) => this.receive({ name: 'operationCompleted', payload })),
            subscribe('jitSession:attemptFinished', (payload) => this.receive({ name: 'attemptFinished', payload })),
            subscribe('jitSession:attemptRechecked', (payload) => this.receive({ name: 'attemptRechecked', payload })),
            subscribe('jitSession:controlChanged', (payload) => this.receive({ name: 'controlChanged', payload })),
        ];
    }

    /** Bind cached events to a session; selecting another rejects outstanding waits. */
    select(sessionId: string): void {
        if (this.sessionId === sessionId) return;
        this.rejectAll(new Error('JIT session changed'));
        this.sessionId = sessionId;
        this.seen.clear();
        this.operations.clear();
        const early = this.early;
        this.early = [];
        for (const event of early) this.receive(event);
    }

    /** Subscribe to deduplicated events for the selected session. */
    listen(listener: (event: JitSessionClientEvent) => void): () => void {
        if (this.disposed) throw new Error('JIT session client is disposed');
        this.listeners.add(listener);
        return () => this.listeners.delete(listener);
    }

    private receive(event: JitSessionClientEvent): void {
        if (this.disposed) return;
        if (!this.sessionId) {
            this.early.push(event);
            if (this.early.length > 256) this.early.shift();
            return;
        }
        if (event.payload.sessionId !== this.sessionId || this.seen.has(event.payload.eventSeq)) return;
        this.seen.add(event.payload.eventSeq);
        if (event.name === 'operationCompleted') this.complete(event.payload.operation);
        this.observe(event);
        for (const listener of [...this.listeners]) listener(event);
    }

    private complete(operation: JitSessionOperation): void {
        if (operation.status === 'pending') return;
        this.operations.set(operation.operationId, operation);
        if (this.operations.size > 128) {
            const oldest = this.operations.keys().next().value;
            if (oldest !== undefined) this.operations.delete(oldest);
        }
        for (const waiter of [...(this.waiters.get(operation.operationId) ?? [])]) waiter.resolve(operation);
    }

    /** Recheck active waits after a snapshot refresh; pending records keep waiting for a terminal. */
    async reconcile(query: (operationId: string) => Promise<JitSessionOperation>): Promise<void> {
        const sessionId = this.sessionId;
        if (this.disposed || !sessionId) return;
        await Promise.all([...this.waiters.keys()].map(async (operationId) => {
            const result = await query(operationId);
            if (!this.disposed && this.sessionId === sessionId) this.complete(result);
        }));
    }

    /** Subscribe before querying so a terminal delivered during the query is retained. */
    wait(operationId: string, query: () => Promise<JitSessionOperation>, options: JitSessionWaitOptions): Promise<JitSessionOperation> {
        if (this.disposed) return Promise.reject(new Error('JIT session client is disposed'));
        if (options.signal?.aborted) return Promise.reject(new DOMException('Aborted', 'AbortError'));
        const timeoutMs = options.timeoutMs ?? 30000;
        if (!Number.isFinite(timeoutMs) || timeoutMs <= 0) return Promise.reject(new RangeError('timeoutMs must be positive and finite'));
        const cached = this.operations.get(operationId);
        if (cached) return Promise.resolve(cached);
        const sessionId = this.sessionId;
        return new Promise((resolve, reject) => {
            const bucket = this.waiters.get(operationId) ?? new Set<Waiter>();
            this.waiters.set(operationId, bucket);
            let settled = false;
            const cleanup = () => {
                clearTimeout(timer);
                options.signal?.removeEventListener('abort', abort);
                bucket.delete(waiter);
                if (bucket.size === 0) this.waiters.delete(operationId);
            };
            const waiter: Waiter = {
                resolve: (result) => { if (!settled) { settled = true; cleanup(); resolve(result); } },
                reject: (error) => { if (!settled) { settled = true; cleanup(); reject(error); } },
            };
            const abort = () => waiter.reject(new DOMException('Aborted', 'AbortError'));
            const timer = setTimeout(() => waiter.reject(new Error('Timed out waiting for the JIT operation')), timeoutMs);
            bucket.add(waiter);
            options.signal?.addEventListener('abort', abort, { once: true });
            void query().then((result) => {
                if (!this.disposed && this.sessionId === sessionId) this.complete(result);
            }, (error: unknown) => waiter.reject(error));
        });
    }

    private rejectAll(error: Error): void {
        for (const bucket of [...this.waiters.values()]) {
            for (const waiter of [...bucket]) waiter.reject(error);
        }
    }

    /** Remove host subscriptions and reject waits; does not release host control. */
    dispose(): void {
        if (this.disposed) return;
        this.disposed = true;
        for (const unsubscribe of this.unsubscribe) unsubscribe();
        this.rejectAll(new Error('JIT session client is disposed'));
        this.listeners.clear();
        this.operations.clear();
        this.seen.clear();
        this.early = [];
    }
}

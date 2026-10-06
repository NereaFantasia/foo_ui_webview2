/**
 * Single listener for `chrome.webview` `sharedbufferreceived`.
 *
 * The host posts each shared buffer with an `additionalData` object whose
 * `purpose` names the feature it belongs to. A buffer and the JSON reply or
 * event that goes with it can arrive in either order, so buffers of a known
 * purpose wait here until their consumer asks for them; buffers nobody
 * claims in time, and buffers of unknown purposes, are released at once so
 * their shared memory can be reclaimed.
 */

/** The `sharedbufferreceived` event as WebView2 dispatches it. */
export interface SharedBufferReceivedEvent {
    getBuffer(): ArrayBuffer;
    /** Parsed `additionalDataAsJson`; `undefined` when the host sent none. */
    additionalData?: unknown;
}

/** The parts of `window.chrome.webview` the receiver uses. */
export interface SharedBufferHost {
    addEventListener(type: 'sharedbufferreceived', listener: (event: SharedBufferReceivedEvent) => void): void;
    removeEventListener(type: 'sharedbufferreceived', listener: (event: SharedBufferReceivedEvent) => void): void;
    /** Releases the page's view; later access to the buffer throws `TypeError`. */
    releaseBuffer(buffer: ArrayBuffer): void;
}

/** A buffer handed to a consumer, which from then on owns it and must release it. */
export interface SharedBufferDelivery {
    buffer: ArrayBuffer;
    /** The parsed `additionalData` object; read its fields with {@link stringField} or your own checks. */
    additionalData: object;
}

/** Extracts the pairing key (task or subscription id) from `additionalData`. */
export type SharedBufferKeyOf = (data: object) => string | undefined;

/** How long an unclaimed buffer of a known purpose is kept, in milliseconds. */
export const SHARED_BUFFER_HOLD_MS = 5000;

interface Held {
    delivery: SharedBufferDelivery;
    timer: ReturnType<typeof setTimeout>;
}

type Consumer = (delivery: SharedBufferDelivery) => void;

/** Read a string field of a parsed JSON object; `undefined` when absent or not a string. */
export function stringField(data: object, name: string): string | undefined {
    const value: unknown = Reflect.get(data, name);
    return typeof value === 'string' ? value : undefined;
}

function asObject(value: unknown): object | undefined {
    return typeof value === 'object' && value !== null && !Array.isArray(value) ? value : undefined;
}

/**
 * Route shared buffers to their consumers by `purpose` and key.
 *
 * Declare each purpose with {@link definePurpose}; consumers then
 * {@link listen} for their key. {@link dispose} removes the event listener
 * and releases every buffer still held.
 */
export class SharedBufferReceiver {
    private readonly purposes = new Map<string, SharedBufferKeyOf>();
    private readonly consumers = new Map<string, Consumer>();
    private readonly held = new Map<string, Held[]>();
    private readonly onEvent = (event: SharedBufferReceivedEvent): void => this.receive(event);

    /**
     * @param host - Usually `window.chrome.webview`.
     * @param holdMs - How long to keep an unclaimed buffer of a known purpose.
     */
    constructor(
        private readonly host: SharedBufferHost,
        private readonly holdMs: number = SHARED_BUFFER_HOLD_MS,
    ) {
        host.addEventListener('sharedbufferreceived', this.onEvent);
    }

    /** Accept buffers whose `additionalData.purpose` equals `purpose`, paired by `keyOf`. */
    definePurpose(purpose: string, keyOf: SharedBufferKeyOf): void {
        this.purposes.set(purpose, keyOf);
    }

    /**
     * Receive every buffer of `purpose` whose key equals `key`, including
     * any that arrived before this call and are still held.
     *
     * Only one consumer per purpose and key; a second call replaces the
     * first. Returns a function that stops the delivery; buffers arriving
     * after that are held and then released like unclaimed ones.
     */
    listen(purpose: string, key: string, consumer: Consumer): () => void {
        const id = this.slotId(purpose, key);
        this.consumers.set(id, consumer);
        const pending = this.held.get(id);
        if (pending) {
            this.held.delete(id);
            for (const entry of pending) {
                clearTimeout(entry.timer);
                consumer(entry.delivery);
            }
        }
        return () => {
            if (this.consumers.get(id) === consumer) this.consumers.delete(id);
        };
    }

    /** Stop listening and release every buffer still held. */
    dispose(): void {
        this.host.removeEventListener('sharedbufferreceived', this.onEvent);
        for (const pending of this.held.values()) {
            for (const entry of pending) {
                clearTimeout(entry.timer);
                this.release(entry.delivery.buffer);
            }
        }
        this.held.clear();
        this.consumers.clear();
    }

    private receive(event: SharedBufferReceivedEvent): void {
        const buffer = event.getBuffer();
        const data = asObject(event.additionalData);
        const purpose = data ? stringField(data, 'purpose') : undefined;
        const keyOf = purpose !== undefined ? this.purposes.get(purpose) : undefined;
        const key = data && keyOf ? keyOf(data) : undefined;
        if (!data || purpose === undefined || key === undefined) {
            this.release(buffer);
            return;
        }
        const delivery: SharedBufferDelivery = { buffer, additionalData: data };
        const id = this.slotId(purpose, key);
        const consumer = this.consumers.get(id);
        if (consumer) {
            consumer(delivery);
            return;
        }
        const entry: Held = {
            delivery,
            timer: setTimeout(() => this.expire(id, entry), this.holdMs),
        };
        const pending = this.held.get(id);
        if (pending) pending.push(entry);
        else this.held.set(id, [entry]);
    }

    private expire(id: string, entry: Held): void {
        const pending = this.held.get(id);
        if (!pending) return;
        const index = pending.indexOf(entry);
        if (index < 0) return;
        pending.splice(index, 1);
        if (pending.length === 0) this.held.delete(id);
        this.release(entry.delivery.buffer);
    }

    /**
     * Free the page's view of a buffer this receiver delivered. A buffer that
     * was already released or transferred is ignored.
     */
    release(buffer: ArrayBuffer): void {
        try {
            this.host.releaseBuffer(buffer);
        } catch {
            // Already released or transferred: nothing is left to free.
        }
    }

    private slotId(purpose: string, key: string): string {
        return `${purpose}\u0000${key}`;
    }
}

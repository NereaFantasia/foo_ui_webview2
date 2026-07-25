import process from "node:process";
import type { Readable, Writable } from "node:stream";

import type { Transport } from "@modelcontextprotocol/sdk/shared/transport.js";
import {
    JSONRPCMessageSchema,
    type JSONRPCMessage,
    type RequestId,
} from "@modelcontextprotocol/sdk/types.js";

import { findRuntimeValueIssue } from "./tool-schema.js";

/**
 * Stdio transport that validates raw JSON values before MCP SDK normalization.
 */
export class GuardedStdioServerTransport implements Transport {
    private buffer = Buffer.alloc(0);
    private started = false;

    onclose?: () => void;
    onerror?: (error: Error) => void;
    onmessage?: Transport["onmessage"];

    constructor(
        private readonly stdin: Readable = process.stdin,
        private readonly stdout: Writable = process.stdout
    ) {}

    private readonly handleData = (chunk: Buffer | string): void => {
        const bytes = typeof chunk === "string" ? Buffer.from(chunk) : chunk;
        this.buffer = Buffer.concat([this.buffer, bytes]);
        this.processBuffer();
    };

    private readonly handleError = (error: Error): void => {
        this.onerror?.(error);
    };

    private readonly handleOutputError = (error: Error): void => {
        this.onerror?.(error);
    };

    async start(): Promise<void> {
        if (this.started) {
            throw new Error("GuardedStdioServerTransport already started");
        }
        this.started = true;
        this.stdin.on("data", this.handleData);
        this.stdin.on("error", this.handleError);
        this.stdout.on("error", this.handleOutputError);
    }

    async close(): Promise<void> {
        this.stdin.off("data", this.handleData);
        this.stdin.off("error", this.handleError);
        this.stdout.off("error", this.handleOutputError);
        if (this.stdin.listenerCount("data") === 0) {
            this.stdin.pause();
        }
        this.buffer = Buffer.alloc(0);
        this.onclose?.();
    }

    send(message: JSONRPCMessage): Promise<void> {
        return new Promise((resolve, reject) => {
            let serialized: string;
            try {
                serialized = `${JSON.stringify(message)}\n`;
            } catch (error) {
                reject(toError(error));
                return;
            }
            let callbackCompleted = false;
            let waitingForDrain: boolean | undefined;
            let settled = false;
            const cleanup = (): void => {
                this.stdout.off("drain", handleDrain);
                this.stdout.off("error", handleWriteError);
            };
            const settle = (error?: Error | null): void => {
                if (settled) {
                    return;
                }
                settled = true;
                cleanup();
                if (error) {
                    reject(error);
                } else {
                    resolve();
                }
            };
            const finishWhenReady = (): void => {
                if (callbackCompleted && waitingForDrain === false) {
                    settle();
                }
            };
            const handleDrain = (): void => {
                waitingForDrain = false;
                finishWhenReady();
            };
            const handleWriteError = (error: Error): void => {
                settle(error);
            };
            try {
                const accepted = this.stdout.write(serialized, (error) => {
                    if (error) {
                        settle(error);
                        return;
                    }
                    callbackCompleted = true;
                    finishWhenReady();
                });
                waitingForDrain = !accepted;
                if (waitingForDrain) {
                    this.stdout.once("drain", handleDrain);
                    this.stdout.once("error", handleWriteError);
                }
                finishWhenReady();
            } catch (error) {
                settle(toError(error));
            }
        });
    }

    private processBuffer(): void {
        while (true) {
            const newline = this.buffer.indexOf("\n");
            if (newline < 0) {
                return;
            }
            const line = this.buffer.toString("utf8", 0, newline).replace(/\r$/, "");
            this.buffer = this.buffer.subarray(newline + 1);
            this.processLine(line);
        }
    }

    private processLine(line: string): void {
        try {
            const rawMessage: unknown = JSON.parse(line);
            const issue = findRuntimeValueIssue(rawMessage, "message");
            if (issue) {
                this.rejectUnsafeMessage(rawMessage, issue);
                return;
            }
            const parsed = JSONRPCMessageSchema.safeParse(rawMessage);
            if (!parsed.success) {
                throw parsed.error;
            }
            this.onmessage?.(parsed.data);
        } catch (error) {
            this.onerror?.(toError(error));
        }
    }

    private rejectUnsafeMessage(rawMessage: unknown, issue: string): void {
        const requestId = getValidRequestId(rawMessage);
        if (requestId === undefined) {
            this.onerror?.(new Error(`Invalid JSON-RPC value: ${issue}`));
            return;
        }
        void this.send({
            jsonrpc: "2.0",
            id: requestId,
            error: {
                code: -32602,
                message: `Invalid JSON-RPC value: ${issue}`,
            },
        }).catch((error: unknown) => {
            this.onerror?.(toError(error));
        });
    }
}

export function getValidRequestId(value: unknown): RequestId | undefined {
    if (!isPlainObject(value)) {
        return undefined;
    }
    const allowedKeys = new Set(["jsonrpc", "id", "method", "params"]);
    if (Reflect.ownKeys(value).some(
        (key) => typeof key !== "string" || !allowedKeys.has(key)
    )) {
        return undefined;
    }
    const jsonrpc = getOwnDataValue(value, "jsonrpc");
    const method = getOwnDataValue(value, "method");
    const id = getOwnDataValue(value, "id");
    if (jsonrpc !== "2.0" || typeof method !== "string") {
        return undefined;
    }
    if (typeof id === "string") {
        return id;
    }
    return typeof id === "number" && Number.isSafeInteger(id) ? id : undefined;
}

function getOwnDataValue(value: object, key: string): unknown {
    const descriptor = Object.getOwnPropertyDescriptor(value, key);
    return descriptor && "value" in descriptor ? descriptor.value : undefined;
}

function isPlainObject(value: unknown): value is Record<string, unknown> {
    if (value === null || typeof value !== "object" || Array.isArray(value)) {
        return false;
    }
    const prototype = Object.getPrototypeOf(value);
    return prototype === Object.prototype || prototype === null;
}

function toError(value: unknown): Error {
    if (value instanceof Error) {
        return value;
    }
    try {
        return new Error(JSON.stringify(value) ?? "Unknown transport error");
    } catch {
        return new Error(Object.prototype.toString.call(value));
    }
}

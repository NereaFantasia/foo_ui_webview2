import { EventEmitter } from "node:events";
import { PassThrough, Writable } from "node:stream";

import { describe, expect, it, vi } from "vitest";

import { GuardedStdioServerTransport } from "../src/guarded-stdio-transport.js";

class CaptureWritable extends Writable {
    readonly chunks: string[] = [];

    override _write(
        chunk: Buffer | string,
        _encoding: BufferEncoding,
        callback: (error?: Error | null) => void
    ): void {
        this.chunks.push(chunk.toString());
        callback();
    }

    takeMessages(): unknown[] {
        const messages = this.chunks
            .join("")
            .split("\n")
            .filter(Boolean)
            .map((line) => JSON.parse(line) as unknown);
        this.chunks.length = 0;
        return messages;
    }
}

class BackpressureWritable extends Writable {
    private pending?: (error?: Error | null) => void;

    constructor() {
        super({ highWaterMark: 1 });
    }

    override _write(
        _chunk: Buffer | string,
        _encoding: BufferEncoding,
        callback: (error?: Error | null) => void
    ): void {
        this.pending = callback;
    }

    release(error?: Error): void {
        const callback = this.pending;
        this.pending = undefined;
        callback?.(error);
    }
}

class ControlledWritable extends EventEmitter {
    private pending?: (error?: Error | null) => void;

    write(
        _chunk: Buffer | string,
        callback: (error?: Error | null) => void
    ): boolean {
        this.pending = callback;
        return false;
    }

    completeCallback(error?: Error): void {
        const callback = this.pending;
        this.pending = undefined;
        callback?.(error);
    }

    emitDrain(): void {
        this.emit("drain");
    }
}

async function flushTransport(): Promise<void> {
    await new Promise<void>((resolve) => setImmediate(resolve));
}

describe("GuardedStdioServerTransport", () => {
    async function createHarness() {
        const input = new PassThrough();
        const output = new CaptureWritable();
        const transport = new GuardedStdioServerTransport(input, output);
        transport.onmessage = vi.fn();
        transport.onerror = vi.fn();
        await transport.start();
        return { input, output, transport };
    }

    it.each([
        ["__proto__", "0"],
        ["prototype", '"request-id"'],
        ["constructor", "7"],
    ])("rejects raw request key %s with an invalid-params response", async (key, id) => {
        const { input, output, transport } = await createHarness();
        try {
            input.write(
                `{"jsonrpc":"2.0","id":${id},"method":"tools/call",`
                + `"params":{"name":"test","arguments":{"${key}":true}}}\n`
            );
            await flushTransport();

            expect(output.takeMessages()).toEqual([{
                jsonrpc: "2.0",
                id: JSON.parse(id) as unknown,
                error: {
                    code: -32602,
                    message: expect.stringMatching(new RegExp(`${key}.*prototype-sensitive`, "i")),
                },
            }]);
            expect(transport.onmessage).not.toHaveBeenCalled();
        } finally {
            await transport.close();
        }
    });

    it.each([
        ["notification", '{"jsonrpc":"2.0","method":"notice","params":{"constructor":true}}'],
        ["result response", '{"jsonrpc":"2.0","id":1,"result":{"__proto__":true}}'],
        ["error response", '{"jsonrpc":"2.0","id":1,"error":{"code":-1,"message":"bad","data":{"prototype":true}}}'],
        ["request/result hybrid", '{"jsonrpc":"2.0","id":1,"method":"tools/call","result":{},"params":{"constructor":true}}'],
        ["request/error hybrid", '{"jsonrpc":"2.0","id":1,"method":"tools/call","error":{"code":-1,"message":"bad"},"params":{"constructor":true}}'],
        ["null-id request", '{"jsonrpc":"2.0","id":null,"method":"tools/call","params":{"constructor":true}}'],
    ])("reports an unsafe %s without sending a response", async (_label, line) => {
        const { input, output, transport } = await createHarness();
        try {
            input.write(`${line}\n`);
            await flushTransport();

            expect(output.takeMessages()).toEqual([]);
            expect(transport.onmessage).not.toHaveBeenCalled();
            expect(transport.onerror).toHaveBeenCalledWith(
                expect.objectContaining({
                    message: expect.stringMatching(/prototype-sensitive/i),
                })
            );
        } finally {
            await transport.close();
        }
    });

    it.each([
        "9007199254740992",
        "1e100",
    ])("does not respond to an unsafe request with invalid numeric id %s", async (id) => {
        const { input, output, transport } = await createHarness();
        try {
            input.write(
                `{"jsonrpc":"2.0","id":${id},"method":"tools/call",`
                + '"params":{"constructor":true}}\n'
            );
            await flushTransport();

            expect(output.takeMessages()).toEqual([]);
            expect(transport.onmessage).not.toHaveBeenCalled();
            expect(transport.onerror).toHaveBeenCalledWith(
                expect.objectContaining({
                    message: expect.stringMatching(/prototype-sensitive/i),
                })
            );
        } finally {
            await transport.close();
        }
    });

    it("forwards a valid raw JSON-RPC message after SDK schema validation", async () => {
        const { input, output, transport } = await createHarness();
        try {
            input.write('{"jsonrpc":"2.0","id":0,"method":"ping"}\n');
            await flushTransport();

            expect(output.takeMessages()).toEqual([]);
            expect(transport.onerror).not.toHaveBeenCalled();
            expect(transport.onmessage).toHaveBeenCalledWith({
                jsonrpc: "2.0",
                id: 0,
                method: "ping",
            });
        } finally {
            await transport.close();
        }
    });

    it("frames partial, CRLF, and multiple messages without loss", async () => {
        const { input, transport } = await createHarness();
        try {
            input.write('{"jsonrpc":"2.0","id":1,"method":"pi');
            expect(transport.onmessage).not.toHaveBeenCalled();

            input.write('ng"}\r\n{"jsonrpc":"2.0","id":2,"method":"ping"}\n');
            await flushTransport();

            expect(transport.onmessage).toHaveBeenNthCalledWith(1, {
                jsonrpc: "2.0",
                id: 1,
                method: "ping",
            });
            expect(transport.onmessage).toHaveBeenNthCalledWith(2, {
                jsonrpc: "2.0",
                id: 2,
                method: "ping",
            });
        } finally {
            await transport.close();
        }
    });

    it("reports malformed JSON and SDK schema errors without forwarding", async () => {
        const { input, transport } = await createHarness();
        try {
            input.write('{not-json}\n{"jsonrpc":"2.0","id":1}\n');
            await flushTransport();

            expect(transport.onmessage).not.toHaveBeenCalled();
            expect(transport.onerror).toHaveBeenCalledTimes(2);
        } finally {
            await transport.close();
        }
    });

    it("rejects a second start and removes listeners on close", async () => {
        const input = new PassThrough();
        const output = new CaptureWritable();
        const transport = new GuardedStdioServerTransport(input, output);
        transport.onclose = vi.fn();
        await transport.start();

        await expect(transport.start()).rejects.toThrow(/already started/i);
        expect(input.listenerCount("data")).toBe(1);
        expect(input.listenerCount("error")).toBe(1);
        expect(output.listenerCount("error")).toBe(1);

        await transport.close();

        expect(input.listenerCount("data")).toBe(0);
        expect(input.listenerCount("error")).toBe(0);
        expect(output.listenerCount("error")).toBe(0);
        expect(input.isPaused()).toBe(true);
        expect(transport.onclose).toHaveBeenCalledOnce();
    });

    it("waits for backpressure release before send resolves", async () => {
        const input = new PassThrough();
        const output = new BackpressureWritable();
        const transport = new GuardedStdioServerTransport(input, output);
        let settled = false;
        const pending = transport.send({
            jsonrpc: "2.0",
            id: 1,
            result: {},
        }).then(() => {
            settled = true;
        });

        await flushTransport();
        expect(settled).toBe(false);
        output.release();
        await pending;
        expect(settled).toBe(true);
    });

    it("waits for drain after the write callback completes", async () => {
        const input = new PassThrough();
        const output = new ControlledWritable();
        const transport = new GuardedStdioServerTransport(
            input,
            output as unknown as Writable
        );
        let settled = false;
        const pending = transport.send({
            jsonrpc: "2.0",
            id: 1,
            result: {},
        }).then(() => {
            settled = true;
        });

        output.completeCallback();
        await flushTransport();
        expect(settled).toBe(false);

        output.emitDrain();
        await pending;
        expect(settled).toBe(true);
    });

    it("rejects send on write callback errors", async () => {
        const input = new PassThrough();
        const output = new BackpressureWritable();
        const transport = new GuardedStdioServerTransport(input, output);
        transport.onerror = vi.fn();
        await transport.start();
        const pending = transport.send({
            jsonrpc: "2.0",
            id: 1,
            result: {},
        });

        output.release(new Error("write failed"));
        await expect(pending).rejects.toThrow(/write failed/i);
        await flushTransport();
        expect(transport.onerror).toHaveBeenCalledWith(
            expect.objectContaining({ message: "write failed" })
        );
        await transport.close();
    });

    it("rejects send when a message cannot be serialized", async () => {
        const input = new PassThrough();
        const output = new CaptureWritable();
        const transport = new GuardedStdioServerTransport(input, output);
        const circular: Record<string, unknown> = {
            jsonrpc: "2.0",
            id: 1,
            result: {},
        };
        circular.self = circular;

        await expect(transport.send(circular as never)).rejects.toThrow();
    });
});

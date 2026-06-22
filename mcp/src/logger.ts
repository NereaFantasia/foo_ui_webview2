/**
 * Structured logger (JSON lines to stderr)
 *
 * MCP uses stdout for protocol messages, so all logging goes to stderr.
 */

type LogLevel = "info" | "warn" | "error" | "debug";

interface LogEntry {
    ts: string;
    level: LogLevel;
    msg: string;
    [key: string]: unknown;
}

function emit(level: LogLevel, msg: string, data?: Record<string, unknown>): void {
    const entry: LogEntry = {
        ts: new Date().toISOString(),
        level,
        msg,
        ...data,
    };
    process.stderr.write(JSON.stringify(entry) + "\n");
}

export const logger = {
    info: (msg: string, data?: Record<string, unknown>) => emit("info", msg, data),
    warn: (msg: string, data?: Record<string, unknown>) => emit("warn", msg, data),
    error: (msg: string, data?: Record<string, unknown>) => emit("error", msg, data),
    debug: (msg: string, data?: Record<string, unknown>) => emit("debug", msg, data),
};

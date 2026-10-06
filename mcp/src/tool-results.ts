/**
 * Tool results shared by the bridge and page tools: text, errors, JSON and
 * pictures, with long text cut to a size the client's context can take.
 */

import type { CallToolResult } from "@modelcontextprotocol/sdk/types.js";

/** Longest text result by default, in characters (roughly 25,000 tokens). */
export const DEFAULT_MAX_RESPONSE_CHARS = 100_000;

/**
 * Largest picture sent as an image by default, in bytes: 3.75 MiB, which
 * base64-encodes to the 5 MiB the Claude API accepts for one image.
 */
export const DEFAULT_MAX_IMAGE_BYTES = 3_932_160;

/** Size limits of one tool result. */
export interface ResultLimits {
    /** Longest text, in characters; a longer one is cut. */
    maxResponseChars?: number;
    /** Largest picture sent as an image, in decoded bytes; a larger one is left out. */
    maxImageBytes?: number;
}

// The picture types MCP clients commonly display; other types are left out of the result.
const IMAGE_TYPES = new Set(["image/png", "image/jpeg", "image/gif", "image/webp"]);

/**
 * A text result. Text longer than `maxChars` is cut there and ends with a
 * note that says how long it was and how to ask for less.
 */
export function textResult(text: string, maxChars = DEFAULT_MAX_RESPONSE_CHARS): CallToolResult {
    return { content: [{ type: "text", text: cut(text, maxChars) }] };
}

function cut(text: string, maxChars: number): string {
    if (text.length <= maxChars) return text;
    const note =
        `\n[Truncated: the result has ${text.length} characters and this server returns at most ${maxChars}. ` +
        "Ask for less, for example a smaller page (offset/limit, start/count) or fewer fields.]";
    return text.slice(0, maxChars) + note;
}

/** A tool execution error, which the client passes to the model so it can correct the call. */
export function errorResult(text: string): CallToolResult {
    return { content: [{ type: "text", text }], isError: true };
}

/** A value as compact JSON text; `undefined` becomes `null`. */
export function jsonResult(data: unknown, maxChars = DEFAULT_MAX_RESPONSE_CHARS): CallToolResult {
    return textResult(JSON.stringify(data ?? null), maxChars);
}

/**
 * A result whose `field` holds a picture as a `data:<mime>;base64,` URL: the
 * other fields as compact JSON text, followed by the picture as an image.
 * Without such a URL in `field`, for example when no picture was found, the
 * whole value is JSON text. A picture of a type outside PNG, JPEG, GIF and
 * WebP, or larger than `maxImageBytes`, is left out, and the text says why;
 * its base64 never goes into the text.
 */
export function pictureResult(data: unknown, field: string, limits: ResultLimits = {}): CallToolResult {
    const maxChars = limits.maxResponseChars ?? DEFAULT_MAX_RESPONSE_CHARS;
    const value = isRecord(data) ? data[field] : undefined;
    const picture = typeof value === "string" ? parseDataUrl(value) : undefined;
    if (!isRecord(data) || !picture) return jsonResult(data, maxChars);

    const text = JSON.stringify(Object.fromEntries(Object.entries(data).filter(([key]) => key !== field)));
    const maxBytes = limits.maxImageBytes ?? DEFAULT_MAX_IMAGE_BYTES;
    let skipped: string | undefined;
    if (!IMAGE_TYPES.has(picture.mimeType)) {
        skipped = `it is ${picture.mimeType}, and this server attaches only PNG, JPEG, GIF and WebP pictures`;
    } else if (picture.bytes > maxBytes) {
        skipped = `it has ${picture.bytes} bytes, more than the ${maxBytes} this server attaches (FB2K_MAX_IMAGE_BYTES)`;
    }
    if (skipped) return textResult(`${text}\n[Picture not attached: ${skipped}.]`, maxChars);
    return {
        content: [
            { type: "text", text: cut(text, maxChars) },
            { type: "image", data: picture.data, mimeType: picture.mimeType },
        ],
    };
}

/** The message of a thrown value. */
export function errorMessage(err: unknown): string {
    return err instanceof Error ? err.message : String(err);
}

function isRecord(value: unknown): value is Record<string, unknown> {
    return value !== null && typeof value === "object" && !Array.isArray(value);
}

/**
 * The MIME type, base64 payload and decoded size of a base64 data URL, or
 * `undefined` when the value is not one or its payload is not valid base64,
 * which a client would refuse as image data.
 */
function parseDataUrl(value: string): { mimeType: string; data: string; bytes: number } | undefined {
    const header = /^data:([\w.+-]+\/[\w.+-]+);base64,/i.exec(value.slice(0, 128));
    if (!header) return undefined;
    const data = value.slice(header[0].length);
    if (data.length % 4 !== 0 || !/^[A-Za-z0-9+/]*={0,2}$/.test(data)) return undefined;
    const padding = data.endsWith("==") ? 2 : data.endsWith("=") ? 1 : 0;
    return { mimeType: header[1].toLowerCase(), data, bytes: (data.length / 4) * 3 - padding };
}

/**
 * Result formatting helpers for bridge and page tools (text, errors, JSON, and images)
 * with output size clamping.
 */

import type { CallToolResult } from "@modelcontextprotocol/sdk/types.js";

/** Default maximum response length in characters (roughly 25,000 tokens). */
export const DEFAULT_MAX_RESPONSE_CHARS = 100_000;

/** Default maximum decoded image size in bytes (3.75 MiB, base64-encoded to ~5 MiB). */
export const DEFAULT_MAX_IMAGE_BYTES = 3_932_160;

/** Size thresholds applied to tool execution results. */
export interface ResultLimits {
    /** Maximum character count for text results before truncation. */
    maxResponseChars?: number;
    /** Maximum decoded byte size for images before omission. */
    maxImageBytes?: number;
}

// MIME types supported by common MCP image viewers; others are omitted from the result.
const IMAGE_TYPES = new Set(["image/png", "image/jpeg", "image/gif", "image/webp"]);

/**
 * Formats a plain text result. Strings exceeding `maxChars` are truncated
 * with an explanatory note indicating the original length and paging suggestions.
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

/** Formats an error result, returned to the model with `isError: true` for corrective action. */
export function errorResult(text: string): CallToolResult {
    return { content: [{ type: "text", text }], isError: true };
}

/** Formats data as compact JSON text, mapping `undefined` to `null`. */
export function jsonResult(data: unknown, maxChars = DEFAULT_MAX_RESPONSE_CHARS): CallToolResult {
    return textResult(JSON.stringify(data ?? null), maxChars);
}

/**
 * Formats a result containing an embedded image URL in `field`.
 *
 * If `field` contains a valid `data:<mime>;base64,` URL within supported formats
 * (PNG, JPEG, GIF, WebP) and size limits, the image is extracted into an MCP image
 * block, and remaining fields are returned as compact JSON text.
 *
 * If `field` is missing or invalid, the entire object is returned as JSON text.
 * If the image exceeds `maxImageBytes` or uses an unsupported format, the image
 * is omitted with an explanatory note appended to the JSON text.
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

/** Extracts an error message from a thrown value. */
export function errorMessage(err: unknown): string {
    return err instanceof Error ? err.message : String(err);
}

function isRecord(value: unknown): value is Record<string, unknown> {
    return value !== null && typeof value === "object" && !Array.isArray(value);
}

/**
 * Parses a base64 data URL into its MIME type, payload, and decoded byte count.
 * Returns `undefined` if the format or base64 payload is invalid.
 */
function parseDataUrl(value: string): { mimeType: string; data: string; bytes: number } | undefined {
    const header = /^data:([\w.+-]+\/[\w.+-]+);base64,/i.exec(value.slice(0, 128));
    if (!header) return undefined;
    const data = value.slice(header[0].length);
    if (data.length % 4 !== 0 || !/^[A-Za-z0-9+/]*={0,2}$/.test(data)) return undefined;
    const padding = data.endsWith("==") ? 2 : data.endsWith("=") ? 1 : 0;
    return { mimeType: header[1].toLowerCase(), data, bytes: (data.length / 4) * 3 - padding };
}

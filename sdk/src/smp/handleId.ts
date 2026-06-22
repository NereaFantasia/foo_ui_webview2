/**
 * Handle-id helpers shared across the SMP wrappers.
 *
 * `foo_ui_webview2` selection / metadb APIs identify a subsong-bearing
 * track with the Spider Monkey Panel convention `path|subsong:N`. The
 * functions below round-trip between the two representations and
 * tolerate every variant the SMP runtime accepts.
 */

import type { SmpHandleId } from './types.js';

/** Suffix marker that separates a path from its sub-song index. */
export const HANDLE_TOKEN = '|subsong:';

/**
 * Build a canonical handle-id from a `path` + optional `subsong` index.
 *
 * - Returns `''` for falsy / non-string `path`.
 * - Subsong `0` (or non-numeric) collapses to the bare path; only
 *   strictly positive sub-song indices receive the `|subsong:N` tail.
 *
 * @example
 *   formatHandleId('C:\\song.flac', 2) === 'C:\\song.flac|subsong:2'
 *   formatHandleId('C:\\song.flac')    === 'C:\\song.flac'
 *   formatHandleId('')                 === ''
 */
export function formatHandleId(path: string, subsong: number = 0): string {
    if (!path) return '';
    const s = Number(subsong) || 0;
    return s > 0 ? `${path}${HANDLE_TOKEN}${s}` : path;
}

/**
 * Parse a handle-id back into its `{ path, subsong, id }` parts.
 *
 * - Non-string input returns the canonical empty result.
 * - Inputs without the `|subsong:` token are treated as plain paths.
 * - The returned `id` is always
 *   {@link formatHandleId}(path, subsong), so callers can use it as a
 *   round-trip-stable cache key.
 */
export function parseHandleId(handleId: unknown): SmpHandleId {
    if (typeof handleId !== 'string') {
        return { path: '', subsong: 0, id: '' };
    }
    const pos = handleId.lastIndexOf(HANDLE_TOKEN);
    if (pos === -1) {
        return { path: handleId, subsong: 0, id: handleId };
    }
    const path = handleId.slice(0, pos);
    const subsongStr = handleId.slice(pos + HANDLE_TOKEN.length);
    const subsong = Number.parseInt(subsongStr, 10);
    const sub = Number.isFinite(subsong) ? subsong : 0;
    return { path, subsong: sub, id: formatHandleId(path, sub) };
}

/**
 * Strip the `|subsong:N` suffix from a handle-id (no-op on plain paths).
 *
 * Convenience helper used by `selection.set` / `metadata.read`-style
 * APIs that only accept a raw filesystem path.
 */
export function stripSubsongSuffix(pathOrHandleId: unknown): string {
    return parseHandleId(pathOrHandleId).path;
}

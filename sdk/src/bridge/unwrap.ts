import type { ApiErrorCode, ApiFailure } from '../types/responses.js';
import type { JsonValue } from '../types/json.js';

/** The success branch of a response union such as `XxxSuccess | ApiFailure`. */
type SuccessOf<R> = Extract<R, { success: true }>;

/**
 * What {@link unwrap} throws for a failure envelope. `message` is the
 * envelope's `error`; `code` and `details` are copied from it, so a `catch`
 * block can branch on the code as it would on the envelope.
 */
export class ApiCallError extends Error {
    /** The failure's `code`, such as `LOCKED` or `NOT_SUPPORTED`. */
    readonly code: ApiErrorCode;
    /** The failure's `details`, when it has any. */
    readonly details?: JsonValue;

    constructor(failure: ApiFailure) {
        super(failure.error);
        this.name = 'ApiCallError';
        this.code = failure.code;
        if (failure.details !== undefined) this.details = failure.details;
    }
}

/**
 * Turn a response into its success branch, throwing when the call failed.
 *
 * Namespace methods resolve with the envelope and do not throw for a failure
 * they report (a call the host rejects outright, such as an unknown method,
 * still rejects). `unwrap` is for code that prefers an exception to checking
 * `success` at every call site. Pass a key to get one field of the success
 * branch instead.
 *
 * @throws {@link ApiCallError} when `success` is `false`, including the
 *         `NOT_SUPPORTED` failure a call answers with when no host is present.
 *
 * @example
 *   const playlists = unwrap(await fb.playlist.getAll(), 'playlists');
 *
 *   try {
 *       unwrap(await fb.library.addToPlaylist(paths));
 *   } catch (e) {
 *       if (e instanceof ApiCallError && e.code === 'LOCKED') showLockedNotice();
 *   }
 */
export function unwrap<R extends { success: boolean }>(response: R): SuccessOf<R>;
export function unwrap<R extends { success: boolean }, K extends keyof SuccessOf<R>>(
    response: R,
    key: K,
): SuccessOf<R>[K];
export function unwrap<R extends { success: boolean }, K extends keyof SuccessOf<R>>(
    response: R,
    key?: K,
): SuccessOf<R> | SuccessOf<R>[K] {
    if (response.success !== true) {
        // Every declared response is `XxxSuccess | ApiFailure`; a value with
        // success other than true is the failure branch.
        throw new ApiCallError(response as unknown as ApiFailure);
    }
    const success = response as SuccessOf<R>;
    return key === undefined ? success : success[key];
}

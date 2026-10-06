/**
 * `rating` — track rating namespace (0-5 integer scale).
 */

import { bridge } from '../Bridge.js';
import type { BaseResponse } from '../../types/responses.js';

export const rating = {
    /**
     * Resolve the 0-5 integer rating stored for the track at `path`.
     *
     * Uses `%rating%` when its `atoi` conversion yields 1 to 5; otherwise
     * reads the file's `RATING` tag and clamps it to 0-5. `0` means unrated.
     * `storage` is `'stats'` for an accepted `%rating%` value and `'file'`
     * otherwise, including when neither source provides a rating.
     *
     * @param path - Absolute file path. May carry a `|subsong:N`
     *               suffix; the host extracts the CUE index.
     */
    get: (path: string) =>
        bridge.invoke<{
            rating: number;
            /** `'stats'` for a valid statistics rating; `'file'` for tag fallback or no rating. */
            storage?: string;
        }>('rating.get', { path }),
    /**
     * Set the 0-5 integer rating for the track at `path`.
     *
     * @param path - Absolute file path. May carry a `|subsong:N`
     *               suffix for CUE entries; the host extracts the
     *               index automatically.
     * @param rating - Integer in `[0, 5]`. `0` clears the rating.
     * @param opts.cueIndex - Explicit CUE subsong index. Takes
     *                        precedence over any `|subsong:N` suffix
     *                        in `path`. Useful when the caller tracks
     *                        subsong indices independently from the
     *                        path string.
     */
    set: (
        path: string,
        rating: number,
        opts?: { cueIndex?: number },
    ) =>
        bridge.invoke<
            BaseResponse & {
                /** Menu hop the rating took (`'foo_playcount'` etc.). */
                menuPath?: string;
                /** Write destination: `'stats'` (foo_playcount) or `'file'` (the `RATING` tag). */
                storage?: string;
                /** Free-form remediation hint when storage fell back. */
                note?: string;
            }
        >('rating.set', {
            path,
            rating,
            ...(opts?.cueIndex != null ? { cueIndex: opts.cueIndex } : {}),
        }),
};

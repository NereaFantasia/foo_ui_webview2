/**
 * `rating` — track rating namespace (0-5 integer scale).
 */

import { bridge } from '../Bridge.js';
import type { BaseResponse } from '../../types/responses.js';

export const rating = {
    /**
     * Resolve the 0-5 integer rating stored for the track at `path`.
     *
     * @param path - Absolute file path. May carry a `|subsong:N`
     *               suffix; the host extracts the CUE index.
     */
    get: (path: string) =>
        bridge.invoke<{ rating: number }>('rating.get', { path }),
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
                /** Storage backend used (`'foo_playcount'` / `'tag'`). */
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

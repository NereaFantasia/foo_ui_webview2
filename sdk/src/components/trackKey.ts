/**
 * Track key shared by components that address one track in host methods.
 */

/** Identity fields of a `Track` row or a `playback:trackChanged` payload. */
export interface TrackIdentity {
    handle?: string;
    path?: string;
    absolutePath?: string;
    subsong?: number;
}

const SUBSONG_MARKER = '|subsong:';

/**
 * Return the key that selects exactly this track in host methods that accept
 * a `|subsong:N` suffix, so a CUE sheet entry resolves to itself rather than
 * to the first subsong of its file.
 *
 * Uses `handle` when present. Otherwise joins `path` (falling back to
 * `absolutePath`) with `subsong` the way the host builds `handle`: the suffix
 * is added only for a positive integer `subsong` and a non-empty path.
 * Returns `''` when there is no track or no path.
 */
export function trackKeyOf(track: TrackIdentity | null | undefined): string {
    if (!track) return '';
    if (typeof track.handle === 'string' && track.handle) return track.handle;
    const base = track.path || track.absolutePath || '';
    const subsong = track.subsong;
    if (!base || typeof subsong !== 'number' || !Number.isInteger(subsong) || subsong <= 0) {
        return base;
    }
    return `${base}${SUBSONG_MARKER}${subsong}`;
}

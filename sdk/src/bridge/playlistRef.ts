/**
 * A playlist, named by its index or by the `guid` that `playlist.getAll`, `playlist.create` and
 * the other playlist answers report. A GUID keeps naming the same playlist while others are
 * added, removed or reordered, and one no playlist has fails with `NOT_FOUND` instead of
 * reaching another playlist. An index names whichever playlist is at that position when the
 * host handles the call.
 */
export type PlaylistRef = number | string;

/**
 * The request key that carries a {@link PlaylistRef}: `playlistGuid` for a string, `playlist`
 * for a number.
 */
export function playlistTarget(ref: PlaylistRef): { playlist: number } | { playlistGuid: string } {
    return typeof ref === 'string' ? { playlistGuid: ref } : { playlist: ref };
}

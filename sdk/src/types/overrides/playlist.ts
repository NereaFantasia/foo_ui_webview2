/**
 * Parameters for grouping adjacent tracks in the `playlist.*` namespace.
 */

/**
 * Parameters for `playlist.getGroupRuns`.
 *
 * Group the selected playlist without reordering its tracks. `patterns`
 * must contain one or two non-empty Title Formatting expressions; the
 * response contains run boundaries rather than track rows.
 *
 * @codegen-override params:playlist.getGroupRuns
 * @codegen-snapshot index:primitive,playlist:primitive
 */
export interface PlaylistGetGroupRunsParams {
    /**
     * One or two Title Formatting expressions. The second one, when present,
     * sub-groups within each run. A non-array, an empty array, more than two
     * entries, a non-string element, an empty string or an expression that
     * fails to compile all resolve with
     * `{ success: false, code: 'INVALID_PARAMS' }`; when a single expression is
     * at fault, `details.pattern` carries its index.
     */
    patterns: string[];
    /**
     * Playlist to group. Defaults to the active playlist. An index past the
     * last playlist resolves with `INVALID_PARAMS`; `-1` selects the active
     * playlist, like an omitted value.
     */
    playlist?: number;
    /** Alias for {@link PlaylistGetGroupRunsParams.playlist}. */
    index?: number;
}

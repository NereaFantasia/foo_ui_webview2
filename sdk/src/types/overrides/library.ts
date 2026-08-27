/**
 * Hand-written overrides for the `library.*` namespace: the `fields`
 * projection list on the two query endpoints.
 */

/**
 * Parameters for `library.query`.
 *
 * `fields` reaches the C++ handler through a shared selection parser instead
 * of a direct `params.value` read, so the extractor can only record it as an
 * existence check and the generated type would widen to `unknown[]`. The
 * handler accepts nothing but an array of whitelisted track key names.
 *
 * @codegen-override params:library.query
 * @codegen-snapshot fields:unknown,limit:primitive,query:primitive,sort:primitive
 */
export interface LibraryQueryParams {
    /**
     * foobar2000 query expression. An empty value resolves with
     * `{ success: false, error: 'query is required' }`.
     * @default ""
     */
    query?: string;
    /**
     * Maximum number of rows returned. Truncation happens after `sort`, so
     * `total` still reports the untruncated hit count.
     * @default 100
     */
    limit?: number;
    /**
     * Title Formatting expression to sort the hits by. An empty value keeps
     * library order; an expression that fails to compile is ignored rather
     * than reported. Unlike `library.search` the query expression itself may
     * not carry `SORT BY` — the host rejects that as invalid syntax.
     * @default ""
     */
    sort?: string;
    /**
     * Track keys to project. Every returned row then holds exactly these keys
     * and nothing else; omit the parameter to receive all 20. Accepted names,
     * matched case-sensitively: `index`, `title`, `artist`, `artists`,
     * `album`, `albumArtist`, `genre`, `date`, `trackNumber`, `discNumber`,
     * `duration`, `path`, `absolutePath`, `fileSize`, `bitrate`,
     * `sampleRate`, `channels`, `codec`, `subsong`, `rating`. A non-array, an
     * empty array, a non-string element or an unknown name resolves with
     * `{ success: false, code: 'INVALID_PARAMS' }` and lists the offending
     * names under `details.unknownFields`.
     */
    fields?: string[];
}

/**
 * Parameters for `library.search`.
 *
 * Same `fields` contract and same extractor blind spot as
 * {@link LibraryQueryParams}; the two differ in paging and in the response
 * envelope, not in the projection.
 *
 * @codegen-override params:library.search
 * @codegen-snapshot fields:unknown,limit:primitive,offset:primitive,query:primitive
 */
export interface LibrarySearchParams {
    /**
     * foobar2000 query expression; may carry `SORT BY`. An empty value
     * resolves with an empty but successful result set.
     * @default ""
     */
    query?: string;
    /**
     * Index of the first row of the page. `total` counts every hit, not just
     * the page, and an out-of-range offset yields an empty page.
     * @default 0
     */
    offset?: number;
    /**
     * Page size.
     * @default 100
     */
    limit?: number;
    /**
     * Track keys to project, from the same case-sensitive 20-name whitelist
     * as {@link LibraryQueryParams.fields}. Every returned row of `tracks`
     * then holds exactly these keys and nothing else; omit the parameter to
     * receive all 20. A non-array, an
     * empty array, a non-string element or an unknown name resolves with
     * `{ success: false, code: 'INVALID_PARAMS' }` and lists the offending
     * names under `details.unknownFields`.
     */
    fields?: string[];
}

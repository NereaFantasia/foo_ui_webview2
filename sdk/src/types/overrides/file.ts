/**
 * Hand-written overrides for the `file.*` namespace.
 */

/**
 * One item of the batch passed to `file.copyAsync` / `file.moveAsync`.
 *
 * Both paths may use `%variable%` placeholders and are echoed back verbatim,
 * unexpanded, in the `file:opProgress` results.
 */
export interface FileOpEntry {
    source: string;
    /**
     * Target path. When it names an existing directory and `source` is a file,
     * the file is placed inside it under its own name.
     */
    destination: string;
}

/**
 * Parameters for `file.copyAsync`.
 *
 * The C++ handler only reaches `items` through `params.contains("items")` and
 * reads `overwrite` through a shared boolean helper, so the extractor records
 * the first as `unknown` and misses the second entirely. `FileCopyAsync` then
 * requires `items` to be present *and* a non-empty array of objects, each
 * carrying a string `source` and `destination`; a missing key, a non-string
 * value, a non-object element and an empty array are all rejected with
 * `INVALID_PARAMS` - hence a required array of objects.
 *
 * Path validation runs before the handler and rejects the whole call on the
 * first bad entry: `source` is checked as `Read`, `destination` as
 * `FileWrite`, and one denied path fails the batch with `PERMISSION_DENIED`
 * without producing an `operationId`. An empty string lands on
 * `PERMISSION_DENIED` rather than `INVALID_PARAMS`, because that check runs
 * ahead of the handler. A partial batch is never dispatched.
 *
 * @codegen-override params:file.copyAsync
 * @codegen-snapshot items:unknown
 */
export interface FileCopyAsyncParams {
    /** Source/destination pairs to copy; must not be empty. */
    items: FileOpEntry[];
    /**
     * Replace an existing destination instead of reporting the entry as
     * `skipped` / `already-exists`. Defaults to `false`. A non-boolean value
     * is rejected with `INVALID_PARAMS`.
     */
    overwrite?: boolean;
}

/**
 * Parameters for `file.moveAsync`.
 *
 * Same shape and same all-or-nothing validation as {@link FileCopyAsyncParams},
 * except that both ends are checked as `FileWrite` because a move deletes the
 * source.
 *
 * @codegen-override params:file.moveAsync
 * @codegen-snapshot items:unknown
 */
export interface FileMoveAsyncParams {
    /** Source/destination pairs to move; must not be empty. */
    items: FileOpEntry[];
    /**
     * Replace an existing destination instead of reporting the entry as
     * `skipped` / `already-exists`. Defaults to `false`, which differs from
     * the synchronous `file.move` - that one always replaces file
     * destinations.
     */
    overwrite?: boolean;
}

/**
 * Parameters for `file.deleteAsync`.
 *
 * `paths` reaches the C++ handler through `params.contains("paths")`, so the
 * extractor records it as `unknown`; `moveToTrash` is read through the same
 * boolean helper as the `overwrite` flags above and is invisible to it.
 * `FileDeleteAsync` requires a non-empty array of strings and rejects a
 * missing key, a non-array value, an empty array and a non-string entry with
 * `INVALID_PARAMS`.
 *
 * Every path is checked as `FileWrite` before the handler runs, fail-fast: one
 * denied path fails the whole batch with `PERMISSION_DENIED` and no
 * `operationId` is produced. An empty string lands there too rather than on
 * `INVALID_PARAMS`, because that check runs ahead of the handler.
 *
 * @codegen-override params:file.deleteAsync
 * @codegen-snapshot paths:unknown
 */
export interface FileDeleteAsyncParams {
    /** Paths to delete; must not be empty. */
    paths: string[];
    /**
     * Send each entry to the Recycle Bin instead of deleting it permanently.
     * Defaults to `true`. Either way non-empty directories are removed. A
     * non-boolean value is rejected with `INVALID_PARAMS`.
     */
    moveToTrash?: boolean;
}

/**
 * Parameters for `file.cancelOp`.
 *
 * The handler requires the key to be present, a string, and non-empty,
 * rejecting anything else with `INVALID_PARAMS`, so the generated optional
 * marker understates the contract. Unlike the batch endpoints this one carries
 * no path parameter and therefore no permission check.
 *
 * @codegen-override params:file.cancelOp
 * @codegen-snapshot operationId:primitive
 */
export interface FileCancelOpParams {
    /** Correlation id from a `file.*Async` dispatch receipt. */
    operationId: string;
}

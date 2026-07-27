/**
 * Hand-written overrides for the `dsp.*` namespace.
 */

/**
 * One entry of the DSP chain passed to `dsp.setChain`.
 *
 * Only `guid` is read by the host; every other property on an entry is
 * ignored. `dsp.getChain` returns richer objects, but feeding one back
 * unchanged is safe because the extra keys are dropped.
 */
export interface DspChainEntry {
    /** GUID of an installed DSP, in `{XXXXXXXX-XXXX-...}` form. */
    guid: string;
}

/**
 * Parameters for `dsp.setChain`.
 *
 * The C++ handler only reaches `dsps` through `params.contains("dsps")`, so
 * the extractor records the key as `unknown` and cannot see the element
 * shape. `DspSetChain` then requires the key to be present *and* an array,
 * rejecting the call with `dsps array is required` otherwise, and requires a
 * non-empty string `guid` on every entry — hence a required array of objects
 * rather than an optional `string[]`.
 *
 * Validation rejects the whole call on the first bad entry; a partial chain is
 * never applied.
 *
 * @codegen-override params:dsp.setChain
 * @codegen-snapshot dsps:unknown
 */
export interface DspSetChainParams {
    /** Ordered DSP chain to apply; replaces the current chain wholesale. */
    dsps: DspChainEntry[];
}

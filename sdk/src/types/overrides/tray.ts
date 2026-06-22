/**
 * Hand-written overrides for the `tray.*` namespace.
 */

/**
 * Parameters for `tray.setIcon`.
 *
 * @codegen-override params:tray.setIcon
 * @codegen-snapshot
 */
export interface TraySetIconParams {
    /** Base64 icon payload; omit or pass null to fall back to foobar2000's main icon. */
    icon?: string | null;
}

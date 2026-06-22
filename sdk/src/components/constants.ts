/**
 * Shared constants and pure helpers used across multiple components.
 *
 * Keeps the per-component files free of duplicated playback-order
 * tables and time-format logic. Imported (rather than re-declared) so
 * a future change to the order vocabulary is a single-file edit.
 */

/**
 * Playback-order vocabulary indexed by the C++ host's order index
 * (0-6). The order of entries must match the host's
 * `playback.setPlaybackOrder` enum exactly; reordering would silently
 * desync UI labels from the runtime state.
 */
export const ORDER_NAMES: readonly string[] = [
    'default',
    'repeat-playlist',
    'repeat-track',
    'random',
    'shuffle-tracks',
    'shuffle-albums',
    'shuffle-folders',
] as const;

/**
 * ReplayGain mode names indexed by the numeric `mode` field carried
 * on `audio:replaygainModeChanged` event payloads.
 */
export const RG_MODE_NAMES = ['none', 'track', 'album', 'auto'] as const;

export type ReplayGainMode = (typeof RG_MODE_NAMES)[number];

/**
 * Order indices that count as "shuffle on" for the
 * `<fb-shuffle-button>` component. Anything inside this set toggles
 * the host attribute `active`; anything outside it clears `active`.
 */
export const SHUFFLE_ORDERS: ReadonlySet<number> = new Set([3, 4, 5, 6]);

/**
 * Format a duration in seconds as `m:ss` or `h:mm:ss`.
 *
 * @param seconds duration. Negative / non-finite / zero ⇒ `'0:00'`.
 */
export function formatTime(seconds: number): string {
    if (!seconds || !isFinite(seconds) || seconds < 0) return '0:00';
    const h = Math.floor(seconds / 3600);
    const m = Math.floor((seconds % 3600) / 60);
    const s = Math.floor(seconds % 60);
    const ss = s.toString().padStart(2, '0');
    return h > 0
        ? `${h}:${m.toString().padStart(2, '0')}:${ss}`
        : `${m}:${ss}`;
}

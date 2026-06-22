/**
 * SMP event-name registry + `fb.onSMP(name, callback)` glue.
 *
 * The Spider Monkey Panel runtime exposed events with snake-case
 * names (`on_playback_starting` etc.) and a per-event positional
 * parameter convention (e.g. `on_playback_stop(reasonNumber)`,
 * `on_playback_starting(commandNumber, paused)`). The C++ host emits
 * the canonical colon-format events (`playback:starting` etc.) with
 * structured object payloads, so this module bridges the two:
 *
 * - `SMP_EVENT_MAP` resolves the SMP name to the canonical FB event.
 * - `SMP_PARAM_ADAPTERS` reshapes each canonical payload into the
 *   positional argument list SMP scripts expect.
 *
 * Migrated from `sdk/smp-compat.js:304-504`.
 */

import type { SmpBridgeShape } from './bridgeShape.js';
import { FbMetadbHandle } from './classes/FbMetadbHandle.js';
import { FbMetadbHandleList } from './classes/FbMetadbHandleList.js';
import type { SmpEventCallback, SmpEventName, SmpHandleLike } from './types.js';

const LOG_PREFIX = '[SMP-Compat]';

function _error(...args: unknown[]): void {
    try {
        // eslint-disable-next-line no-console
        console.error(LOG_PREFIX, ...args);
    } catch {
        /* ignore */
    }
}

function _warn(...args: unknown[]): void {
    // The helper is kept silent in production builds; the `console.warn`
    // call below makes the message observable in dev tools when an
    // unexpected SMP event mapping is triggered.
    try {
        // eslint-disable-next-line no-console
        console.warn(LOG_PREFIX, ...args);
    } catch {
        /* ignore */
    }
}

/**
 * Special markers returned when the SMP name does not resolve to a
 * single canonical event. The bootstrap intercepts these explicitly.
 */
type SmpSpecialEventName = '__special_focus__' | '__special_playlists__';

/**
 * Map from SMP event name to canonical FB event name.
 *
 * Two SMP names resolve to special markers (`__special_*__`) — those
 * are dispatched manually inside the bootstrap because they bind to
 * multiple canonical events.
 */
export const SMP_EVENT_MAP: Record<SmpEventName, string | SmpSpecialEventName> = {
    // Playback
    on_playback_starting: 'playback:starting',
    on_playback_new_track: 'playback:trackChanged',
    on_playback_stop: 'playback:stopped',
    on_playback_seek: 'playback:seeked',
    on_playback_pause: 'playback:paused',
    on_playback_time: 'playback:time',
    on_playback_edited: 'playback:edited',
    on_playback_dynamic_info: 'playback:dynamicInfo',
    on_playback_dynamic_info_track: 'playback:dynamicInfoTrack',
    on_playback_order_changed: 'playback:orderChanged',
    on_playback_queue_changed: 'playback:queueChanged',

    // Playback stats
    on_item_played: 'playback:itemPlayed',

    // Volume
    on_volume_change: 'playback:volumeChanged',

    // Selection
    on_selection_changed: 'selection:changed',

    // Library
    on_library_items_added: 'library:itemsAdded',
    on_library_items_removed: 'library:itemsRemoved',
    on_library_items_changed: 'library:itemsModified',

    // Metadb
    on_metadb_changed: 'metadb:changed',

    // Playlist
    on_playlist_switch: 'playlist:activated',
    on_playlist_items_added: 'playlist:itemsAdded',
    on_playlist_items_removed: 'playlist:itemsRemoved',
    on_playlist_items_reordered: 'playlist:itemsReordered',
    on_playlist_items_selection_change: 'playlist:selectionChanged',
    on_item_focus_change: 'playlist:focusChanged',

    // Audio / DSP
    on_dsp_preset_changed: 'audio:dspPresetChanged',
    on_output_device_changed: 'audio:outputDeviceChanged',
    on_replaygain_mode_changed: 'audio:replaygainModeChanged',

    // UI
    on_colours_changed: 'ui:coloursChanged',
    on_font_changed: 'ui:fontChanged',

    // App state
    on_always_on_top_changed: 'window:alwaysOnTopChanged',
    on_cursor_follow_playback_changed: 'playback:cursorFollowChanged',
    on_playback_follow_cursor_changed: 'playback:followCursorChanged',
    on_playlist_stop_after_current_changed: 'playback:stopAfterCurrentChanged',

    // Special
    on_focus: '__special_focus__',
    on_playlists_changed: '__special_playlists__',
};

/**
 * Per-event parameter adapter. Returning:
 *
 * - `undefined`  → invoke `callback()` with no args.
 * - an array     → spread it: `callback(...args)`.
 * - any other    → invoke `callback(value)`.
 */
type SmpParamAdapter = (data: unknown) => unknown;

const PLAYBACK_STOP_REASON: Record<string, number> = {
    user: 0,
    eof: 1,
    starting_another: 2,
    shutting_down: 3,
};

const PLAYBACK_STARTING_CMD: Record<string, number> = {
    play: 1,
    next: 2,
    previous: 3,
    random: 5,
};

const PLAYBACK_QUEUE_ORIGIN: Record<string, number> = {
    user_added: 0,
    user_removed: 1,
    playback_advance: 2,
};

/** Adapter table — keyed by SMP event name. */
export const SMP_PARAM_ADAPTERS: Partial<Record<SmpEventName, SmpParamAdapter>> = {
    on_playback_new_track: (d) => d,
    on_playback_stop: (d) => {
        const reason = (d as { reason?: string })?.reason;
        return PLAYBACK_STOP_REASON[reason ?? ''] ?? 0;
    },
    on_playback_pause: (d) => !!(d as { paused?: boolean })?.paused,
    on_playback_time: (d) => {
        const pos = (d as { position?: number })?.position;
        return typeof pos === 'number' ? pos : 0;
    },
    on_playback_seek: (d) => {
        const pos = (d as { position?: number })?.position;
        return typeof pos === 'number' ? pos : 0;
    },
    on_playback_starting: (d) => {
        const command = (d as { command?: string })?.command;
        const cmd = PLAYBACK_STARTING_CMD[command ?? ''] ?? 0;
        return [cmd, !!(d as { paused?: boolean })?.paused];
    },
    on_playback_order_changed: (d) => {
        const idx = (d as { orderIndex?: number })?.orderIndex;
        return typeof idx === 'number' ? idx : 0;
    },
    on_volume_change: (d) => {
        const v = (d as { volumeDb?: number })?.volumeDb;
        return typeof v === 'number' ? v : -100;
    },
    on_selection_changed: () => undefined,
    on_library_items_added: () => undefined,
    on_library_items_removed: () => undefined,
    on_library_items_changed: () => undefined,
    on_metadb_changed: (d) => {
        const data = d as { tracks?: unknown[]; fromHook?: boolean } | null;
        const tracks: unknown[] = Array.isArray(data?.tracks) ? data!.tracks! : [];
        const fromHook = !!data?.fromHook;
        try {
            const list = new FbMetadbHandleList();
            for (const t of tracks) list.Add(t as SmpHandleLike);
            return [list, fromHook];
        } catch {
            return [tracks, fromHook];
        }
    },
    on_item_played: (d) => {
        try {
            return new FbMetadbHandle(d as SmpHandleLike);
        } catch {
            return d;
        }
    },
    on_replaygain_mode_changed: (d) => {
        const m = (d as { mode?: number })?.mode;
        return typeof m === 'number' ? m : 0;
    },
    on_dsp_preset_changed: () => undefined,
    on_output_device_changed: () => undefined,
    on_playlist_switch: () => undefined,
    on_playlist_items_added: (d) => {
        const pl = (d as { playlist?: number })?.playlist;
        return typeof pl === 'number' ? pl : 0;
    },
    on_playlist_items_removed: (d) => {
        const obj = d as { playlist?: number; newCount?: number } | null;
        const pl = typeof obj?.playlist === 'number' ? obj.playlist : 0;
        const newCount = typeof obj?.newCount === 'number' ? obj.newCount : 0;
        return [pl, newCount];
    },
    on_playlist_items_reordered: (d) => {
        const pl = (d as { playlist?: number })?.playlist;
        return typeof pl === 'number' ? pl : 0;
    },
    on_playlist_items_selection_change: () => undefined,
    on_item_focus_change: (d) => {
        const obj = d as { playlist?: number; from?: number; to?: number } | null;
        const pl = typeof obj?.playlist === 'number' ? obj.playlist : 0;
        const from = typeof obj?.from === 'number' ? obj.from : -1;
        const to = typeof obj?.to === 'number' ? obj.to : -1;
        return [pl, from, to];
    },
    on_playback_queue_changed: (d) => {
        const origin = (d as { origin?: string })?.origin;
        return PLAYBACK_QUEUE_ORIGIN[origin ?? ''] ?? 0;
    },
    on_always_on_top_changed: (d) => !!(d as { enabled?: boolean })?.enabled,
    on_playlist_stop_after_current_changed: (d) => !!(d as { enabled?: boolean })?.enabled,
    on_cursor_follow_playback_changed: (d) => !!(d as { enabled?: boolean })?.enabled,
    on_playback_follow_cursor_changed: (d) => !!(d as { enabled?: boolean })?.enabled,
};

/**
 * Construct the `fb.onSMP(name, callback)` function bound to the
 * provided bridge instance.
 *
 * Returns an unsubscribe callback. Special markers (`on_focus` /
 * `on_playlists_changed`) are dispatched manually because they bind
 * to multiple canonical events.
 */
export function createOnSmp(
    fb: SmpBridgeShape,
): (smpEventName: SmpEventName | string, callback: SmpEventCallback) => () => void {
    return (smpEventName, callback) => {
        if (typeof callback !== 'function') {
            _warn('fb.onSMP callback must be a function:', smpEventName);
            return () => {
                /* no-op */
            };
        }

        // Special: focus / blur are wired to two distinct events.
        if (smpEventName === 'on_focus') {
            const unsub1 = fb.on('panel:focus', () => {
                try {
                    callback(true);
                } catch (e) {
                    _error('on_focus handler error:', e);
                }
            });
            const unsub2 = fb.on('panel:blur', () => {
                try {
                    callback(false);
                } catch (e) {
                    _error('on_focus handler error:', e);
                }
            });
            return () => {
                try {
                    unsub1();
                } catch {
                    /* ignore */
                }
                try {
                    unsub2();
                } catch {
                    /* ignore */
                }
            };
        }

        // Special: playlists_changed is the union of five playlist mutation events.
        if (smpEventName === 'on_playlists_changed') {
            const handler = (): void => {
                try {
                    callback();
                } catch (e) {
                    _error('on_playlists_changed handler error:', e);
                }
            };
            const events = [
                'playlist:created',
                'playlist:removed',
                'playlist:reordered',
                'playlist:renamed',
                'playlist:lockChanged',
            ] as const;
            const unsubs = events.map((evt) => fb.on(evt, handler));
            return () => {
                for (const u of unsubs) {
                    try {
                        u();
                    } catch {
                        /* ignore */
                    }
                }
            };
        }

        const fb2kEvent = SMP_EVENT_MAP[smpEventName as SmpEventName];
        if (!fb2kEvent || fb2kEvent.startsWith('__special_')) {
            _warn('Unknown SMP event:', smpEventName);
            return () => {
                /* no-op */
            };
        }

        const adapter = SMP_PARAM_ADAPTERS[smpEventName as SmpEventName];

        const wrapped = (data: unknown): void => {
            try {
                if (!adapter) {
                    callback(data);
                    return;
                }
                const adapted = adapter(data);
                if (Array.isArray(adapted)) {
                    callback(...adapted);
                } else if (adapted !== undefined) {
                    callback(adapted);
                } else {
                    callback();
                }
            } catch (e) {
                _error(`Error in ${smpEventName}:`, e);
            }
        };

        const unsub = fb.on(fb2kEvent, wrapped);
        return () => {
            try {
                unsub();
            } catch {
                /* ignore */
            }
        };
    };
}

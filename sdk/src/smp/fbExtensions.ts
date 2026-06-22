import type { JsonValue } from '../types/json.js';
/**
 * SMP-style extensions on the runtime `fb` object.
 *
 * Spider Monkey Panel scripts expect properties / methods directly on
 * `fb` (e.g. `fb.Play()`, `fb.Volume = -10`, `fb.IsPlaying`,
 * `fb.TitleFormat(expr)`). This module attaches them to the runtime
 * aggregate without overriding any value the bridge already supplies.
 *
 * Cache-rollback rule: every cache-backed setter writes the new value
 * optimistically, fires the C++ invoke, and on rejection reverts the
 * cache value so subsequent reads stay consistent with the host.
 */

import type { ExtensibleFb, SmpBridgeShape } from './bridgeShape.js';
import type { BaseResponse } from '../types/responses.js';
import { ContextMenuManager } from './classes/ContextMenuManager.js';
import { FbMetadbHandle } from './classes/FbMetadbHandle.js';
import { FbMetadbHandleList } from './classes/FbMetadbHandleList.js';
import { FbProfiler } from './classes/FbProfiler.js';
import { FbTitleFormat } from './classes/FbTitleFormat.js';
import { FbUiSelectionHolder } from './classes/FbUiSelectionHolder.js';
import { MainMenuManager } from './classes/MainMenuManager.js';
import { stripSubsongSuffix } from './handleId.js';
import type { SMPPlman, SmpCompatCache, SmpHandleLike } from './types.js';
import { clamp, normalizeHandleList } from './utils.js';

const LOG_PREFIX = '[SMP-Compat]';

function _warn(...args: unknown[]): void {
    try {
        // eslint-disable-next-line no-console
        console.warn(LOG_PREFIX, ...args);
    } catch {
        /* ignore */
    }
}

/** Attach a property only if it is not already defined on `target`. */
function _defineIfMissing(
    target: object,
    prop: string,
    desc: PropertyDescriptor,
): boolean {
    if (Object.prototype.hasOwnProperty.call(target, prop)) return false;
    Object.defineProperty(target, prop, { configurable: true, ...desc });
    return true;
}

/** Linear `0..100` percent → SMP dB scale conversion (used by VolumeUp/Down). */
function _dbToPercent(db: number): number {
    if (typeof db !== 'number' || !Number.isFinite(db)) return 0;
    if (db <= -100) return 0;
    if (db >= 0) return 100;
    const v = 100 * Math.pow(10, db / 20);
    return clamp(v, 0, 100);
}

/** Resolve a handle-like input to a plain filesystem path. */
function _toPath(handleLike: SmpHandleLike): string {
    if (!handleLike) return '';
    if (typeof handleLike === 'string') return stripSubsongSuffix(handleLike);
    const obj = handleLike as { Path?: unknown; absolutePath?: unknown; path?: unknown };
    if (typeof obj.Path === 'string') return stripSubsongSuffix(obj.Path);
    if (typeof obj.absolutePath === 'string') return stripSubsongSuffix(obj.absolutePath);
    if (typeof obj.path === 'string') return stripSubsongSuffix(obj.path);
    return '';
}

/**
 * Install every SMP-style extension on `fb` and `plman`.
 *
 * All wrapper classes are bundled statically with this module; the
 * extensions are attached in a single synchronous pass.
 */
export function attachFbExtensions(
    fb: SmpBridgeShape,
    plman: SMPPlman,
    cache: SmpCompatCache,
    schedulePlaylistRefresh: () => void,
): void {
    const _invoke = fb.invoke.bind(fb);
    const fbExt = fb as ExtensibleFb;

    // ─── Volume helpers ─────────────────────────────────────────────

    function _setVolumeDb(db: number): Promise<BaseResponse> {
        const nextDb = clamp(Number(db) || -100, -100, 0);
        const oldDb = cache.volumeDb;
        cache.volumeDb = nextDb;
        return _invoke<BaseResponse>('playback.setVolume', {
            volume: _dbToPercent(nextDb),
        }).catch((e) => {
            _warn('setVolumeDb failed, rolling back:', e);
            cache.volumeDb = oldDb;
            throw e;
        });
    }

    // ─── Base playback methods ──────────────────────────────────────

    _defineIfMissing(fbExt, 'Play', {
        value: () =>
            fb.player?.play
                ? fb.player.play()
                : _invoke('playback.play', {}),
    });
    _defineIfMissing(fbExt, 'Pause', {
        value: () =>
            fb.player?.pause
                ? fb.player.pause()
                : _invoke('playback.pause', {}),
    });
    _defineIfMissing(fbExt, 'Stop', {
        value: () =>
            fb.player?.stop
                ? fb.player.stop()
                : _invoke('playback.stop', {}),
    });
    _defineIfMissing(fbExt, 'Next', {
        value: () =>
            fb.player?.next
                ? fb.player.next()
                : _invoke('playback.next', {}),
    });
    _defineIfMissing(fbExt, 'Prev', {
        value: () =>
            fb.player?.prev
                ? fb.player.prev()
                : _invoke('playback.previous', {}),
    });
    _defineIfMissing(fbExt, 'Random', {
        value: () =>
            fb.player?.random
                ? fb.player.random()
                : _invoke('playback.random', {}),
    });
    _defineIfMissing(fbExt, 'PlayOrPause', {
        value: () =>
            fb.player?.toggle
                ? fb.player.toggle()
                : _invoke('playback.playOrPause', {}),
    });

    _defineIfMissing(fbExt, 'VolumeUp', {
        value: () =>
            _invoke('playback.volumeUp', {})
                .catch(() => _setVolumeDb((cache.volumeDb ?? -100) + 1))
                .catch((e) => _warn('VolumeUp failed:', e)),
    });
    _defineIfMissing(fbExt, 'VolumeDown', {
        value: () =>
            _invoke('playback.volumeDown', {})
                .catch(() => _setVolumeDb((cache.volumeDb ?? -100) - 1))
                .catch((e) => _warn('VolumeDown failed:', e)),
    });
    _defineIfMissing(fbExt, 'VolumeMute', {
        value: () =>
            _invoke('playback.toggleMute', {}).catch(() =>
                fb.player?.mute
                    ? fb.player.mute()
                    : _invoke('playback.mute', {}),
            ),
    });

    _defineIfMissing(fbExt, 'Exit', {
        value: () =>
            _invoke('misc.exit', {}).catch(() =>
                fb.ui?.close
                    ? fb.ui.close()
                    : _invoke('window.close', {}),
            ),
    });

    // ─── Cache-backed sync properties ───────────────────────────────

    if (!Object.getOwnPropertyDescriptor(fbExt, 'IsPlaying')) {
        Object.defineProperty(fbExt, 'IsPlaying', {
            configurable: true,
            get: () => !!cache.isPlaying,
        });
    }
    if (!Object.getOwnPropertyDescriptor(fbExt, 'IsPaused')) {
        Object.defineProperty(fbExt, 'IsPaused', {
            configurable: true,
            get: () => !!cache.isPaused,
        });
    }
    if (!Object.getOwnPropertyDescriptor(fbExt, 'Volume')) {
        Object.defineProperty(fbExt, 'Volume', {
            configurable: true,
            get: () => (typeof cache.volumeDb === 'number' ? cache.volumeDb : -100),
            set: (db: number) => {
                _setVolumeDb(db).catch((e) => _warn('set Volume failed:', e));
            },
        });
    }
    if (!Object.getOwnPropertyDescriptor(fbExt, 'PlaybackTime')) {
        Object.defineProperty(fbExt, 'PlaybackTime', {
            configurable: true,
            get: () =>
                typeof cache.playbackTime === 'number' ? cache.playbackTime : 0,
            set: (seconds: number) => {
                const s = Number(seconds) || 0;
                const oldValue = cache.playbackTime;
                cache.playbackTime = s;
                const seek = fb.player?.seek
                    ? fb.player.seek(s)
                    : _invoke('playback.setPosition', { seconds: s });
                Promise.resolve(seek).catch((e) => {
                    _warn('set PlaybackTime failed, rolling back:', e);
                    cache.playbackTime = oldValue;
                });
            },
        });
    }
    if (!Object.getOwnPropertyDescriptor(fbExt, 'PlaybackLength')) {
        Object.defineProperty(fbExt, 'PlaybackLength', {
            configurable: true,
            get: () =>
                typeof cache.playbackLength === 'number' ? cache.playbackLength : 0,
        });
    }
    if (!Object.getOwnPropertyDescriptor(fbExt, 'StopAfterCurrent')) {
        Object.defineProperty(fbExt, 'StopAfterCurrent', {
            configurable: true,
            get: () => !!cache.stopAfterCurrent,
            set: (enabled: boolean) => {
                const v = !!enabled;
                const oldValue = cache.stopAfterCurrent;
                cache.stopAfterCurrent = v;
                const p = fb.player?.setStopAfterCurrent
                    ? fb.player.setStopAfterCurrent(v)
                    : _invoke('playback.setStopAfterCurrent', { enabled: v });
                Promise.resolve(p).catch((e) => {
                    _warn('set StopAfterCurrent failed, rolling back:', e);
                    cache.stopAfterCurrent = oldValue;
                });
            },
        });
    }
    if (!Object.getOwnPropertyDescriptor(fbExt, 'AlwaysOnTop')) {
        Object.defineProperty(fbExt, 'AlwaysOnTop', {
            configurable: true,
            get: () => !!cache.alwaysOnTop,
            set: (enabled: boolean) => {
                const v = !!enabled;
                const oldValue = cache.alwaysOnTop;
                cache.alwaysOnTop = v;
                const p = fb.ui?.setAlwaysOnTop
                    ? fb.ui.setAlwaysOnTop(v)
                    : _invoke('window.setAlwaysOnTop', { enabled: v });
                Promise.resolve(p).catch((e) => {
                    _warn('set AlwaysOnTop failed, rolling back:', e);
                    cache.alwaysOnTop = oldValue;
                });
            },
        });
    }

    // === Wrappers and commonly used SMP APIs ===

    _defineIfMissing(fbExt, 'TitleFormat', {
        value: (expression: string) => new FbTitleFormat(expression),
    });
    _defineIfMissing(fbExt, 'CreateHandleList', {
        value: () => new FbMetadbHandleList(),
    });
    _defineIfMissing(fbExt, 'CreateProfiler', {
        value: (name?: string) => new FbProfiler(name),
    });

    _defineIfMissing(fbExt, 'GetSelection', {
        value: async () => {
            const res = (await _invoke('selection.get', { limit: 0 })) as
                | { handles?: unknown[] }
                | null;
            const handles: unknown[] = Array.isArray(res?.handles) ? res!.handles! : [];
            const list = new FbMetadbHandleList();
            for (const h of handles) {
                list.Add(new FbMetadbHandle(h as SmpHandleLike));
            }
            return list;
        },
    });
    _defineIfMissing(fbExt, 'GetSelectionType', {
        value: async () => {
            const res = (await _invoke('selection.getType', {})) as
                | { type?: number }
                | null;
            return typeof res?.type === 'number' ? res.type : 0;
        },
    });

    _defineIfMissing(fbExt, 'IsLibraryEnabled', {
        value: async () => {
            const res = (await _invoke('library.getStatus', {})) as
                | { enabled?: boolean; initialized?: boolean }
                | null;
            return !!(res?.enabled ?? res?.initialized);
        },
    });
    _defineIfMissing(fbExt, 'IsMetadbInMediaLibrary', {
        value: async (handleLike: SmpHandleLike) => {
            const path = _toPath(handleLike);
            if (!path) return false;
            const res = (await _invoke('library.getByPath', { path })) as
                | { found?: boolean }
                | null;
            return !!res?.found;
        },
    });

    _defineIfMissing(fbExt, 'GetLibraryItems', {
        value: async () => {
            const list = new FbMetadbHandleList();
            const chunk = 500;
            let offset = 0;
            let total: number | null = null;

            while (total === null || offset < total) {
                const res = (await _invoke('library.getAll', {
                    offset,
                    limit: chunk,
                })) as { tracks?: unknown[]; items?: unknown[]; total?: number } | null;
                const tracks: unknown[] = Array.isArray(res?.tracks)
                    ? res!.tracks!
                    : Array.isArray(res?.items)
                        ? res!.items!
                        : [];

                if (!Array.isArray(tracks) || tracks.length === 0) break;

                for (const t of tracks) {
                    list.Add(new FbMetadbHandle(t as SmpHandleLike));
                }

                if (typeof res?.total === 'number') total = res.total;
                offset += tracks.length;

                if (tracks.length < chunk && (total === null || offset >= total)) break;
            }
            return list;
        },
    });
    _defineIfMissing(fbExt, 'GetQueryItems', {
        value: async (_handlesLike: unknown, query: string) => {
            // The handlesLike argument is intentionally ignored; the
            // search runs against the entire library.
            // (SMP API quirk preserved for behavioural parity.)
            const q = String(query ?? '');
            if (!q) return new FbMetadbHandleList();

            const list = new FbMetadbHandleList();
            const chunk = 500;
            let offset = 0;
            let total: number | null = null;

            while (total === null || offset < total) {
                const res = (await _invoke('library.search', {
                    query: q,
                    offset,
                    limit: chunk,
                })) as { tracks?: unknown[]; items?: unknown[]; total?: number } | null;
                const tracks: unknown[] = Array.isArray(res?.tracks)
                    ? res!.tracks!
                    : Array.isArray(res?.items)
                        ? res!.items!
                        : [];

                if (!Array.isArray(tracks) || tracks.length === 0) break;

                for (const t of tracks) {
                    list.Add(new FbMetadbHandle(t as SmpHandleLike));
                }

                if (typeof res?.total === 'number') total = res.total;
                offset += tracks.length;

                if (tracks.length < chunk && (total === null || offset >= total)) break;
            }
            return list;
        },
    });

    _defineIfMissing(fbExt, 'GetNowPlaying', {
        value: async () => {
            const res = (await _invoke('playback.getCurrentTrack', {})) as
                | { found?: boolean; [key: string]: JsonValue }
                | null;
            if (res && res.found === false) return null;
            if (!res || typeof res !== 'object') return null;
            return new FbMetadbHandle(res as SmpHandleLike);
        },
    });
    _defineIfMissing(fbExt, 'GetFocusItem', {
        value: async (_force?: boolean) => {
            const pl = cache.activePlaylist | 0;
            const focus = (await _invoke('playlist.getFocusedTrack', {
                playlist: pl,
            })) as { index?: number } | null;
            const idx = typeof focus?.index === 'number' ? focus.index | 0 : -1;
            if (idx < 0) return null;

            const res = (await _invoke('playlist.getTracks', {
                playlist: pl,
                start: idx,
                count: 1,
            })) as { tracks?: unknown[] } | null;
            const t =
                Array.isArray(res?.tracks) && res!.tracks!.length > 0
                    ? res!.tracks![0]
                    : null;
            if (!t) return null;
            return new FbMetadbHandle(t as SmpHandleLike);
        },
    });

    // === Menu, misc, and config ===

    _defineIfMissing(fbExt, 'RunMainMenuCommand', {
        value: (command: string) =>
            _invoke('menu.runMainMenuCommand', { command: String(command ?? '') }),
    });
    _defineIfMissing(fbExt, 'RunContextCommand', {
        value: (command: string) =>
            _invoke('menu.runContextCommand', { command: String(command ?? '') }),
    });
    _defineIfMissing(fbExt, 'ShowConsole', {
        value: () => _invoke('misc.showConsole', {}),
    });
    _defineIfMissing(fbExt, 'ShowPreferences', {
        value: () => _invoke('misc.showPreferences', {}),
    });
    _defineIfMissing(fbExt, 'ShowLibrarySearchUI', {
        value: (query?: string) =>
            _invoke('misc.showLibrarySearch', { query: String(query ?? '') }),
    });
    _defineIfMissing(fbExt, 'ShowPopupMessage', {
        value: (msg: string, title?: string) =>
            _invoke('misc.showPopupMessage', {
                message: String(msg ?? ''),
                title: String(title ?? ''),
            }),
    });
    _defineIfMissing(fbExt, 'Restart', {
        value: () => _invoke('misc.restart', {}),
    });

    _defineIfMissing(fbExt, 'AcquireUiSelectionHolder', {
        value: () => new FbUiSelectionHolder(),
    });

    if (!Object.getOwnPropertyDescriptor(fbExt, 'ComponentPath')) {
        Object.defineProperty(fbExt, 'ComponentPath', {
            configurable: true,
            get: () => String(cache.componentPath || ''),
        });
    }
    if (!Object.getOwnPropertyDescriptor(fbExt, 'FoobarPath')) {
        Object.defineProperty(fbExt, 'FoobarPath', {
            configurable: true,
            get: () => String(cache.foobarPath || ''),
        });
    }
    if (!Object.getOwnPropertyDescriptor(fbExt, 'ProfilePath')) {
        Object.defineProperty(fbExt, 'ProfilePath', {
            configurable: true,
            get: () => String(cache.profilePath || ''),
        });
    }

    if (!Object.getOwnPropertyDescriptor(fbExt, 'CursorFollowPlayback')) {
        Object.defineProperty(fbExt, 'CursorFollowPlayback', {
            configurable: true,
            get: () => !!cache.cursorFollowPlayback,
            set: (enabled: boolean) => {
                const v = !!enabled;
                const oldValue = cache.cursorFollowPlayback;
                cache.cursorFollowPlayback = v;
                _invoke('config.setCursorFollowPlayback', { enabled: v }).catch((e) => {
                    _warn('set CursorFollowPlayback failed, rolling back:', e);
                    cache.cursorFollowPlayback = oldValue;
                });
            },
        });
    }
    if (!Object.getOwnPropertyDescriptor(fbExt, 'PlaybackFollowCursor')) {
        Object.defineProperty(fbExt, 'PlaybackFollowCursor', {
            configurable: true,
            get: () => !!cache.playbackFollowCursor,
            set: (enabled: boolean) => {
                const v = !!enabled;
                const oldValue = cache.playbackFollowCursor;
                cache.playbackFollowCursor = v;
                _invoke('config.setPlaybackFollowCursor', { enabled: v }).catch((e) => {
                    _warn('set PlaybackFollowCursor failed, rolling back:', e);
                    cache.playbackFollowCursor = oldValue;
                });
            },
        });
    }
    if (!Object.getOwnPropertyDescriptor(fbExt, 'ReplaygainMode')) {
        Object.defineProperty(fbExt, 'ReplaygainMode', {
            configurable: true,
            get: () =>
                typeof cache.replaygainMode === 'number' ? cache.replaygainMode : 0,
            set: (mode: number) => {
                const n = Number.isFinite(Number(mode)) ? Number(mode) | 0 : 0;
                const oldValue = cache.replaygainMode;
                cache.replaygainMode = n;
                _invoke('config.setReplaygainMode', { mode: n }).catch((e) => {
                    _warn('set ReplaygainMode failed, rolling back:', e);
                    cache.replaygainMode = oldValue;
                });
            },
        });
    }

    // === Menu manager factories ===

    _defineIfMissing(fbExt, 'CreateContextMenuManager', {
        value: () => new ContextMenuManager(),
    });
    _defineIfMissing(fbExt, 'CreateMainMenuManager', {
        value: () => new MainMenuManager(),
    });

    // === Clipboard, DSP, output, and version ===

    _defineIfMissing(fbExt, 'CheckClipboardContents', {
        value: async () => {
            const res = (await _invoke('clipboard.read', {})) as
                | { hasFiles?: boolean }
                | null;
            return !!res?.hasFiles;
        },
    });

    _defineIfMissing(fbExt, 'GetClipboardContents', {
        value: async () => {
            const res = (await _invoke('clipboard.read', {})) as
                | { files?: Array<string | { path?: string }> }
                | null;
            const files: Array<string | { path?: string }> = Array.isArray(res?.files)
                ? res!.files!
                : [];
            const list = new FbMetadbHandleList();
            for (const f of files) {
                const id = typeof f === 'string' ? f : f?.path ?? '';
                list.Add(new FbMetadbHandle(id));
            }
            return list;
        },
    });

    _defineIfMissing(fbExt, 'CopyHandleListToClipboard', {
        value: async (handlesLike: unknown) => {
            const items = normalizeHandleList(handlesLike);
            const paths: string[] = [];
            for (const h of items) {
                const p = _toPath(h as SmpHandleLike);
                if (p) paths.push(p);
            }
            if (paths.length === 0) return false;
            const res = (await _invoke('clipboard.writeFiles', { paths })) as
                | { success?: boolean }
                | null;
            return !!res?.success;
        },
    });

    _defineIfMissing(fbExt, 'GetDSPPresets', {
        value: async () => {
            const res = (await _invoke('config.getDspPresets', {})) as unknown[] | null;
            const presets = Array.isArray(res) ? res : [];
            return JSON.stringify(presets);
        },
    });
    _defineIfMissing(fbExt, 'SetDSPPreset', {
        value: async (idx: number) => {
            await _invoke('config.setActiveDspPreset', { index: idx | 0 });
        },
    });

    _defineIfMissing(fbExt, 'GetOutputDevices', {
        value: async () => {
            const res = (await _invoke('config.getOutputDevices', {})) as
                | { devices?: unknown[] }
                | unknown[]
                | null;
            const devices = Array.isArray(res)
                ? res
                : Array.isArray((res as { devices?: unknown[] })?.devices)
                    ? (res as { devices: unknown[] }).devices
                    : [];
            return JSON.stringify(devices);
        },
    });
    _defineIfMissing(fbExt, 'SetOutputDevice', {
        value: async (guid: string, deviceId: string) => {
            await _invoke('config.setOutputDevice', {
                outputId: String(guid ?? ''),
                deviceId: String(deviceId ?? ''),
            });
        },
    });

    if (!Object.getOwnPropertyDescriptor(fbExt, 'Version')) {
        Object.defineProperty(fbExt, 'Version', {
            configurable: true,
            get: () => String(cache.version || ''),
        });
    }

    _defineIfMissing(fbExt, 'RunContextCommandWithMetadb', {
        value: async (
            command: string,
            handleLike: SmpHandleLike,
            _flags?: number,
        ) => {
            const res = (await _invoke('menu.runContextCommand', {
                command: String(command ?? ''),
                handles: handleLike ? [_toPath(handleLike)] : [],
            })) as { success?: boolean } | null;
            return !!res?.success;
        },
    });

    _defineIfMissing(fbExt, 'ClearPlaylist', {
        value: async () => {
            const pl = cache.activePlaylist | 0;
            const res = (await _invoke('playlist.clear', { playlist: pl })) as
                | { success?: boolean }
                | null;
            schedulePlaylistRefresh();
            return !!res?.success;
        },
    });

    _defineIfMissing(fbExt, 'GetLibraryRelativePath', {
        value: (handleLike: SmpHandleLike): string => {
            const p = _toPath(handleLike);
            const fbPath = cache.foobarPath || '';
            if (!p || !fbPath) return p;
            const norm = (s: string): string =>
                s.replace(/\//g, '\\').replace(/\\+$/, '');
            const np = norm(p);
            const nfb = norm(fbPath);
            if (np.toLowerCase().startsWith(nfb.toLowerCase())) {
                return np.slice(nfb.length).replace(/^\\/, '');
            }
            return p;
        },
    });

    // `plman.AddItemToPlaybackQueue` and friends are installed by
    // `plman.ts`; reference `plman` here so the bootstrap caller can see
    // that the invariant is satisfied (no mutation performed).
    void plman;
}

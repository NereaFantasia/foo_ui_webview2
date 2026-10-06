import type { JsonValue } from '../types/json.js';
import type { JsonObject } from '../types/json.js';
import type { PlaylistGetTracksResponse } from '../types/generated/responses.js';
/**
 * `plman` — Spider Monkey Panel playlist manager compatibility layer.
 *
 * Builds the runtime `plman` object the IIFE bundle installs onto
 * `window.plman`. Every method maps to a single C++ API call (or a
 * small composition); the cache-backed properties read from the
 * shared {@link SmpCompatCache}.
 *
 * Cache rollback rule: every cache-backed setter writes the new value
 * optimistically, fires the C++ invoke, and on rejection rolls the
 * cache back so UI subscribers see eventual consistency. Sync helpers
 * (`GetPlaylistName`, `FindPlaylist`, ...) read directly from cache.
 */

import type { SmpBridgeShape } from './bridgeShape.js';
import { typedCall } from '../utils/typedCall.js';
import { FbMetadbHandle } from './classes/FbMetadbHandle.js';
import { FbMetadbHandleList } from './classes/FbMetadbHandleList.js';
import { smpWarn } from './smpLog.js';
import type { SMPPlman, SmpCompatCache, SmpHandleLike, SmpPlaybackQueueItem } from './types.js';
import { toHandleId, successOf } from './utils.js';

/** Coerce a number / number-like iterable into a flat numeric array. */
function _toIndexArray(listLike: unknown): number[] {
    if (!listLike) return [];

    if (Array.isArray(listLike)) {
        return listLike
            .map((n) => (Number(n) | 0))
            .filter((n) => Number.isFinite(n) && n >= 0);
    }

    const iterable = listLike as Iterable<number | unknown>;
    if (typeof iterable[Symbol.iterator] === 'function') {
        const out: number[] = [];
        for (const v of iterable) {
            const n = Number(v) | 0;
            if (Number.isFinite(n) && n >= 0) out.push(n);
        }
        return out;
    }

    return [];
}

/**
 * Build the `plman` object using the supplied bridge + cache + refresh
 * scheduler. The result is installed onto `window.plman` by the
 * bootstrap.
 *
 * The schedulePlaylistRefresh callback is invoked after any operation
 * that changes the playlist set or per-playlist contents so the cache
 * eventually catches up via `playlist.getAll`.
 */
export function buildPlman(
    fb: SmpBridgeShape,
    cache: SmpCompatCache,
    schedulePlaylistRefresh: () => void,
): SMPPlman {
    const _invoke = typedCall(fb.invoke.bind(fb));

    const plman = {} as SMPPlman;

    // ───── Properties (cache-backed, with rollback) ─────────────────

    Object.defineProperties(plman, {
        ActivePlaylist: {
            configurable: true,
            get: () => cache.activePlaylist | 0,
            set: (idx: number) => {
                const n = idx | 0;
                const oldValue = cache.activePlaylist;
                cache.activePlaylist = n;
                _invoke('playlist.setActive', { playlist: n }).catch((e) => {
                    smpWarn('plman.ActivePlaylist set failed, rolling back cache:', e);
                    cache.activePlaylist = oldValue;
                    schedulePlaylistRefresh();
                });
            },
        },
        PlayingPlaylist: {
            configurable: true,
            get: () => cache.playingPlaylist | 0,
        },
        PlaylistCount: {
            configurable: true,
            get: () => cache.playlistCount | 0,
        },
        PlaybackOrder: {
            configurable: true,
            get: () => cache.playbackOrder | 0,
            set: (order: number) => {
                const n = order | 0;
                const oldValue = cache.playbackOrder;
                cache.playbackOrder = n;
                _invoke('playback.setPlaybackOrder', { order: n }).catch((e) => {
                    smpWarn('plman.PlaybackOrder set failed, rolling back cache:', e);
                    cache.playbackOrder = oldValue;
                });
            },
        },
    });

    // ───── Synchronous helpers ──────────────────────────────────────

    plman.GetPlaylistName = (playlistIdx: number): string => {
        const idx = playlistIdx | 0;
        const all = Array.isArray(cache.playlists) ? cache.playlists : [];
        const p = all.find((x) => (Number(x?.index) | 0) === idx);
        return typeof p?.name === 'string' ? p.name : '';
    };

    plman.PlaylistItemCount = (playlistIdx: number): number => {
        const idx = playlistIdx | 0;
        const all = Array.isArray(cache.playlists) ? cache.playlists : [];
        const p = all.find((x) => (Number(x?.index) | 0) === idx);
        return typeof p?.trackCount === 'number' ? p.trackCount | 0 : 0;
    };

    plman.FindPlaylist = (name: string): number => {
        const n = String(name ?? '');
        const all = Array.isArray(cache.playlists) ? cache.playlists : [];
        const exact = all.find(
            (p) => p && typeof p.name === 'string' && p.name === n,
        );
        if (exact && typeof exact.index === 'number') return exact.index | 0;

        const lower = n.toLowerCase();
        const ci = all.find(
            (p) => p && typeof p.name === 'string' && p.name.toLowerCase() === lower,
        );
        return typeof ci?.index === 'number' ? ci.index | 0 : -1;
    };

    plman.IsAutoPlaylist = (playlistIdx: number): boolean => {
        const idx = playlistIdx | 0;
        const all = Array.isArray(cache.playlists) ? cache.playlists : [];
        const p = all.find((x) => (Number(x?.index) | 0) === idx);
        return !!p?.isAutoplaylist;
    };

    plman.IsPlaylistLocked = (playlistIdx: number): boolean => {
        const idx = playlistIdx | 0;
        const all = Array.isArray(cache.playlists) ? cache.playlists : [];
        const p = all.find((x) => (Number(x?.index) | 0) === idx);
        return !!p?.isLocked;
    };

    // ───── Async playlist mutation ──────────────────────────────────

    plman.CreateAutoPlaylist = async (
        _playlistIdx: number,
        name: string,
        query: string,
        sort?: string,
        flags?: number,
    ): Promise<number> => {
        // The SMP signature accepts `playlistIdx` as the insertion
        // position, but the C++ `playlist.createAutoplaylist` handler
        // always appends. The argument is accepted for SMP API
        // compatibility but silently ignored.
        const keepSorted = !!((flags ?? 0) & 1);
        const res = successOf(await _invoke('playlist.createAutoplaylist', {
            name: String(name ?? 'New Autoplaylist'),
            query: String(query ?? ''),
            sort: String(sort ?? ''),
            keepSorted,
        }));
        schedulePlaylistRefresh();
        return typeof res?.index === 'number' ? res.index | 0 : -1;
    };

    plman.RenamePlaylist = async (
        playlistIdx: number,
        name: string,
    ): Promise<boolean> => {
        const idx = playlistIdx | 0;
        const res = successOf(await _invoke('playlist.rename', {
            playlist: idx,
            name: String(name ?? ''),
        }));
        schedulePlaylistRefresh();
        return !!res?.success;
    };

    plman.CreatePlaylist = async (
        position: number,
        name: string,
    ): Promise<number> => {
        const pos = position | 0;
        const params: JsonObject = {
            name: String(name ?? 'New Playlist'),
        };
        if (pos >= 0) params.position = pos;
        const res = successOf(await _invoke('playlist.create', params));
        schedulePlaylistRefresh();
        return typeof res?.index === 'number' ? res.index | 0 : -1;
    };

    plman.RemovePlaylist = async (playlistIdx: number): Promise<boolean> => {
        const idx = playlistIdx | 0;
        const res = successOf(await _invoke('playlist.remove', {
            playlist: idx,
        }));
        schedulePlaylistRefresh();
        return !!res?.success;
    };

    plman.ClearPlaylist = async (playlistIdx: number): Promise<boolean> => {
        const idx = playlistIdx | 0;
        const res = successOf(await _invoke('playlist.clear', {
            playlist: idx,
        }));
        schedulePlaylistRefresh();
        return !!res?.success;
    };

    plman.GetPlaylistFocusItemIndex = async (
        playlistIdx: number,
    ): Promise<number> => {
        const idx = playlistIdx | 0;
        const res = successOf(await _invoke('playlist.getFocusedTrack', {
            playlist: idx,
        }));
        return typeof res?.index === 'number' ? res.index | 0 : -1;
    };

    plman.SetPlaylistFocusItem = async (
        playlistIdx: number,
        itemIdx: number,
    ): Promise<boolean> => {
        const idx = playlistIdx | 0;
        const item = itemIdx | 0;
        const res = successOf(await _invoke('playlist.setFocusedTrack', {
            playlist: idx,
            index: item,
        }));
        return !!res?.success;
    };

    plman.SetPlaylistSelection = async (
        playlistIdx: number,
        items: number[] | Iterable<number>,
        state: boolean,
    ): Promise<boolean> => {
        const idx = playlistIdx | 0;
        const indices = _toIndexArray(items);
        if (indices.length === 0) return true;

        if (state) {
            const res = successOf(await _invoke('playlist.setSelection', {
                playlist: idx,
                indices,
                clearOthers: false,
            }));
            return !!res?.success;
        }

        // Deselect: rebuild selection as (current - indices), written back to
        // the playlist the selection was read from even if the index moved.
        const cur = successOf(
            await _invoke('playlist.getSelection', { playlist: idx }).catch(() => null),
        );
        if (!cur?.playlistGuid) return false;
        const toRemove = new Set(indices);
        const toKeep = cur.items.filter((i: number) => !toRemove.has(i));
        const res = successOf(await _invoke('playlist.setSelection', {
            playlistGuid: cur.playlistGuid,
            indices: toKeep,
            clearOthers: true,
        }));
        return !!res?.success;
    };

    plman.ClearPlaylistSelection = async (
        playlistIdx: number,
    ): Promise<boolean> => {
        const idx = playlistIdx | 0;
        const res = successOf(await _invoke('playlist.deselectAll', {
            playlist: idx,
        }));
        return !!res?.success;
    };

    plman.RemovePlaylistSelection = async (
        playlistIdx: number,
        crop?: boolean,
    ): Promise<boolean> => {
        const idx = playlistIdx | 0;
        const doCrop = !!crop;

        if (!doCrop) {
            const res = successOf(await _invoke('playlist.removeSelectedTracks', {
                playlist: idx,
            }));
            schedulePlaylistRefresh();
            return !!res?.success;
        }

        // Crop = remove unselected. Every step after the first names the
        // playlist the selection was read from, so a playlist added or removed
        // in between cannot turn this into a crop of another one.
        const sel = successOf(
            await _invoke('playlist.getSelection', { playlist: idx }).catch(() => null),
        );
        if (!sel?.playlistGuid) return false;
        const target = { playlistGuid: sel.playlistGuid };
        const selected = sel.items;

        if (selected.length === 0) {
            const res = successOf(await _invoke('playlist.clear', target));
            schedulePlaylistRefresh();
            return !!res?.success;
        }

        const cnt = successOf(
            await _invoke('playlist.getTrackCount', target).catch(() => null),
        );
        if (typeof cnt?.count !== 'number') return false;
        const total = cnt.count | 0;

        const keep = new Set(selected.map((n: number) => n | 0));
        const remove: number[] = [];
        for (let i = 0; i < total; i++) {
            if (!keep.has(i)) remove.push(i);
        }

        if (remove.length > 0) {
            const res = successOf(await _invoke('playlist.removeTracks', {
                ...target,
                items: remove,
            }));
            schedulePlaylistRefresh();
            return !!res?.success;
        }

        return true;
    };

    plman.SortByFormat = async (
        playlistIdx: number,
        pattern: string,
        selectedOnly?: boolean,
    ): Promise<boolean> => {
        const idx = playlistIdx | 0;
        const res = successOf(await _invoke('playlist.sort', {
            playlist: idx,
            pattern: String(pattern ?? '%title%'),
            selectedOnly: !!selectedOnly,
            descending: false,
        }));
        return !!res?.success;
    };

    plman.UndoBackup = (_playlistIdx: number): boolean => {
        // SMP semantics: create an undo backup point. The backend
        // already auto-saves an undo point before any mutating op,
        // so this is a no-op rather than dispatching `playlist.undo`
        // (which would actually undo, breaking SMP semantics).
        return true;
    };

    plman.MovePlaylistSelection = async (
        playlistIdx: number,
        delta: number,
    ): Promise<boolean> => {
        const idx = playlistIdx | 0;
        const d = delta | 0;
        const res = successOf(await _invoke('playlist.moveTracks', {
            playlist: idx,
            items: [],
            delta: d,
        }));
        return !!res?.success;
    };

    plman.DuplicatePlaylist = async (
        from: number,
        name?: string,
    ): Promise<number> => {
        const idx = from | 0;
        const params: JsonObject = { playlist: idx };
        const newName = String(name ?? '');
        if (newName) params.name = newName;
        const res = successOf(await _invoke('playlist.duplicate', params));
        schedulePlaylistRefresh();
        return typeof res?.index === 'number' ? res.index | 0 : -1;
    };

    plman.AddLocations = async (
        playlistIdx: number,
        locations: string[] | Iterable<string>,
        select?: boolean,
    ): Promise<number> => {
        const idx = playlistIdx | 0;
        const locs: string[] = Array.isArray(locations)
            ? locations.map((s) => String(s))
            : typeof (locations as Iterable<string>)?.[Symbol.iterator] === 'function'
                ? Array.from(locations as Iterable<string>, (s) => String(s))
                : [];

        const res = successOf(await _invoke('playlist.addPaths', {
            playlist: idx,
            paths: locs,
        }));
        schedulePlaylistRefresh();

        const added = typeof res?.addedCount === 'number' ? res.addedCount | 0 : 0;

        // The rows were counted in the playlist the tracks went into; select
        // them there, by GUID, even if its index moved meanwhile.
        if (select && added > 0 && res?.playlistGuid) {
            const start = res.countBefore | 0;
            const indices: number[] = [];
            for (let i = 0; i < added; i++) indices.push(start + i);
            await _invoke('playlist.setSelection', {
                playlistGuid: res.playlistGuid,
                indices,
                clearOthers: true,
            }).catch(() => null);
        }

        return added;
    };

    // === Handle-list returning helpers ===

    plman.GetPlaylistSelectedItems = async (
        playlistIdx: number,
    ): Promise<FbMetadbHandleList> => {
        const idx = playlistIdx | 0;
        const res = successOf(await _invoke('playlist.getSelectedTracks', {
            playlist: idx,
        }));
        const tracks: unknown[] = Array.isArray(res?.tracks) ? res!.tracks! : [];
        const list = new FbMetadbHandleList();
        for (const t of tracks) {
            list.Add(new FbMetadbHandle(t as SmpHandleLike));
        }
        return list;
    };

    plman.GetPlaylistItems = async (
        playlistIdx: number,
    ): Promise<FbMetadbHandleList> => {
        const idx = playlistIdx | 0;
        const list = new FbMetadbHandleList();
        const chunk = 500;

        let start = 0;
        let total: number | null = null;
        // The first page names the playlist by index; the later pages name the
        // playlist that page came from, so the pages cannot mix two playlists.
        let target: { playlist: number } | { playlistGuid: string } = { playlist: idx };

        while (total === null || start < total) {
            // Annotated because `target` is assigned from `res` below.
            const res: PlaylistGetTracksResponse | undefined = successOf(
                await _invoke('playlist.getTracks', { ...target, start, count: chunk }),
            );
            if (!res?.playlistGuid) break;
            target = { playlistGuid: res.playlistGuid };
            const tracks: unknown[] = res.tracks ?? [];
            if (!Array.isArray(tracks) || tracks.length === 0) break;

            for (const t of tracks) {
                list.Add(new FbMetadbHandle(t as SmpHandleLike));
            }

            if (typeof res?.total === 'number') total = res.total;
            start += tracks.length;

            if (tracks.length < chunk && (total === null || start >= total)) break;
        }

        return list;
    };

    plman.InsertPlaylistItems = async (
        playlistIdx: number,
        base: number,
        handlesLike: Iterable<unknown> | unknown,
        select?: boolean,
    ): Promise<number> => {
        const idx = playlistIdx | 0;
        const pos = Math.max(0, base | 0);

        const handles: string[] = [];
        const collect = (h: unknown): void => {
            const id = toHandleId(h);
            if (id) handles.push(id);
        };

        if (Array.isArray(handlesLike)) {
            for (const h of handlesLike) collect(h);
        } else if (
            handlesLike &&
            typeof (handlesLike as Iterable<unknown>)[Symbol.iterator] === 'function'
        ) {
            for (const h of handlesLike as Iterable<unknown>) collect(h);
        }

        if (handles.length === 0) return 0;

        const res = successOf(await _invoke('playlist.insertTracks', {
            playlist: idx,
            position: pos,
            handles,
        }));
        schedulePlaylistRefresh();

        const added = typeof res?.addedCount === 'number' ? res.addedCount | 0 : 0;

        // Selected in the playlist the tracks went into, by GUID, as in AddLocations.
        if (select && added > 0 && res?.playlistGuid) {
            const start =
                typeof res.insertIndex === 'number' ? res.insertIndex | 0 : pos;
            const indices: number[] = [];
            for (let i = 0; i < added; i++) indices.push(start + i);
            await _invoke('playlist.setSelection', {
                playlistGuid: res.playlistGuid,
                indices,
                clearOthers: true,
            }).catch(() => null);
        }

        return added;
    };

    plman.AddItemToPlaybackQueue = async (
        handleLike: SmpHandleLike,
    ): Promise<number> => {
        const id = toHandleId(handleLike);
        if (!id) return 0;
        // queue.addPaths accepts the `|subsong:N` suffix, so a subsong is
        // queued as itself rather than as the file's first subsong.
        const res = successOf(await _invoke('queue.addPaths', {
            paths: [id],
            useQueuePlaylist: true,
        }));
        return typeof res?.addedCount === 'number'
            ? res.addedCount | 0
            : res?.success
                ? 1
                : 0;
    };

    plman.GetPlaybackQueueContents = async (): Promise<SmpPlaybackQueueItem[]> => {
        const res = successOf(await _invoke('queue.get', {}));
        const items: unknown[] = Array.isArray(res?.items) ? res!.items! : [];
        return items.map((rawItem) => {
            const it = rawItem as {
                playlist?: number;
                playlistItem?: number;
                queueIndex?: number;
                [key: string]: JsonValue;
            };
            return {
                Handle: new FbMetadbHandle(rawItem as SmpHandleLike),
                PlaylistIndex: typeof it?.playlist === 'number' ? it.playlist | 0 : -1,
                PlaylistItemIndex:
                    typeof it?.playlistItem === 'number' ? it.playlistItem | 0 : -1,
                QueueIndex:
                    typeof it?.queueIndex === 'number' ? it.queueIndex | 0 : undefined,
            } satisfies SmpPlaybackQueueItem;
        });
    };

    plman.FlushPlaybackQueue = async (): Promise<boolean> => {
        await _invoke('queue.clear', {});
        return true;
    };

    // === Extension methods ===

    plman.AddPlaylistItemToPlaybackQueue = async (
        playlistIdx: number,
        playlistItemIdx: number,
    ): Promise<boolean> => {
        const pl = playlistIdx | 0;
        const item = playlistItemIdx | 0;
        const res = successOf(await _invoke('queue.add', {
            playlist: pl,
            track: item,
        }));
        return !!res?.success;
    };

    plman.GetPlaybackQueueHandles = async (): Promise<FbMetadbHandleList> => {
        const res = successOf(await _invoke('queue.get', {}));
        const items: unknown[] = Array.isArray(res?.items) ? res!.items! : [];
        const list = new FbMetadbHandleList();
        for (const it of items) {
            list.Add(new FbMetadbHandle(it as SmpHandleLike));
        }
        return list;
    };

    plman.IsPlaylistItemSelected = async (
        playlistIdx: number,
        itemIdx: number,
    ): Promise<boolean> => {
        const pl = playlistIdx | 0;
        const item = itemIdx | 0;
        const res = successOf(await _invoke('playlist.getSelection', {
            playlist: pl,
        }));
        const selected = Array.isArray(res?.items) ? res.items! : [];
        return selected.includes(item);
    };

    plman.FindOrCreatePlaylist = async (
        name: string,
        _unlocked?: boolean,
    ): Promise<number> => {
        const n = String(name ?? '');
        const findFn = plman.FindPlaylist as (name: string) => number;
        const idx = findFn(n);
        if (idx >= 0) return idx;
        const res = successOf(await _invoke('playlist.create', { name: n }));
        schedulePlaylistRefresh();
        return typeof res?.index === 'number' ? res.index | 0 : -1;
    };

    plman.MovePlaylist = async (from: number, to: number): Promise<boolean> => {
        const count = cache.playlistCount | 0;
        const f = from | 0;
        const t = to | 0;
        if (f < 0 || f >= count || t < 0 || t >= count || f === t) return false;
        // Built from the cached playlists the indices refer to, and sent as
        // GUIDs: the host fails the move when the playlists changed since the
        // cache was read, instead of moving whichever playlist is at `from` now.
        const guids = cache.playlists.map((p) => p.guid);
        if (guids.length !== count) return false;
        const [moved] = guids.splice(f, 1);
        guids.splice(t, 0, moved!);
        const res = successOf(await _invoke('playlist.reorderPlaylists', {
            newOrderGuids: guids,
        }));
        schedulePlaylistRefresh();
        return !!res?.success;
    };

    return plman as unknown as SMPPlman;
}

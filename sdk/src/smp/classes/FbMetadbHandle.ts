import type { JsonValue } from '../../types/json.js';
/**
 * `FbMetadbHandle` — Spider Monkey Panel handle wrapper.
 *
 * Represents one foobar2000 track address (`path` + optional sub-song
 * index). Constructable from a string handle-id, an existing
 * `FbMetadbHandle` (copy ctor), or a track-info object that carries
 * `absolutePath` / `path` / `Path` plus optional `subsong`.
 *
 * Mirrors `sdk/smp/FbMetadbHandle.js` field-for-field; the only TS
 * change is making `_trackInfo` typed as `Partial<TrackInfo>`-shaped
 * loose record so accessors can pull `duration` / `fileSize` without
 * `any`.
 */

import { formatHandleId, parseHandleId, stripSubsongSuffix } from '../handleId.js';
import { getInvoke } from '../utils.js';
import { FbFileInfo } from './FbFileInfo.js';
import type { SmpHandleLike } from '../types.js';

interface MetadbInput {
    path?: string;
    Path?: string;
    absolutePath?: string;
    subsong?: number;
    SubSong?: number;
    subSong?: number;
    duration?: number;
    fileSize?: number;
    [key: string]: JsonValue;
}

export class FbMetadbHandle {
    private _path: string = '';
    private _subsong: number = 0;
    private _trackInfo: MetadbInput | null = null;

    constructor(input?: SmpHandleLike | MetadbInput | FbMetadbHandle) {
        // Copy constructor
        if (input instanceof FbMetadbHandle) {
            this._path = input._path;
            this._subsong = input._subsong;
            this._trackInfo = input._trackInfo;
            return;
        }

        // String handle id: "C:\\...\\file.flac|subsong:2"
        if (typeof input === 'string') {
            const parsed = parseHandleId(input);
            this._path = parsed.path || '';
            this._subsong = parsed.subsong || 0;
            return;
        }

        if (!input || typeof input !== 'object') return;

        const obj = input as MetadbInput;
        const abs = typeof obj.absolutePath === 'string' ? obj.absolutePath : '';
        const rawPath = typeof obj.path === 'string' ? obj.path : '';
        const pathLike = abs || rawPath || (typeof obj.Path === 'string' ? obj.Path : '');

        if (pathLike) {
            const parsed = parseHandleId(pathLike);
            this._path = parsed.path || '';
            this._subsong = parsed.subsong || 0;
        }

        // subsong may also be provided separately
        const s = obj.subsong ?? obj.SubSong ?? obj.subSong;
        if ((this._subsong | 0) === 0 && typeof s === 'number' && s > 0) {
            this._subsong = s | 0;
        }

        // Keep the original object for Length / FileSize / etc.
        this._trackInfo = obj;
    }

    /** Filesystem path with no `|subsong:N` suffix. */
    get Path(): string {
        return this._path || '';
    }

    /** Same as {@link Path}; kept for SMP API parity. */
    get RawPath(): string {
        return this._path || '';
    }

    /** Sub-song index (`0` for single-stream files). */
    get SubSong(): number {
        return this._subsong | 0;
    }

    /** Track length in seconds (`0` if unknown). */
    get Length(): number {
        const d = this._trackInfo?.duration;
        return typeof d === 'number' ? d : 0;
    }

    /** File size in bytes (`0` if unknown). */
    get FileSize(): number {
        const s = this._trackInfo?.fileSize;
        return typeof s === 'number' ? s : 0;
    }

    /**
     * Round-trip-stable identifier matching `selection.set` /
     * `selection.get` schemata (path + optional `|subsong:N` suffix).
     */
    get HandleId(): string {
        return formatHandleId(this.Path, this.SubSong);
    }

    /** Path + sub-song equality check; coerces `other` if necessary. */
    Compare(other: unknown): boolean {
        if (!other) return false;
        const o = other instanceof FbMetadbHandle ? other : new FbMetadbHandle(other as SmpHandleLike);
        return this.Path === o.Path && this.SubSong === o.SubSong;
    }

    /**
     * Resolve full file info via `metadata.read`. Returns `null` when
     * the path is empty, the bridge is unavailable, or the read fails.
     */
    async GetFileInfo(): Promise<FbFileInfo | null> {
        const inv = getInvoke();
        if (!inv) return null;

        const path = stripSubsongSuffix(this.Path);
        if (!path) return null;

        try {
            const res = (await inv('metadata.read', { path })) as
                | { success?: boolean; [key: string]: JsonValue }
                | null;
            if (!res || res.success === false) return null;
            return new FbFileInfo(res);
        } catch {
            return null;
        }
    }

    toString(): string {
        return this.HandleId;
    }
}

import { call } from '../call.js';
import type { LyricsSaveOptions } from '../../types/responses.js';
import type {
    LyricsExistsParams,
    LyricsGetParams,
    LyricsSaveParams,
} from '../../types/generated/params.js';

/**
 * `lyrics` — lyrics fetch / save namespace.
 */
export const lyrics = {
    /**
     * Read a track's lyrics: embedded tags first, then sidecar files next to
     * the audio file. Omit `path` for the playing track; with nothing playing
     * the host answers `NO_ACTIVE_ITEM`. A non-empty `filename` replaces the
     * automatic file candidates; use `source: 'file'` to skip embedded tags.
     */
    get: (path?: string, options?: Omit<LyricsGetParams, 'path'>) =>
        call('lyrics.get', {
            ...(path ? { path } : {}),
            ...(options || {}),
        }),
    /**
     * List which lyrics sources exist for `path`, including empty tags or files.
     * A non-empty `filename` selects one file next to the audio file.
     */
    exists: (path: string, options?: Omit<LyricsExistsParams, 'path'>) =>
        call('lyrics.exists', {
            path,
            ...(options || {}),
        }),
    /**
     * Save lyrics to one or more targets. The host takes `target` as an array;
     * a single string is sent as a one-element array. With one target the
     * response describes that write, with several each outcome is keyed under
     * `results`. `all` writes the file and embedded targets. A non-empty
     * `filename` selects the output file without applying `format`.
     */
    save: (path: string, lyricsText: string, opts?: LyricsSaveOptions) => {
        const { target, ...rest } = opts ?? {};
        const params: LyricsSaveParams = {
            path,
            lyrics: lyricsText,
            ...rest,
            ...(target === undefined
                ? {}
                : { target: Array.isArray(target) ? target : [target] }),
        };
        return call('lyrics.save', params);
    },
};

/**
 * `lyrics` — lyrics fetch / save namespace.
 */

import { bridge } from '../Bridge.js';
import type {
    BaseResponse,
    LyricsGetResponse,
} from '../../types/responses.js';
import type {
    LyricsGetParams,
    LyricsSaveParams,
} from '../../types/generated/params.js';

export const lyrics = {
    get: (path?: string, options?: Omit<LyricsGetParams, 'path'>) =>
        bridge.invoke<LyricsGetResponse>('lyrics.get', {
            ...(path ? { path } : {}),
            ...(options || {}),
        }),
    exists: (path: string) =>
        bridge.invoke<{ exists: boolean }>('lyrics.exists', { path }),
    save: (
        path: string,
        lyricsText: string,
        opts?: Omit<LyricsSaveParams, 'path' | 'lyrics'>,
    ) =>
        bridge.invoke<
            BaseResponse & {
                results?: Array<{ target: string; success: boolean; error?: string }>;
                savedTo?: string[];
            }
        >('lyrics.save', {
            path,
            lyrics: lyricsText,
            ...(opts || {}),
        }),
};

/**
 * `clipboard` — clipboard read/write namespace.
 */

import { bridge } from '../Bridge.js';
import type { BaseResponse } from '../../types/responses.js';

export const clipboard = {
    read: () => bridge.invoke<{ text: string }>('clipboard.read'),
    write: (text: string) =>
        bridge.invoke<BaseResponse>('clipboard.write', { text }),
    writeHTML: (html: string, plainText?: string) =>
        bridge.invoke<
            BaseResponse & { htmlWritten?: boolean; textWritten?: boolean }
        >('clipboard.writeHTML', {
            html,
            ...(plainText ? { plainText } : {}),
        }),
    writeFiles: (paths: string[]) =>
        bridge.invoke<BaseResponse & { fileCount?: number }>(
            'clipboard.writeFiles',
            { paths },
        ),
};

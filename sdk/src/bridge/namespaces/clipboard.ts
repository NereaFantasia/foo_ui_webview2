import { call } from '../call.js';

/**
 * `clipboard` — clipboard read/write namespace.
 */
export const clipboard = {
    read: () => call('clipboard.read'),
    write: (text: string) =>
        call('clipboard.write', {
            text,
        }),
    writeHTML: (html: string, plainText?: string) =>
        call('clipboard.writeHTML', {
            html,
            ...(plainText ? { plainText } : {}),
        }),
    writeFiles: (paths: string[]) =>
        call('clipboard.writeFiles', {
            paths,
        }),
};

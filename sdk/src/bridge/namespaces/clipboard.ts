import { call } from '../call.js';

/**
 * `clipboard` — clipboard read/write namespace.
 */
export const clipboard = {
    /** Report what the Windows clipboard holds: text, file list, or image. */
    read: () => call('clipboard.read'),
    /** Replace the clipboard contents with Unicode text. */
    write: (text: string) =>
        call('clipboard.write', {
            text,
        }),
    /** Replace the clipboard contents with rich text in HTML format, plus a plain-text fallback. */
    writeHTML: (html: string, plainText?: string) =>
        call('clipboard.writeHTML', {
            html,
            ...(plainText ? { plainText } : {}),
        }),
    /** Place a file list on the clipboard (`CF_HDROP`) for Explorer and other shell targets to paste. */
    writeFiles: (paths: string[]) =>
        call('clipboard.writeFiles', {
            paths,
        }),
};

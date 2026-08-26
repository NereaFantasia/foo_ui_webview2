/**
 * `dialog` — file/folder pickers + confirm/prompt.
 */

import { bridge } from '../Bridge.js';
import type {
    DialogConfirmResponse,
    DialogOpenFileResponse,
    DialogSaveFileResponse,
    DialogOpenFolderResponse,
} from '../../types/responses.js';
import type {
    DialogConfirmParams,
    DialogOpenFileParams,
    DialogOpenFolderParams,
    DialogSaveFileParams,
} from '../../types/generated/params.js';

export const dialog = {
    /** Resolves with `{ canceled, filePaths }`; `filePaths` is empty when cancelled. */
    openFile: (opts?: DialogOpenFileParams) =>
        bridge.invoke<DialogOpenFileResponse>('dialog.openFile', opts),
    /** Resolves with `{ canceled, filePath }`; `filePath` is empty when cancelled. */
    saveFile: (opts?: DialogSaveFileParams) =>
        bridge.invoke<DialogSaveFileResponse>('dialog.saveFile', opts),
    /** Resolves with `{ canceled, folderPath }`; `folderPath` is empty when cancelled. */
    openFolder: (opts?: DialogOpenFolderParams) =>
        bridge.invoke<DialogOpenFolderResponse>('dialog.openFolder', opts),
    /**
     * Shows a modal confirmation dialog.
     *
     * Resolves with `{ response }`, the zero-based index of the clicked button
     * in `buttons`. The default button set is `['OK', 'Cancel']`, so `0` means
     * confirmed and `1` means cancelled - there is no `confirmed` flag.
     * The task dialog is created without `TDF_ALLOW_DIALOG_CANCELLATION`, so
     * Escape and the close button do not dismiss it and every result comes
     * from an actual button click. `-1` appears only on the host's fallback
     * path, when even a plain message box could not be shown.
     *
     * `response` is typed `unknown` because the host builds it arithmetically
     * and the extractor cannot see the result type; narrow it at the call site.
     */
    confirm: (opts?: DialogConfirmParams) =>
        bridge.invoke<DialogConfirmResponse>('dialog.confirm', opts),
};

import { call } from '../call.js';
import type {
    DialogConfirmParams,
    DialogOpenFileParams,
    DialogOpenFolderParams,
    DialogSaveFileParams,
} from '../../types/generated/params.js';

/**
 * `dialog` — file/folder pickers + confirm/prompt.
 */
export const dialog = {
    /**
     * Resolves with `{ success: true, canceled, filePaths }`; `filePaths` is empty when
     * cancelled. When the dialog cannot be shown at all it resolves with the failure
     * envelope `{ success: false, error, code }` instead.
     */
    openFile: (opts?: DialogOpenFileParams) =>
        call('dialog.openFile', opts),
    /** Resolves like {@link openFile}, with `filePath` (empty when cancelled) in place of `filePaths`. */
    saveFile: (opts?: DialogSaveFileParams) =>
        call('dialog.saveFile', opts),
    /** Resolves like {@link openFile}, with `folderPath` (empty when cancelled) in place of `filePaths`. */
    openFolder: (opts?: DialogOpenFolderParams) =>
        call('dialog.openFolder', opts),
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
     */
    confirm: (opts?: DialogConfirmParams) =>
        call('dialog.confirm', opts),
};

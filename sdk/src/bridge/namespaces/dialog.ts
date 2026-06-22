/**
 * `dialog` — file/folder pickers + confirm/prompt.
 */

import { bridge } from '../Bridge.js';
import type {
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
    openFile: (opts?: DialogOpenFileParams) =>
        bridge.invoke<DialogOpenFileResponse>('dialog.openFile', opts),
    saveFile: (opts?: DialogSaveFileParams) =>
        bridge.invoke<DialogSaveFileResponse>('dialog.saveFile', opts),
    openFolder: (opts?: DialogOpenFolderParams) =>
        bridge.invoke<DialogOpenFolderResponse>('dialog.openFolder', opts),
    confirm: (opts?: DialogConfirmParams) =>
        bridge.invoke<{ confirmed: boolean }>('dialog.confirm', opts),
};

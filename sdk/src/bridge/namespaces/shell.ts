/**
 * `shell` — OS-level integration namespace.
 *
 * Note: `spawn` short-circuits with `{ success: false }` when an empty
 * `cwd` string is passed.
 */

import { bridge } from '../Bridge.js';
import type {
    BaseResponse,
    ShellExecResponse,
    ShellSpawnResponse,
} from '../../types/responses.js';
import type {
    ShellExecParams,
    ShellSpawnParams,
} from '../../types/generated/params.js';

/** @deprecated Use {@link ShellExecParams}. */
export type ShellExecOptions = Omit<ShellExecParams, 'command'>;

/** @deprecated Use {@link ShellSpawnParams}. */
export type ShellSpawnOptions = Omit<ShellSpawnParams, 'executable'>;

export const shell = {
    showInExplorer: (path: string) =>
        bridge.invoke<BaseResponse>('shell.showInExplorer', { path }),
    openWith: (path: string) =>
        bridge.invoke<BaseResponse>('shell.openWith', { path }),
    openExternal: (url: string) =>
        bridge.invoke<BaseResponse>('shell.openExternal', { url }),
    exec: (command: string, options?: Omit<ShellExecParams, 'command'>) =>
        bridge.invoke<ShellExecResponse>('shell.exec', { command, ...options }),
    spawn: (
        executable: string,
        options?: Omit<ShellSpawnParams, 'executable'>,
    ): Promise<ShellSpawnResponse> => {
        const opts: ShellSpawnParams = { executable, ...options };
        if (
            opts.cwd &&
            typeof opts.cwd === 'string' &&
            opts.cwd.trim() === ''
        ) {
            return Promise.resolve({
                success: false,
                error: 'cwd is empty string',
            });
        }
        return bridge.invoke<ShellSpawnResponse>('shell.spawn', opts);
    },
};

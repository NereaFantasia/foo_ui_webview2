import { call } from '../call.js';
import type {
    ShellExecParams,
    ShellSpawnParams,
} from '../../types/generated/params.js';
import type {
    ShellSpawnResponse,
} from '../../types/generated/responses.js';

/** @deprecated Use {@link ShellExecParams}. */
export type ShellExecOptions = Omit<ShellExecParams, 'command'>;

/** @deprecated Use {@link ShellSpawnParams}. */
export type ShellSpawnOptions = Omit<ShellSpawnParams, 'executable'>;

/**
 * `shell` — OS-level integration namespace.
 *
 * Note: `spawn` short-circuits with `{ success: false }` when an empty
 * `cwd` string is passed.
 */
export const shell = {
    showInExplorer: (path: string) =>
        call('shell.showInExplorer', {
            path,
        }),
    openWith: (path: string) =>
        call('shell.openWith', {
            path,
        }),
    openExternal: (url: string) =>
        call('shell.openExternal', {
            url,
        }),
    exec: (command: string, options?: Omit<ShellExecParams, 'command'>) =>
        call('shell.exec', {
            command,
            ...options,
        }),
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
            // A failure envelope carries none of the declared fields; the flat
            // response type cannot express that, hence the assertion.
            return Promise.resolve({
                success: false,
                error: 'cwd is empty string',
                code: 'INVALID_PARAMS',
            } as ShellSpawnResponse);
        }
        return call('shell.spawn', opts);
    },
};

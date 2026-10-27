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
    /** Reveal a file or folder in Windows Explorer, selecting it. */
    showInExplorer: (path: string) =>
        call('shell.showInExplorer', {
            path,
        }),
    /** Open a file with the application Windows associates with its type. */
    openWith: (path: string) =>
        call('shell.openWith', {
            path,
        }),
    /** Open an http, https, or mailto URL with the default handler. */
    openExternal: (url: string) =>
        call('shell.openExternal', {
            url,
        }),
    /** Start a process from a command line and return at once. */
    exec: (command: string, options?: Omit<ShellExecParams, 'command'>) =>
        call('shell.exec', {
            command,
            ...options,
        }),
    /** Start a process from an executable and an argument list. */
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

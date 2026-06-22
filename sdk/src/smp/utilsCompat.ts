import type { JsonValue } from '../types/json.js';
import type { JsonObject } from '../types/json.js';
/**
 * `utilsCompat` — SMP `window.utils` namespace compatibility layer.
 *
 * Reproduces the Spider Monkey Panel `utils` global with three classes
 * of helpers:
 *   1. Pure JS (`FormatDuration`, `FormatFileSize`, `SplitFilePath`, ...).
 *   2. Backend-delegating (`FileExists`, `ReadTextFile`, `WriteTextFile`,
 *      `ListFiles`, `ListFolders`, `Glob`, `GetClipboardText`,
 *      `SetClipboardText`, `CheckComponent`).
 *   3. WebView2-only fallbacks (`ColourPicker`, `CheckFont`, `InputBox`).
 *
 * Exports a single `smpUtilsNamespace` object that the IIFE bootstrap
 * installs onto `window.utils` (with a `__smpUtilsCompat: true` marker
 * so re-loads are idempotent).
 */

import { getInvoke } from './utils.js';

interface FileInfoResponse {
    isFile?: boolean;
    isDirectory?: boolean;
    size?: number;
    [key: string]: JsonValue;
}

interface FileReadResponse {
    content?: string;
    [key: string]: JsonValue;
}

interface FileWriteResponse {
    success?: boolean;
    [key: string]: JsonValue;
}

interface FileListResponse {
    items?: Array<string | { path?: string; name?: string; isDirectory?: boolean; [key: string]: JsonValue }>;
    directories?: Array<string | { path?: string; [key: string]: JsonValue }>;
    [key: string]: JsonValue;
}

interface ClipboardReadResponse {
    text?: string;
    [key: string]: JsonValue;
}

interface ConfigComponentEntry {
    name?: string;
    fileName?: string;
    filename?: string;
    [key: string]: JsonValue;
}

type ConfigComponentsResponse =
    | ConfigComponentEntry[]
    | { components?: ConfigComponentEntry[]; [key: string]: JsonValue };

async function _invoke<T = unknown>(
    method: string,
    params?: JsonObject,
): Promise<T> {
    const inv = getInvoke();
    if (!inv) {
        throw new Error('[SMP-Utils] smp.invoke not available');
    }
    return (await inv(method, params ?? {})) as T;
}

// ---------------------------------------------------------------------------
// 1. Pure JS helpers
// ---------------------------------------------------------------------------

/** Format an integer second count as `H:MM:SS` (or `M:SS` < 1 hour). */
export function FormatDuration(seconds: unknown): string {
    const s = Math.max(0, Math.round(Number(seconds) || 0));
    const h = Math.floor(s / 3600);
    const m = Math.floor((s % 3600) / 60);
    const sec = s % 60;
    const pad = (n: number): string => (n < 10 ? '0' + n : String(n));
    return h > 0 ? `${h}:${pad(m)}:${pad(sec)}` : `${m}:${pad(sec)}`;
}

/** Format a byte count using binary IEC units (KiB/MiB/...). */
export function FormatFileSize(bytes: unknown): string {
    const b = Number(bytes) || 0;
    if (b < 0) return '0 B';
    const units = ['B', 'KB', 'MB', 'GB', 'TB'] as const;
    let i = 0;
    let v = b;
    while (v >= 1024 && i < units.length - 1) {
        v /= 1024;
        i++;
    }
    return i === 0 ? `${v} ${units[i]}` : `${v.toFixed(2)} ${units[i]}`;
}

/** Split a Windows path into `[directory, fileNameNoExt, extWithDot]`. */
export function SplitFilePath(path: unknown): [string, string, string] {
    const p = String(path ?? '');
    const norm = p.replace(/\//g, '\\');
    const lastSep = norm.lastIndexOf('\\');
    const dir = lastSep >= 0 ? norm.slice(0, lastSep + 1) : '';
    const fullName = lastSep >= 0 ? norm.slice(lastSep + 1) : norm;
    const dotPos = fullName.lastIndexOf('.');
    const fileName = dotPos > 0 ? fullName.slice(0, dotPos) : fullName;
    const ext = dotPos > 0 ? fullName.slice(dotPos) : '';
    return [dir, fileName, ext];
}

/**
 * Simple glob match (`*` / `?`) — case-insensitive, anchored.
 * Returns `false` on regex compilation failure.
 */
export function PathWildcardMatch(path: unknown, pattern: unknown): boolean {
    const p = String(path ?? '').toLowerCase();
    const pat = String(pattern ?? '').toLowerCase();
    const escaped = pat
        .replace(/[.+^${}()|[\]\\]/g, '\\$&')
        .replace(/\*/g, '.*')
        .replace(/\?/g, '.');
    try {
        return new RegExp(`^${escaped}$`).test(p);
    } catch {
        return false;
    }
}

/** Render a Unix-epoch second count as a localised date string. */
export function DateStringFromTimestamp(ts: unknown): string {
    const t = Number(ts) || 0;
    try {
        return new Date(t * 1000).toLocaleString();
    } catch {
        return '';
    }
}

// ---------------------------------------------------------------------------
// 2. Backend-delegating helpers
// ---------------------------------------------------------------------------

/** True iff the file or directory exists (`file.exists`). */
export async function FileExists(path: unknown): Promise<boolean> {
    const res = await _invoke<{ exists?: boolean }>('file.exists', {
        path: String(path ?? ''),
    });
    return !!res?.exists;
}

/** True iff `path` resolves to a regular file (`file.getInfo`). */
export async function IsFile(path: unknown): Promise<boolean> {
    const res = await _invoke<FileInfoResponse>('file.getInfo', {
        path: String(path ?? ''),
    });
    return !!res?.isFile;
}

/** True iff `path` resolves to a directory (`file.getInfo`). */
export async function IsDirectory(path: unknown): Promise<boolean> {
    const res = await _invoke<FileInfoResponse>('file.getInfo', {
        path: String(path ?? ''),
    });
    return !!res?.isDirectory;
}

/** Resolve the file size in bytes (`file.getInfo`). */
export async function GetFileSize(path: unknown): Promise<number> {
    const res = await _invoke<FileInfoResponse>('file.getInfo', {
        path: String(path ?? ''),
    });
    return typeof res?.size === 'number' ? res.size : 0;
}

/**
 * Read a text file (`file.read`). The `codepage` argument is accepted
 * for SMP API compatibility but currently ignored — the backend
 * always returns UTF-8 strings.
 */
export async function ReadTextFile(path: unknown, _codepage?: unknown): Promise<string> {
    const res = await _invoke<FileReadResponse | string>('file.read', {
        path: String(path ?? ''),
    });
    if (typeof res === 'string') return res;
    return typeof res?.content === 'string' ? res.content : '';
}

/** Convenience alias for `ReadTextFile(path, 65001)`. */
export function ReadUTF8(path: unknown): Promise<string> {
    return ReadTextFile(path, 65001);
}

/**
 * Write a text file (`file.write`). The `writeBom` argument is
 * accepted for SMP API compatibility but currently ignored.
 */
export async function WriteTextFile(
    path: unknown,
    content: unknown,
    _writeBom?: unknown,
): Promise<boolean> {
    const res = await _invoke<FileWriteResponse>('file.write', {
        path: String(path ?? ''),
        content: String(content ?? ''),
    });
    return !!res?.success;
}

/**
 * Glob a directory tree (`file.list` non-recursive + name filter).
 *
 * Matches the SMP semantics — splits `pattern` into a directory part
 * and a file-pattern part, lists the directory, then runs each
 * candidate through {@link PathWildcardMatch}. The `excMask` /
 * `incMask` arguments are accepted for SMP API compatibility but
 * currently ignored.
 *
 * Returns absolute paths (the backend may return bare filenames in
 * non-recursive mode; we prepend the directory in that case).
 */
export async function Glob(
    pattern: unknown,
    _excMask?: unknown,
    _incMask?: unknown,
): Promise<string[]> {
    const p = String(pattern ?? '');
    const norm = p.replace(/\//g, '\\');
    const lastSep = norm.lastIndexOf('\\');
    const dir = lastSep >= 0 ? norm.slice(0, lastSep) : '.';
    const globPart = lastSep >= 0 ? norm.slice(lastSep + 1) : norm;

    const res = await _invoke<FileListResponse>('file.list', {
        path: dir,
        recursive: false,
    });
    const rawItems = Array.isArray(res?.items) ? res!.items! : [];
    const prefix = dir.endsWith('\\') ? dir : `${dir}\\`;

    const candidates: string[] = rawItems
        .map((item) => {
            if (typeof item === 'string') {
                return item.includes('\\') || item.includes('/')
                    ? item
                    : prefix + item;
            }
            if (typeof item.path === 'string') return item.path;
            if (typeof item.name === 'string') return prefix + item.name;
            return '';
        })
        .filter(Boolean);

    return candidates.filter((f) => {
        if (!globPart || globPart === '*' || globPart === '*.*') return true;
        const name = f.split('\\').pop() ?? '';
        return PathWildcardMatch(name, globPart);
    });
}

/** List files in `folder` (`file.list`). */
export async function ListFiles(folder: unknown, recursion?: unknown): Promise<string[]> {
    const folderStr = String(folder ?? '');
    const isRecursive = !!recursion;
    const res = await _invoke<FileListResponse>('file.list', {
        path: folderStr,
        recursive: isRecursive,
    });
    const items = Array.isArray(res?.items) ? res!.items! : [];
    const prefix =
        !isRecursive && folderStr
            ? folderStr.endsWith('\\') || folderStr.endsWith('/')
                ? folderStr
                : `${folderStr}\\`
            : '';

    return items
        .filter((item) => {
            if (typeof item === 'object' && item !== null) {
                return !(item as { isDirectory?: boolean }).isDirectory;
            }
            return true;
        })
        .map((item) => {
            if (typeof item === 'string') {
                if (prefix && !item.includes('\\') && !item.includes('/')) {
                    return prefix + item;
                }
                return item;
            }
            return typeof (item as { path?: string }).path === 'string'
                ? (item as { path: string }).path
                : '';
        })
        .filter(Boolean);
}

/** List sub-folders in `folder` (`file.list`). */
export async function ListFolders(
    folder: unknown,
    recursion?: unknown,
): Promise<string[]> {
    const folderStr = String(folder ?? '');
    const isRecursive = !!recursion;
    const res = await _invoke<FileListResponse>('file.list', {
        path: folderStr,
        recursive: isRecursive,
    });
    const dirs = Array.isArray(res?.directories) ? res!.directories! : [];
    const prefix =
        !isRecursive && folderStr
            ? folderStr.endsWith('\\') || folderStr.endsWith('/')
                ? folderStr
                : `${folderStr}\\`
            : '';

    return dirs
        .map((item) => {
            if (typeof item === 'string') {
                if (prefix && !item.includes('\\') && !item.includes('/')) {
                    return prefix + item;
                }
                return item;
            }
            return typeof (item as { path?: string }).path === 'string'
                ? (item as { path: string }).path
                : '';
        })
        .filter(Boolean);
}

/** Read clipboard text (`clipboard.read`). */
export async function GetClipboardText(): Promise<string> {
    const res = await _invoke<ClipboardReadResponse>('clipboard.read', {});
    return typeof res?.text === 'string' ? res.text : '';
}

/** Write clipboard text (`clipboard.write`). */
export async function SetClipboardText(text: unknown): Promise<void> {
    await _invoke('clipboard.write', { text: String(text ?? '') });
}

/**
 * Check whether a foobar2000 component is loaded.
 *
 * Matches `config.getComponents` against the requested name in two modes:
 * - `isDll = true`  → match the file name (`fileName`).
 * - `isDll = false` → match the human-readable name (`name`) or the file
 *   name (substring match).
 */
export async function CheckComponent(
    name: unknown,
    isDll?: unknown,
): Promise<boolean> {
    const n = String(name ?? '').toLowerCase();
    if (!n) return false;
    const res = await _invoke<ConfigComponentsResponse>('config.getComponents', {});
    const components: ConfigComponentEntry[] = Array.isArray(res)
        ? res
        : Array.isArray((res as { components?: ConfigComponentEntry[] })?.components)
            ? (res as { components: ConfigComponentEntry[] }).components
            : [];

    return components.some((c) => {
        if (!c || typeof c !== 'object') return false;
        const cName = String(c.name ?? '').toLowerCase();
        const cFile = String(c.fileName ?? c.filename ?? '').toLowerCase();
        if (isDll) {
            return cFile === n || cFile.endsWith('\\' + n) || cFile.endsWith('/' + n);
        }
        return cName === n || cName.includes(n) || cFile.includes(n);
    });
}

/**
 * Read a value from an INI file (`file.read` + JS parse).
 *
 * Returns `defaultVal` (or `''`) when the file does not exist, the
 * section / key is missing, or the read fails.
 */
export async function ReadINI(
    path: unknown,
    section: unknown,
    key: unknown,
    defaultVal?: unknown,
): Promise<string> {
    const def = (defaultVal ?? '') as string;
    try {
        const res = await _invoke<FileReadResponse | string>('file.read', {
            path: String(path ?? ''),
        });
        const content = typeof res === 'string'
            ? res
            : typeof res?.content === 'string' ? res.content : '';
        if (!content) return def;

        const sec = String(section ?? '').toLowerCase();
        const k = String(key ?? '').toLowerCase();
        const lines = content.split(/\r?\n/);
        let inSection = false;

        for (const line of lines) {
            const trimmed = line.trim();
            if (trimmed.startsWith('[') && trimmed.endsWith(']')) {
                inSection = trimmed.slice(1, -1).trim().toLowerCase() === sec;
                continue;
            }
            if (inSection && trimmed.includes('=')) {
                const eqPos = trimmed.indexOf('=');
                const lineKey = trimmed.slice(0, eqPos).trim().toLowerCase();
                if (lineKey === k) {
                    return trimmed.slice(eqPos + 1).trim();
                }
            }
        }
        return def;
    } catch {
        return def;
    }
}

/** Write a value to an INI file (`file.read` + edit + `file.write`). */
export async function WriteINI(
    path: unknown,
    section: unknown,
    key: unknown,
    val: unknown,
): Promise<boolean> {
    const filePath = String(path ?? '');
    const sec = String(section ?? '');
    const k = String(key ?? '');
    const v = String(val ?? '');

    try {
        let content = '';
        try {
            const res = await _invoke<FileReadResponse | string>('file.read', {
                path: filePath,
            });
            content = typeof res === 'string'
                ? res
                : typeof res?.content === 'string' ? res.content : '';
        } catch {
            // File does not exist yet
        }

        const lines: string[] = content ? content.split(/\r?\n/) : [];
        const secHeader = `[${sec}]`;
        let sectionFound = false;
        let keyFound = false;
        let inTargetSection = false;
        let lastSectionLine = -1;

        for (let i = 0; i < lines.length; i++) {
            const trimmed = lines[i].trim();
            if (trimmed.startsWith('[') && trimmed.endsWith(']')) {
                if (inTargetSection && !keyFound) {
                    lines.splice(i, 0, `${k}=${v}`);
                    keyFound = true;
                    break;
                }
                inTargetSection = trimmed.toLowerCase() === secHeader.toLowerCase();
                if (inTargetSection) {
                    sectionFound = true;
                    lastSectionLine = i;
                }
                continue;
            }
            if (inTargetSection && trimmed.includes('=')) {
                const eqPos = trimmed.indexOf('=');
                const lineKey = trimmed.slice(0, eqPos).trim();
                if (lineKey.toLowerCase() === k.toLowerCase()) {
                    lines[i] = `${k}=${v}`;
                    keyFound = true;
                    break;
                }
                lastSectionLine = i;
            }
        }

        if (!sectionFound) {
            if (lines.length > 0 && lines[lines.length - 1].trim() !== '') {
                lines.push('');
            }
            lines.push(secHeader);
            lines.push(`${k}=${v}`);
        } else if (!keyFound) {
            lines.splice(lastSectionLine + 1, 0, `${k}=${v}`);
        }

        const newContent = lines.join('\r\n');
        const writeRes = await _invoke<FileWriteResponse>('file.write', {
            path: filePath,
            content: newContent,
        });
        return !!writeRes?.success;
    } catch {
        return false;
    }
}

// ---------------------------------------------------------------------------
// 3. WebView2-only fallbacks
// ---------------------------------------------------------------------------

/**
 * Show a simple input prompt — uses the host browser's native
 * `prompt()` since the SMP `InputBox` ABI predates WebView2 dialogs.
 *
 * @throws When `errorOnCancel` is truthy and the user cancels.
 */
export function InputBox(
    _windowId: unknown,
    promptText: unknown,
    _caption: unknown,
    defaultVal?: unknown,
    errorOnCancel?: unknown,
): string {
    const def = (defaultVal ?? '') as string;
    const result =
        typeof window !== 'undefined' && typeof window.prompt === 'function'
            ? window.prompt(String(promptText ?? ''), String(def))
            : null;
    if (result === null) {
        if (errorOnCancel) throw new Error('Dialog cancelled');
        return def;
    }
    return result;
}

/**
 * `ColourPicker` placeholder — WebView2 lacks a native colour-picker
 * dialog at parity with SMP, so we return the supplied default.
 * Theme code that needs a real picker should use `<input type="color">`.
 */
export function ColourPicker(
    _windowId: unknown,
    defaultColour?: unknown,
): number {
    return typeof defaultColour === 'number' ? defaultColour : 0;
}

/**
 * Detect whether a font face is available via the `document.fonts.check`
 * API. Returns `false` when the document API is not present.
 */
export function CheckFont(name: unknown): boolean {
    if (typeof document === 'undefined') return false;
    try {
        return document.fonts?.check?.(`12px "${name}"`) ?? false;
    } catch {
        return false;
    }
}

// ---------------------------------------------------------------------------
// Aggregate namespace
// ---------------------------------------------------------------------------

/**
 * Aggregate `utils` namespace object exported as a side-effect value.
 *
 * `Version` is exposed via a getter so consumers that read it via
 * `Object.defineProperty(utils, 'Version', { get })` see the same
 * shape (the test harness reads it directly).
 *
 * `__smpUtilsCompat: true` is the idempotency marker checked at
 * install time. Re-loads (e.g. from a hot-reload session) skip the
 * helper redefinitions when this flag is already set.
 */
export const smpUtilsNamespace = {
    get Version(): string {
        return '1.0.0';
    },
    __smpUtilsCompat: true as const,

    // Pure JS
    FormatDuration,
    FormatFileSize,
    SplitFilePath,
    PathWildcardMatch,
    DateStringFromTimestamp,

    // Backend
    FileExists,
    IsFile,
    IsDirectory,
    GetFileSize,
    ReadTextFile,
    ReadUTF8,
    WriteTextFile,
    Glob,
    ListFiles,
    ListFolders,
    GetClipboardText,
    SetClipboardText,
    CheckComponent,
    ReadINI,
    WriteINI,

    // WebView2 fallbacks
    InputBox,
    ColourPicker,
    CheckFont,
} as const;

export type SmpUtilsNamespaceShape = typeof smpUtilsNamespace;

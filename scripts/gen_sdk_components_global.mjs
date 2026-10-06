#!/usr/bin/env node
// scripts/gen_sdk_components_global.mjs
//
// Codegen for `sdk/src/components/generated/global.d.ts` —
// the global TypeScript ambient module that augments the standard
// `HTMLElementTagNameMap` and `HTMLElementEventMap` for every
// `fb-*` Web Component shipped by the SDK.
//
// Why a separate generator
// ────────────────────────
// `gen_sdk_types.mjs` derives interfaces from the API declarations in
// `src/api/schema/`. Web Component tag names and CustomEvent payload
// shapes live entirely in TypeScript source under `sdk/src/components/`,
// so they have no presence in those declarations and therefore no entry
// in that codegen pipeline. Without the
// global augmentation, idiomatic DOM access such as
//
//     const btn = document.querySelector('fb-play-button');
//     btn.addEventListener('fb-play', e => e.detail);
//
// loses every type guarantee.
//
// Inputs
// ──────
// 1. `sdk/src/components/register.ts` — the `defaultComponents` record
//    is the single source of truth for tag → class binding.
// 2. `sdk/src/components/types.ts` — every `Fb*Detail` interface used
//    in CustomEvent payloads.
// 3. `sdk/src/components/Fb*.ts` — `_emit<FbXxxDetail>('fb-name', ...)`
//    call sites build the event-name → detail-type map.
//
// Output
// ──────
// `sdk/src/components/generated/global.d.ts` containing a single
// `declare global { ... }` block with two augmented interfaces:
//
//     interface HTMLElementTagNameMap {
//         'fb-play-button': FbPlayButton;
//         ...
//     }
//     interface HTMLElementEventMap {
//         'fb-play': CustomEvent<FbPlayDetail>;
//         ...
//     }
//
// CLI
// ───
//   node scripts/gen_sdk_components_global.mjs               # write
//   node scripts/gen_sdk_components_global.mjs --diff        # exit 3 on drift
//   node scripts/gen_sdk_components_global.mjs --dry-run     # stdout only
//   node scripts/gen_sdk_components_global.mjs --components-dir DIR
//   node scripts/gen_sdk_components_global.mjs --out PATH
//   node scripts/gen_sdk_components_global.mjs --quiet
//   node scripts/gen_sdk_components_global.mjs --help
//
// Exit codes
// ──────────
//   0  success
//   1  unrecoverable error (bad CLI / missing input file)
//   2  schema violation (orphan event / unknown detail type / unmapped tag)
//   3  drift detected (--diff mode and on-disk content stale)

import fs from 'node:fs';
import path from 'node:path';
import process from 'node:process';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const REPO_ROOT = path.resolve(__dirname, '..');
const DEFAULT_COMPONENTS_DIR = path.join(REPO_ROOT, 'sdk', 'src', 'components');
const DEFAULT_OUT_PATH = path.join(
    DEFAULT_COMPONENTS_DIR,
    'generated',
    'global.d.ts',
);

const EXIT_OK = 0;
const EXIT_ERROR = 1;
const EXIT_SCHEMA = 2;
const EXIT_DRIFT = 3;

// ── CLI parsing ─────────────────────────────────────────────────────

function parseArgs(argv) {
    const opts = {
        componentsDir: DEFAULT_COMPONENTS_DIR,
        outPath: DEFAULT_OUT_PATH,
        diff: false,
        dryRun: false,
        quiet: false,
        help: false,
    };
    for (let i = 0; i < argv.length; i++) {
        const a = argv[i];
        if (a === '--help' || a === '-h') opts.help = true;
        else if (a === '--diff') opts.diff = true;
        else if (a === '--dry-run') opts.dryRun = true;
        else if (a === '--quiet') opts.quiet = true;
        else if (a === '--components-dir') opts.componentsDir = argv[++i];
        else if (a.startsWith('--components-dir=')) {
            opts.componentsDir = a.slice('--components-dir='.length);
        } else if (a === '--out' || a === '-o') opts.outPath = argv[++i];
        else if (a.startsWith('--out=')) {
            opts.outPath = a.slice('--out='.length);
        } else if (a.startsWith('-')) {
            throw new Error(
                `Unknown flag: ${a}\nRun with --help for usage.`,
            );
        }
    }
    return opts;
}

function printHelp() {
    process.stdout.write(
        [
            'Usage: node scripts/gen_sdk_components_global.mjs [options]',
            '',
            'Generates the ambient global type augmentation for all',
            "fb-* Web Components shipped under sdk/src/components/.",
            '',
            'Options:',
            '  --components-dir DIR   override sdk/src/components/ root',
            '  --out PATH             override output path',
            '  --diff                 exit 3 if disk content is stale',
            '  --dry-run              write generated content to stdout',
            '  --quiet                suppress informational stderr output',
            '  --help, -h             show this help',
            '',
            'Exit codes:',
            '  0  success / no drift',
            '  1  unrecoverable error (bad CLI / missing input)',
            '  2  schema violation (orphan event, unknown detail, unmapped tag)',
            '  3  drift detected (--diff)',
            '',
        ].join('\n'),
    );
}

// ── Source extraction ───────────────────────────────────────────────

/**
 * Strip line `//` and block `/* * /` comments so regex passes are not
 * confused by example code in JSDoc. Strings inside comments are
 * removed wholesale; this is sufficient for our token-level scans.
 */
function stripComments(src) {
    return src
        .replace(/\/\*[\s\S]*?\*\//g, '')
        .replace(/(^|[^:])\/\/[^\n]*/g, (_m, prefix) => prefix);
}

/**
 * Parse `defaultComponents` from `register.ts`. Returns a map keyed
 * by tag name (`fb-play-button`) with the constructor identifier
 * (`FbPlayButton`).
 */
export function extractDefaultComponents(registerSource) {
    const stripped = stripComments(registerSource);
    const blockMatch = stripped.match(
        /export\s+const\s+defaultComponents\s*:[^=]+=\s*\{([\s\S]*?)\n\}\s*;/,
    );
    if (!blockMatch) {
        throw new Error(
            "could not locate `export const defaultComponents` " +
                'in register.ts (block boundary regex failed)',
        );
    }
    const body = blockMatch[1];
    const entryRe = /['"]([\w-]+)['"]\s*:\s*([A-Z][\w]*)\s*,?/g;
    const entries = new Map();
    let m;
    while ((m = entryRe.exec(body)) !== null) {
        const tag = m[1];
        const ctor = m[2];
        if (!tag.startsWith('fb-')) {
            throw new Error(
                `defaultComponents tag "${tag}" must start with 'fb-'`,
            );
        }
        if (entries.has(tag)) {
            throw new Error(
                `defaultComponents duplicate tag: ${tag}`,
            );
        }
        entries.set(tag, ctor);
    }
    if (entries.size === 0) {
        throw new Error(
            'defaultComponents is empty — register.ts likely refactored',
        );
    }
    return entries;
}

/**
 * Parse every `export interface FbXxxDetail` / `export interface FbXxxDetail extends ...`
 * declaration from `types.ts`. Returns a `Set<string>` of names.
 */
export function extractDetailInterfaces(typesSource) {
    const stripped = stripComments(typesSource);
    const detailRe = /export\s+interface\s+(Fb\w*Detail)\b/g;
    const names = new Set();
    let m;
    while ((m = detailRe.exec(stripped)) !== null) {
        names.add(m[1]);
    }
    if (names.size === 0) {
        throw new Error(
            'no `Fb*Detail` interfaces found in types.ts',
        );
    }
    return names;
}

/**
 * Scan one component source for `_emit<FbXxxDetail>('fb-name', ...)`
 * call sites. The regex tolerates whitespace/newlines between the
 * generic argument, the opening parenthesis, and the literal event
 * name, which mirrors the prevailing prettier formatting.
 */
export function extractEmitSites(componentSource) {
    const stripped = stripComments(componentSource);
    const emitRe =
        /_emit\s*<\s*(Fb\w*Detail)\s*>\s*\(\s*['"]([\w-]+)['"]/g;
    const sites = [];
    let m;
    while ((m = emitRe.exec(stripped)) !== null) {
        sites.push({ detail: m[1], event: m[2] });
    }
    return sites;
}

// ── Aggregation + validation ────────────────────────────────────────

/**
 * Walk the components directory, collect every `_emit<...>(...)`
 * site, deduplicate by event name, and reject conflicting detail
 * types for the same event.
 */
function collectEvents(componentsDir, knownDetails) {
    const files = fs.readdirSync(componentsDir).filter((f) => {
        if (!f.endsWith('.ts')) return false;
        if (f.endsWith('.d.ts')) return false;
        if (f.startsWith('_')) return false;
        return /^[A-Z]/.test(f);
    });
    const eventMap = new Map();
    const unknownDetails = new Set();
    for (const f of files.sort()) {
        const abs = path.join(componentsDir, f);
        const src = fs.readFileSync(abs, 'utf8');
        const sites = extractEmitSites(src);
        for (const { detail, event } of sites) {
            if (!knownDetails.has(detail)) {
                unknownDetails.add(`${detail} (in ${f}, event=${event})`);
                continue;
            }
            if (!event.startsWith('fb-')) {
                throw new Error(
                    `event name "${event}" in ${f} must start with 'fb-'`,
                );
            }
            const prior = eventMap.get(event);
            if (prior && prior.detail !== detail) {
                throw new Error(
                    `event "${event}" mapped to multiple detail types: ` +
                        `${prior.detail} (${prior.file}) vs ${detail} (${f})`,
                );
            }
            if (!prior) {
                eventMap.set(event, { detail, file: f });
            }
        }
    }
    if (unknownDetails.size) {
        const list = [...unknownDetails].sort().join('\n  - ');
        throw Object.assign(
            new Error(
                'one or more `_emit<X>(...)` call sites reference a ' +
                    'detail type not declared in types.ts:\n  - ' +
                    list,
            ),
            { exitCode: EXIT_SCHEMA },
        );
    }
    return eventMap;
}

// ── Output rendering ────────────────────────────────────────────────

function header() {
    return [
        '// ─────────────────────────────────────────────────────────────',
        '// GENERATED FILE — DO NOT EDIT',
        '// File: sdk/src/components/generated/global.d.ts',
        '// Emitter: scripts/gen_sdk_components_global.mjs',
        '// Regenerate: npm run gen:components-global (in sdk/) or',
        "//             `node scripts/gen_sdk_components_global.mjs`",
        '// ─────────────────────────────────────────────────────────────',
        '',
        '/* eslint-disable */',
        '',
    ].join('\n');
}

function renderImportClassBlock(componentEntries) {
    const ctors = [...new Set(componentEntries.values())].sort();
    const lines = ctors.map((c) => `    ${c},`);
    return ['import type {', ...lines, "} from '../index.js';"].join('\n');
}

function renderImportDetailBlock(eventMap) {
    const details = [...new Set([...eventMap.values()].map((v) => v.detail))]
        .sort();
    const lines = details.map((d) => `    ${d},`);
    return ['import type {', ...lines, "} from '../types.js';"].join('\n');
}

function renderTagNameMap(componentEntries) {
    const lines = [];
    const sorted = [...componentEntries.entries()].sort(([a], [b]) =>
        a.localeCompare(b),
    );
    for (const [tag, ctor] of sorted) {
        lines.push(`        '${tag}': ${ctor};`);
    }
    return [
        '    interface HTMLElementTagNameMap {',
        ...lines,
        '    }',
    ].join('\n');
}

function renderEventMap(eventMap) {
    const lines = [];
    const sorted = [...eventMap.entries()].sort(([a], [b]) =>
        a.localeCompare(b),
    );
    for (const [event, { detail }] of sorted) {
        lines.push(`        '${event}': CustomEvent<${detail}>;`);
    }
    return [
        '    interface HTMLElementEventMap {',
        ...lines,
        '    }',
    ].join('\n');
}

export function renderGlobalDts(componentEntries, eventMap) {
    const sections = [
        header(),
        renderImportClassBlock(componentEntries),
        '',
        renderImportDetailBlock(eventMap),
        '',
        'declare global {',
        renderTagNameMap(componentEntries),
        '',
        renderEventMap(eventMap),
        '}',
        '',
        'export {};',
        '',
    ];
    return sections.join('\n');
}

// ── Stats helpers ───────────────────────────────────────────────────

function summary(componentEntries, eventMap) {
    return (
        `${componentEntries.size} components, ${eventMap.size} events`
    );
}

// ── Entry point ─────────────────────────────────────────────────────

export function run(argv) {
    let opts;
    try {
        opts = parseArgs(argv);
    } catch (err) {
        process.stderr.write(`${err.message}\n`);
        return EXIT_ERROR;
    }
    if (opts.help) {
        printHelp();
        return EXIT_OK;
    }

    const componentsDirAbs = path.isAbsolute(opts.componentsDir)
        ? opts.componentsDir
        : path.resolve(process.cwd(), opts.componentsDir);
    const registerPath = path.join(componentsDirAbs, 'register.ts');
    const typesPath = path.join(componentsDirAbs, 'types.ts');
    if (!fs.existsSync(registerPath)) {
        process.stderr.write(`register.ts not found: ${registerPath}\n`);
        return EXIT_ERROR;
    }
    if (!fs.existsSync(typesPath)) {
        process.stderr.write(`types.ts not found: ${typesPath}\n`);
        return EXIT_ERROR;
    }

    let componentEntries;
    let knownDetails;
    let eventMap;
    try {
        const registerSrc = fs.readFileSync(registerPath, 'utf8');
        const typesSrc = fs.readFileSync(typesPath, 'utf8');
        componentEntries = extractDefaultComponents(registerSrc);
        knownDetails = extractDetailInterfaces(typesSrc);
        eventMap = collectEvents(componentsDirAbs, knownDetails);
    } catch (err) {
        process.stderr.write(`${err.message}\n`);
        return err.exitCode || EXIT_ERROR;
    }

    const content = renderGlobalDts(componentEntries, eventMap);

    if (opts.dryRun) {
        process.stdout.write(content);
        if (!opts.quiet) {
            process.stderr.write(
                `[dry-run] ${summary(componentEntries, eventMap)}\n`,
            );
        }
        return EXIT_OK;
    }

    const outAbs = path.isAbsolute(opts.outPath)
        ? opts.outPath
        : path.resolve(process.cwd(), opts.outPath);

    if (opts.diff) {
        const existing = fs.existsSync(outAbs)
            ? fs.readFileSync(outAbs, 'utf8')
            : null;
        if (existing !== content) {
            process.stderr.write(
                `[diff] ${path
                    .relative(REPO_ROOT, outAbs)
                    .replace(/\\/g, '/')} is ${
                    existing === null ? 'missing' : 'stale'
                }.\n`,
            );
            process.stderr.write(
                'Regenerate via `npm --prefix sdk run gen:components-global` ' +
                    'and stage the result.\n',
            );
            return EXIT_DRIFT;
        }
        if (!opts.quiet) {
            process.stderr.write(
                `[diff] no drift — ${summary(componentEntries, eventMap)}\n`,
            );
        }
        return EXIT_OK;
    }

    fs.mkdirSync(path.dirname(outAbs), { recursive: true });
    fs.writeFileSync(outAbs, content, 'utf8');
    if (!opts.quiet) {
        process.stderr.write(
            `wrote ${path
                .relative(REPO_ROOT, outAbs)
                .replace(/\\/g, '/')} (${summary(componentEntries, eventMap)})\n`,
        );
    }
    return EXIT_OK;
}

// CLI entry — only executed when invoked as a script, not when imported.
// On Windows `import.meta.url` is `file:///E:/...%20...` (with three slashes
// and URL-encoded path), so a string comparison against `process.argv[1]`
// is fragile. `fileURLToPath` round-trips the URL into a real path that
// matches `process.argv[1]` byte-for-byte across platforms.
const isMainModule =
    process.argv[1] &&
    fileURLToPath(import.meta.url) === path.resolve(process.argv[1]);
if (isMainModule) {
    process.exit(run(process.argv.slice(2)));
}

// scripts/audit_sourcemap.mjs
//
// Checks the SDK's published source maps.
//
// Verifies `sdk/dist/*.map` integrity:
//
//   (A) Absolute path leakage — `sources[]` must not contain Windows
//       drive letters, POSIX absolute paths, or any CI path prefix.
//
//   (B) Source-file existence — every `sources[]` entry, resolved
//       relative to the .map file, must point to a real file under
//       `sdk/src/`.
//
//   (C) Entry-source boundary — bridge / components / smp-compat
//       entries must only reference their own primary subtree plus a
//       short shared allowlist (types/ utils/). `index` is an alias
//       entry over `src/bridge/index.ts` (value re-export, not
//       type-only), so `index.js.map` carries the bridge subtree and
//       is held to the same boundary as `bridge` itself.
//
// ESM / IIFE twin pairs (`bridge.js.map` vs `bridge.global.js.map`)
// share the same source tree — this is **not** a violation. The same
// applies to the `index` / `bridge` pair: one bundle, two entry names.
//
// Exit codes:
//   0 = pass, or warn-only with no new violation
//   1 = gate violation (strict mode)
//   2 = script/config error (dist missing, no *.map, etc.)
//
// CLI:
//   --json         Emit JSON (for CI parsing)
//   --strict       Fail with exit 1 on any violation
//   --warn-only    Downgrade violations to warnings, always exit 0
//   --baseline F   Compare against baseline file, only report new
//
// Usage:
//   node scripts/audit_sourcemap.mjs                 # human table
//   node scripts/audit_sourcemap.mjs --json          # JSON report
//   node scripts/audit_sourcemap.mjs --strict        # gate mode
//   node scripts/audit_sourcemap.mjs --warn-only     # report only

import fs from 'node:fs';
import path from 'node:path';
import { argv, exit } from 'node:process';

// Version of the CLI shared by the audit_*.mjs scripts (--help / --strict / --warn-only / --baseline / --json).
export const AUDIT_PROTOCOL_VERSION = 1;

// ============================================================================
// Configuration
// ============================================================================

const ROOT = process.cwd();
const DIST_DIR = path.join(ROOT, 'sdk', 'dist');
const SDK_SRC = path.join(ROOT, 'sdk', 'src');

// Shared subtrees any entry may legitimately reference (kept small on
// purpose; expand only when a cross-entry import is intentional).
const SHARED_ALLOWED_PREFIXES = ['types/', 'utils/'];

// Per-entry primary subtree. `primaryPrefix: null` means the entry is
// expected to emit an empty `sources[]` (type-only re-export).
//
// `index` maps to `bridge/` rather than null: `src/index.ts` re-exports
// the bridge *values* (`export * from './bridge/index.js'` plus the
// default export), so tsup emits a full bridge bundle under the `index`
// name and its map legitimately references the bridge subtree. It was
// type-only when this gate was written (2026-05-03: 68-byte stub,
// `sources: []`); treating it as empty now would flag every bridge
// source in `index.js.map`.
const ENTRY_RULES = [
    { entry: 'bridge', primaryPrefix: 'bridge/' },
    { entry: 'components', primaryPrefix: 'components/' },
    { entry: 'smp-compat', primaryPrefix: 'smp/' },
    { entry: 'index', primaryPrefix: 'bridge/' },
    { entry: 'schema', primaryPrefix: 'schema/' },
];

// ============================================================================
// CLI parsing
// ============================================================================

const args = argv.slice(2);
const flags = {
    json: args.includes('--json'),
    strict: args.includes('--strict'),
    warnOnly: args.includes('--warn-only'),
    baseline: null,
};

const baselineIdx = args.indexOf('--baseline');
if (baselineIdx >= 0 && args[baselineIdx + 1]) {
    flags.baseline = args[baselineIdx + 1];
}

if (args.includes('--help') || args.includes('-h')) {
    console.log(
        [
            'Usage: node scripts/audit_sourcemap.mjs [--json] [--strict|--warn-only] [--baseline <file>] [--help|-h]',
            '',
            'Checks the source maps in sdk/dist.',
            'Verifies sdk/dist/*.map integrity: (A) no absolute-path leakage,',
            '(B) every sources[] entry resolves to a real file under sdk/src,',
            '(C) entry-source subtree isolation (bridge/components/smp-compat',
            'entries only reference their own primary subtree plus shared allow-list).',
            '',
            'Options:',
            '  --json             Emit machine-readable JSON report (suitable for CI / baseline diff)',
            '  --strict           Fail with exit 1 on any violation',
            '  --warn-only        Downgrade all findings to warnings; always exit 0',
            '  --baseline <file>  Compare against a JSON baseline; only NEW violations are surfaced',
            '  --help, -h         Print this usage and exit 0',
            '',
            'Exit codes:',
            '  0  pass / warn-only / no NEW violation vs baseline',
            '  1  --strict + violations present',
            '  2  script/config error (sdk/dist missing, no *.map, etc.)',
        ].join('\n'),
    );
    exit(0);
}

// ============================================================================
// Core helpers
// ============================================================================

/**
 * Absolute-path leak detector. Matches Windows drive letters
 * (`C:\...` or `C:/...`) and POSIX absolute paths (`/home/...`).
 * Explicitly excludes conventional relative paths (`../`).
 */
function isAbsolutePath(p) {
    if (typeof p !== 'string' || p.length === 0) return false;
    if (/^[A-Za-z]:[\\/]/.test(p)) return true;
    if (p.startsWith('/')) return true;
    return false;
}

/**
 * Parse a dist map filename into { entry, variant }.
 *   bridge.js.map          → { entry: 'bridge', variant: 'esm' }
 *   bridge.global.js.map   → { entry: 'bridge', variant: 'iife' }
 *   smp-compat.js.map      → { entry: 'smp-compat', variant: 'esm' }
 */
function parseEntry(filename) {
    const m = filename.match(/^(.+?)(\.global)?\.js\.map$/);
    if (!m) return { entry: 'unknown', variant: 'unknown' };
    return { entry: m[1], variant: m[2] ? 'iife' : 'esm' };
}

/**
 * Load a sourcemap and return its `sources[]` (defensive: may be absent
 * or a non-array on malformed maps).
 */
function loadMap(mapPath) {
    const raw = fs.readFileSync(mapPath, 'utf8');
    let parsed;
    try {
        parsed = JSON.parse(raw);
    } catch (err) {
        return { ok: false, error: `invalid JSON: ${err.message}`, sources: [] };
    }
    const sources = Array.isArray(parsed.sources) ? parsed.sources : [];
    return { ok: true, sources, file: parsed.file };
}

/**
 * Core audit entry point. Accepts an explicit `distDir` so fixture
 * tests can point at a synthetic tree without touching real output.
 *
 * Returns a structured report without touching stdout / exit code —
 * the CLI layer owns presentation + exit semantics.
 */
export function audit(distDir = DIST_DIR, sdkSrc = SDK_SRC) {
    if (!fs.existsSync(distDir)) {
        return {
            ok: false,
            error: `dist directory not found: ${distDir}`,
            entries: [],
            violations: { A: [], B: [], C: [] },
            total: 0,
        };
    }

    const mapFiles = fs
        .readdirSync(distDir)
        .filter((f) => f.endsWith('.js.map'))
        .sort();

    if (mapFiles.length === 0) {
        return {
            ok: false,
            error: `no *.js.map files in ${distDir}`,
            entries: [],
            violations: { A: [], B: [], C: [] },
            total: 0,
        };
    }

    const violations = { A: [], B: [], C: [] };
    const entries = [];

    for (const filename of mapFiles) {
        const mapPath = path.join(distDir, filename);
        const { ok, error, sources } = loadMap(mapPath);
        const info = parseEntry(filename);

        if (!ok) {
            violations.B.push({
                map: filename,
                source: '<map parse error>',
                reason: error,
            });
            entries.push({
                file: filename,
                entry: info.entry,
                variant: info.variant,
                sourceCount: 0,
                parseError: error,
            });
            continue;
        }

        const rule = ENTRY_RULES.find((r) => r.entry === info.entry);

        // Per-entry empty-sources contract (e.g. index.js.map)
        if (rule && rule.primaryPrefix === null && sources.length > 0) {
            for (const src of sources) {
                violations.C.push({
                    map: filename,
                    entry: info.entry,
                    source: src,
                    reason: 'expected-empty',
                });
            }
        }

        for (const src of sources) {
            // (A) absolute-path leak
            if (isAbsolutePath(src)) {
                violations.A.push({ map: filename, source: src });
                continue;
            }

            // (B) source file existence
            const resolved = path.resolve(distDir, src);
            if (!fs.existsSync(resolved)) {
                violations.B.push({
                    map: filename,
                    source: src,
                    resolved,
                });
                continue;
            }

            // (C) entry-source boundary (only when rule defines a
            // primaryPrefix; empty-sources contract is handled above).
            if (rule && rule.primaryPrefix !== null) {
                const relToSrc = path
                    .relative(sdkSrc, resolved)
                    .replace(/\\/g, '/');

                const insidePrimary = relToSrc.startsWith(rule.primaryPrefix);
                const insideShared = SHARED_ALLOWED_PREFIXES.some((p) =>
                    relToSrc.startsWith(p),
                );

                if (!insidePrimary && !insideShared) {
                    violations.C.push({
                        map: filename,
                        entry: info.entry,
                        source: relToSrc,
                        primaryPrefix: rule.primaryPrefix,
                        sharedPrefixes: SHARED_ALLOWED_PREFIXES,
                    });
                }
            }
        }

        entries.push({
            file: filename,
            entry: info.entry,
            variant: info.variant,
            sourceCount: sources.length,
        });
    }

    const total =
        violations.A.length + violations.B.length + violations.C.length;

    return { ok: true, entries, violations, total };
}

// ============================================================================
// Baseline diff
// ============================================================================

function loadBaseline(file) {
    if (!fs.existsSync(file)) return null;
    try {
        return JSON.parse(fs.readFileSync(file, 'utf8'));
    } catch {
        return null;
    }
}

function diffBaseline(current, baseline) {
    if (!baseline || !baseline.violations) return current;
    const sig = (v) =>
        [v.map, v.source, v.entry || '', v.reason || ''].join('|');
    const baselineSet = new Set(
        [
            ...(baseline.violations.A || []),
            ...(baseline.violations.B || []),
            ...(baseline.violations.C || []),
        ].map(sig),
    );
    const filtered = {
        A: current.violations.A.filter((v) => !baselineSet.has(sig(v))),
        B: current.violations.B.filter((v) => !baselineSet.has(sig(v))),
        C: current.violations.C.filter((v) => !baselineSet.has(sig(v))),
    };
    return {
        ...current,
        violations: filtered,
        total: filtered.A.length + filtered.B.length + filtered.C.length,
    };
}

// ============================================================================
// Output
// ============================================================================

function printTable(report) {
    console.log('===== sdk/dist sourcemap audit =====');
    console.log(`Maps inspected: ${report.entries.length}`);
    console.log();

    console.log('Per-entry summary:');
    for (const e of report.entries) {
        const tag = e.parseError ? ` [parse-error: ${e.parseError}]` : '';
        console.log(
            `  ${e.file.padEnd(30)} [${e.variant.padEnd(4)}] sources=${e.sourceCount}${tag}`,
        );
    }
    console.log();

    const { A, B, C } = report.violations;

    if (A.length > 0) {
        console.log(`[A] Absolute path leak: ${A.length}`);
        for (const v of A) console.log(`  - ${v.map} → ${v.source}`);
        console.log();
    }
    if (B.length > 0) {
        console.log(`[B] Unresolved source (file missing): ${B.length}`);
        for (const v of B) {
            const why = v.reason ? ` (${v.reason})` : '';
            const resolved = v.resolved ? ` → ${v.resolved}` : '';
            console.log(`  - ${v.map}: ${v.source}${why}${resolved}`);
        }
        console.log();
    }
    if (C.length > 0) {
        console.log(`[C] Entry-source boundary violation: ${C.length}`);
        for (const v of C) {
            if (v.reason === 'expected-empty') {
                console.log(
                    `  - ${v.map} (entry='${v.entry}' expected empty sources) → ${v.source}`,
                );
            } else {
                console.log(
                    `  - ${v.map} (entry='${v.entry}' primary='${v.primaryPrefix}', shared=${JSON.stringify(v.sharedPrefixes)}) → ${v.source}`,
                );
            }
        }
        console.log();
    }

    console.log(
        `Total violations: ${report.total} (A=${A.length} B=${B.length} C=${C.length})`,
    );
}

// ============================================================================
// CLI entry
// ============================================================================

function mainCli() {
    let report = audit();

    if (!report.ok) {
        if (flags.json) {
            console.log(JSON.stringify({ ok: false, error: report.error }));
        } else {
            console.error(`[audit_sourcemap] ${report.error}`);
            console.error('Hint: run `npm --prefix sdk run build` first.');
        }
        exit(2);
    }

    if (flags.baseline) {
        const baseline = loadBaseline(flags.baseline);
        report = diffBaseline(report, baseline);
    }

    if (flags.json) {
        console.log(JSON.stringify(report, null, 2));
    } else {
        printTable(report);
    }

    if (report.total === 0) exit(0);
    if (flags.warnOnly && !flags.strict) exit(0);
    exit(1);
}

// Only run CLI when invoked directly (not when imported by tests).
const isDirectRun =
    import.meta.url === `file://${process.argv[1].replace(/\\/g, '/')}` ||
    import.meta.url.endsWith(path.basename(process.argv[1]));
if (isDirectRun) mainCli();

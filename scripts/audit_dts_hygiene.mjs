// scripts/audit_dts_hygiene.mjs
//
// Checks the SDK's published declaration files for internal leftovers.
//
// Scans `sdk/dist/*.d.ts` (all public declaration files including
// auto-generated chunks like `events-<hash>.d.ts`) for four classes of
// leakage:
//
//   (I)   @internal / @internal-only / @private tags that should have
//         been stripped by `tsconfig.json#stripInternal: true`.
//
//   (II)  @alpha / @beta TSDoc stability markers. Current baseline is
//         zero — preventive gate so unstable APIs cannot silently
//         reach consumers.
//
//   (III) Plan-reference strings (Phase \d, SMP 兼容层迁移,
//         SDK_TYPESCRIPT_MIGRATION_PLAN, ...) leaked from dev docs
//         into consumer-facing .d.ts.
//
//   (IV)  Temporary markers: TODO / FIXME / XXX in JSDoc or inline
//         comments.
//
//   (V)   CJK characters: even if every source JSDoc is English, a
//         build step that injects Chinese text into dist/*.d.ts is
//         still caught. Range: [\u2E80-\u9FFF\uF900-\uFAFF].
//
// Exit codes:
//   0 = pass, or warn-only with no new violation
//   1 = gate violation (strict mode)
//   2 = script/config error (dist missing, no *.d.ts, etc.)
//
// CLI:
//   --json         Emit JSON (for CI parsing)
//   --strict       Fail with exit 1 on any violation
//   --warn-only    Downgrade violations to warnings, always exit 0
//   --baseline F   Compare against baseline file, only report new
//
// Usage:
//   node scripts/audit_dts_hygiene.mjs                # human table
//   node scripts/audit_dts_hygiene.mjs --json         # JSON report
//   node scripts/audit_dts_hygiene.mjs --strict       # gate mode
//   node scripts/audit_dts_hygiene.mjs --warn-only    # report only

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

// Detection rules. Each rule returns a list of { line, lineNo, match }
// for every hit inside one file. Line numbers are 1-indexed.
const RULES = [
    {
        id: 'I',
        name: 'internal tag leakage',
        pattern: /@(?:internal(?:-only)?|private)\b/g,
        description: '@internal / @internal-only / @private JSDoc tag',
    },
    {
        id: 'II',
        name: 'stability tag leakage',
        pattern: /@(?:alpha|beta)\b/g,
        description: '@alpha / @beta TSDoc stability marker',
    },
    {
        id: 'III',
        name: 'plan-reference leakage',
        // Careful patterns to avoid false positives:
        //   - "Phase \d" (dev phase tags: Phase 2, Phase 5.1, ...)
        //   - "SDK_TYPESCRIPT_MIGRATION_PLAN" (plan anchor)
        //   - "SMP 兼容层迁移" (specific migration jargon)
        pattern:
            /(?:Phase\s+\d+(?:\.\d+)?|SDK_TYPESCRIPT_MIGRATION_PLAN|SMP\s*兼容层迁移)/g,
        description: 'dev-plan reference (Phase N / migration plan anchor)',
    },
    {
        id: 'IV',
        name: 'temporary marker',
        pattern: /\b(?:TODO|FIXME|XXX)\b/g,
        description: 'TODO / FIXME / XXX temporary marker',
    },
    {
        id: 'V',
        name: 'CJK leakage',
        pattern: /[\u2E80-\u9FFF\uF900-\uFAFF]+/g,
        description: 'Chinese/CJK character leaked into public DTS (G5 partner)',
    },
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
            'Usage: node scripts/audit_dts_hygiene.mjs [--json] [--strict|--warn-only] [--baseline <file>] [--help|-h]',
            '',
            'Checks sdk/dist/*.d.ts for internal leftovers.',
            'Scans sdk/dist/*.d.ts for 5 classes of leakage: (I) @internal /',
            '@private tags that stripInternal missed, (II) @alpha / @beta',
            'stability markers, (III) Phase N / SDK_TYPESCRIPT_MIGRATION_PLAN',
            'plan-reference strings, (IV) TODO / FIXME / XXX markers,',
            '(V) CJK character leakage into consumer-facing .d.ts.',
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
            '  2  script/config error (sdk/dist missing, no *.d.ts, etc.)',
        ].join('\n'),
    );
    exit(0);
}

// ============================================================================
// Core
// ============================================================================

/**
 * Scan a single `.d.ts` string against every rule, returning a flat
 * array of { ruleId, file, lineNo, line, match } for each hit. Line
 * numbers are 1-indexed. Duplicate-per-line hits all reported.
 */
function scanFile(content, filename) {
    const lines = content.split(/\r?\n/);
    const hits = [];

    for (const rule of RULES) {
        for (let i = 0; i < lines.length; i++) {
            const lineText = lines[i];
            rule.pattern.lastIndex = 0; // reset global regex state
            let m;
            while ((m = rule.pattern.exec(lineText)) !== null) {
                hits.push({
                    ruleId: rule.id,
                    file: filename,
                    lineNo: i + 1,
                    line: lineText.trim().slice(0, 160),
                    match: m[0],
                });
                if (m.index === rule.pattern.lastIndex) {
                    rule.pattern.lastIndex++; // avoid zero-width infloop
                }
            }
        }
    }

    return hits;
}

/**
 * Core audit entry point. `distDir` is resolved caller-side so fixture
 * tests can point at a synthetic tree.
 */
export function audit(distDir = DIST_DIR) {
    if (!fs.existsSync(distDir)) {
        return {
            ok: false,
            error: `dist directory not found: ${distDir}`,
            files: [],
            violations: {},
            total: 0,
        };
    }

    const dtsFiles = fs
        .readdirSync(distDir)
        .filter((f) => f.endsWith('.d.ts'))
        .sort();

    if (dtsFiles.length === 0) {
        return {
            ok: false,
            error: `no *.d.ts files in ${distDir}`,
            files: [],
            violations: {},
            total: 0,
        };
    }

    // Initialise one bucket per rule id.
    const violations = Object.fromEntries(RULES.map((r) => [r.id, []]));

    const filePerFileSummary = [];

    for (const filename of dtsFiles) {
        const fp = path.join(distDir, filename);
        const content = fs.readFileSync(fp, 'utf8');
        const hits = scanFile(content, filename);

        const perRuleCount = Object.fromEntries(RULES.map((r) => [r.id, 0]));
        for (const h of hits) {
            violations[h.ruleId].push(h);
            perRuleCount[h.ruleId] += 1;
        }

        filePerFileSummary.push({
            file: filename,
            size: content.length,
            lines: content.split(/\r?\n/).length,
            counts: perRuleCount,
        });
    }

    let total = 0;
    for (const r of RULES) total += violations[r.id].length;

    return { ok: true, files: filePerFileSummary, violations, total };
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
    const sig = (v) => [v.ruleId, v.file, v.lineNo, v.match].join('|');

    const baselineSet = new Set();
    for (const r of RULES) {
        for (const v of baseline.violations[r.id] || []) {
            baselineSet.add(sig(v));
        }
    }

    const filteredViolations = {};
    let total = 0;
    for (const r of RULES) {
        filteredViolations[r.id] = (current.violations[r.id] || []).filter(
            (v) => !baselineSet.has(sig(v)),
        );
        total += filteredViolations[r.id].length;
    }

    return { ...current, violations: filteredViolations, total };
}

// ============================================================================
// Output
// ============================================================================

function printTable(report) {
    console.log('===== sdk/dist DTS hygiene audit (G6) =====');
    console.log(`Files inspected: ${report.files.length}`);
    console.log();

    console.log('Per-file counts (I=internal II=stability III=plan IV=todo V=CJK):');
    console.log(
        '  ' +
            'file'.padEnd(32) +
            'size'.padStart(8) +
            '  I'.padStart(5) +
            'II'.padStart(5) +
            'III'.padStart(5) +
            'IV'.padStart(5) +
            '    V',
    );
    for (const f of report.files) {
        const c = f.counts;
        console.log(
            '  ' +
                f.file.padEnd(32) +
                String(f.size).padStart(8) +
                String(c.I).padStart(5) +
                String(c.II).padStart(5) +
                String(c.III).padStart(5) +
                String(c.IV).padStart(5) +
                String(c.V).padStart(5),
        );
    }
    console.log();

    for (const rule of RULES) {
        const hits = report.violations[rule.id] || [];
        if (hits.length === 0) continue;
        console.log(
            `[${rule.id}] ${rule.description}: ${hits.length}`,
        );
        // Cap per-rule output to avoid drowning the console on big
        // CJK leakage baselines.
        const shown = hits.slice(0, 10);
        for (const h of shown) {
            const snippet = h.line.length > 90 ? h.line.slice(0, 87) + '...' : h.line;
            console.log(`  - ${h.file}:${h.lineNo} | "${h.match}" | ${snippet}`);
        }
        if (hits.length > shown.length) {
            console.log(`  ... (+${hits.length - shown.length} more)`);
        }
        console.log();
    }

    const totals = Object.fromEntries(
        RULES.map((r) => [r.id, (report.violations[r.id] || []).length]),
    );
    console.log(
        `Total violations: ${report.total} (I=${totals.I} II=${totals.II} III=${totals.III} IV=${totals.IV} V=${totals.V})`,
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
            console.error(`[audit_dts_hygiene] ${report.error}`);
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

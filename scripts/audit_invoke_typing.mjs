// scripts/audit_invoke_typing.mjs
//
// Finds under-typed bridge calls in the SDK source.
//
// Detects under-typed bridge invoke / event subscriptions in
// `sdk/src/**/*.ts` (excluding *.test.ts and __tests__/), so they can be
// replaced with concrete response types.
//
// Three classes of weakness are reported:
//
//   (A) `Promise<unknown>` / `Promise<any>` return type on a function /
//       arrow-function declaration. Direct caller-visible weakness.
//
//   (B) `bridge.invoke<unknown>` / `invoke<any>` / `invoke<{}>` call
//       sites. Usually paired with (A) but may appear on `void`-typed
//       wrappers too.
//
//   (C) `bridge.on<unknown>` / `on<any>` event subscriptions.
//
// Detection strategy: text-pattern. The weaknesses are these exact
// tokens, so a regex pass is deterministic, fast, and dependency-free. The fixture suite covers true-positive / true-negative.
//
// Exit codes:
//   0 = pass, or --warn-only
//   1 = violations present (strict, the default)
//   2 = script/config error
//
// CLI:
//   --json         Emit JSON
//   --strict       Fail with exit 1 on any violation (default)
//   --warn-only    Always exit 0
//   --baseline F   Compare against baseline file, only report new
//
// Usage:
//   node scripts/audit_invoke_typing.mjs                # human table
//   node scripts/audit_invoke_typing.mjs --json         # JSON
//   node scripts/audit_invoke_typing.mjs --strict       # gate mode
//   node scripts/audit_invoke_typing.mjs --warn-only    # explicit

import fs from 'node:fs';
import path from 'node:path';
import { argv, exit } from 'node:process';

// Version of the CLI shared by the audit_*.mjs scripts (--help / --strict / --warn-only / --baseline / --json).
export const AUDIT_PROTOCOL_VERSION = 1;

const ROOT = process.cwd();
const SDK_SRC = path.join(ROOT, 'sdk', 'src');

const RULES = [
    {
        id: 'A',
        name: 'weak Promise return',
        // Match `Promise<unknown>`, `Promise<any>`, `Promise<{}>` where
        // the surrounding context is a return / declaration position.
        // Negation lookarounds keep `Array<Promise<unknown>>` (a typed
        // collection of weak promises — still weak) reported.
        pattern: /Promise\s*<\s*(?:unknown|any|\{\s*\})\s*>/g,
        description:
            '`Promise<unknown>` / `Promise<any>` / `Promise<{}>` return type',
    },
    {
        id: 'B',
        name: 'weak invoke generic',
        pattern: /\binvoke\s*<\s*(?:unknown|any|\{\s*\})\s*>/g,
        description:
            '`bridge.invoke<unknown>` / `invoke<any>` / `invoke<{}>` call site',
    },
    {
        id: 'C',
        name: 'weak event subscription',
        pattern: /\b(?:on|once|off)\s*<\s*(?:unknown|any|\{\s*\})\s*>/g,
        description:
            '`bridge.on<unknown>` / `once<unknown>` / `off<unknown>` weak event subscription',
    },
];

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
            'Usage: node scripts/audit_invoke_typing.mjs [--json] [--strict|--warn-only] [--baseline <file>] [--help|-h]',
            '',
            'Finds under-typed bridge calls in sdk/src.',
            'Detects under-typed bridge invoke / event subscriptions in sdk/src/**/*.ts',
            '(excluding *.test.ts and __tests__/):',
            '(A) Promise<unknown> / Promise<any> / Promise<{}> return types',
            '(B) bridge.invoke<unknown> / invoke<any> / invoke<{}> call sites',
            '(C) bridge.on<unknown> / on<any> subscription call sites.',
            '',
            'Options:',
            '  --json             Emit machine-readable JSON report (suitable for CI / baseline diff)',
            '  --strict           Fail with exit 1 on any violation (default)',
            '  --warn-only        Downgrade all findings to warnings; always exit 0 (opt-in diagnostic)',
            '  --baseline <file>  Compare against a JSON baseline; only NEW violations are surfaced',
            '  --help, -h         Print this usage and exit 0',
            '',
            'Exit codes:',
            '  0  pass / warn-only / no NEW violation vs baseline',
            '  1  violations present (strict, the default)',
            '  2  script/config error (sdk/src missing, etc.)',
        ].join('\n'),
    );
    exit(0);
}

function collectTsFiles(dir) {
    const out = [];
    if (!fs.existsSync(dir)) return out;
    const entries = fs.readdirSync(dir, { withFileTypes: true });
    for (const e of entries) {
        const full = path.join(dir, e.name);
        if (e.isDirectory()) {
            if (e.name === '__tests__') continue;
            out.push(...collectTsFiles(full));
        } else if (e.isFile() && e.name.endsWith('.ts')) {
            if (e.name.endsWith('.test.ts')) continue;
            out.push(full);
        }
    }
    return out;
}

/**
 * Strip comments (single-line `//` and block `/* ... *\/`) from a
 * source string so JSDoc examples and disabled code don't pollute
 * the violation count.
 */
function stripComments(src) {
    let out = '';
    let i = 0;
    let inBlock = false;
    let inLine = false;
    let inString = null;
    while (i < src.length) {
        const c = src[i];
        const next = src[i + 1];
        if (inBlock) {
            if (c === '*' && next === '/') {
                inBlock = false;
                i += 2;
            } else {
                i++;
            }
            continue;
        }
        if (inLine) {
            if (c === '\n') {
                inLine = false;
                out += c;
            }
            i++;
            continue;
        }
        if (inString) {
            out += c;
            if (c === '\\' && i + 1 < src.length) {
                out += src[i + 1];
                i += 2;
                continue;
            }
            if (c === inString) inString = null;
            i++;
            continue;
        }
        if (c === '/' && next === '*') {
            inBlock = true;
            i += 2;
            continue;
        }
        if (c === '/' && next === '/') {
            inLine = true;
            i += 2;
            continue;
        }
        if (c === '"' || c === "'" || c === '`') {
            inString = c;
            out += c;
            i++;
            continue;
        }
        out += c;
        i++;
    }
    return out;
}

function scanFile(content, filename) {
    const stripped = stripComments(content);
    const hits = [];
    const originalLines = content.split(/\r?\n/);
    const strippedLines = stripped.split(/\r?\n/);

    for (const rule of RULES) {
        for (let i = 0; i < strippedLines.length; i++) {
            const line = strippedLines[i];
            rule.pattern.lastIndex = 0;
            let m;
            while ((m = rule.pattern.exec(line)) !== null) {
                hits.push({
                    ruleId: rule.id,
                    file: filename,
                    lineNo: i + 1,
                    match: m[0],
                    snippet: (originalLines[i] || '').trim().slice(0, 160),
                });
                if (m.index === rule.pattern.lastIndex) {
                    rule.pattern.lastIndex++;
                }
            }
        }
    }
    return hits;
}

export function audit(srcDir = SDK_SRC) {
    if (!fs.existsSync(srcDir)) {
        return {
            ok: false,
            error: `source directory not found: ${srcDir}`,
            files: [],
            violations: {},
            total: 0,
        };
    }

    const tsFiles = collectTsFiles(srcDir);
    if (tsFiles.length === 0) {
        return {
            ok: false,
            error: `no *.ts files under ${srcDir}`,
            files: [],
            violations: {},
            total: 0,
        };
    }

    const violations = Object.fromEntries(RULES.map((r) => [r.id, []]));
    const perFile = [];

    for (const fp of tsFiles.sort()) {
        const content = fs.readFileSync(fp, 'utf8');
        const rel = path.relative(srcDir, fp).replace(/\\/g, '/');
        const hits = scanFile(content, rel);

        const counts = Object.fromEntries(RULES.map((r) => [r.id, 0]));
        for (const h of hits) {
            violations[h.ruleId].push(h);
            counts[h.ruleId] += 1;
        }
        perFile.push({ file: rel, size: content.length, counts });
    }

    let total = 0;
    for (const r of RULES) total += violations[r.id].length;

    return { ok: true, files: perFile, violations, total };
}

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
    const set = new Set();
    for (const r of RULES)
        for (const v of baseline.violations[r.id] || []) set.add(sig(v));
    const filtered = {};
    let total = 0;
    for (const r of RULES) {
        filtered[r.id] = (current.violations[r.id] || []).filter(
            (v) => !set.has(sig(v)),
        );
        total += filtered[r.id].length;
    }
    return { ...current, violations: filtered, total };
}

function printTable(report) {
    console.log('===== sdk/src invoke-typing audit (G2) =====');
    console.log(`Files inspected: ${report.files.length}`);

    const dirty = report.files.filter(
        (f) => f.counts.A + f.counts.B + f.counts.C > 0,
    );
    console.log(`Files with weak typings: ${dirty.length}`);
    console.log();

    if (dirty.length > 0) {
        console.log(
            '  ' +
                'file'.padEnd(50) +
                'A'.padStart(5) +
                'B'.padStart(5) +
                'C'.padStart(5),
        );
        for (const f of dirty.slice(0, 30)) {
            console.log(
                '  ' +
                    f.file.padEnd(50) +
                    String(f.counts.A).padStart(5) +
                    String(f.counts.B).padStart(5) +
                    String(f.counts.C).padStart(5),
            );
        }
        if (dirty.length > 30)
            console.log(`  ... (+${dirty.length - 30} more files)`);
        console.log();
    }

    for (const rule of RULES) {
        const hits = report.violations[rule.id] || [];
        if (hits.length === 0) continue;
        console.log(`[${rule.id}] ${rule.description}: ${hits.length}`);
        for (const h of hits.slice(0, 5)) {
            console.log(
                `  - ${h.file}:${h.lineNo} | "${h.match}" | ${h.snippet.slice(0, 100)}`,
            );
        }
        if (hits.length > 5) console.log(`  ... (+${hits.length - 5} more)`);
        console.log();
    }

    const t = Object.fromEntries(
        RULES.map((r) => [r.id, (report.violations[r.id] || []).length]),
    );
    console.log(
        `Total weak typings: ${report.total} (A=${t.A} B=${t.B} C=${t.C})`,
    );
    console.log(
        'Mode: strict. Any new weak typing fails the check.',
    );
}

function mainCli() {
    let report = audit();
    if (!report.ok) {
        if (flags.json)
            console.log(JSON.stringify({ ok: false, error: report.error }));
        else console.error(`[audit_invoke_typing] ${report.error}`);
        exit(2);
    }
    if (flags.baseline) {
        const baseline = loadBaseline(flags.baseline);
        report = diffBaseline(report, baseline);
    }
    if (flags.json) console.log(JSON.stringify(report, null, 2));
    else printTable(report);
    if (report.total === 0) exit(0);
    if (flags.warnOnly && !flags.strict) exit(0);
    // Strict unless --warn-only is set.
    exit(1);
}

const isDirectRun =
    import.meta.url === `file://${process.argv[1].replace(/\\/g, '/')}` ||
    import.meta.url.endsWith(path.basename(process.argv[1]));
if (isDirectRun) mainCli();

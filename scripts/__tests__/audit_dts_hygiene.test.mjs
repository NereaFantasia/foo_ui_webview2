// scripts/__tests__/audit_dts_hygiene.test.mjs
//
// Fixture tests for scripts/audit_dts_hygiene.mjs, which checks the SDK's
// published declaration files for internal leftovers.
//
// Covers:
//   F1     true-negative - clean public .d.ts, 0 violations
//   F2     (I) @internal / @private tag leakage
//   F3     (II) @alpha / @beta stability markers
//   F4     (III) Phase N / SDK_TYPESCRIPT_MIGRATION_PLAN plan-reference
//   F5     (IV) TODO / FIXME / XXX markers
//   F6     (V) CJK character leakage into public DTS
//   F7     error path - dist missing - ok=false
//   F8     error path - dist present but no *.d.ts - ok=false
//   F9     baseline diff - known violation filtered out
//   FCLI-1 CLI - --help five-section contract
//   FCLI-2 CLI - --json parseable + required keys
//   FCLI-3 CLI - --strict + violations - exit 1  (negative red line)
//   FCLI-4 CLI - --warn-only + violations - exit 0
//   FCLI-5 CLI - dist missing - exit 2
//
// Uses node:test + node:assert. Zero external deps.
//
// Run:
//   node --test scripts/__tests__/audit_dts_hygiene.test.mjs

import { test, describe, before, after } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

import { audit, AUDIT_PROTOCOL_VERSION } from '../audit_dts_hygiene.mjs';

const SCRIPT_PATH = fileURLToPath(
    new URL('../audit_dts_hygiene.mjs', import.meta.url),
);

function spawnAudit(args, cwd) {
    return spawnSync(process.execPath, [SCRIPT_PATH, ...args], {
        cwd: cwd || process.cwd(),
        encoding: 'utf8',
        env: { ...process.env, NODE_NO_WARNINGS: '1' },
    });
}

function makeWorkspace({ dist = true } = {}) {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'audit-dts-'));
    const distDir = path.join(root, 'sdk', 'dist');
    if (dist) fs.mkdirSync(distDir, { recursive: true });
    return {
        root,
        distDir,
        cleanup() {
            fs.rmSync(root, { recursive: true, force: true });
        },
    };
}

function writeFile(dir, relPath, content) {
    const full = path.join(dir, relPath);
    fs.mkdirSync(path.dirname(full), { recursive: true });
    fs.writeFileSync(full, content, 'utf8');
    return full;
}

// CJK sample built from \u escapes so the test SOURCE stays pure ASCII while
// the WRITTEN fixture file carries real CJK codepoints in [\u2E80-\u9FFF].
const CJK = '\u64ad\u653e\u5f53\u524d\u66f2\u76ee'; // play-current-track

// ============================================================================
// Sanity - protocol version
// ============================================================================

describe('audit_dts_hygiene - protocol version', () => {
    test('exports AUDIT_PROTOCOL_VERSION = 1', () => {
        assert.equal(AUDIT_PROTOCOL_VERSION, 1);
    });
});

// ============================================================================
// F1 - True-negative
// ============================================================================

describe('audit_dts_hygiene - F1 true-negative', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeFile(
            ws.distDir,
            'bridge.d.ts',
            `export interface Bridge {
    /** Play a track. */
    play(): Promise<void>;
}
`,
        );
    });
    after(() => ws.cleanup());

    test('ok=true, total=0', () => {
        const r = audit(ws.distDir);
        assert.equal(r.ok, true);
        assert.equal(r.total, 0);
    });
});

// ============================================================================
// F2 - (I) internal tag leakage
// ============================================================================

describe('audit_dts_hygiene - F2 internal tag', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeFile(
            ws.distDir,
            'bridge.d.ts',
            `/** @internal do not ship */
export declare const secret: number;
/** @private */
export declare const hidden: number;
`,
        );
    });
    after(() => ws.cleanup());

    test('violations.I reports internal/private', () => {
        const r = audit(ws.distDir);
        assert.equal(r.ok, true);
        assert.ok(r.violations.I.length >= 2);
    });
});

// ============================================================================
// F3 - (II) stability markers
// ============================================================================

describe('audit_dts_hygiene - F3 stability markers', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeFile(
            ws.distDir,
            'bridge.d.ts',
            `/** @alpha */
export declare const a: number;
/** @beta */
export declare const b: number;
`,
        );
    });
    after(() => ws.cleanup());

    test('violations.II reports alpha/beta', () => {
        const r = audit(ws.distDir);
        assert.equal(r.ok, true);
        assert.equal(r.violations.II.length, 2);
    });
});

// ============================================================================
// F4 - (III) plan-reference
// ============================================================================

describe('audit_dts_hygiene - F4 plan-reference', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeFile(
            ws.distDir,
            'bridge.d.ts',
            `/** Added in Phase 5 per SDK_TYPESCRIPT_MIGRATION_PLAN. */
export declare const a: number;
`,
        );
    });
    after(() => ws.cleanup());

    test('violations.III reports plan refs', () => {
        const r = audit(ws.distDir);
        assert.equal(r.ok, true);
        assert.ok(r.violations.III.length >= 1);
    });
});

// ============================================================================
// F5 - (IV) temporary markers
// ============================================================================

describe('audit_dts_hygiene - F5 temporary markers', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeFile(
            ws.distDir,
            'bridge.d.ts',
            `// TODO: finish this
// FIXME: broken
export declare const a: number;
`,
        );
    });
    after(() => ws.cleanup());

    test('violations.IV reports TODO + FIXME', () => {
        const r = audit(ws.distDir);
        assert.equal(r.ok, true);
        assert.ok(r.violations.IV.length >= 2);
    });
});

// ============================================================================
// F6 - (V) CJK leakage
// ============================================================================

describe('audit_dts_hygiene - F6 CJK leakage', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeFile(
            ws.distDir,
            'bridge.d.ts',
            `/** ${CJK} */
export declare const a: number;
`,
        );
    });
    after(() => ws.cleanup());

    test('violations.V reports CJK', () => {
        const r = audit(ws.distDir);
        assert.equal(r.ok, true);
        assert.ok(r.violations.V.length >= 1);
    });
});

// ============================================================================
// F7 - error path - dist missing
// ============================================================================

describe('audit_dts_hygiene - F7 dist missing', () => {
    test('ok=false when dist dir absent', () => {
        const r = audit('/nonexistent/dist/path');
        assert.equal(r.ok, false);
        assert.match(r.error, /not found/);
    });
});

// ============================================================================
// F8 - error path - dist present but no *.d.ts
// ============================================================================

describe('audit_dts_hygiene - F8 no dts files', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        // Plain .js without .d.ts present.
        writeFile(ws.distDir, 'bridge.js', 'export const x = 1;');
    });
    after(() => ws.cleanup());

    test('ok=false when dist has no *.d.ts', () => {
        const r = audit(ws.distDir);
        assert.equal(r.ok, false);
        assert.match(r.error, /no \*\.d\.ts/);
    });
});

// ============================================================================
// F9 - baseline diff
// ============================================================================

describe('audit_dts_hygiene - F9 baseline diff filters known', () => {
    let ws;
    let baselineFile;
    before(() => {
        ws = makeWorkspace();
        writeFile(ws.distDir, 'bridge.d.ts', `/** @internal */\nexport declare const a: number;`);
        const first = audit(ws.distDir);
        assert.ok(first.total >= 1);
        baselineFile = path.join(ws.root, 'dts-baseline.json');
        fs.writeFileSync(
            baselineFile,
            JSON.stringify({ violations: first.violations }, null, 2),
        );
    });
    after(() => ws.cleanup());

    test('CLI --strict + --baseline (only known) exits 0', () => {
        const r = spawnAudit(['--strict', '--baseline', baselineFile], ws.root);
        assert.equal(r.status, 0, `exit=${r.status} stderr=${r.stderr}`);
    });
});

// ============================================================================
// FCLI-1 - --help self-describing contract
// ============================================================================

describe('audit_dts_hygiene - FCLI-1 --help self-describing', () => {
    test('--help exit=0 lists all five options + 3 exit codes', () => {
        const r = spawnAudit(['--help']);
        assert.equal(r.status, 0, `exit=${r.status} stderr=${r.stderr}`);
        const out = r.stdout;
        assert.match(out, /Usage:.*\[--help\|-h\]/);
        assert.match(out, /Options:/);
        assert.match(out, /Exit codes/);
        for (const opt of ['--json', '--strict', '--warn-only', '--baseline <file>', '--help, -h']) {
            assert.ok(out.includes(opt), `--help must list "${opt}"; got:\n${out}`);
        }
        for (const code of ['  0 ', '  1 ', '  2 ']) {
            assert.ok(out.includes(code), `--help must document exit code "${code.trim()}"`);
        }
    });

    test('-h short flag behaves identically', () => {
        const r = spawnAudit(['-h']);
        assert.equal(r.status, 0);
        assert.match(r.stdout, /Usage:/);
        assert.match(r.stdout, /--help, -h/);
    });
});

// ============================================================================
// FCLI-2 - --json parseable
// ============================================================================

describe('audit_dts_hygiene - FCLI-2 --json output shape', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeFile(ws.distDir, 'bridge.d.ts', `/** @internal */\nexport declare const a: number;`);
    });
    after(() => ws.cleanup());

    test('stdout JSON.parse-able with required keys', () => {
        const r = spawnAudit(['--json'], ws.root);
        let parsed;
        assert.doesNotThrow(() => {
            parsed = JSON.parse(r.stdout);
        }, '--json stdout must be JSON.parse-able');
        for (const key of ['ok', 'files', 'violations', 'total']) {
            assert.ok(
                Object.prototype.hasOwnProperty.call(parsed, key),
                `--json report missing required key "${key}"`,
            );
        }
        assert.ok(parsed.violations.I.length >= 1);
    });
});

// ============================================================================
// FCLI-3 - --strict + violations - exit 1  (M4 negative red line)
// ============================================================================

describe('audit_dts_hygiene - FCLI-3 --strict gates exit 1', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeFile(ws.distDir, 'bridge.d.ts', `/** @internal */\nexport declare const a: number;`);
    });
    after(() => ws.cleanup());

    test('exit=1 when violations present (strict)', () => {
        const r = spawnAudit(['--strict'], ws.root);
        assert.equal(r.status, 1, `--strict + violations must exit 1; got ${r.status}\nstdout=${r.stdout}\nstderr=${r.stderr}`);
    });
});

// ============================================================================
// FCLI-4 - --warn-only + violations - exit 0
// ============================================================================

describe('audit_dts_hygiene - FCLI-4 --warn-only always exit 0', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeFile(ws.distDir, 'bridge.d.ts', `/** @internal */\nexport declare const a: number;`);
    });
    after(() => ws.cleanup());

    test('exit=0 even with violations under --warn-only', () => {
        const r = spawnAudit(['--warn-only'], ws.root);
        assert.equal(r.status, 0, `--warn-only must exit 0; got ${r.status}`);
    });
});

// ============================================================================
// FCLI-5 - dist missing - exit 2
// ============================================================================

describe('audit_dts_hygiene - FCLI-5 dist missing - exit 2', () => {
    let ws;
    before(() => {
        ws = makeWorkspace({ dist: false });
    });
    after(() => ws.cleanup());

    test('exit=2 when sdk/dist absent', () => {
        const r = spawnAudit([], ws.root);
        assert.equal(r.status, 2, `missing dist must exit 2; got ${r.status}\nstderr=${r.stderr}`);
    });
});

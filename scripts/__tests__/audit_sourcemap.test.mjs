// scripts/__tests__/audit_sourcemap.test.mjs
//
// Fixture tests for scripts/audit_sourcemap.mjs, which checks the SDK's
// published source maps.
//
// Covers:
//   F1     true-negative - clean map, all sources resolve, 0 violations
//   F2     (A) absolute-path leak - Windows drive + POSIX absolute source
//   F3     (B) unresolved source - sources[] entry points at missing file
//   F4     (C) entry-source boundary - index.js.map must be empty
//   F5     (C) entry-source boundary - bridge entry references foreign subtree
//   F6     error path - dist dir missing - ok=false
//   F7     error path - dist dir present but no *.map - ok=false
//   F8     baseline diff - known violation filtered out
//   FCLI-1 CLI - --help five-section contract
//   FCLI-2 CLI - --json parseable + required keys
//   FCLI-3 CLI - --strict + violations - exit 1  (negative red line)
//   FCLI-4 CLI - --warn-only + violations - exit 0
//   FCLI-5 CLI - dist missing - exit 2
//
// Uses node:test + node:assert. Zero external deps.
//
// Run:
//   node --test scripts/__tests__/audit_sourcemap.test.mjs

import { test, describe, before, after } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

import { audit, AUDIT_PROTOCOL_VERSION } from '../audit_sourcemap.mjs';

const SCRIPT_PATH = fileURLToPath(
    new URL('../audit_sourcemap.mjs', import.meta.url),
);

function spawnAudit(args, cwd) {
    return spawnSync(process.execPath, [SCRIPT_PATH, ...args], {
        cwd: cwd || process.cwd(),
        encoding: 'utf8',
        env: { ...process.env, NODE_NO_WARNINGS: '1' },
    });
}

// ----------------------------------------------------------------------------
// Fixture builders
// ----------------------------------------------------------------------------

function makeWorkspace({ dist = true } = {}) {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'audit-sourcemap-'));
    const distDir = path.join(root, 'sdk', 'dist');
    const sdkSrc = path.join(root, 'sdk', 'src');
    if (dist) fs.mkdirSync(distDir, { recursive: true });
    fs.mkdirSync(sdkSrc, { recursive: true });
    return {
        root,
        distDir,
        sdkSrc,
        opts() {
            return [distDir, sdkSrc];
        },
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

function writeMap(distDir, name, sources) {
    writeFile(distDir, name, JSON.stringify({ version: 3, file: name.replace(/\.map$/, ''), sources }));
}

// ============================================================================
// Sanity - protocol version
// ============================================================================

describe('audit_sourcemap - protocol version', () => {
    test('exports AUDIT_PROTOCOL_VERSION = 1', () => {
        assert.equal(AUDIT_PROTOCOL_VERSION, 1);
    });
});

// ============================================================================
// F1 - True-negative
// ============================================================================

describe('audit_sourcemap - F1 true-negative', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        // bridge entry referencing its own subtree (relative to dist/).
        writeFile(ws.sdkSrc, 'bridge/index.ts', '// src');
        writeMap(ws.distDir, 'bridge.js.map', ['../src/bridge/index.ts']);
        // index entry must be empty.
        writeMap(ws.distDir, 'index.js.map', []);
    });
    after(() => ws.cleanup());

    test('ok=true, total=0', () => {
        const r = audit(...ws.opts());
        assert.equal(r.ok, true);
        assert.equal(r.total, 0);
        assert.ok(r.entries.length >= 2);
    });
});

// ============================================================================
// F2 - (A) absolute-path leak
// ============================================================================

describe('audit_sourcemap - F2 absolute-path leak', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeMap(ws.distDir, 'bridge.js.map', [
            'C:\\work\\sdk\\src\\bridge\\index.ts',
            '/home/ci/sdk/src/bridge/leak.ts',
        ]);
    });
    after(() => ws.cleanup());

    test('violations.A has both absolute paths', () => {
        const r = audit(...ws.opts());
        assert.equal(r.ok, true);
        assert.equal(r.violations.A.length, 2);
        assert.ok(r.total >= 2);
    });
});

// ============================================================================
// F3 - (B) unresolved source
// ============================================================================

describe('audit_sourcemap - F3 unresolved source', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        // Source path is relative + non-absolute but file does not exist.
        writeMap(ws.distDir, 'bridge.js.map', ['../src/bridge/missing.ts']);
    });
    after(() => ws.cleanup());

    test('violations.B reports the missing source', () => {
        const r = audit(...ws.opts());
        assert.equal(r.ok, true);
        assert.equal(r.violations.B.length, 1);
        assert.match(r.violations.B[0].source, /missing\.ts$/);
    });
});

// ============================================================================
// F4 - (C) index.js.map shares the bridge subtree
// ============================================================================

describe('audit_sourcemap - F4 index map tracks the bridge subtree', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        // `src/index.ts` re-exports bridge values (`export * from
        // './bridge/index.js'` plus the default), so tsup bundles the whole
        // bridge tree into `dist/index.js` and its map mirrors
        // `bridge.js.map`. Referencing `bridge/` is therefore in-contract.
        writeFile(ws.sdkSrc, 'bridge/index.ts', '// src');
        writeMap(ws.distDir, 'index.js.map', ['../src/bridge/index.ts']);
    });
    after(() => ws.cleanup());

    test('bridge sources are in-contract for the index entry', () => {
        const r = audit(...ws.opts());
        assert.equal(r.ok, true);
        assert.equal(r.violations.C.length, 0);
    });
});

// ============================================================================
// F4b - (C) the index entry is still bounded to the bridge subtree
// ============================================================================

describe('audit_sourcemap - F4b index map rejects foreign subtree', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        // Counterpart to F4: widening `index` from "empty sources" to
        // "bridge subtree" must not degrade into an unchecked entry.
        writeFile(ws.sdkSrc, 'components/Fb.ts', '// src');
        writeMap(ws.distDir, 'index.js.map', ['../src/components/Fb.ts']);
    });
    after(() => ws.cleanup());

    test('violations.C flags a components source under the index entry', () => {
        const r = audit(...ws.opts());
        assert.equal(r.ok, true);
        assert.ok(r.violations.C.length >= 1);
        assert.ok(
            r.violations.C.some(
                (v) => v.entry === 'index' && v.primaryPrefix === 'bridge/',
            ),
        );
    });
});

// ============================================================================
// F5 - (C) entry-source boundary - foreign subtree
// ============================================================================

describe('audit_sourcemap - F5 boundary foreign subtree', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        // bridge entry referencing components/ subtree (foreign, not shared).
        writeFile(ws.sdkSrc, 'components/Fb.ts', '// src');
        writeMap(ws.distDir, 'bridge.js.map', ['../src/components/Fb.ts']);
    });
    after(() => ws.cleanup());

    test('violations.C flags cross-entry boundary breach', () => {
        const r = audit(...ws.opts());
        assert.equal(r.ok, true);
        assert.ok(r.violations.C.length >= 1);
    });
});

// ============================================================================
// F6 - error path - dist missing
// ============================================================================

describe('audit_sourcemap - F6 dist missing', () => {
    test('ok=false when dist dir absent', () => {
        const r = audit('/nonexistent/dist/path', '/nonexistent/src');
        assert.equal(r.ok, false);
        assert.match(r.error, /not found/);
    });
});

// ============================================================================
// F7 - error path - dist present but no *.map
// ============================================================================

describe('audit_sourcemap - F7 no maps', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
    });
    after(() => ws.cleanup());

    test('ok=false when dist has no *.js.map', () => {
        const r = audit(...ws.opts());
        assert.equal(r.ok, false);
        assert.match(r.error, /no \*\.js\.map/);
    });
});

// ============================================================================
// F8 - baseline diff
// ============================================================================

describe('audit_sourcemap - F8 baseline diff filters known', () => {
    let ws;
    let baselineFile;
    before(() => {
        ws = makeWorkspace();
        writeMap(ws.distDir, 'bridge.js.map', ['/home/ci/leak.ts']);
        const first = audit(...ws.opts());
        assert.equal(first.total, 1);
        baselineFile = path.join(ws.root, 'sourcemap-baseline.json');
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

describe('audit_sourcemap - FCLI-1 --help self-describing', () => {
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

describe('audit_sourcemap - FCLI-2 --json output shape', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeMap(ws.distDir, 'bridge.js.map', ['/home/ci/leak.ts']);
    });
    after(() => ws.cleanup());

    test('stdout JSON.parse-able with required keys', () => {
        const r = spawnAudit(['--json'], ws.root);
        // total > 0 + default mode (not warn-only) - exit 1; --json still valid.
        let parsed;
        assert.doesNotThrow(() => {
            parsed = JSON.parse(r.stdout);
        }, '--json stdout must be JSON.parse-able');
        for (const key of ['ok', 'entries', 'violations', 'total']) {
            assert.ok(
                Object.prototype.hasOwnProperty.call(parsed, key),
                `--json report missing required key "${key}"`,
            );
        }
        assert.equal(parsed.violations.A.length, 1);
    });
});

// ============================================================================
// FCLI-3 - --strict + violations - exit 1  (M4 negative red line)
// ============================================================================

describe('audit_sourcemap - FCLI-3 --strict gates exit 1', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeMap(ws.distDir, 'bridge.js.map', ['/home/ci/leak.ts']);
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

describe('audit_sourcemap - FCLI-4 --warn-only always exit 0', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeMap(ws.distDir, 'bridge.js.map', ['/home/ci/leak.ts']);
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

describe('audit_sourcemap - FCLI-5 dist missing - exit 2', () => {
    let ws;
    before(() => {
        // No sdk/dist created.
        ws = makeWorkspace({ dist: false });
        fs.rmSync(ws.distDir, { recursive: true, force: true });
    });
    after(() => ws.cleanup());

    test('exit=2 when sdk/dist absent', () => {
        const r = spawnAudit([], ws.root);
        assert.equal(r.status, 2, `missing dist must exit 2; got ${r.status}\nstderr=${r.stderr}`);
    });
});

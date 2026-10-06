// scripts/__tests__/audit_invoke_typing.test.mjs
//
// Fixture tests for scripts/audit_invoke_typing.mjs, which finds under-typed
// bridge calls in the SDK source (strict by default).
//
// Covers:
//   F1     true-negative - concrete response types, 0 violations
//   F2     (A) Promise<unknown> / Promise<any> / Promise<{}> return types
//   F3     (B) bridge.invoke<unknown> / invoke<any> call sites
//   F4     (C) bridge.on<unknown> / once<any> / off<{}> subscriptions
//   F5     comment exemption - weak generics inside // and /* */ comments ignored
//   F6     *.test.ts / __tests__ excluded
//   F7     error path - src dir missing - ok=false
//   F8     error path - src present but no *.ts - ok=false
//   FCLI-1 CLI - --help five-section contract
//   FCLI-2 CLI - --json parseable + required keys
//   FCLI-3 CLI - default (strict) + violations - exit 1  (negative red line)
//   FCLI-4 CLI - --warn-only + violations - exit 0
//   FCLI-5 CLI - src missing - exit 2
//
// Uses node:test + node:assert. Zero external deps.
//
// Run:
//   node --test scripts/__tests__/audit_invoke_typing.test.mjs

import { test, describe, before, after } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

import { audit, AUDIT_PROTOCOL_VERSION } from '../audit_invoke_typing.mjs';

const SCRIPT_PATH = fileURLToPath(
    new URL('../audit_invoke_typing.mjs', import.meta.url),
);

function spawnAudit(args, cwd) {
    return spawnSync(process.execPath, [SCRIPT_PATH, ...args], {
        cwd: cwd || process.cwd(),
        encoding: 'utf8',
        env: { ...process.env, NODE_NO_WARNINGS: '1' },
    });
}

function makeWorkspace() {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'audit-invoke-'));
    const sdkSrc = path.join(root, 'sdk', 'src');
    fs.mkdirSync(sdkSrc, { recursive: true });
    return {
        root,
        sdkSrc,
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

// ============================================================================
// Sanity - protocol version
// ============================================================================

describe('audit_invoke_typing - protocol version', () => {
    test('exports AUDIT_PROTOCOL_VERSION = 1', () => {
        assert.equal(AUDIT_PROTOCOL_VERSION, 1);
    });
});

// ============================================================================
// F1 - True-negative
// ============================================================================

describe('audit_invoke_typing - F1 true-negative', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeFile(
            ws.sdkSrc,
            'clean.ts',
            `export function play(): Promise<PlayResponse> {
    return bridge.invoke<PlayResponse>('playback.play', {});
}
bridge.on<StateChanged>('playback:stateChanged', () => {});
`,
        );
    });
    after(() => ws.cleanup());

    test('ok=true, total=0', () => {
        const r = audit(ws.sdkSrc);
        assert.equal(r.ok, true);
        assert.equal(r.total, 0);
    });
});

// ============================================================================
// F2 - (A) weak Promise return
// ============================================================================

describe('audit_invoke_typing - F2 weak Promise', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeFile(
            ws.sdkSrc,
            'weak.ts',
            `export function a(): Promise<unknown> { return x; }
export function b(): Promise<any> { return y; }
export function c(): Promise<{}> { return z; }
`,
        );
    });
    after(() => ws.cleanup());

    test('violations.A reports all three', () => {
        const r = audit(ws.sdkSrc);
        assert.equal(r.ok, true);
        assert.equal(r.violations.A.length, 3);
    });
});

// ============================================================================
// F3 - (B) weak invoke generic
// ============================================================================

describe('audit_invoke_typing - F3 weak invoke', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeFile(
            ws.sdkSrc,
            'weak.ts',
            `bridge.invoke<unknown>('a.b', {});
bridge.invoke<any>('c.d', {});
`,
        );
    });
    after(() => ws.cleanup());

    test('violations.B reports both', () => {
        const r = audit(ws.sdkSrc);
        assert.equal(r.ok, true);
        assert.equal(r.violations.B.length, 2);
    });
});

// ============================================================================
// F4 - (C) weak event subscription
// ============================================================================

describe('audit_invoke_typing - F4 weak event sub', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeFile(
            ws.sdkSrc,
            'weak.ts',
            `bridge.on<unknown>('e:one', () => {});
bridge.once<any>('e:two', () => {});
bridge.off<{}>('e:three', () => {});
`,
        );
    });
    after(() => ws.cleanup());

    test('violations.C reports all three', () => {
        const r = audit(ws.sdkSrc);
        assert.equal(r.ok, true);
        assert.equal(r.violations.C.length, 3);
    });
});

// ============================================================================
// F5 - comment exemption
// ============================================================================

describe('audit_invoke_typing - F5 comment exemption', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeFile(
            ws.sdkSrc,
            'comments.ts',
            `// bridge.invoke<unknown>('ignored.line', {});
/* Promise<any> in a block comment is ignored too. */
export const x = 1;
`,
        );
    });
    after(() => ws.cleanup());

    test('weak generics inside comments do NOT count', () => {
        const r = audit(ws.sdkSrc);
        assert.equal(r.ok, true);
        assert.equal(r.total, 0);
    });
});

// ============================================================================
// F6 - *.test.ts / __tests__ excluded
// ============================================================================

describe('audit_invoke_typing - F6 test files excluded', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeFile(ws.sdkSrc, 'foo.test.ts', `bridge.invoke<unknown>('x.y', {});`);
        writeFile(ws.sdkSrc, '__tests__/bar.ts', `bridge.invoke<any>('x.y', {});`);
        writeFile(ws.sdkSrc, 'real.ts', `bridge.invoke<RealResp>('x.y', {});`);
    });
    after(() => ws.cleanup());

    test('ok=true, total=0 (test artifacts ignored)', () => {
        const r = audit(ws.sdkSrc);
        assert.equal(r.ok, true);
        assert.equal(r.total, 0);
    });
});

// ============================================================================
// F7 - error path - src missing
// ============================================================================

describe('audit_invoke_typing - F7 src missing', () => {
    test('ok=false when src dir absent', () => {
        const r = audit('/nonexistent/src/path');
        assert.equal(r.ok, false);
        assert.match(r.error, /not found/);
    });
});

// ============================================================================
// F8 - error path - src present but no *.ts
// ============================================================================

describe('audit_invoke_typing - F8 no ts files', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
    });
    after(() => ws.cleanup());

    test('ok=false when src has no *.ts', () => {
        const r = audit(ws.sdkSrc);
        assert.equal(r.ok, false);
        assert.match(r.error, /no \*\.ts/);
    });
});

// ============================================================================
// FCLI-1 - --help self-describing contract
// ============================================================================

describe('audit_invoke_typing - FCLI-1 --help self-describing', () => {
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

describe('audit_invoke_typing - FCLI-2 --json output shape', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeFile(ws.sdkSrc, 'weak.ts', `bridge.invoke<unknown>('x.y', {});`);
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
        assert.equal(parsed.violations.B.length, 1);
    });
});

// ============================================================================
// FCLI-3 - default (strict) + violations - exit 1  (M4 negative red line)
// ============================================================================

describe('audit_invoke_typing - FCLI-3 strict default gates exit 1', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeFile(ws.sdkSrc, 'weak.ts', `bridge.invoke<unknown>('x.y', {});`);
    });
    after(() => ws.cleanup());

    test('exit=1 when violations present (strict by default)', () => {
        const r = spawnAudit([], ws.root);
        assert.equal(r.status, 1, `strict default + violations must exit 1; got ${r.status}\nstdout=${r.stdout}\nstderr=${r.stderr}`);
    });

    test('--strict explicit also exits 1', () => {
        const r = spawnAudit(['--strict'], ws.root);
        assert.equal(r.status, 1, `--strict + violations must exit 1; got ${r.status}`);
    });
});

// ============================================================================
// FCLI-4 - --warn-only + violations - exit 0
// ============================================================================

describe('audit_invoke_typing - FCLI-4 --warn-only always exit 0', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeFile(ws.sdkSrc, 'weak.ts', `bridge.invoke<unknown>('x.y', {});`);
    });
    after(() => ws.cleanup());

    test('exit=0 even with violations under --warn-only', () => {
        const r = spawnAudit(['--warn-only'], ws.root);
        assert.equal(r.status, 0, `--warn-only must exit 0; got ${r.status}`);
    });
});

// ============================================================================
// FCLI-5 - src missing - exit 2
// ============================================================================

describe('audit_invoke_typing - FCLI-5 src missing - exit 2', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        fs.rmSync(ws.sdkSrc, { recursive: true, force: true });
    });
    after(() => ws.cleanup());

    test('exit=2 when sdk/src absent', () => {
        const r = spawnAudit([], ws.root);
        assert.equal(r.status, 2, `missing src must exit 2; got ${r.status}\nstderr=${r.stderr}`);
    });
});

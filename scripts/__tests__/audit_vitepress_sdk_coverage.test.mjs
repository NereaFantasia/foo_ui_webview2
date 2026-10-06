// scripts/__tests__/audit_vitepress_sdk_coverage.test.mjs
//
// Fixture tests for scripts/audit_vitepress_sdk_coverage.mjs.
//
// Covers:
//   F1      true-positive: SDK wrapper method missing from VitePress SDK docs
//   F2      true-negative: all SDK wrapper methods documented
//   F3      baseline diff filters known documentation gaps
//   F4      --include-namespace limits the audit scope
//   FCLI-1  --help lists the options and exit codes
//   FCLI-2  --json produces JSON.parse-able stdout with required keys
//   FCLI-3  --strict + NEW finding -> exit 1
//   FCLI-4  --strict + baseline-covered finding -> exit 0
//   FCLI-5  missing docs/sdk tree -> exit 2
//   FCLI-6  importing from node -e does not execute CLI guard
//   F5      consoleApi namespace is documented as public fb.console
//   FCLI-7  importing with caller --help args does not print audit help
//   F6      JSDoc braces inside namespace objects do not hide methods
//   F7      public object properties using sync/async helper aliases are counted
//
// Run:
//   node --test scripts/__tests__/audit_vitepress_sdk_coverage.test.mjs

import { test, describe, before, after } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { spawnSync } from 'node:child_process';
import { fileURLToPath, pathToFileURL } from 'node:url';

import {
    AUDIT_PROTOCOL_VERSION,
    audit,
} from '../audit_vitepress_sdk_coverage.mjs';

const SCRIPT_PATH = fileURLToPath(
    new URL('../audit_vitepress_sdk_coverage.mjs', import.meta.url),
);

function spawnAudit(args, cwd) {
    const workspace = cwd && cwd !== process.cwd() ? cwd : null;
    const injected = [];
    // Only pin fixture workspaces (temp dirs that contain a synthetic sdk tree).
    // Do not rewrite paths when cwd is simply a subdirectory of the real repo.
    const looksLikeFixture =
        workspace &&
        (workspace.includes(os.tmpdir()) ||
            fs.existsSync(path.join(workspace, 'sdk/src/bridge/namespaces')));
    if (looksLikeFixture) {
        const hasDocsRoot = args.some(
            (a) => a === '--docs-root' || String(a).startsWith('--docs-root='),
        );
        const hasSdkRoot = args.some(
            (a) => a === '--sdk-root' || String(a).startsWith('--sdk-root='),
        );
        if (!hasSdkRoot) {
            injected.push('--sdk-root', path.join(workspace, 'sdk/src/bridge/namespaces'));
        }
        if (!hasDocsRoot) {
            injected.push('--docs-root', path.join(workspace, 'docs/vitepress/sdk'));
        }
    }
    return spawnSync(process.execPath, [SCRIPT_PATH, ...injected, ...args], {
        cwd: cwd || process.cwd(),
        encoding: 'utf8',
        env: {
            ...process.env,
            NODE_NO_WARNINGS: '1',
            PYTHONIOENCODING: 'utf-8',
        },
    });
}

function makeWorkspace({ withDocs = true } = {}) {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'audit-vp-sdk-'));
    const sdkRoot = path.join(root, 'sdk/src/bridge/namespaces');
    const docsRoot = path.join(root, 'docs/vitepress/sdk');
    fs.mkdirSync(sdkRoot, { recursive: true });
    if (withDocs) fs.mkdirSync(docsRoot, { recursive: true });
    return {
        root,
        sdkRoot,
        docsRoot,
        opts(extra = {}) {
            return { sdkRoot, docsRoot, relRoot: root, ...extra };
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

function writeNamespace(ws, name, source) {
    return writeFile(ws.sdkRoot, `${name}.ts`, source);
}

function writeDoc(ws, name, source) {
    return writeFile(ws.docsRoot, `${name}.md`, source);
}

describe('audit_vitepress_sdk_coverage · protocol version', () => {
    test('exports AUDIT_PROTOCOL_VERSION = 1', () => {
        assert.equal(AUDIT_PROTOCOL_VERSION, 1);
    });
});

describe('audit_vitepress_sdk_coverage · F1 true-positive', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeNamespace(
            ws,
            'foo',
            [
                "import { bridge } from '../Bridge.js';",
                'export const foo = {',
                "    present: () => bridge.invoke('foo.present'),",
                "    missing: (id: string) => bridge.invoke('foo.missing', { id }),",
                '};',
            ].join('\n'),
        );
        writeDoc(ws, 'foo', '```js\nawait fb.foo.present();\n```\n');
    });
    after(() => ws.cleanup());

    test('reports undocumented SDK wrapper methods', () => {
        const r = audit(ws.opts());
        assert.equal(r.totalSdkMethods, 2);
        assert.equal(r.totalDocumentedMethods, 1);
        assert.equal(r.totalGaps, 1);
        assert.deepEqual(r.findings.map((v) => v.api), ['foo.missing']);
    });
});

describe('audit_vitepress_sdk_coverage · F2 true-negative', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeNamespace(
            ws,
            'bar',
            [
                "import { bridge } from '../Bridge.js';",
                'export const bar = {',
                "    one: () => bridge.invoke('bar.one'),",
                "    two: () => bridge.invoke('bar.two'),",
                '};',
            ].join('\n'),
        );
        writeDoc(
            ws,
            'bar',
            '```js\nawait fb.bar.one();\nawait fb.bar.two();\n```\n',
        );
    });
    after(() => ws.cleanup());

    test('returns no gaps when all wrapper methods are documented', () => {
        const r = audit(ws.opts());
        assert.equal(r.totalGaps, 0);
        assert.equal(r.findings.length, 0);
        assert.equal(r.ok, true);
    });
});

describe('audit_vitepress_sdk_coverage · F3 baseline diff', () => {
    let ws;
    let baselineFile;
    before(() => {
        ws = makeWorkspace();
        writeNamespace(
            ws,
            'baz',
            [
                "import { bridge } from '../Bridge.js';",
                'export const baz = {',
                "    knownGap: () => bridge.invoke('baz.knownGap'),",
                '};',
            ].join('\n'),
        );
        writeDoc(ws, 'baz', '# baz\n');
        const baseline = audit(ws.opts());
        baselineFile = path.join(ws.root, 'baseline.json');
        fs.writeFileSync(baselineFile, JSON.stringify(baseline, null, 2), 'utf8');
    });
    after(() => ws.cleanup());

    test('strict + baseline that covers all gaps -> exit 0', () => {
        const r = spawnAudit(['--strict', '--baseline', baselineFile], ws.root);
        assert.equal(r.status, 0, `stdout=${r.stdout}\nstderr=${r.stderr}`);
    });

    test('new gap after baseline -> strict exit 1', () => {
        writeNamespace(
            ws,
            'baz',
            [
                "import { bridge } from '../Bridge.js';",
                'export const baz = {',
                "    knownGap: () => bridge.invoke('baz.knownGap'),",
                "    newGap: () => bridge.invoke('baz.newGap'),",
                '};',
            ].join('\n'),
        );
        const r = spawnAudit(['--strict', '--baseline', baselineFile], ws.root);
        assert.equal(r.status, 1, `stdout=${r.stdout}\nstderr=${r.stderr}`);
        assert.match(r.stdout, /baz\.newGap/);
    });
});

describe('audit_vitepress_sdk_coverage · F4 include namespace', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeNamespace(ws, 'alpha', "export const alpha = { a: () => bridge.invoke('alpha.a') };\n");
        writeNamespace(ws, 'beta', "export const beta = { b: () => bridge.invoke('beta.b') };\n");
        writeDoc(ws, 'alpha', '```js\nawait fb.alpha.a();\n```\n');
        writeDoc(ws, 'beta', '# beta\n');
    });
    after(() => ws.cleanup());

    test('includeNamespaces limits findings to selected namespace', () => {
        const r = audit(ws.opts({ includeNamespaces: new Set(['alpha']) }));
        assert.equal(r.namespacesScanned, 1);
        assert.equal(r.totalGaps, 0);
    });
});

describe('audit_vitepress_sdk_coverage · F5 public namespace aliases', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeNamespace(
            ws,
            'consoleApi',
            [
                "import { bridge } from '../Bridge.js';",
                'export const consoleApi = {',
                "    log: (message: string) => bridge.invoke('console.log', { message }),",
                '};',
            ].join('\n'),
        );
        writeDoc(ws, 'console', '```js\nawait fb.console.log(\'hello\');\n```\n');
    });
    after(() => ws.cleanup());

    test('consoleApi wrapper is covered by fb.console documentation', () => {
        const r = audit(ws.opts());
        assert.equal(r.totalSdkMethods, 1);
        assert.equal(r.totalGaps, 0);
        assert.equal(r.findings.length, 0);
    });
});

describe('audit_vitepress_sdk_coverage · F6 JSDoc braces', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeNamespace(
            ws,
            'cursor',
            [
                "import { bridge } from '../Bridge.js';",
                'export const cursor = {',
                '    /** Returns `{ hidden: false }` when unavailable. */',
                "    isHidden: () => bridge.invoke('cursor.isHidden'),",
                '};',
            ].join('\n'),
        );
        writeDoc(ws, 'cursor', '# cursor\n');
    });
    after(() => ws.cleanup());

    test('still detects the method after braces in a JSDoc comment', () => {
        const r = audit(ws.opts());
        assert.equal(r.totalSdkMethods, 1);
        assert.equal(r.totalGaps, 1);
        assert.deepEqual(r.findings.map((v) => v.api), ['cursor.isHidden']);
    });

    test('still detects methods after apostrophes inside block comments', () => {
        writeNamespace(
            ws,
            'cursor',
            [
                "import { bridge } from '../Bridge.js';",
                'export const cursor = {',
                "    /** Hide or restore the calling window's client-area cursor. */",
                "    setHidden: (hidden: boolean) => bridge.invoke('cursor.setHidden', { hidden }),",
                '};',
            ].join('\n'),
        );
        const r = audit(ws.opts());
        assert.equal(r.totalSdkMethods, 1);
        assert.equal(r.totalGaps, 1);
        assert.deepEqual(r.findings.map((v) => v.api), ['cursor.setHidden']);
    });
});

describe('audit_vitepress_sdk_coverage · F7 helper alias wrappers', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeNamespace(
            ws,
            'http',
            [
                "import { bridge } from '../Bridge.js';",
                "function httpGet(url: string) { return bridge.invoke('http.get', { url }); }",
                "async function httpPost(url: string) { return bridge.invoke('http.post', { url }); }",
                "function _httpRequest(method: string, payload: object) { return bridge.invoke(method, payload); }",
                "function httpRequest(url: string) { return _httpRequest('http.get', { url }); }",
                "function internalOnly() { return bridge.invoke('http.internal'); }",
                "function diagnosticOnly() { return console.warn('http.warned'); }",
                'export const http = {',
                '    get: httpGet,',
                '    /** Async helper aliases remain public SDK methods. */',
                '    post: httpPost,',
                "    download: (url: string) => bridge.invoke('http.download', { url }),",
                '    request: httpRequest,',
                '    diagnostic: diagnosticOnly,',
                '};',
            ].join('\n'),
        );
        writeDoc(
            ws,
            'http',
            '```js\nawait fb.http.get(url);\nawait fb.http.post(url);\nawait fb.http.download(url);\nawait fb.http.request(url);\n```\n',
        );
    });
    after(() => ws.cleanup());

    test('counts exported object properties that point at helper functions', () => {
        const r = audit(ws.opts());
        assert.equal(r.totalSdkMethods, 4);
        assert.equal(r.totalGaps, 0);
        assert.equal(r.findings.length, 0);
        assert.deepEqual(r.findings.map((v) => v.api), []);
    });
});

describe('audit_vitepress_sdk_coverage · FCLI-1 --help contract', () => {
    test('--help exit 0 and lists 5 options + 3 exit codes', () => {
        const r = spawnAudit(['--help'], process.cwd());
        assert.equal(r.status, 0);
        const out = r.stdout;
        assert.match(out, /Usage:\s+node\s+scripts\/audit_vitepress_sdk_coverage\.mjs.*\[--help\|-h\]/);
        for (const flag of ['--json', '--strict', '--warn-only', '--baseline', '--help, -h']) {
            assert.ok(out.includes(flag), `--help must mention ${flag}\nstdout=${out}`);
        }
        assert.match(out, /Exit codes/);
        for (const code of ['0', '1', '2']) {
            assert.match(out, new RegExp(`\\b${code}\\b`));
        }
    });
});

describe('audit_vitepress_sdk_coverage · FCLI-2 --json', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeNamespace(ws, 'jsonns', "export const jsonns = { miss: () => bridge.invoke('jsonns.miss') };\n");
        writeDoc(ws, 'jsonns', '# jsonns\n');
    });
    after(() => ws.cleanup());

    test('JSON stdout has required keys', () => {
        const r = spawnAudit(['--json'], ws.root);
        assert.equal(r.status, 0);
        const obj = JSON.parse(r.stdout);
        for (const k of [
            'ok',
            'namespacesScanned',
            'totalSdkMethods',
            'totalDocumentedMethods',
            'totalGaps',
            'newFindings',
            'findings',
        ]) {
            assert.ok(k in obj, `JSON must contain key "${k}"`);
        }
        assert.equal(obj.totalGaps, 1);
    });
});

describe('audit_vitepress_sdk_coverage · FCLI-3 --strict', () => {
    let ws;
    before(() => {
        ws = makeWorkspace();
        writeNamespace(ws, 'strictns', "export const strictns = { miss: () => bridge.invoke('strictns.miss') };\n");
        writeDoc(ws, 'strictns', '# strictns\n');
    });
    after(() => ws.cleanup());

    test('strict without baseline -> exit 1 because finding is new', () => {
        const r = spawnAudit(['--strict'], ws.root);
        assert.equal(r.status, 1, `expected exit 1 but got ${r.status}\nstdout=${r.stdout}`);
    });
});

describe('audit_vitepress_sdk_coverage · FCLI-4 baseline-covered strict', () => {
    let ws;
    let baselineFile;
    before(() => {
        ws = makeWorkspace();
        writeNamespace(ws, 'covered', "export const covered = { miss: () => bridge.invoke('covered.miss') };\n");
        writeDoc(ws, 'covered', '# covered\n');
        baselineFile = path.join(ws.root, 'baseline.json');
        fs.writeFileSync(baselineFile, JSON.stringify(audit(ws.opts()), null, 2), 'utf8');
    });
    after(() => ws.cleanup());

    test('strict with covering baseline exits 0', () => {
        const r = spawnAudit(['--strict', '--baseline', baselineFile], ws.root);
        assert.equal(r.status, 0, `stdout=${r.stdout}\nstderr=${r.stderr}`);
    });
});

describe('audit_vitepress_sdk_coverage · FCLI-5 missing docs', () => {
    let ws;
    before(() => {
        ws = makeWorkspace({ withDocs: false });
        writeNamespace(ws, 'nodocs', "export const nodocs = { miss: () => bridge.invoke('nodocs.miss') };\n");
    });
    after(() => ws.cleanup());

    test('missing docs/vitepress/sdk tree -> exit 2', () => {
        const r = spawnAudit([], ws.root);
        assert.equal(r.status, 2, `stdout=${r.stdout}\nstderr=${r.stderr}`);
    });
});

describe('audit_vitepress_sdk_coverage · FCLI-6 import guard', () => {
    test('module import from node -e does not crash when process.argv[1] is absent', () => {
        const r = spawnSync(
            process.execPath,
            [
                '--input-type=module',
                '-e',
                `import { AUDIT_PROTOCOL_VERSION } from ${JSON.stringify(pathToFileURL(SCRIPT_PATH).href)}; process.stdout.write(String(AUDIT_PROTOCOL_VERSION));`,
            ],
            {
                cwd: process.cwd(),
                encoding: 'buffer',
                env: {
                    ...process.env,
                    NODE_NO_WARNINGS: '1',
                    // Neutralize host code-page effects on Windows consoles.
                    LANG: 'C.UTF-8',
                },
            },
        );
        assert.equal(r.status, 0, `stdout=${r.stdout}\nstderr=${r.stderr}`);
        assert.equal(Buffer.from(r.stdout).toString('utf8').replace(/^\uFEFF/, '').trim(), '1');
    });
});

describe('audit_vitepress_sdk_coverage · FCLI-7 imported help isolation', () => {
    test('importing module while caller has --help does not run audit CLI', () => {
        const r = spawnSync(
            process.execPath,
            [
                '--input-type=module',
                '-e',
                `import ${JSON.stringify(pathToFileURL(SCRIPT_PATH).href)}; process.stdout.write('caller-help');`,
                '--',
                '--help',
            ],
            {
                cwd: process.cwd(),
                encoding: 'buffer',
                env: {
                    ...process.env,
                    NODE_NO_WARNINGS: '1',
                    LANG: 'C.UTF-8',
                },
            },
        );
        assert.equal(r.status, 0, `stdout=${r.stdout}\nstderr=${r.stderr}`);
        assert.equal(
            Buffer.from(r.stdout).toString('utf8').replace(/^\uFEFF/, '').trim(),
            'caller-help',
        );
    });
});

describe('audit_vitepress_sdk_coverage · docs-root CLI', () => {
    test('help mentions --docs-root', () => {
        const r = spawnAudit(['--help']);
        assert.equal(r.status, 0);
        assert.match(r.stdout, /--docs-root/);
        assert.match(r.stdout, /repository root/i);
    });

    test('explicit --docs-root appears in human and json output', () => {
        const ws = makeWorkspace();
        try {
            writeNamespace(ws, 'foo', "export const foo = { ok: () => bridge.invoke('foo.ok') };\n");
            writeDoc(ws, 'foo', '```js\nfb.foo.ok()\n```\n');
            const alt = path.join(ws.root, 'alt-docs');
            fs.mkdirSync(alt, { recursive: true });
            writeFile(alt, 'foo.md', '```js\nfb.foo.ok()\n```\n');

            const human = spawnAudit(
                ['--sdk-root', ws.sdkRoot, '--docs-root', alt],
                ws.root,
            );
            assert.equal(human.status, 0, human.stdout + human.stderr);
            assert.match(human.stdout, /Docs root:/);

            const jsonRun = spawnAudit(
                ['--json', '--sdk-root', ws.sdkRoot, '--docs-root', alt],
                ws.root,
            );
            assert.equal(jsonRun.status, 0, jsonRun.stdout + jsonRun.stderr);
            const json = JSON.parse(jsonRun.stdout);
            assert.ok(json.docsRoot);
            assert.equal(json.totalGaps, 0);
        } finally {
            ws.cleanup();
        }
    });

    test('missing explicit docs-root exits 2', () => {
        const ws = makeWorkspace();
        try {
            writeNamespace(ws, 'foo', "export const foo = { ok: () => bridge.invoke('foo.ok') };\n");
            const r = spawnAudit(
                ['--sdk-root', ws.sdkRoot, '--docs-root', path.join(ws.root, 'no-such-docs')],
                ws.root,
            );
            assert.equal(r.status, 2, r.stdout + r.stderr);
        } finally {
            ws.cleanup();
        }
    });

    test('strict fails when explicit docs-root lacks a method page', () => {
        const ws = makeWorkspace();
        try {
            writeNamespace(
                ws,
                'foo',
                [
                    "import { bridge } from '../Bridge.js';",
                    'export const foo = {',
                    "    present: () => bridge.invoke('foo.present'),",
                    "    missing: () => bridge.invoke('foo.missing'),",
                    '};',
                    '',
                ].join('\n'),
            );
            const alt = path.join(ws.root, 'alt-docs');
            fs.mkdirSync(alt, { recursive: true });
            writeFile(alt, 'foo.md', '```js\nfb.foo.present()\n```\n');
            const r = spawnAudit(
                ['--strict', '--sdk-root', ws.sdkRoot, '--docs-root', alt],
                ws.root,
            );
            assert.equal(r.status, 1, r.stdout + r.stderr);
        } finally {
            ws.cleanup();
        }
    });

    test('relative --docs-root anchors to repo root when cwd is a subdirectory', () => {
        const sub = path.join(process.cwd(), 'docs', 'vitepress');
        const r = spawnAudit(
            ['--json', '--include-namespace=tray', '--docs-root', 'docs/vitepress/sdk'],
            sub,
        );
        if (r.status === null && ['EPERM', 'ENOENT'].includes(r.error?.code)) {
            return;
        }
        assert.equal(r.status, 0, r.stdout + r.stderr + (r.error ? `\n${r.error.message}` : ''));
        const json = JSON.parse(r.stdout);
        assert.match(String(json.docsRoot), /docs\/vitepress\/sdk/);
        assert.ok(json.totalSdkMethods > 0, 'must not empty-scan when cwd is a subdirectory');
    });
});

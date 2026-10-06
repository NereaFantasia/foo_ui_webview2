// scripts/audit_vitepress_sdk_coverage.mjs
//
// Checks that every SDK facade method has an example on the docs site.
//
// Compares public SDK wrapper methods in sdk/src/bridge/namespaces/*.ts with
// VitePress SDK documentation examples under docs/vitepress/sdk/*.md. A method
// is considered documented when the docs contain a call shaped like:
//
//   fb.<namespace>.<method>(
//
// The source of truth is the SDK facade method name (for example fb.ui.minimize),
// not the lower-level bridge.invoke target (for example window.minimize). The
// invoke targets are still captured as evidence for diagnosis.
//
// Exit codes:
//   0 = pass (clean OR all findings present in baseline OR --warn-only)
//   1 = --strict and at least one NEW finding (not present in baseline)
//   2 = script/config error (missing sdk namespace tree or docs tree)
//
// CLI:
//   --json                       Emit JSON report to stdout
//   --strict                     Fail with exit 1 when there are NEW gaps
//   --warn-only                  Downgrade gaps to warnings, always exit 0
//   --baseline <file>            Compare against an existing JSON baseline file
//   --include-namespace=<ns>     Restrict audit to one namespace (repeatable)
//   --help, -h                   Print usage and exit 0
//
// Usage:
//   node scripts/audit_vitepress_sdk_coverage.mjs
//   node scripts/audit_vitepress_sdk_coverage.mjs --include-namespace=ui
//   node scripts/audit_vitepress_sdk_coverage.mjs --strict --baseline <baseline>

import fs from 'node:fs';
import path from 'node:path';
import { exit } from 'node:process';
import { fileURLToPath } from 'node:url';

// Version of the CLI shared by the audit_*.mjs scripts (--help / --strict / --warn-only / --baseline / --json).
export const AUDIT_PROTOCOL_VERSION = 1;

const REPO_ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const DEFAULT_SDK_ROOT = path.join(REPO_ROOT, 'sdk/src/bridge/namespaces');
const DEFAULT_DOCS_ROOT = path.join(REPO_ROOT, 'docs/vitepress/sdk');
const PUBLIC_NAMESPACE_ALIASES = new Map([['consoleApi', 'console']]);

function resolveRepoPath(input) {
    if (!input) return null;
    return path.isAbsolute(input) ? path.normalize(input) : path.resolve(REPO_ROOT, input);
}

function parseFlags(args) {
    const flags = {
        json: args.includes('--json'),
        strict: args.includes('--strict'),
        warnOnly: args.includes('--warn-only'),
        baseline: null,
        docsRoot: null,
        sdkRoot: null,
        includeNamespaces: new Set(),
        help: args.includes('--help') || args.includes('-h'),
    };

    const baselineIdx = args.indexOf('--baseline');
    if (baselineIdx >= 0 && args[baselineIdx + 1]) {
        flags.baseline = args[baselineIdx + 1];
    }

    for (let i = 0; i < args.length; i++) {
        const arg = args[i];
        if (arg.startsWith('--include-namespace=')) {
            const ns = arg.slice('--include-namespace='.length).trim();
            if (ns) flags.includeNamespaces.add(ns);
        } else if (arg === '--include-namespace' && args[i + 1]) {
            flags.includeNamespaces.add(args[i + 1].trim());
            i++;
        } else if (arg.startsWith('--docs-root=')) {
            flags.docsRoot = arg.slice('--docs-root='.length).trim();
        } else if (arg === '--docs-root' && args[i + 1]) {
            flags.docsRoot = args[i + 1].trim();
            i++;
        } else if (arg.startsWith('--sdk-root=')) {
            flags.sdkRoot = arg.slice('--sdk-root='.length).trim();
        } else if (arg === '--sdk-root' && args[i + 1]) {
            flags.sdkRoot = args[i + 1].trim();
            i++;
        }
    }

    return flags;
}

function printHelp() {
    console.log(
        [
            'Usage: node scripts/audit_vitepress_sdk_coverage.mjs [--json] [--strict|--warn-only] [--baseline <file>] [--docs-root <path>] [--include-namespace=<ns>] [--help|-h]',
            '',
            'Repository Health Gate — VitePress SDK Coverage Audit.',
            'Scans sdk/src/bridge/namespaces/*.ts public wrapper methods and verifies',
            'docs/vitepress/sdk/*.md contains fb.<namespace>.<method>( call examples.',
            '',
            'Options:',
            '  --json                       Emit machine-readable JSON report',
            '  --strict                     Fail with exit 1 on any NEW gap (not in baseline)',
            '  --warn-only                  Downgrade all gaps to warnings; always exit 0',
            '  --baseline <file>            Compare against a JSON baseline; only NEW gaps surfaced',
            '  --docs-root=<path>           Docs root to scan (also: --docs-root <path>)',
            '                               Relative paths resolve against the repository root',
            '                               (directory containing this scripts/ folder), not process.cwd().',
            '                               Default: docs/vitepress/sdk',
            '  --sdk-root=<path>            SDK namespaces root (also: --sdk-root <path>); same',
            '                               relative-path contract as --docs-root',
            '                               Default: sdk/src/bridge/namespaces',
            '  --include-namespace=<ns>     Restrict audit to one namespace (repeatable)',
            '  --help, -h                   Print this usage and exit 0',
            '',
            'Exit codes:',
            '  0  pass (clean or all gaps in baseline or --warn-only)',
            '  1  --strict and at least one NEW gap',
            '  2  script/config error (missing SDK namespace tree or VitePress SDK docs tree)',
            '',
        ].join('\n'),
    );
}

function collectFiles(dir, predicate) {
    const out = [];
    if (!fs.existsSync(dir)) return out;
    const walk = (d) => {
        let entries;
        try {
            entries = fs.readdirSync(d, { withFileTypes: true });
        } catch {
            return;
        }
        for (const entry of entries) {
            const full = path.join(d, entry.name);
            if (entry.isDirectory()) {
                if (entry.name === 'node_modules' || entry.name === '.git') continue;
                walk(full);
            } else if (entry.isFile() && predicate(full)) {
                out.push(full);
            }
        }
    };
    walk(dir);
    return out.sort();
}

function readUtf8(file) {
    return fs.readFileSync(file, 'utf8');
}

function findMatchingBrace(src, openIdx) {
    let depth = 0;
    let inString = null;
    let inBlockComment = false;
    let inLineComment = false;
    let escaped = false;
    for (let i = openIdx; i < src.length; i++) {
        const ch = src[i];
        const next = src[i + 1];

        if (inLineComment) {
            if (ch === '\n') inLineComment = false;
            continue;
        }
        if (inBlockComment) {
            if (ch === '*' && next === '/') {
                inBlockComment = false;
                i++;
            }
            continue;
        }

        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch === '\\') {
                escaped = true;
            } else if (ch === inString) {
                inString = null;
            }
            continue;
        }
        if (ch === '/' && next === '/') {
            inLineComment = true;
            i++;
            continue;
        }
        if (ch === '/' && next === '*') {
            inBlockComment = true;
            i++;
            continue;
        }
        if (ch === '"' || ch === "'" || ch === '`') {
            inString = ch;
            continue;
        }
        if (ch === '{') depth++;
        if (ch === '}') {
            depth--;
            if (depth === 0) return i;
        }
    }
    return -1;
}

function lineOf(src, idx) {
    return src.slice(0, idx).split(/\r?\n/).length;
}

function extractExportedNamespaceObject(src) {
    const re = /export\s+const\s+([A-Za-z_$][\w$]*)\s*=\s*\{/g;
    const m = re.exec(src);
    if (!m) return null;
    const openIdx = src.indexOf('{', m.index);
    const closeIdx = findMatchingBrace(src, openIdx);
    if (closeIdx < 0) return null;
    return {
        namespace: m[1],
        body: src.slice(openIdx + 1, closeIdx),
        bodyStart: openIdx + 1,
    };
}

function findTopLevelProperties(body) {
    const props = [];
    let depth = 0;
    let inString = null;
    let inBlockComment = false;
    let inLineComment = false;
    let escaped = false;
    let lineStart = 0;
    const propRe = /^\s*([A-Za-z_$][\w$]*)\s*:/;

    for (let i = 0; i <= body.length; i++) {
        const ch = body[i] ?? '\n';
        const next = body[i + 1];
        const atLineEnd = ch === '\n';

        if (atLineEnd && depth === 0 && !inString && !inBlockComment && !inLineComment) {
            const line = body.slice(lineStart, i);
            const m = line.match(propRe);
            if (m) {
                props.push({ name: m[1], start: lineStart });
            }
        }

        if (inLineComment) {
            if (atLineEnd) inLineComment = false;
        } else if (inBlockComment) {
            if (ch === '*' && next === '/') {
                inBlockComment = false;
                i++;
            }
        } else if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch === '\\') {
                escaped = true;
            } else if (ch === inString) {
                inString = null;
            }
        } else {
            if (ch === '/' && next === '/') {
                inLineComment = true;
                i++;
            } else if (ch === '/' && next === '*') {
                inBlockComment = true;
                i++;
            } else if (ch === '"' || ch === "'" || ch === '`') {
                inString = ch;
            } else if (ch === '{' || ch === '(' || ch === '[') {
                depth++;
            } else if (ch === '}' || ch === ')' || ch === ']') {
                depth = Math.max(0, depth - 1);
            }
        }

        if (atLineEnd) {
            lineStart = i + 1;
        }
    }

    return props;
}

function extractInvokeTargets(segment) {
    const targets = new Set();
    // Facades call the host through the typed `call(...)` entry; `bridge.invoke(...)` remains
    // for the undeclared methods.
    const re = /(?:bridge\.invoke|(?<![.\w])call)(?:<[^>]+>)?\s*\(\s*['"]([^'"]+)['"]/g;
    let m;
    while ((m = re.exec(segment)) !== null) {
        targets.add(m[1]);
    }
    return [...targets];
}

function extractHelperCallTargets(segment) {
    const targets = new Set(extractInvokeTargets(segment));
    const re = /\b_?[A-Za-z_$][\w$]*Request\s*\(\s*['"]([a-z][\w]*\.[A-Za-z_$][\w$]*)['"]/g;
    let m;
    while ((m = re.exec(segment)) !== null) {
        targets.add(m[1]);
    }
    return [...targets];
}

function collectHelperTargets(src) {
    const helpers = new Map();
    const functionRe = /(?:async\s+)?function\s+([A-Za-z_$][\w$]*)\s*\([^)]*\)\s*(?::[^;{]+)?\{/g;
    let m;
    while ((m = functionRe.exec(src)) !== null) {
        const openIdx = src.indexOf('{', m.index);
        const closeIdx = findMatchingBrace(src, openIdx);
        if (closeIdx < 0) continue;
        const segment = src.slice(m.index, closeIdx + 1);
        const targets = extractHelperCallTargets(segment);
        if (targets.length > 0) helpers.set(m[1], targets);
        functionRe.lastIndex = closeIdx + 1;
    }

    const constRe = /const\s+([A-Za-z_$][\w$]*)\s*=\s*/g;
    while ((m = constRe.exec(src)) !== null) {
        const semiIdx = src.indexOf(';', m.index);
        if (semiIdx < 0) continue;
        const segment = src.slice(m.index, semiIdx + 1);
        const targets = extractHelperCallTargets(segment);
        if (targets.length > 0) helpers.set(m[1], targets);
    }

    return helpers;
}

function extractAliasIdentifier(prop, segment) {
    const re = new RegExp(
        String.raw`^\s*${prop.name}\s*:\s*([A-Za-z_$][\w$]*)\s*,`,
    );
    const m = segment.match(re);
    return m?.[1] ?? null;
}

export function extractSdkMethodsFromSource(src, file) {
    const object = extractExportedNamespaceObject(src);
    if (!object) return [];
    const props = findTopLevelProperties(object.body);
    const helperTargets = collectHelperTargets(src);
    const methods = [];

    for (let i = 0; i < props.length; i++) {
        const prop = props[i];
        const nextStart = props[i + 1]?.start ?? object.body.length;
        const segment = object.body.slice(prop.start, nextStart);
        let invokeTargets = extractInvokeTargets(segment);
        if (invokeTargets.length === 0) {
            const alias = extractAliasIdentifier(prop, segment);
            invokeTargets = alias ? (helperTargets.get(alias) ?? []) : [];
        }
        if (invokeTargets.length === 0) continue;
        methods.push({
            namespace: object.namespace,
            method: prop.name,
            api: `${object.namespace}.${prop.name}`,
            invokeTargets,
            file,
            line: lineOf(src, object.bodyStart + prop.start),
        });
    }
    return methods;
}

function collectSdkMethods(sdkRoot, relRoot, includeNamespaces) {
    const files = collectFiles(
        sdkRoot,
        (file) => file.endsWith('.ts') && !file.endsWith('.test.ts'),
    );
    const methods = [];
    for (const file of files) {
        const rel = path.relative(relRoot, file).replace(/\\/g, '/');
        const src = readUtf8(file);
        const extracted = extractSdkMethodsFromSource(src, rel);
        for (const method of extracted) {
            const publicNamespace = PUBLIC_NAMESPACE_ALIASES.get(method.namespace) ?? method.namespace;
            if (includeNamespaces?.size > 0 && !includeNamespaces.has(publicNamespace)) continue;
            methods.push({
                ...method,
                sourceNamespace: method.namespace,
                namespace: publicNamespace,
                api: `${publicNamespace}.${method.method}`,
            });
        }
    }
    methods.sort((a, b) => (a.api < b.api ? -1 : a.api > b.api ? 1 : 0));
    return methods;
}

function collectDocumentedCalls(docsRoot, relRoot = REPO_ROOT) {
    const files = collectFiles(docsRoot, (file) => file.endsWith('.md'));
    const documented = new Map();
    const re = /\bfb\.([A-Za-z_$][\w$]*)\.([A-Za-z_$][\w$]*)\s*\(/g;
    for (const file of files) {
        const rel = path.relative(relRoot, file).replace(/\\/g, '/');
        const src = readUtf8(file);
        let m;
        while ((m = re.exec(src)) !== null) {
            const api = `${m[1]}.${m[2]}`;
            if (!documented.has(api)) documented.set(api, []);
            documented.get(api).push({ file: rel, line: lineOf(src, m.index) });
        }
    }
    return documented;
}

export function audit(opts = {}) {
    const {
        sdkRoot = DEFAULT_SDK_ROOT,
        docsRoot = DEFAULT_DOCS_ROOT,
        relRoot = REPO_ROOT,
        includeNamespaces = null,
    } = opts;

    const sdkExists = fs.existsSync(sdkRoot);
    const docsExists = fs.existsSync(docsRoot);
    if (!sdkExists || !docsExists) {
        return {
            ok: false,
            sdkExists,
            docsExists,
            namespacesScanned: 0,
            totalSdkMethods: 0,
            totalDocumentedMethods: 0,
            totalGaps: 0,
            findings: [],
        };
    }

    const methods = collectSdkMethods(sdkRoot, relRoot, includeNamespaces);
    const documented = collectDocumentedCalls(docsRoot, relRoot);
    const findings = methods
        .filter((method) => !documented.has(method.api))
        .map((method) => ({
            namespace: method.namespace,
            method: method.method,
            api: method.api,
            file: method.file,
            line: method.line,
            invokeTargets: method.invokeTargets,
        }));

    const namespaces = new Set(methods.map((method) => method.namespace));
    const documentedInScope = [...documented.keys()].filter((api) => {
        const [ns] = api.split('.');
        return includeNamespaces?.size > 0 ? includeNamespaces.has(ns) : true;
    });

    return {
        ok: findings.length === 0,
        sdkExists,
        docsExists,
        namespacesScanned: namespaces.size,
        totalSdkMethods: methods.length,
        totalDocumentedMethods: documentedInScope.length,
        totalGaps: findings.length,
        findings,
    };
}

function loadBaseline(baselinePath) {
    if (!baselinePath) return null;
    try {
        return JSON.parse(fs.readFileSync(baselinePath, 'utf8'));
    } catch {
        return null;
    }
}

function findingKey(v) {
    return `${v.namespace}.${v.method}`;
}

function computeNewFindings(report, baseline) {
    if (!baseline || !Array.isArray(baseline.findings)) return report.findings;
    const seen = new Set(baseline.findings.map(findingKey));
    return report.findings.filter((v) => !seen.has(findingKey(v)));
}

function printHuman(report, newFindings, docsRootDisplay) {
    console.log('\nVitePress SDK Coverage Audit');
    console.log(`Docs root: ${docsRootDisplay}`);
    console.log(
        `Namespaces: ${report.namespacesScanned} | SDK methods: ${report.totalSdkMethods} | Documented calls: ${report.totalDocumentedMethods} | Gaps: ${report.totalGaps}`,
    );
    if (newFindings.length > 0) {
        console.log(`\nNew gaps (not in baseline): ${newFindings.length}`);
        for (const v of newFindings.slice(0, 40)) {
            console.log(
                `  ${v.api}  (${v.file}:L${v.line}; invokes ${v.invokeTargets.join(', ')})`,
            );
        }
        if (newFindings.length > 40) {
            console.log(`  ... and ${newFindings.length - 40} more`);
        }
    } else if (report.totalGaps > 0) {
        console.log('  (all gaps are in the baseline — no new findings)');
    } else {
        console.log('  No VitePress SDK coverage gaps');
    }
    console.log('');
}

function printJson(report, newFindings, docsRootDisplay) {
    console.log(
        JSON.stringify(
            {
                ok: newFindings.length === 0,
                docsRoot: docsRootDisplay,
                namespacesScanned: report.namespacesScanned,
                totalSdkMethods: report.totalSdkMethods,
                totalDocumentedMethods: report.totalDocumentedMethods,
                totalGaps: report.totalGaps,
                newFindings: newFindings.length,
                findings: report.findings,
            },
            null,
            2,
        ),
    );
}

function mainCli(args = process.argv.slice(2)) {
    const flags = parseFlags(args);
    if (flags.help) {
        printHelp();
        exit(0);
    }

    const docsRoot = flags.docsRoot
        ? resolveRepoPath(flags.docsRoot)
        : DEFAULT_DOCS_ROOT;
    const sdkRoot = flags.sdkRoot
        ? resolveRepoPath(flags.sdkRoot)
        : DEFAULT_SDK_ROOT;
    // When CLI points at a fixture workspace shaped like <root>/sdk/src/bridge/namespaces,
    // keep finding paths relative to that workspace root; otherwise use the real repo root.
    const displayRoot = /[\\/]sdk[\\/]src[\\/]bridge[\\/]namespaces$/i.test(
        path.resolve(sdkRoot).replace(/\\/g, '/'),
    )
        ? path.resolve(sdkRoot, '../../../..')
        : REPO_ROOT;
    const docsRootDisplay =
        path.relative(displayRoot, docsRoot).replace(/\\/g, '/') ||
        docsRoot.replace(/\\/g, '/');

    const report = audit({
        includeNamespaces: flags.includeNamespaces,
        docsRoot,
        sdkRoot,
        relRoot: displayRoot,
    });
    if (!report.sdkExists || !report.docsExists) {
        const msg = `[audit_vitepress_sdk_coverage] required tree missing: sdkExists=${report.sdkExists}, docsExists=${report.docsExists}, docsRoot=${docsRootDisplay}`;
        if (flags.json) {
            console.error(JSON.stringify({ ok: false, error: msg, docsRoot: docsRootDisplay }));
        } else {
            console.error(msg);
        }
        exit(2);
    }

    const baseline = loadBaseline(flags.baseline);
    const newFindings = computeNewFindings(report, baseline);

    if (flags.json) {
        printJson(report, newFindings, docsRootDisplay);
    } else {
        printHuman(report, newFindings, docsRootDisplay);
    }

    if (flags.strict && newFindings.length > 0) {
        exit(1);
    }
    if (flags.warnOnly) {
        exit(0);
    }
    exit(0);
}

const entryArg = process.argv[1];
const isDirectRun =
    typeof entryArg === 'string' &&
    fileURLToPath(import.meta.url) === path.resolve(entryArg);
if (isDirectRun) mainCli();

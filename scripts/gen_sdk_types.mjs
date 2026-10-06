#!/usr/bin/env node
// scripts/gen_sdk_types.mjs
// Generates the SDK's TypeScript types from the API declarations under src/api/schema,
// into sdk/src/types/generated/:
//   schema-types.ts  the named interfaces the declarations share
//   params.ts        `{Api}Params` for every declared method + `ApiParamsMap`
//   responses.ts     `{Api}Success` / `{Api}Response` + `ApiResponseMap`
//   events.ts        `{Event}Payload` + `FBEventName` + `FBEventPayloadMap`
//   index.ts         barrel re-export + `ApiMethodMap`
//   param-shapes.ts  the parameter keys of every declared method as runtime data, for the
//                    `foo-webview-sdk/schema` entry; not re-exported by index.ts
// Methods registered without a declaration (the menu overlay's `menu.__*`, `test.echo`,
// `test.ping`) get no generated types; the SDK types `test.echo` and `test.ping` by hand.
//
//   node scripts/gen_sdk_types.mjs --all              write the generated files
//   node scripts/gen_sdk_types.mjs --all --diff       exit 3 when a generated file is stale
//   node scripts/gen_sdk_types.mjs --all --dry-run    list the files, write nothing
//   node scripts/gen_sdk_types.mjs --all --validate   write, then run the SDK type-check
//
// `--all` is accepted for the existing callers; generating everything is the only mode.

import fs from 'node:fs';
import path from 'node:path';
import process from 'node:process';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

import { allProperties, findType, loadNamespaces, SchemaError } from './api-schema/schema.mjs';
import {
  apiTypeName,
  emitTsEventPayload,
  emitTsMethodLifecycle,
  emitTsNamedTypes,
  emitTsParams,
  emitTsResponse,
  eventTypeName,
} from './api-schema/emit-ts.mjs';

const REPO_ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const DEFAULT_GENERATED_DIR = path.join(REPO_ROOT, 'sdk', 'src', 'types', 'generated');
const DEFAULT_SDK_DIR = path.join(REPO_ROOT, 'sdk');

const EXIT_OK = 0;
const EXIT_ERROR = 1; // bad CLI, invalid declarations, IO failure
const EXIT_DRIFT = 3; // --diff found a stale file
const EXIT_TYPECHECK = 4; // --validate: the SDK type-check failed

function parseArgs(argv) {
  const opts = { outDir: DEFAULT_GENERATED_DIR, sdkDir: DEFAULT_SDK_DIR, dryRun: false, diff: false, validate: false, verbose: false, stats: false, help: false };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === '--help' || a === '-h') opts.help = true;
    else if (a === '--all') continue;
    else if (a === '--dry-run') opts.dryRun = true;
    else if (a === '--diff') opts.diff = true;
    else if (a === '--validate') opts.validate = true;
    else if (a === '--verbose') opts.verbose = true;
    else if (a === '--stats') opts.stats = true;
    else if (a === '--out-dir') opts.outDir = argv[++i];
    else if (a.startsWith('--out-dir=')) opts.outDir = a.slice('--out-dir='.length);
    else if (a === '--sdk-dir') opts.sdkDir = argv[++i];
    else if (a.startsWith('--sdk-dir=')) opts.sdkDir = a.slice('--sdk-dir='.length);
    else throw new Error(`Unknown argument: ${a}\nRun with --help for usage.`);
  }
  opts.outDir = path.resolve(REPO_ROOT, opts.outDir);
  opts.sdkDir = path.resolve(REPO_ROOT, opts.sdkDir);
  return opts;
}

function printHelp() {
  process.stdout.write(
    [
      'Usage: node scripts/gen_sdk_types.mjs [--all] [options]',
      '',
      'Generates sdk/src/types/generated/ from the declarations under src/api/schema.',
      '',
      '  --out-dir DIR   Target directory (default: sdk/src/types/generated)',
      '  --sdk-dir DIR   SDK root whose type-check --validate runs (default: sdk/)',
      '  --dry-run       List the files and their sizes, write nothing',
      '  --diff          Compare with the files on disk; exit 3 when any differs',
      '  --validate      Write, then run the SDK type-check',
      '  --stats         Print sizes and counts',
      '  --verbose       Print progress on stderr',
      '  --help, -h      Show this help',
      '',
      'Exit codes: 0 ok, 1 error, 3 stale file (--diff), 4 type-check failed (--validate)',
      '',
    ].join('\n'),
  );
}

// ── Declarations ────────────────────────────────────────────────────────────

// Every declared method and event. The menu overlay's internal `<ns>.__*` endpoints are
// undeclared; the filter keeps any that get declared out of the public types too.
function collectDeclarations(repoRoot) {
  const namespaces = loadNamespaces(repoRoot);
  const methods = new Map();
  const events = new Map();
  for (const ns of namespaces) {
    for (const m of ns.methods) if (!m.api.includes('.__')) methods.set(m.api, m);
    for (const e of ns.events ?? []) events.set(e.name, e);
  }
  return { namespaces, methods: sorted(methods), events: sorted(events) };
}

function sorted(map) {
  return new Map([...map.entries()].sort(([a], [b]) => a.localeCompare(b)));
}

// Two names that PascalCase to the same interface would overwrite each other in the maps.
function assertNoCollisions(names, typeName, what) {
  const seen = new Map();
  for (const name of names) {
    const t = typeName(name);
    if (seen.has(t)) throw new SchemaError(`${what} ${seen.get(t)} and ${name} both map to ${t}`);
    seen.set(t, name);
  }
}

// ── File rendering ──────────────────────────────────────────────────────────

function fileHeader(fileName) {
  return [
    '// ─────────────────────────────────────────────────────────────',
    '// GENERATED FILE — DO NOT EDIT',
    `// File: sdk/src/types/generated/${fileName}`,
    '// Source: src/api/schema/*.ts',
    '// Emitter: scripts/gen_sdk_types.mjs',
    '// Regenerate: npm run gen:types (in sdk/) or `node scripts/gen_sdk_types.mjs --all`',
    '// ─────────────────────────────────────────────────────────────',
    '',
    '/* eslint-disable */',
    '',
  ].join('\n');
}

// Joins the sections, drops empty ones and collapses runs of blank lines.
function assemble(sections) {
  const content = sections.filter(Boolean).join('\n').replace(/\n{3,}/g, '\n\n');
  return content.endsWith('\n') ? content : `${content}\n`;
}

// The JSON helper types (sdk/src/types/json.ts) a file's blocks use.
function jsonTypeImports(text) {
  const names = ['JsonObject', 'JsonValue'].filter((n) => new RegExp(`\\b${n}\\b`).test(text));
  return names.length ? [`import type { ${names.join(', ')} } from '../json.js';`, ''] : [];
}

// The named interfaces (schema-types.ts) a file's blocks refer to.
function schemaTypeImports(usedTypes) {
  if (!usedTypes.size) return [];
  return [`import type { ${[...usedTypes].sort().join(', ')} } from './schema-types.js';`, ''];
}

function renderSchemaTypes(decl) {
  const blocks = emitTsNamedTypes(decl.namespaces).join('\n\n');
  return assemble([
    fileHeader('schema-types.ts'),
    jsonTypeImports(blocks).join('\n'),
    '// ── Named interfaces from src/api/schema ────────────────────────────────',
    blocks || 'export {};',
    '',
  ]);
}

function methodMap(decl, mapName, role, what) {
  return [
    '/**',
    ` * Map from each declared method name to its ${what} type.`,
    ' */',
    `export interface ${mapName} {`,
    ...[...decl.methods].flatMap(([api, method]) => [...emitTsMethodLifecycle(method, '    '), `    ${JSON.stringify(api)}: ${apiTypeName(api, role)};`]),
    '}',
  ].join('\n');
}

function renderParams(decl) {
  const used = new Set();
  const blocks = [...decl.methods.entries()].map(([api, m]) => emitTsParams(m, apiTypeName(api, 'Params'), used)).join('\n\n');
  return assemble([
    fileHeader('params.ts'),
    [...jsonTypeImports(blocks), ...schemaTypeImports(used)].join('\n'),
    blocks,
    '',
    '// ── API-name → Params map ────────────────────────────────────────────────',
    methodMap(decl, 'ApiParamsMap', 'Params', '`*Params`'),
    '',
  ]);
}

function renderResponses(decl) {
  const used = new Set();
  const blocks = [...decl.methods.entries()].map(([api, m]) => emitTsResponse(m, apiTypeName(api, 'Response'), used)).join('\n\n');
  // A response is `XxxSuccess | ApiFailure`; the failure envelope is hand-written next to
  // ApiErrorCode in sdk/src/types/responses.ts.
  const failureImport = /\bApiFailure\b/.test(blocks) ? ["import type { ApiFailure } from '../responses.js';", ''] : [];
  return assemble([
    fileHeader('responses.ts'),
    [...jsonTypeImports(blocks), ...failureImport, ...schemaTypeImports(used)].join('\n'),
    blocks,
    '',
    '// ── API-name → Response map ──────────────────────────────────────────────',
    methodMap(decl, 'ApiResponseMap', 'Response', '`*Response`'),
    '',
  ]);
}

function renderEvents(decl) {
  const used = new Set();
  const names = [...decl.events.keys()];
  const blocks = [...decl.events.entries()].map(([name, e]) => emitTsEventPayload(e, eventTypeName(name), used)).join('\n\n');
  // The union closes with a semicolon on the last member so an added event is a one-line diff.
  const union = ['export type FBEventName =', ...names.map((n) => `    | ${JSON.stringify(n)}`)];
  union[union.length - 1] += ';';
  return assemble([
    fileHeader('events.ts'),
    [...jsonTypeImports(blocks), ...schemaTypeImports(used)].join('\n'),
    blocks,
    '',
    '// ── Event-name union ─────────────────────────────────────────────────────',
    '/**',
    ' * Literal-union of every event declared under `src/api/schema/`.',
    ' */',
    union.join('\n'),
    '',
    '// ── Event-name → Payload map ─────────────────────────────────────────────',
    '/**',
    ' * Master map from event name to its generated `*Payload` type. Consumed by',
    ' * typed `fb.on(...)` overloads.',
    ' */',
    ['export interface FBEventPayloadMap {', ...names.map((n) => `    ${JSON.stringify(n)}: ${eventTypeName(n)};`), '}'].join('\n'),
    '',
  ]);
}

function renderIndex(decl) {
  const apis = [...decl.methods.keys()];
  return assemble([
    fileHeader('index.ts'),
    '// ── Barrel re-export ─────────────────────────────────────────────────────',
    "export * from './schema-types.js';",
    "export * from './params.js';",
    "export * from './responses.js';",
    "export * from './events.js';",
    '',
    '// ── API-method map (tuple of [Params, Response] per handler) ──────────────',
    '/**',
    ' * The master `api_name` → `[Params, Response]` tuple map.',
    ' *',
    ' * Consumed by the typed `call(method, params)` entry in `bridge/call.ts`:',
    ' * the method name selects the params and response types. The public',
    ' * `bridge.invoke` stays untyped for dynamic dispatch.',
    ' */',
    'import type {',
    ...apis.map((api) => `    ${apiTypeName(api, 'Params')},`),
    "} from './params.js';",
    'import type {',
    ...apis.map((api) => `    ${apiTypeName(api, 'Response')},`),
    "} from './responses.js';",
    '',
    ['export interface ApiMethodMap {', ...apis.flatMap((api) => [...emitTsMethodLifecycle(decl.methods.get(api), '    '), `    ${JSON.stringify(api)}: [${apiTypeName(api, 'Params')}, ${apiTypeName(api, 'Response')}];`]), '}'].join('\n'),
    '',
  ]);
}

// The keys every declared method accepts, as the host's generated parser checks them: which
// keys an object takes, which it needs, whether it takes other keys too (a map), and the same
// for a nested object or the objects of an array. A recursive type (a menu item holding
// submenu items) is written once in PARAM_SHAPE_TYPES and named where it recurs.
function renderParamShapes(decl) {
  const shared = new Map();
  const shapeOf = (obj, ns) => {
    const resolve = (name, common) => findType(name, ns, decl.namespaces, common);
    const keys = {};
    const required = [];
    for (const p of allProperties(obj, resolve)) {
      keys[p.key] = valueOf(p, ns);
      if (p.required) required.push(p.key);
    }
    const shape = { keys };
    if (required.length) shape.required = required;
    if (obj.additional) shape.open = true;
    return shape;
  };
  const valueOf = (p, ns) => {
    if (p.kind === 'array') return valueOf(p.items, ns);
    if (p.kind === 'ref') {
      if (!shared.has(p.name)) {
        shared.set(p.name, null);
        const target = findType(p.name, ns, decl.namespaces, p.common);
        if (!target) throw new SchemaError(`${p.name} is used but no schema declares it`);
        shared.set(p.name, shapeOf(target, ns));
      }
      return p.name;
    }
    if (p.kind === 'object' && (p.properties.length || p.extends)) return shapeOf(p, ns);
    return null;
  };
  const nsOf = new Map();
  for (const ns of decl.namespaces) for (const m of ns.methods) nsOf.set(m.api, ns);
  const methods = {};
  for (const [api, m] of decl.methods) methods[api] = shapeOf(m.params, nsOf.get(api));
  const types = Object.fromEntries([...shared.entries()].sort(([a], [b]) => a.localeCompare(b)));
  return assemble([
    fileHeader('param-shapes.ts'),
    "import type { ApiParamsMap } from './params.js';",
    '',
    '/**',
    ' * The keys a parameter object takes, as the host checks them before a handler runs.',
    ' */',
    'export interface ParamShape {',
    '    /**',
    '     * Every key the object takes. A shape describes the value, or each element when the value',
    '     * is an array; a string names a shape in {@link PARAM_SHAPE_TYPES}; `null` means the value',
    '     * is not checked further.',
    '     */',
    '    readonly keys: { readonly [key: string]: ParamShape | string | null };',
    '    /** Keys that have to be present and not `null`. */',
    '    readonly required?: readonly string[];',
    '    /** Whether keys other than {@link keys} are taken as well, as by a map. */',
    '    readonly open?: boolean;',
    '}',
    '',
    '/** Shapes of the parameter types that contain themselves, by name. */',
    `export const PARAM_SHAPE_TYPES: { readonly [name: string]: ParamShape } = ${JSON.stringify(types, null, 4)};`,
    '',
    '/** The shape of the parameter object of every declared method. */',
    `export const API_PARAM_SHAPES: { readonly [M in keyof ApiParamsMap]: ParamShape } = ${JSON.stringify(methods, null, 4)};`,
    '',
  ]);
}

/** Renders the generated files from the declarations, in memory. */
export function renderAll(repoRoot = REPO_ROOT) {
  const decl = collectDeclarations(repoRoot);
  assertNoCollisions(decl.methods.keys(), (api) => apiTypeName(api, 'Params'), 'methods');
  assertNoCollisions(decl.events.keys(), eventTypeName, 'events');
  return {
    decl,
    files: [
      { name: 'schema-types.ts', content: renderSchemaTypes(decl) },
      { name: 'params.ts', content: renderParams(decl) },
      { name: 'responses.ts', content: renderResponses(decl) },
      { name: 'events.ts', content: renderEvents(decl) },
      { name: 'index.ts', content: renderIndex(decl) },
      { name: 'param-shapes.ts', content: renderParamShapes(decl) },
    ],
  };
}

// ── Main ────────────────────────────────────────────────────────────────────

function runTypeCheck(sdkDir) {
  const result = spawnSync(process.platform === 'win32' ? 'npm.cmd' : 'npm', ['run', '--silent', 'type-check'], {
    cwd: sdkDir,
    encoding: 'utf8',
    // npm.cmd is a batch file; cmd.exe finds it through PATHEXT.
    shell: process.platform === 'win32',
  });
  return { code: typeof result.status === 'number' ? result.status : 1, output: `${result.stdout ?? ''}${result.stderr ?? ''}` };
}

const rel = (p) => path.relative(REPO_ROOT, p).replace(/\\/g, '/');
const lineCount = (text) => text.split('\n').length - 1;

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

  let rendered;
  try {
    rendered = renderAll();
  } catch (err) {
    if (!(err instanceof SchemaError)) throw err;
    process.stderr.write(`api-schema: ${err.message}\n`);
    return EXIT_ERROR;
  }
  const { decl, files } = rendered;
  if (opts.verbose) process.stderr.write(`${decl.methods.size} declared methods, ${decl.events.size} declared events\n`);

  if (opts.dryRun) {
    process.stdout.write(`[dry-run] would write ${files.length} file(s) to ${rel(opts.outDir)}/\n`);
    for (const f of files) {
      process.stdout.write(`  ${f.name.padEnd(16)} ${String(Buffer.byteLength(f.content, 'utf8')).padStart(7)} bytes  ${String(lineCount(f.content)).padStart(5)} lines\n`);
    }
  }

  if (opts.diff) {
    const stale = files.filter((f) => {
      const target = path.join(opts.outDir, f.name);
      return !fs.existsSync(target) || fs.readFileSync(target, 'utf8') !== f.content;
    });
    if (stale.length) {
      process.stderr.write(`[diff] ${stale.length} file(s) differ from ${rel(opts.outDir)}/: ${stale.map((f) => f.name).join(', ')}\n`);
      process.stderr.write('Run `npm run gen:types` in sdk/ and commit the result.\n');
      return EXIT_DRIFT;
    }
    if (opts.verbose) process.stderr.write(`[diff] ${files.length} file(s) up to date\n`);
  }

  if (!opts.dryRun && !opts.diff) {
    fs.mkdirSync(opts.outDir, { recursive: true });
    for (const f of files) fs.writeFileSync(path.join(opts.outDir, f.name), f.content, 'utf8');
    if (opts.verbose) process.stderr.write(`wrote ${files.length} file(s) to ${rel(opts.outDir)}/\n`);
  }

  if (opts.validate) {
    const tc = runTypeCheck(opts.sdkDir);
    if (tc.code !== 0) {
      process.stderr.write(`[validate] SDK type-check failed (exit ${tc.code})\n${tc.output}`);
      return EXIT_TYPECHECK;
    }
  }

  if (opts.stats) {
    process.stdout.write(`methods=${decl.methods.size} events=${decl.events.size}\n`);
    for (const f of files) process.stdout.write(`${f.name.padEnd(16)} ${lineCount(f.content)} lines\n`);
  }
  return EXIT_OK;
}

// Run when invoked directly, not when imported by the tests. Both sides are resolved to
// filesystem paths: on Windows import.meta.url is a file:/// URL and argv[1] a plain path.
if (process.argv[1] && path.resolve(process.argv[1]) === path.resolve(fileURLToPath(import.meta.url))) {
  process.exit(run(process.argv.slice(2)));
}

// Generates the C++ parameter headers, the docs regions and the MCP bridge tools from
// src/api/schema/<namespace>.ts (the MCP tools also read mcp/tool-table.json, see emit-mcp.mjs).
// The SDK TypeScript types come from the same declarations through scripts/gen_sdk_types.mjs.
//
//   node scripts/api-schema/generate.mjs --write       regenerate headers, docs regions and MCP tools
//   node scripts/api-schema/generate.mjs --write-cpp   regenerate the headers only (the build runs this)
//   node scripts/api-schema/generate.mjs --check       exit 1 when anything is stale or unregistered
//
// --write-cpp leaves the docs and the MCP tools alone so that a C++ build never edits Markdown
// or fails on a missing docs region; --check and --write also verify that every declared method is
// registered through api::RegisterApi and every declared event emitted through the helpers of
// src/api/EventEmit.h. Exit code 2 means the declarations themselves are wrong.
import fs from 'node:fs';
import path from 'node:path';
import process from 'node:process';
import { fileURLToPath } from 'node:url';

import { loadNamespaces, SchemaError } from './schema.mjs';
import { cppHeaderPath, emitCppHeader, emitEventRegistry, EVENT_REGISTRY_PATH, hasCppHeader } from './emit-cpp.mjs';
import { planDocs } from './emit-docs.mjs';
import { planMcp } from './emit-mcp.mjs';
import { checkRegistrations, readSources } from './registrations.mjs';
import { checkEmits, readEmitSources } from './emits.mjs';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const GENERATED_DIR = 'src/api/generated';

function plan({ docs }) {
  const namespaces = loadNamespaces(repoRoot);
  const files = new Map();
  for (const ns of namespaces) if (hasCppHeader(ns)) files.set(cppHeaderPath(ns), emitCppHeader(ns, namespaces));
  const registry = emitEventRegistry(namespaces);
  if (registry) files.set(EVENT_REGISTRY_PATH, registry);
  const problems = [];
  if (docs) {
    const planned = planDocs(repoRoot, namespaces);
    for (const c of planned.changes) files.set(c.file, c.next);
    const mcp = planMcp(repoRoot, namespaces);
    for (const [rel, next] of mcp.files) files.set(rel, next);
    problems.push(...planned.problems, ...mcp.problems, ...checkRegistrations(namespaces, readSources(repoRoot)), ...checkEmits(namespaces, readEmitSources(repoRoot)));
  }
  // Headers left behind by a schema that no longer exists.
  const stray = [];
  const dir = path.join(repoRoot, GENERATED_DIR);
  if (fs.existsSync(dir)) {
    for (const f of fs.readdirSync(dir)) {
      const rel = `${GENERATED_DIR}/${f}`;
      if (!files.has(rel)) stray.push(rel);
    }
  }
  return { namespaces, files, problems, stray };
}

function main() {
  const modes = { '--write': 'write', '--write-cpp': 'write-cpp', '--check': 'check' };
  const mode = modes[process.argv.slice(2).find((a) => a in modes)];
  if (!mode) {
    console.error('usage: node scripts/api-schema/generate.mjs --write | --write-cpp | --check');
    return 2;
  }
  let p;
  try {
    p = plan({ docs: mode !== 'write-cpp' });
  } catch (e) {
    if (e instanceof SchemaError) {
      console.error(`api-schema: ${e.message}`);
      return 2;
    }
    throw e;
  }
  const stale = [];
  for (const [rel, next] of p.files) {
    const abs = path.join(repoRoot, rel);
    const cur = fs.existsSync(abs) ? fs.readFileSync(abs, 'utf8') : null;
    if (cur === next) continue;
    stale.push(rel);
    if (mode !== 'check') {
      fs.mkdirSync(path.dirname(abs), { recursive: true });
      fs.writeFileSync(abs, next);
    }
  }
  if (mode !== 'check') {
    for (const rel of p.stray) fs.rmSync(path.join(repoRoot, rel));
    for (const msg of p.problems) console.error(`api-schema: ${msg}`);
    console.log(`api-schema: ${p.namespaces.length} namespace(s); wrote ${stale.length} file(s), removed ${p.stray.length}.`);
    return p.problems.length ? 1 : 0;
  }
  for (const rel of stale) console.error(`api-schema: stale ${rel}`);
  for (const rel of p.stray) console.error(`api-schema: stray ${rel}`);
  for (const msg of p.problems) console.error(`api-schema: ${msg}`);
  if (stale.length || p.stray.length) console.error('api-schema: run `node scripts/api-schema/generate.mjs --write` and stage the result.');
  const bad = stale.length + p.stray.length + p.problems.length;
  if (!bad) console.log(`api-schema: ${p.namespaces.length} namespace(s) up to date.`);
  return bad ? 1 : 0;
}

process.exit(main());

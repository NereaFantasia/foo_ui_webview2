// Ties each declared event to the C++ that emits it, the way registrations.mjs ties methods
// to their registrations.
//
// A declared event is emitted through the helpers in src/api/EventEmit.h, which take the
// generated descriptor (`api::<ns>::events::<Event>`) and read the name from it. So the name
// of a declared event never appears as a string literal in src/ outside the generated
// headers: a literal means an emission, a comparison or a name picked at run time that
// bypasses the declaration. And an event that no source names through its descriptor is
// declared but never emitted.
//
// The other direction: an emission whose first argument (the second for SendEventTo, whose
// first is the window) is the literal name of an undeclared event is reported, unless the
// name is in INTERNAL. Names built at run time are not literals and are not seen here.
import fs from 'node:fs';
import path from 'node:path';

import { pascal } from './schema.mjs';
import { cppNamespace } from './emit-cpp.mjs';

const SOURCE_ROOT = 'src';
const GENERATED_DIR = 'src/api/generated';
const EVENT_NAME = '[a-z][A-Za-z0-9]*:[A-Za-z_][A-Za-z0-9_]*';

// Events emitted on the private bridge of the menu overlay page, which no public page sees.
export const INTERNAL = new Map([
  ['menu:show', 'menu overlay page only'],
  ['menu:__hide', 'menu overlay page only'],
  ['menu:__placed', 'menu overlay page only'],
  ['menu:__submenuClosed', 'menu overlay page only'],
  ['menu:__submenuOpened', 'menu overlay page only'],
  ['menu:__submenuVisible', 'menu overlay page only'],
]);

// Every C++ source under src/ except the generated headers, as { file, text }.
export function readEmitSources(repoRoot) {
  const out = [];
  const walk = (dir) => {
    for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
      const abs = path.join(dir, entry.name);
      const rel = path.relative(repoRoot, abs).split(path.sep).join('/');
      if (entry.isDirectory()) {
        if (rel !== GENERATED_DIR) walk(abs);
      } else if (/\.(cpp|h)$/.test(entry.name)) {
        out.push({ file: rel, text: fs.readFileSync(abs, 'utf8') });
      }
    }
  };
  const root = path.join(repoRoot, SOURCE_ROOT);
  if (fs.existsSync(root)) walk(root);
  return out;
}

function lineOf(text, index) {
  let line = 1;
  for (let i = 0; i < index; i += 1) if (text.charCodeAt(i) === 10) line += 1;
  return line;
}

// Calls whose first argument is the event name, and SendEventTo, whose second is.
const EMIT_FIRST = new RegExp(`\\b(?:EmitEvent|BroadcastEvent|BroadcastEventExcept|EmitEventTo|PostEventMessage|EmitApiEvent|EmitPluginEvent|Emit)\\s*\\(\\s*"(${EVENT_NAME})"`, 'g');
const EMIT_SECOND = new RegExp(`\\bSendEventTo\\s*\\([^;"]*?,\\s*"(${EVENT_NAME})"`, 'g');

// Whether a source names the descriptor of one event: through its namespace, with or without
// `api::`, or through an alias the file declares for that namespace (`namespace cur =
// api::cursor;`). Two namespaces may declare events with the same key, so the namespace is
// part of the match.
function namesDescriptor(text, ns, key) {
  const cppNs = cppNamespace(ns);
  const names = [cppNs];
  for (const m of text.matchAll(/\bnamespace\s+(\w+)\s*=\s*(?:::)?api::(\w+)\s*;/g)) if (m[2] === cppNs) names.push(m[1]);
  return new RegExp(`(?<![\\w:])(?:(?:::)?api::)?(?:${names.join('|')})::events::${pascal(key)}\\b`).test(text);
}

export function checkEmits(namespaces, sources, { internal = INTERNAL } = {}) {
  const problems = [];
  const declared = new Map();
  for (const ns of namespaces) for (const e of ns.events ?? []) declared.set(e.name, { ns, event: e });

  for (const [name, { ns, event }] of declared) {
    const literal = new RegExp(`"${name.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}"`, 'g');
    const descriptor = `${cppNamespace(ns)}::events::${pascal(event.key)}`;
    let emitted = false;
    for (const { file, text } of sources) {
      for (const hit of text.matchAll(literal)) {
        problems.push(`${name} is declared in ${ns.file} but its name appears as a literal at ${file}:${lineOf(text, hit.index)}; emit it through src/api/EventEmit.h with ${descriptor}`);
      }
      if (namesDescriptor(text, ns, event.key)) emitted = true;
    }
    if (!emitted) problems.push(`${name} is declared in ${ns.file} but no source under src/ emits it through ${descriptor}`);
  }

  for (const { file, text } of sources) {
    for (const re of [EMIT_FIRST, EMIT_SECOND]) {
      for (const hit of text.matchAll(re)) {
        const name = hit[1];
        if (declared.has(name) || internal.has(name)) continue;
        problems.push(`${name} is emitted at ${file}:${lineOf(text, hit.index)} without a declaration; declare it in the Events of src/api/schema/ and emit it through src/api/EventEmit.h`);
      }
    }
  }
  return problems;
}

// Ties each declared method to its registration in the C++ sources, and each registration to
// a declaration.
//
// The match is exact, not inferred: api::RegisterApi and api::RegisterApiDeferred take the
// method name as a string literal that MethodName compares against the generated kMethod at
// compile time, so a declared method is registered iff `api::RegisterApi("<ns.method>"` or
// `api::RegisterApiDeferred("<ns.method>"` appears in a source file. BridgeCore's own
// RegisterApi and RegisterApiDeferred are private to those two, so the only other way in is
// BridgeCore::RegisterUndeclaredApi, which skips the generated parser: a declared name there
// is reported.
//
// The other direction: a typed registration of an undeclared name does not compile, and every
// literal name passed to RegisterUndeclaredApi has to be in UNDECLARED_RAW, each with its
// reason; a listed name that nothing registers is reported too. Registrations whose name is
// built at run time (PluginRegistry) are not string literals and are not seen here.
import fs from 'node:fs';
import path from 'node:path';

const SOURCE_ROOT = 'src';

// Methods registered through BridgeCore::RegisterUndeclaredApi without a declaration, with the reason.
export const UNDECLARED_RAW = new Map([
  ['menu.__getMenuState', 'menu overlay page only; the name does not pass the method key rule'],
  ['menu.__select', 'menu overlay page only; the name does not pass the method key rule'],
  ['menu.__dismiss', 'menu overlay page only; the name does not pass the method key rule'],
  ['menu.__ready', 'menu overlay page only; the name does not pass the method key rule'],
  ['menu.__submenuPanel', 'menu overlay page only; the name does not pass the method key rule'],
  ['menu.__valueChanged', 'menu overlay page only; the name does not pass the method key rule'],
  ['test.echo', 'echoes whatever keys it receives, which the generated reader refuses'],
  ['test.ping', 'self-test companion of test.echo'],
]);

function escapeRegex(s) {
  return s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

// Every C++ implementation file under src/, as { file, text } with repo-relative paths.
export function readSources(repoRoot) {
  const out = [];
  const walk = (dir) => {
    for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
      const abs = path.join(dir, entry.name);
      if (entry.isDirectory()) walk(abs);
      else if (entry.name.endsWith('.cpp')) {
        out.push({ file: path.relative(repoRoot, abs).split(path.sep).join('/'), text: fs.readFileSync(abs, 'utf8') });
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

// Returns one problem string per declared method that no source registers through the typed
// api:: functions, one per registration of a declared method that skips them, one per untyped
// registration of a name that is neither declared nor in `undeclared`, and one per name in
// `undeclared` that no source registers through RegisterUndeclaredApi.
export function checkRegistrations(namespaces, sources, undeclared = UNDECLARED_RAW) {
  const problems = [];
  const declared = new Set();
  for (const ns of namespaces) {
    for (const m of ns.methods) {
      declared.add(m.api);
      const literal = escapeRegex(`("${m.api}"`);
      const typed = new RegExp(`\\bapi::RegisterApi(?:Deferred)?\\s*${literal}`);
      const raw = new RegExp(`(?<!api::)\\bRegister(?:Undeclared)?Api(?:Deferred)?\\s*${literal}`, 'g');
      let registered = false;
      for (const { file, text } of sources) {
        if (typed.test(text)) registered = true;
        for (const hit of text.matchAll(raw)) {
          const via = hit[0].startsWith('RegisterUndeclaredApi') ? 'RegisterUndeclaredApi' : hit[0].startsWith('RegisterApiDeferred') ? 'RegisterApiDeferred' : 'the raw-json RegisterApi';
          problems.push(`${m.api} is declared in ${ns.file} but ${file}:${lineOf(text, hit.index)} registers it through ${via}; use api::RegisterApi or api::RegisterApiDeferred so the generated parser runs`);
        }
      }
      if (!registered) problems.push(`${m.api} is declared in ${ns.file} but no src/**/*.cpp registers it with api::RegisterApi("${m.api}" or api::RegisterApiDeferred("${m.api}"`);
    }
  }
  const rawAny = /(?<!api::)\bRegister(Undeclared)?Api(?:Deferred)?\s*\(\s*"([^"]+)"/g;
  const undeclaredSeen = new Set();
  for (const { file, text } of sources) {
    for (const hit of text.matchAll(rawAny)) {
      const name = hit[2];
      if (hit[1] && undeclared.has(name)) undeclaredSeen.add(name);
      if (declared.has(name) || undeclared.has(name)) continue;
      problems.push(`${name} is registered at ${file}:${lineOf(text, hit.index)} without a declaration; declare it in src/api/schema/ and register it with api::RegisterApi, or list it in UNDECLARED_RAW with the reason`);
    }
  }
  for (const name of undeclared.keys()) {
    if (!undeclaredSeen.has(name)) problems.push(`${name} is listed in UNDECLARED_RAW but no src/**/*.cpp registers it with RegisterUndeclaredApi("${name}"`);
  }
  return problems;
}

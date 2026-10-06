// Type-checks the js/ts code blocks of the docs site with TypeScript, through the
// twoslash engine, against the types of the SDK the site documents.
//
// Diagnostics that already existed when this check was introduced are recorded in
// baseline.json, keyed by page and message but not by line, so edits that move a
// block do not churn it. The check fails on any diagnostic that is not in the
// baseline, and on baseline entries that no longer occur, so the file shrinks as
// examples get fixed.
//
// Usage (from docs/vitepress, or pass paths relative to the repo root):
//   node .examples/check.mjs                      check every page
//   node .examples/check.mjs <page.md> ...        check only these pages
//   node .examples/check.mjs --update-baseline [<page.md> ...]
//
// A block opts out with `@ts-nocheck` in its fence info string, the flag Electron's
// docs use for the same purpose:  ```js @ts-nocheck
import fs from 'node:fs';
import path from 'node:path';
import process from 'node:process';

const here = import.meta.dirname;
const docsRoot = path.resolve(here, '..');
const repoRoot = path.resolve(docsRoot, '../..');
const baselinePath = path.join(here, 'baseline.json');
const LANGS = new Map([['js', 'js'], ['javascript', 'js'], ['ts', 'ts'], ['typescript', 'ts']]);

const args = process.argv.slice(2);
const update = args.includes('--update-baseline');
const requested = args.filter((a) => !a.startsWith('--'));

if (!fs.existsSync(path.join(repoRoot, 'sdk/dist/index.d.ts'))) {
  console.error('check-examples: sdk/dist/index.d.ts is missing; run `npm --prefix sdk run build` first.');
  process.exit(2);
}

let createTwoslasher, ts, MarkdownIt;
try {
  ({ createTwoslasher } = await import('twoslash'));
  ts = (await import('typescript')).default;
  MarkdownIt = (await import('markdown-it')).default;
} catch (e) {
  console.error(`check-examples: ${e.message}\nRun \`npm install\` in docs/vitepress first.`);
  process.exit(2);
}

function listPages() {
  const out = [];
  (function walk(dir) {
    for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
      if (e.name.startsWith('.') || e.name === 'node_modules' || e.name === 'dist' || e.name === 'public') continue;
      const p = path.join(dir, e.name);
      if (e.isDirectory()) walk(p);
      else if (e.name.endsWith('.md')) out.push(p);
    }
  })(docsRoot);
  return out;
}

// Accept paths relative to the repo root (as a git hook passes them) or to the current directory.
function resolvePage(p) {
  for (const base of [process.cwd(), repoRoot]) {
    const abs = path.resolve(base, p);
    if (fs.existsSync(abs)) return abs;
  }
  return null;
}

const pageKey = (abs) => path.relative(docsRoot, abs).split(path.sep).join('/');

const unresolved = requested.filter((p) => !resolvePage(p));
if (unresolved.length) {
  console.error(`check-examples: no such file (paths are relative to the repo root or the current directory):\n  ${unresolved.join('\n  ')}`);
  process.exit(2);
}
const pages = requested.length
  ? requested.map(resolvePage).filter((p) => p.startsWith(docsRoot + path.sep) && p.endsWith('.md'))
  : listPages();
if (requested.length && pages.length === 0) process.exit(0);

const env = fs.readFileSync(path.join(here, 'env.d.ts'), 'utf8');
const twoslasher = createTwoslasher({
  vfsRoot: docsRoot,
  compilerOptions: {
    target: ts.ScriptTarget.ESNext,
    module: ts.ModuleKind.ESNext,
    moduleResolution: ts.ModuleResolutionKind.Bundler,
    lib: ['lib.esnext.d.ts', 'lib.dom.d.ts', 'lib.dom.iterable.d.ts'],
    allowJs: true,
    checkJs: true,
    // Examples are written as plain JS for readers; strictness would flag style, not bugs.
    strict: false,
    noImplicitAny: false,
    skipLibCheck: true,
    types: [],
  },
  handbookOptions: { noErrorValidation: true },
});
const md = new MarkdownIt();

// TypeScript prints an object type in full, up to a "... N more ..." elision, so a
// message about the `fb2k` object changes whenever the SDK gains a namespace. The
// baseline keeps messages with every quoted object type collapsed to '{...}'.
function baselineMessage(message) {
  return message.replace(/'\{.*?\}'(?=[.,;: ]|$)/g, "'{...}'");
}

// Returns [{ line, message }] for one page; line is 1-based in the .md file.
function checkPage(abs) {
  const src = fs.readFileSync(abs, 'utf8');
  const found = [];
  for (const tok of md.parse(src, {})) {
    if (tok.type !== 'fence' || !tok.map) continue;
    const [lang, ...meta] = tok.info.trim().split(/\s+/);
    const ext = LANGS.get((lang || '').toLowerCase().replace(/[{:].*$/, ''));
    if (!ext || meta.includes('@ts-nocheck') || !tok.content.trim()) continue;
    const firstCodeLine = tok.map[0] + 2;
    let errors;
    try {
      errors = twoslasher(tok.content, ext, { extraFiles: { 'docs-examples-env.d.ts': env } }).errors;
    } catch (e) {
      errors = [{ code: 'crash', text: String(e.message), line: 0 }];
    }
    for (const e of errors) {
      found.push({ line: firstCodeLine + (e.line ?? 0), message: baselineMessage(`TS${e.code}: ${String(e.text).split('\n')[0]}`) });
    }
  }
  return found;
}

const baseline = fs.existsSync(baselinePath) ? JSON.parse(fs.readFileSync(baselinePath, 'utf8')) : {};
const newOnes = [];
const fixed = [];
let total = 0;
for (const abs of pages) {
  const key = pageKey(abs);
  const current = checkPage(abs);
  total += current.length;
  if (update) {
    if (current.length) baseline[key] = current.map((d) => d.message).sort();
    else delete baseline[key];
    continue;
  }
  // Multiset difference in both directions, ignoring line numbers.
  const known = new Map();
  for (const m of (baseline[key] ?? []).map(baselineMessage)) known.set(m, (known.get(m) ?? 0) + 1);
  for (const d of current) {
    const n = known.get(d.message) ?? 0;
    if (n > 0) known.set(d.message, n - 1);
    else newOnes.push(`${key}:${d.line}  ${d.message}`);
  }
  for (const [m, n] of known) for (let i = 0; i < n; i++) fixed.push(`${key}  ${m}`);
}

if (update) {
  const sorted = Object.fromEntries(Object.entries(baseline).sort(([a], [b]) => a.localeCompare(b)));
  fs.writeFileSync(baselinePath, JSON.stringify(sorted, null, 2) + '\n');
  console.log(`check-examples: baseline updated for ${pages.length} page(s); ${total} known diagnostic(s) in them.`);
  process.exit(0);
}

console.log(`check-examples: ${pages.length} page(s), ${total} diagnostic(s), ${newOnes.length} new, ${fixed.length} no longer occurring.`);
if (newOnes.length) {
  console.log('\nNew diagnostics (fix the example, or add @ts-nocheck to its fence if it is not meant to run):');
  for (const l of newOnes) console.log(`  ${l}`);
}
if (fixed.length) {
  console.log('\nThese baseline entries no longer occur; refresh with `node docs/vitepress/.examples/check.mjs --update-baseline <page>`:');
  for (const l of fixed) console.log(`  ${l}`);
}
process.exit(newOnes.length || fixed.length ? 1 : 0);

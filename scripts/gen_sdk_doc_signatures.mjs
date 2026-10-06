// Keeps the `Signature:` / `签名：` lines of the docs site in step with the SDK's own types.
//
// Each such line names one facade method in a code span, `fb.<namespace>.<method>(...)`. The
// script reads that method's call signature from sdk/src through the TypeScript compiler and
// renders it in the same shape, so the line shows what the SDK accepts and returns rather than
// what someone once typed. Parameter types and explicit return types keep their source
// spelling; an inferred return type is printed by the compiler.
//
//   node scripts/gen_sdk_doc_signatures.mjs            report lines that differ, exit 1 if any
//   node scripts/gen_sdk_doc_signatures.mjs --write    rewrite them in place
//
// A line whose method the SDK does not have, or whose method has overloads a single line
// cannot show, is reported and left as it is; both make the check fail.
import fs from 'node:fs';
import path from 'node:path';
import process from 'node:process';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
export const repoRoot = path.resolve(here, '..');
const DOCS_ROOT = path.join(repoRoot, 'docs', 'vitepress');
const SDK_ROOT = path.join(repoRoot, 'sdk');

// `Signature: \`fb.ns.method(...)...\`` with whatever follows the code span kept as it is.
const LINE_RE = /^(\s*(?:Signature|签名)\s*[:：]\s*)`(fb\.([A-Za-z_$][\w$]*)\.([A-Za-z_$][\w$]*)[^`]*)`(.*)$/;

export function parseSignatureLine(line) {
  const m = LINE_RE.exec(line);
  if (!m) return null;
  return { prefix: m[1], code: m[2], namespace: m[3], method: m[4], suffix: m[5], names: parameterNames(m[2]) };
}

// The parameter names a documented signature uses, or null when its parameter list cannot be
// read. Brackets, generics and quoted strings nest; `=>` is not a closing angle bracket.
export function parameterNames(code) {
  const open = code.indexOf('(');
  if (open < 0) return null;
  const params = [];
  let depth = 0;
  let quote = null;
  let start = open + 1;
  for (let i = open + 1; i < code.length; i++) {
    const c = code[i];
    if (quote) {
      if (c === quote) quote = null;
      continue;
    }
    if (c === "'" || c === '"' || c === '`') quote = c;
    else if (c === '=' && code[i + 1] === '>') i++;
    else if ('([{<'.includes(c)) depth++;
    else if (')]}>'.includes(c)) {
      if (depth === 0 && c === ')') {
        params.push(code.slice(start, i));
        break;
      }
      depth--;
    } else if (c === ',' && depth === 0) {
      params.push(code.slice(start, i));
      start = i + 1;
    }
  }
  const chunks = params.map((p) => p.trim()).filter((p) => p !== '');
  const names = [];
  for (const chunk of chunks) {
    const m = /^(?:\.\.\.)?\s*([A-Za-z_$][\w$]*)\s*\??\s*:/.exec(chunk);
    if (!m) return null;
    names.push(m[1]);
  }
  return names;
}

function loadTypeScript() {
  const require = createRequire(path.join(SDK_ROOT, 'package.json'));
  return require('typescript');
}

const collapse = (text) => text.replace(/\s+/g, ' ').trim();

// Returns a resolver from (namespace, method) to the rendered code span, or to { problem }.
export function createSignatureRenderer(ts = loadTypeScript()) {
  const configPath = path.join(SDK_ROOT, 'tsconfig.json');
  const parsed = ts.parseJsonConfigFileContent(
    ts.readConfigFile(configPath, ts.sys.readFile).config,
    ts.sys,
    SDK_ROOT,
  );
  const entry = path.join(SDK_ROOT, 'src', 'bridge', 'index.ts');
  const program = ts.createProgram([entry], parsed.options);
  const checker = program.getTypeChecker();
  const moduleSymbol = checker.getSymbolAtLocation(program.getSourceFile(entry));
  const fbSymbol = checker.getExportsOfModule(moduleSymbol).find((s) => s.name === 'fb');
  if (!fbSymbol) throw new Error('sdk/src/bridge/index.ts does not export fb');
  const fbType = checker.getTypeOfSymbol(fbSymbol);
  const flags = ts.TypeFormatFlags.NoTruncation | ts.TypeFormatFlags.UseAliasDefinedOutsideCurrentScope;

  const renderParam = (param, docName) => {
    const name = docName ?? (ts.isIdentifier(param.name) ? param.name.text : 'options');
    const rest = param.dotDotDotToken ? '...' : '';
    const optional = param.questionToken || param.initializer ? '?' : '';
    const type = param.type
      ? collapse(param.type.getText())
      : checker.typeToString(checker.getTypeAtLocation(param), param, flags);
    return `${rest}${name}${optional}: ${type}`;
  };

  // An overloaded function is documented by its implementation signature, the one general
  // shape the overloads narrow.
  const implementationOf = (type) =>
    type.getSymbol()?.declarations?.find((d) => ts.isFunctionDeclaration(d) && d.body);

  // docNames are the parameter names the page already uses. They are kept when the page lists
  // as many parameters as the SDK, so tables and prose that refer to them stay consistent;
  // otherwise the SDK's own names are used.
  return (namespace, method, docNames = null) => {
    const nsSymbol = checker.getPropertyOfType(fbType, namespace);
    if (!nsSymbol) return { problem: `fb.${namespace} does not exist` };
    const nsType = checker.getTypeOfSymbol(nsSymbol);
    const methodSymbol = checker.getPropertyOfType(nsType, method);
    if (!methodSymbol) return { problem: `fb.${namespace}.${method} does not exist` };
    const methodType = checker.getTypeOfSymbol(methodSymbol);
    const signatures = checker.getSignaturesOfType(methodType, ts.SignatureKind.Call);
    let signature = signatures[0];
    if (signatures.length !== 1) {
      const impl = implementationOf(methodType);
      if (!impl) {
        return { problem: `fb.${namespace}.${method} has ${signatures.length} call signatures and no implementation to show` };
      }
      signature = checker.getSignatureFromDeclaration(impl);
    }
    const decl = signature.getDeclaration();
    const typeParams = decl?.typeParameters?.length
      ? `<${decl.typeParameters.map((tp) => collapse(tp.getText())).join(', ')}>`
      : '';
    const keepNames = docNames && decl && docNames.length === decl.parameters.length;
    const params = decl
      ? decl.parameters.map((p, i) => renderParam(p, keepNames ? docNames[i] : undefined)).join(', ')
      : '';
    const ret = decl?.type
      ? collapse(decl.type.getText())
      : checker.typeToString(checker.getReturnTypeOfSignature(signature), decl, flags);
    return { code: `fb.${namespace}.${method}${typeParams}(${params}): ${ret}` };
  };
}

function listPages(dir) {
  const out = [];
  for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
    if (e.name.startsWith('.') || e.name === 'node_modules' || e.name === 'dist' || e.name === 'public') continue;
    const p = path.join(dir, e.name);
    if (e.isDirectory()) out.push(...listPages(p));
    else if (e.name.endsWith('.md')) out.push(p);
  }
  return out;
}

// Returns { changes: [{ file, line, from, to }], problems: [string], pages: Map<file, text> }.
export function planSignatures(render, pages = listPages(DOCS_ROOT)) {
  const changes = [];
  const problems = [];
  const next = new Map();
  for (const abs of pages) {
    const rel = path.relative(repoRoot, abs).split(path.sep).join('/');
    const lines = fs.readFileSync(abs, 'utf8').split('\n');
    let touched = false;
    lines.forEach((line, i) => {
      const eol = line.endsWith('\r') ? '\r' : '';
      const hit = parseSignatureLine(eol ? line.slice(0, -1) : line);
      if (!hit) return;
      const rendered = render(hit.namespace, hit.method, hit.names);
      if (rendered.problem) {
        problems.push(`${rel}:${i + 1}: ${rendered.problem}`);
        return;
      }
      if (rendered.code === hit.code) return;
      changes.push({ file: rel, line: i + 1, from: hit.code, to: rendered.code });
      lines[i] = `${hit.prefix}\`${rendered.code}\`${hit.suffix}${eol}`;
      touched = true;
    });
    if (touched) next.set(abs, lines.join('\n'));
  }
  return { changes, problems, next };
}

function main() {
  const write = process.argv.includes('--write');
  const { changes, problems, next } = planSignatures(createSignatureRenderer());
  if (write) {
    for (const [abs, text] of next) fs.writeFileSync(abs, text);
    console.log(`sdk-doc-signatures: rewrote ${changes.length} line(s) in ${next.size} page(s).`);
  } else {
    for (const c of changes) console.log(`${c.file}:${c.line}\n  doc: ${c.from}\n  sdk: ${c.to}`);
    if (changes.length) {
      console.log(`sdk-doc-signatures: ${changes.length} line(s) differ from the SDK; run \`node scripts/gen_sdk_doc_signatures.mjs --write\`.`);
    } else if (!problems.length) {
      console.log('sdk-doc-signatures: every signature line matches the SDK.');
    }
  }
  for (const p of problems) console.error(`sdk-doc-signatures: ${p}`);
  process.exit((!write && changes.length) || problems.length ? 1 : 0);
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) main();

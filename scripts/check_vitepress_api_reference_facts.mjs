#!/usr/bin/env node
/**
 * Phase 3 API/Reference facts gate (rules-driven).
 *
 * Validates confirmed-stale closures, ErrorEnvelope codes, permissions counts
 * and ghost APIs. The API names and path-security levels come from the
 * declarations under src/api/schema plus the undeclared registrations listed in
 * scripts/api-schema/registrations.mjs; the parameter and return tables of a
 * declared method are generated from its declaration and checked by
 * `generate.mjs --check`, not here. Does not treat prose docs as authority.
 *
 * Usage:
 *   node scripts/check_vitepress_api_reference_facts.mjs \
 *     --repo-root . --docs-root docs/vitepress --locale-root docs/vitepress/zh \
 *     --authority-root . --rules scripts/vitepress-api-reference-facts.json \
 *     [--strict] [--json]
 */
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { loadNamespaces } from './api-schema/schema.mjs';
import { UNDECLARED_RAW } from './api-schema/registrations.mjs';

const SCRIPT_DIR = path.dirname(fileURLToPath(import.meta.url));
const DEFAULT_REPO_ROOT = path.resolve(SCRIPT_DIR, '..');
const ALLOWED_OPS = new Set([
  'literal-forbids',
  'literal-requires',
  'regex-requires',
  'regex-forbids',
  'payload-keys-equal',
  'error-codes-match-authority',
  'registerapi-token-exists',
  'security-level-counts-match-authority',
  'permissions-documentation-match-authority',
  'smp-compat-documentation-contract',
  'source-layer-requires',
  'file-sha256',
]);

function parseArgs(argv) {
  const out = {
    repoRoot: DEFAULT_REPO_ROOT,
    docsRoot: null,
    localeRoot: null,
    authorityRoot: null,
    rules: null,
    strict: false,
    json: false,
    help: false,
  };
  for (let i = 0; i < argv.length; i += 1) {
    const a = argv[i];
    const next = () => argv[++i];
    if (a === '--help' || a === '-h') out.help = true;
    else if (a === '--strict') out.strict = true;
    else if (a === '--json') out.json = true;
    else if (a === '--repo-root') out.repoRoot = path.resolve(next());
    else if (a.startsWith('--repo-root=')) out.repoRoot = path.resolve(a.slice('--repo-root='.length));
    else if (a === '--docs-root') out.docsRoot = next();
    else if (a.startsWith('--docs-root=')) out.docsRoot = a.slice('--docs-root='.length);
    else if (a === '--locale-root') out.localeRoot = next();
    else if (a.startsWith('--locale-root=')) out.localeRoot = a.slice('--locale-root='.length);
    else if (a === '--authority-root') out.authorityRoot = next();
    else if (a.startsWith('--authority-root=')) out.authorityRoot = a.slice('--authority-root='.length);
    else if (a === '--rules') out.rules = next();
    else if (a.startsWith('--rules=')) out.rules = a.slice('--rules='.length);
  }
  return out;
}

function printHelp() {
  console.log([
    'Usage: node scripts/check_vitepress_api_reference_facts.mjs --docs-root <path> --locale-root <path> --authority-root <path> --rules <path> [--repo-root <path>] [--strict] [--json]',
    '',
    'Validate API/Reference documentation facts against runtime authority.',
    'Exit codes: 0 ok/help, 1 strict findings, 2 config error',
  ].join('\n'));
}

function resolveMaybe(root, p) {
  if (!p) return null;
  return path.isAbsolute(p) ? path.normalize(p) : path.resolve(root, p);
}

function toPosix(p) {
  return String(p).split(path.sep).join('/');
}

function finding(kind, message, extra = {}) {
  return { findingKind: kind, message, ...extra };
}

function readText(abs) {
  return fs.readFileSync(abs, 'utf8');
}

function extractErrorCodes(authorityRoot) {
  const header = path.join(authorityRoot, 'src/api/ErrorEnvelope.h');
  if (!fs.existsSync(header)) {
    // fixture-friendly: search any ErrorEnvelope.h under authority root
    const candidates = [];
    const walk = (d) => {
      if (!fs.existsSync(d)) return;
      for (const e of fs.readdirSync(d, { withFileTypes: true })) {
        const p = path.join(d, e.name);
        if (e.isDirectory()) walk(p);
        else if (e.name === 'ErrorEnvelope.h') candidates.push(p);
      }
    };
    walk(authorityRoot);
    if (!candidates.length) return [];
    return extractCodesFromText(readText(candidates[0]));
  }
  return extractCodesFromText(readText(header));
}

function extractCodesFromText(text) {
  const codes = new Set();
  for (const m of text.matchAll(/constexpr\s+const\s+char\*\s+\w+\s*=\s*"([A-Z0-9_]+)"/g)) {
    codes.add(m[1]);
  }
  return [...codes].sort();
}

// Every method name that exists: the declared ones and the undeclared registrations.
// generate.mjs --check ties both lists to the registrations in the C++ sources.
function extractRegisterApis(authorityRoot) {
  const set = new Set(UNDECLARED_RAW.keys());
  for (const ns of loadNamespaces(authorityRoot)) for (const m of ns.methods) set.add(m.api);
  return set;
}

// Path-security specs are the `x-security` / `x-path-key` declarations in src/api/schema:
// one spec per path parameter, or per member of an array of objects whose members take
// different levels.
function extractSecurityLevelCounts(authorityRoot) {
  const counts = {};
  const specs = [];
  for (const ns of loadNamespaces(authorityRoot)) {
    for (const m of ns.methods) {
      for (const p of m.params.properties) {
        const levels = p.pathKeys ? p.pathKeys.map(({ level }) => level) : p.security ? [p.security] : [];
        for (const level of levels) {
          counts[level] = (counts[level] || 0) + 1;
          specs.push({ api: m.api, level });
        }
      }
    }
  }
  return {
    counts,
    specs,
    totalSpecs: specs.length,
    uniqueApis: new Set(specs.map((spec) => spec.api)).size,
  };
}

function parsePositiveInt(value) {
  const match = String(value).replace(/`|\*|,/g, '').match(/\b(\d+)\b/);
  return match ? Number(match[1]) : null;
}

function normalizedLevel(value) {
  const match = String(value).replace(/`|\*/g, '').match(/\b(None|Read|Write|MediaRead|MediaWrite|FileWrite)\b/);
  return match?.[1] || null;
}

function documentedLevelCounts(table) {
  const counts = {};
  for (const row of table.rows) {
    const level = normalizedLevel(row[0]);
    const count = parsePositiveInt(row[1]);
    if (level && count != null) counts[level] = count;
  }
  return counts;
}

function findHeadingSection(text, level) {
  const lines = text.split(/\r?\n/);
  const heading = new RegExp(`^###\\s+${level}\\b`, 'i');
  const start = lines.findIndex((line) => heading.test(line));
  if (start < 0) return null;
  let end = lines.length;
  for (let i = start + 1; i < lines.length; i += 1) {
    if (/^#{1,3}\s+/.test(lines[i])) {
      end = i;
      break;
    }
  }
  const body = lines.slice(start, end).join('\n');
  const headingCount = parsePositiveInt(lines[start]);
  const table = parseMarkdownTables(body)[0] || null;
  return { headingCount, table };
}

function comparePermissionCounts(findings, fact, locale, rel, actual, expected, source) {
  for (const [level, authorityCount] of Object.entries(expected)) {
    if (actual[level] !== authorityCount) {
      findings.push(finding('permissions-document-count-mismatch', `${fact.id}: ${source} ${level} on ${locale}/${rel}: doc=${actual[level] ?? 'missing'} authority=${authorityCount}`, {
        factId: fact.id, locale, file: rel, level, source, expected: authorityCount, actual: actual[level] ?? null,
      }));
    }
  }
}

function checkPermissionsDocumentation(fact, docsRoot, localeRoot, authority, findings) {
  const expected = authority.counts;
  const levels = Object.keys(expected).filter((level) => level !== 'None');
  const files = Array.isArray(fact.files) ? fact.files : ['reference/permissions.md'];
  const locales = Array.isArray(fact.locales) ? fact.locales : ['root', 'zh'];
  for (const locale of locales) {
    for (const rel of files) {
      const abs = resolveDocFile(docsRoot, localeRoot, rel, locale);
      if (!fs.existsSync(abs)) {
        findings.push(finding('missing-page', `missing ${locale} ${rel}`, { factId: fact.id, locale, file: rel }));
        continue;
      }
      const text = readText(abs);
      const tables = parseMarkdownTables(text);
      const countTables = tables.filter((table) => {
        const actual = documentedLevelCounts(table);
        return levels.some((level) => Object.hasOwn(actual, level));
      });
      // One count table: the page keeps a single counts layer at the top.
      if (countTables.length < 1) {
        findings.push(finding('permissions-count-table-missing', `${fact.id}: ${locale}/${rel} must contain the per-level count table`, {
          factId: fact.id, locale, file: rel, actualTableCount: countTables.length,
        }));
      }
      for (const [index, table] of countTables.entries()) {
        comparePermissionCounts(findings, fact, locale, rel, documentedLevelCounts(table), expected, `count table at line ${table.startLine}`);
        const totalRow = table.rows.find((row) => /total|合计/i.test(row[0] || ''));
        if (totalRow) {
          const total = parsePositiveInt(totalRow[1]);
          if (total !== authority.totalSpecs) {
            findings.push(finding('permissions-total-count-mismatch', `${fact.id}: count table at line ${table.startLine} total on ${locale}/${rel}: doc=${total ?? 'missing'} authority=${authority.totalSpecs}`, {
              factId: fact.id, locale, file: rel, source: `count table at line ${table.startLine}`, expected: authority.totalSpecs, actual: total,
            }));
          }
          const uniqueApis = parsePositiveInt(totalRow[2]);
          if (uniqueApis != null && uniqueApis !== authority.uniqueApis) {
            findings.push(finding('permissions-unique-api-count-mismatch', `${fact.id}: count table at line ${table.startLine} unique APIs on ${locale}/${rel}: doc=${uniqueApis} authority=${authority.uniqueApis}`, {
              factId: fact.id, locale, file: rel, source: `count table at line ${table.startLine}`, expected: authority.uniqueApis, actual: uniqueApis,
            }));
          }
        }
      }
      for (const level of levels) {
        const section = findHeadingSection(text, level);
        if (!section?.table) {
          findings.push(finding('permissions-level-table-missing', `${fact.id}: ${level} table missing on ${locale}/${rel}`, { factId: fact.id, locale, file: rel, level }));
          continue;
        }
        const entries = section.table.rows.filter((row) => /^`?[a-z][\w]*\.[\w.]+`?$/.test(String(row[0]).trim()));
        if (section.headingCount !== expected[level]) {
          findings.push(finding('permissions-level-heading-count-mismatch', `${fact.id}: ${level} heading on ${locale}/${rel}: doc=${section.headingCount ?? 'missing'} authority=${expected[level]}`, {
            factId: fact.id, locale, file: rel, level, expected: expected[level], actual: section.headingCount,
          }));
        }
        if (entries.length !== expected[level]) {
          findings.push(finding('permissions-level-entry-count-mismatch', `${fact.id}: ${level} table entries on ${locale}/${rel}: doc=${entries.length} authority=${expected[level]}`, {
            factId: fact.id, locale, file: rel, level, expected: expected[level], actual: entries.length,
          }));
        }
      }
    }
  }
}

/**
 * Parse the markdown tables of a page: header, rows and a kind taken from the first header
 * cell. Cells may contain `|` inside backticks or escaped as `\|`.
 */
function parseMarkdownTables(text) {
  const lines = text.split(/\r?\n/);
  const tables = [];
  let i = 0;
  while (i < lines.length) {
    if (!/^\|/.test(lines[i])) {
      i += 1;
      continue;
    }
    const start = i;
    const block = [];
    while (i < lines.length && /^\|/.test(lines[i])) {
      block.push(lines[i]);
      i += 1;
    }
    if (block.length < 2) continue;
    if (!/^\|\s*:?-{3,}/.test(block[1].replace(/\s+/g, ''))) {
      // allow standard --- separators with spaces
      if (!/^\|[\s|:-]+$/.test(block[1])) continue;
    }
    // Split on '|' outside backticks / after escapes so cells may contain
    // `path\|subsong`, `integer|string`, or `array<object|string>`.
    const splitRow = (row) => {
      const raw = row.replace(/^\|/, '').replace(/\|$/, '');
      const cells = [];
      let current = '';
      let inCode = false;
      for (let idx = 0; idx < raw.length; idx += 1) {
        const ch = raw[idx];
        if (ch === '`') {
          inCode = !inCode;
          current += ch;
          continue;
        }
        if (!inCode && ch === '\\') {
          // Support both `\|` and `\ |` legacy escapes inside table cells.
          if (raw[idx + 1] === '|') {
            current += '|';
            idx += 1;
            continue;
          }
          if (raw[idx + 1] === ' ' && raw[idx + 2] === '|') {
            current += '|';
            idx += 2;
            continue;
          }
        }
        if (!inCode && ch === '|') {
          cells.push(current.trim());
          current = '';
          continue;
        }
        current += ch;
      }
      cells.push(current.trim());
      return cells;
    };
    const header = splitRow(block[0]);
    const rows = block.slice(2).map(splitRow);
    const colCount = header.length;
    const malformed = rows.some((row) => row.length !== colCount) || colCount < 2;
    const header0 = (header[0] || '').replace(/`/g, '').toLowerCase();
    let kind = 'other';
    if (header0 === 'parameter' || header0 === '参数') kind = 'parameter';
    else if (header0 === 'field' || header0 === '字段') kind = 'return-field';
    else if (header0 === 'event' || header0 === '事件') kind = 'event';
    else if (header0 === 'property' || header0 === '属性') kind = 'property';
    tables.push({
      startLine: start + 1,
      header,
      rows,
      colCount,
      malformed,
      kind,
      raw: block.join('\n'),
    });
  }
  return tables;
}

function extractDocErrorCodes(md) {
  const codes = new Set();
  for (const m of md.matchAll(/\b([A-Z][A-Z0-9_]{2,})\b/g)) {
    // keep likely error codes only when near code/error context later; here collect all UPPER tokens from tables/code
    if (m[1].includes('_') || ['INVALID', 'ERROR', 'FAILED', 'DENIED', 'FOUND'].some((x) => m[1].includes(x))) {
      codes.add(m[1]);
    }
  }
  // Prefer fenced/code and table cells with pure codes
  const refined = new Set();
  for (const m of md.matchAll(/`([A-Z][A-Z0-9_]+)`/g)) refined.add(m[1]);
  for (const m of md.matchAll(/^\|\s*`?([A-Z][A-Z0-9_]+)`?\s*\|/gm)) refined.add(m[1]);
  return refined.size ? [...refined].sort() : [...codes].sort();
}

function loadRules(abs) {
  const data = JSON.parse(readText(abs).replace(/^\uFEFF/, ''));
  if (!data || !Array.isArray(data.facts)) throw new Error('rules must contain facts[]');
  return data;
}

function resolveDocFile(docsRoot, localeRoot, rel, locale) {
  if (locale === 'zh') return path.join(localeRoot, rel);
  return path.join(docsRoot, rel);
}

export function checkApiReferenceFacts(opts = {}) {
  const repoRoot = path.resolve(opts.repoRoot || DEFAULT_REPO_ROOT);
  const docsRoot = resolveMaybe(repoRoot, opts.docsRoot);
  const localeRoot = resolveMaybe(repoRoot, opts.localeRoot);
  const authorityRoot = resolveMaybe(repoRoot, opts.authorityRoot);
  const rulesPath = resolveMaybe(repoRoot, opts.rules);

  if (!docsRoot || !localeRoot || !authorityRoot || !rulesPath) {
    return { ok: false, configError: 'docs-root, locale-root, authority-root and rules are required' };
  }
  if (!fs.existsSync(rulesPath)) return { ok: false, configError: `rules missing: ${rulesPath}` };

  let rules;
  try {
    rules = loadRules(rulesPath);
  } catch (e) {
    return { ok: false, configError: `invalid rules: ${e.message}` };
  }

  const findings = [];
  const registerApis = extractRegisterApis(authorityRoot);
  const authorityErrorCodes = extractErrorCodes(authorityRoot);
  const authoritySecurityLevelData = extractSecurityLevelCounts(authorityRoot);
  const authoritySecurityLevels = authoritySecurityLevelData.counts;

  for (const fact of rules.facts) {
    if (!fact || typeof fact !== 'object' || typeof fact.id !== 'string') {
      findings.push(finding('invalid-rule-schema', 'fact missing id'));
      continue;
    }
    if (!ALLOWED_OPS.has(fact.op)) {
      findings.push(finding('invalid-rule-schema', `unsupported op ${fact.op}`, { factId: fact.id }));
      continue;
    }
    const locales = Array.isArray(fact.locales) ? fact.locales : ['root', 'zh'];
    const files = Array.isArray(fact.files) ? fact.files : fact.file ? [fact.file] : [];

    if (fact.op === 'permissions-documentation-match-authority') {
      checkPermissionsDocumentation(fact, docsRoot, localeRoot, authoritySecurityLevelData, findings);
      continue;
    }

    if (fact.op === 'smp-compat-documentation-contract') {
      for (const requirement of fact.requirements || []) {
        const source = path.resolve(authorityRoot, requirement.source);
        if (!fs.existsSync(source) || !readText(source).includes(requirement.contains)) {
          findings.push(finding('smp-source-entry-mismatch', `${fact.id}: ${requirement.source} lacks ${requirement.contains}`, { factId: fact.id, source: requirement.source }));
        }
      }
      for (const locale of locales) {
        for (const rel of files) {
          const abs = resolveDocFile(docsRoot, localeRoot, rel, locale);
          const literals = fact.localeLiterals?.[locale] || [];
          if (!fs.existsSync(abs)) {
            findings.push(finding('missing-page', `missing ${locale} ${rel}`, { factId: fact.id, locale, file: rel }));
            continue;
          }
          const text = readText(abs);
          for (const literal of literals) {
            if (!text.includes(literal)) findings.push(finding('smp-documentation-contract-missing', `${fact.id}: required literal missing in ${locale}/${rel}: ${literal}`, { factId: fact.id, locale, file: rel, literal }));
          }
        }
      }
      continue;
    }

    if (fact.op === 'security-level-counts-match-authority') {
      const expected = fact.expectedCounts || {};
      for (const [level, count] of Object.entries(expected)) {
        if (!Number.isInteger(count)) {
          findings.push(finding('invalid-rule-schema', `${fact.id}: expected count for ${level} must be integer`, { factId: fact.id }));
          continue;
        }
        if ((authoritySecurityLevels[level] || 0) !== count) {
          findings.push(finding('security-level-count-mismatch', `${fact.id}: ${level} doc=${count} authority=${authoritySecurityLevels[level] || 0}`, {
            factId: fact.id,
            level,
            expected: count,
            actual: authoritySecurityLevels[level] || 0,
          }));
        }
      }
      continue;
    }

    if (fact.op === 'source-layer-requires') {
      for (const requirement of fact.requirements || []) {
        const source = path.resolve(authorityRoot, requirement.source);
        if (!fs.existsSync(source)) {
          findings.push(finding('authority-missing', `${fact.id}: source missing ${requirement.source}`, { factId: fact.id }));
          continue;
        }
        if (requirement.contains && !readText(source).includes(requirement.contains)) {
          findings.push(finding('source-layer-authority-mismatch', `${fact.id}: ${requirement.source} lacks ${requirement.contains}`, {
            factId: fact.id,
            source: requirement.source,
          }));
        }
      }
      continue;
    }

    if (fact.op === 'error-codes-match-authority') {
      for (const locale of locales) {
        for (const rel of files) {
          const abs = resolveDocFile(docsRoot, localeRoot, rel, locale);
          if (!fs.existsSync(abs)) {
            findings.push(finding('missing-page', `missing ${locale} ${rel}`, { factId: fact.id, locale, file: rel }));
            continue;
          }
          const docCodes = extractDocErrorCodes(readText(abs));
          const missing = authorityErrorCodes.filter((c) => !docCodes.includes(c));
          const extra = docCodes.filter((c) => !authorityErrorCodes.includes(c) && c.length > 3);
          if (missing.length) {
            findings.push(finding('error-code-missing', `${fact.id}: missing codes on ${locale}/${rel}: ${missing.join(',')}`, {
              factId: fact.id,
              locale,
              file: rel,
              missing,
            }));
          }
          if (fact.forbidExtra && extra.length) {
            findings.push(finding('error-code-extra', `${fact.id}: extra codes on ${locale}/${rel}: ${extra.join(',')}`, {
              factId: fact.id,
              locale,
              file: rel,
              extra,
            }));
          }
        }
      }
      continue;
    }

    if (fact.op === 'registerapi-token-exists') {
      const tokens = Array.isArray(fact.tokens) ? fact.tokens : [];
      for (const token of tokens) {
        if (!registerApis.has(token)) {
          findings.push(finding('ghost-api', `${fact.id}: token not registered: ${token}`, { factId: fact.id, token }));
        }
      }
      // also scan docs for ghost invoke tokens if requested
      if (fact.scanDocs) {
        for (const locale of locales) {
          for (const rel of files) {
            const abs = resolveDocFile(docsRoot, localeRoot, rel, locale);
            if (!fs.existsSync(abs)) continue;
            const text = readText(abs);
            for (const m of text.matchAll(/\b([a-z]\w*\.[a-z]\w*)\b/g)) {
              const token = m[1];
              if (token.includes('.__')) continue;
              if (!registerApis.has(token) && !token.startsWith('console.') && !token.startsWith('Math.')) {
                // only flag if appears as invoke-like
                if (new RegExp(`invoke\\(\\s*['"]${token.replace('.', '\\.')}['"]`).test(text) || new RegExp(`^#{2,4}\\s+${token.replace('.', '\\.')}\\s*$`, 'm').test(text)) {
                  findings.push(finding('ghost-api', `${fact.id}: unregistered API token ${token} in ${locale}/${rel}`, {
                    factId: fact.id,
                    token,
                    locale,
                    file: rel,
                  }));
                }
              }
            }
          }
        }
      }
      continue;
    }

    if (fact.op === 'file-sha256') {
      const abs = resolveMaybe(authorityRoot, fact.authorityFile || fact.path);
      if (!abs || !fs.existsSync(abs)) {
        findings.push(finding('authority-missing', `${fact.id}: authority file missing`, { factId: fact.id }));
        continue;
      }
      const sha = crypto.createHash('sha256').update(fs.readFileSync(abs)).digest('hex');
      if (fact.expectedSha256 && fact.expectedSha256.toLowerCase() !== sha.toLowerCase()) {
        findings.push(finding('authority-sha-mismatch', `${fact.id}: authority sha mismatch`, { factId: fact.id }));
      }
      continue;
    }

    // text operators on locale files
    for (const locale of locales) {
      for (const rel of files) {
        const abs = resolveDocFile(docsRoot, localeRoot, rel, locale);
        if (!fs.existsSync(abs)) {
          findings.push(finding('missing-page', `missing ${locale} ${rel}`, { factId: fact.id, locale, file: rel }));
          continue;
        }
        const text = readText(abs);
        if (fact.op === 'literal-forbids') {
          for (const lit of fact.literals || []) {
            if (text.includes(lit)) {
              findings.push(finding(fact.findingKind || 'forbidden-literal', `${fact.id}: forbidden literal present in ${locale}/${rel}: ${lit}`, {
                factId: fact.id,
                locale,
                file: rel,
                literal: lit,
              }));
            }
          }
        } else if (fact.op === 'literal-requires') {
          for (const lit of fact.literals || []) {
            if (!text.includes(lit)) {
              findings.push(finding(fact.findingKind || 'required-literal-missing', `${fact.id}: required literal missing in ${locale}/${rel}: ${lit}`, {
                factId: fact.id,
                locale,
                file: rel,
                literal: lit,
              }));
            }
          }
        } else if (fact.op === 'regex-requires' || fact.op === 'regex-forbids') {
          let re;
          try {
            re = new RegExp(fact.pattern, fact.flags || 'm');
          } catch (e) {
            findings.push(finding('invalid-rule-schema', `${fact.id}: bad regex ${e.message}`, { factId: fact.id }));
            continue;
          }
          const hit = re.test(text);
          if (fact.op === 'regex-requires' && !hit) {
            findings.push(finding(fact.findingKind || 'required-pattern-missing', `${fact.id}: required pattern missing in ${locale}/${rel}`, {
              factId: fact.id,
              locale,
              file: rel,
            }));
          }
          if (fact.op === 'regex-forbids' && hit) {
            findings.push(finding(fact.findingKind || 'forbidden-pattern', `${fact.id}: forbidden pattern present in ${locale}/${rel}`, {
              factId: fact.id,
              locale,
              file: rel,
            }));
          }
        } else if (fact.op === 'payload-keys-equal') {
          // ensure all listed events appear with expected key sets in file
          for (const item of fact.events || []) {
            const event = item.event;
            const expected = [...(item.keys || [])].sort();
            // find table row payload cell
            const rowRe = new RegExp(`\\|\\s*\`?${event.replace(/[.*+?^${}()|[\\]\\\\]/g, '\\$&')}\`?\\s*\\|([^|]*)\\|([^|]*)\\|`);
            const m = text.match(rowRe);
            if (!m) {
              findings.push(finding('event-row-missing', `${fact.id}: event row missing ${event} in ${locale}/${rel}`, {
                factId: fact.id,
                event,
                locale,
                file: rel,
              }));
              continue;
            }
            const cell = `${m[1]} ${m[2]}`;
            const keys = new Set();
            for (const km of cell.matchAll(/\{([^}]*)\}/g)) {
              const inner = km[1].trim();
              if (!inner || inner === '-' || inner === '—') continue;
              for (const part of inner.split(',')) {
                const p = part.match(/`?([A-Za-z_][\w]*)`?/);
                if (p) keys.add(p[1]);
              }
            }
            // also typed form { a: type, b: type }
            for (const km of cell.matchAll(/([A-Za-z_][\w]*)\s*:/g)) keys.add(km[1]);
            const got = [...keys].sort();
            const eq = got.length === expected.length && got.every((k, i) => k === expected[i]);
            if (!eq) {
              findings.push(finding(fact.findingKind || 'payload-key-mismatch', `${fact.id}: ${event} keys doc=[${got.join(',')}] expected=[${expected.join(',')}] in ${locale}/${rel}`, {
                factId: fact.id,
                event,
                locale,
                file: rel,
                got,
                expected,
              }));
            }
          }
        }
      }
    }
  }

  // dedupe
  const seen = new Set();
  const deduped = [];
  for (const f of findings) {
    const key = `${f.findingKind}|${f.factId || ''}|${f.file || ''}|${f.locale || ''}|${f.message}`;
    if (seen.has(key)) continue;
    seen.add(key);
    deduped.push(f);
  }

  return {
    ok: deduped.length === 0,
    authorityErrorCodes,
    authoritySecurityLevels,
    authoritySecuritySpecs: {
      totalSpecs: authoritySecurityLevelData.totalSpecs,
      uniqueApis: authoritySecurityLevelData.uniqueApis,
    },
    publicRegisterCount: [...registerApis].filter((t) => !t.includes('.__')).length,
    findings: deduped,
  };
}

function main(argv = process.argv.slice(2)) {
  const flags = parseArgs(argv);
  if (flags.help) {
    printHelp();
    process.exit(0);
  }
  const report = checkApiReferenceFacts(flags);
  if (report.configError) {
    if (flags.json) console.log(JSON.stringify(report, null, 2));
    else console.error(`ERROR: ${report.configError}`);
    process.exit(2);
  }
  if (flags.json) console.log(JSON.stringify(report, null, 2));
  else {
    console.log(`findings=${report.findings.length}`);
    for (const f of report.findings.slice(0, 40)) console.log(`- [${f.findingKind}] ${f.message}`);
  }
  if (flags.strict && !report.ok) process.exit(1);
  process.exit(0);
}

if (process.argv[1] && pathToFileURL(path.resolve(process.argv[1])).href === import.meta.url) {
  main();
}

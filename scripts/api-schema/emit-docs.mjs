// Rewrites the generated regions of the docs site from the declared schema. A region
// looks like this, and everything between the two comments is replaced:
//
//   <!-- api-schema:begin titleformat.eval -->
//   ...
//   <!-- api-schema:end -->
//
// The region holds the method description, the parameter table and the returns table.
// Pages under docs/vitepress/zh/ get the Chinese text, falling back to English where a
// description has no x-description-zh. Prose outside regions stays hand-written.
//
// A declared event has an `event:` region, named by the event, with its description, who
// receives it and the payload table:
//
//   <!-- api-schema:begin event:playback:trackChanged -->
//   ...
//   <!-- api-schema:end -->
//
// A type from common.ts is documented once, in a `type:` region on reference/types.md of
// each locale, and every table that uses it links there instead of repeating its fields:
//
//   <!-- api-schema:begin type:Track -->
//   ...
//   <!-- api-schema:end -->
//
// reference/permissions.md of each locale carries the path-security specs the declarations
// give (x-security and x-path-key): `permissions:counts` holds the count per level, and one
// `permissions:<Level>` region per level holds its heading and the table of its specs.
//
// An `index:namespaces` region lists every namespace with its method count and links to the
// pages of the same locale whose regions document its methods, wherever they are.
import fs from 'node:fs';
import path from 'node:path';

import { allProperties, COMMON_NAMESPACE, findType } from './schema.mjs';

export const DOCS_ROOT = 'docs/vitepress';
export const TYPES_PAGE = 'reference/types.md';
export const ERRORS_PAGE = 'reference/errors.md';
export const NAMESPACE_INDEX = 'index:namespaces';
const REGION_RE = /<!-- api-schema:begin ([\w.:]+) -->\n[\s\S]*?<!-- api-schema:end -->/g;
export const PERMISSIONS_PAGE = 'reference/permissions.md';
export const SECURITY_LEVELS = ['Read', 'Write', 'MediaRead', 'MediaWrite', 'FileWrite'];

const L = {
  en: {
    param: 'Parameter', field: 'Field', type: 'Type', required: 'Required', desc: 'Description', yes: 'Yes', no: 'No', returns: '**Returns**', none: 'This method takes no parameters.', def: (v) => `Default: \`${v}\`.`, each: 'each key',
    notEmpty: 'Must not be empty.', minLength: (n) => `At least ${n} characters.`, minItems: (n) => `At least ${n} items.`,
    between: (a, b) => `Between \`${a}\` and \`${b}\` inclusive.`, atLeast: (a) => `At least \`${a}\`.`, atMost: (b) => `At most \`${b}\`.`,
    eachItem: (s) => `Each item: ${s[0].toLowerCase()}${s.slice(1)}`,
    envelope: (link) => `\`success\` is \`true\` on success. On failure the response is \`{ success: false, error, code }\`; see [Error codes](${link}) for \`code\`.`,
    inherits: (link) => `Every field of ${link}.`, partial: (link) => `Every field of ${link}, each optional.`,
    skipped: 'Number of paths the security check dropped before the call ran; present only when at least one was dropped.',
    payload: '**Payload**', noPayload: 'The event carries no fields.', sharedPayload: (link) => `The payload is a ${link}.`,
    delivery: { broadcast: 'Sent to every window.', caller: 'Sent to the page that made the call.', owner: 'Sent to the page that owns the subscription or task.', target: 'Sent to the window the caller named.', window: 'Sent to the page of the window or panel it concerns.', main: "Sent to the main window's page, whichever page caused it." },
    customName: 'A subscriber can have it delivered under a name of its own; this is the name otherwise.',
    perm: {
      countHeader: '| Level | Spec count | Meaning |',
      total: (specs, apis) => `| **Total** | **${specs}** | **${apis} unique APIs** |`,
      meaning: { Read: 'Ordinary filesystem read checks', Write: 'Strict write destinations (config/temp style policy)', MediaRead: 'Media-context read checks', MediaWrite: 'Media-context write checks', FileWrite: 'General file writes (`file.*`)' },
      label: { Read: 'filesystem read', Write: 'strict write destinations', MediaRead: 'media reads', MediaWrite: 'media mutation', FileWrite: 'general file writes' },
      heading: (level, label, n) => `### ${level} — ${label} (${n} ${n === 1 ? 'spec' : 'specs'})`,
      tableHeader: '| API | Parameter | Array | Nested key |',
      yes: 'yes',
    },
    index: {
      total: (methods, namespaces) => `${methods} ${methods === 1 ? 'method' : 'methods'} in ${namespaces} ${namespaces === 1 ? 'namespace' : 'namespaces'}.`,
      header: '| Namespace | Methods | Documented on |',
    },
  },
  zh: {
    param: '参数', field: '字段', type: '类型', required: '必填', desc: '说明', yes: '是', no: '否', returns: '**返回值**', none: '无参数。', def: (v) => `默认 \`${v}\`。`, each: '每个键',
    notEmpty: '不能为空。', minLength: (n) => `至少 ${n} 个字符。`, minItems: (n) => `至少 ${n} 项。`,
    between: (a, b) => `取值 \`${a}\` 到 \`${b}\`（含端点）。`, atLeast: (a) => `不小于 \`${a}\`。`, atMost: (b) => `不大于 \`${b}\`。`,
    eachItem: (s) => `每一项${s}`,
    envelope: (link) => `成功时 \`success\` 为 \`true\`；失败时返回 \`{ success: false, error, code }\`，\`code\` 见[错误码](${link})。`,
    inherits: (link) => `包含 ${link} 的全部字段。`, partial: (link) => `包含 ${link} 的全部字段，每个都可选。`,
    skipped: '调用前被路径安全检查丢弃的路径数；只在至少丢弃一条时出现。',
    payload: '**载荷**', noPayload: '事件不带字段。', sharedPayload: (link) => `载荷是一个 ${link}。`,
    delivery: { broadcast: '发给所有窗口。', caller: '发给发起调用的页面。', owner: '发给订阅或任务所属的页面。', target: '发给调用方指定的窗口。', window: '发给它所涉及的窗口或面板里的页面。', main: '发给主窗口的页面，不论是哪个页面引起的。' },
    customName: '订阅方可以让它以自己起的名字送达；没起名时用这个名字。',
    perm: {
      countHeader: '| 级别 | spec 条数 | 含义 |',
      total: (specs, apis) => `| **合计** | **${specs}** | **${apis} 个唯一 API** |`,
      meaning: { Read: '普通文件系统只读校验', Write: '严格写入目标（配置/临时目录策略）', MediaRead: '媒体上下文只读校验', MediaWrite: '媒体上下文写校验', FileWrite: '通用文件写入（`file.*`）' },
      label: { Read: '只读文件系统', Write: '严格写入目标', MediaRead: '读取媒体文件', MediaWrite: '修改媒体文件', FileWrite: '通用文件写入' },
      heading: (level, label, n) => `### ${level} — ${label}（${n} 条）`,
      tableHeader: '| API | 参数 | 数组 | 嵌套键 |',
      yes: '是',
    },
    index: {
      total: (methods, namespaces) => `共 ${methods} 个方法，分属 ${namespaces} 个命名空间。`,
      header: '| 命名空间 | 方法数 | 所在页面 |',
    },
  },
};

const cell = (s) => String(s ?? '').replace(/\|/g, '\\|').replace(/\n/g, ' ');
const describe = (node, lang) => (lang === 'zh' && node.descriptionZh) || node.description || '';
const describeType = (node, lang) => (lang === 'zh' && node.typeDescriptionZh) || node.typeDescription || '';

// The link from a page (repo-relative, under DOCS_ROOT) to a page of its own locale, given
// relative to the locale root; a page under docs/vitepress/zh/ links into the Chinese tree.
export function pageLinkFor(pageRel, target) {
  const zh = pageRel.startsWith(`${DOCS_ROOT}/zh/`);
  const localeRoot = zh ? `${DOCS_ROOT}/zh` : DOCS_ROOT;
  const pageDir = path.posix.dirname(path.posix.relative(localeRoot, pageRel));
  let rel = path.posix.relative(pageDir === '.' ? '' : pageDir, target);
  if (!rel.startsWith('.')) rel = `./${rel}`;
  return rel;
}

// How a page links a common type: relative to the page, into the types page of its locale.
function typeLinkFor(pageRel) {
  const rel = pageLinkFor(pageRel, TYPES_PAGE);
  return (name) => `[${name}](${rel}#${name.toLowerCase()})`;
}

// Everything a region needs beyond the method: type lookup, the link to a common type and the
// link to the error codes.
function contextFor(ns, namespaces, pageRel) {
  return {
    resolve: (name, common) => findType(name, ns, namespaces, common),
    link: typeLinkFor(pageRel),
    errors: pageLinkFor(pageRel, ERRORS_PAGE),
  };
}

// Without a page, links are written as from a page one level below the locale root.
const NO_CTX = { resolve: () => null, link: (name) => name, errors: `../${ERRORS_PAGE}` };

function typeLabel(p, ctx) {
  const t = baseLabel(p, ctx);
  return p.nullable ? `${t} | null` : t;
}

// A common type is a link; a named local type shows its name and its fields follow in the
// table; a recursive use shows the name only.
function baseLabel(p, ctx) {
  switch (p.kind) {
    case 'string': return p.enum ? p.enum.map((e) => JSON.stringify(e)).join(' | ') : 'string';
    case 'array': {
      const item = typeLabel(p.items, ctx);
      // An array of a linked type keeps the whole label inside the link.
      const link = item.match(/^\[([^\]]+)\]\((.+)\)$/);
      if (link) return `[${link[1]}[]](${link[2]})`;
      return item.includes(' ') ? `(${item})[]` : `${item}[]`;
    }
    case 'ref': return p.common ? ctx.link(p.name) : p.name;
    case 'object':
      if (p.common) return ctx.link(p.name);
      if (p.name && p.additional) return `${p.name} & Record<string, ${typeLabel(p.additional, ctx)}>`;
      if (p.properties.length || p.extends) return p.name ?? 'object';
      return `Record<string, ${typeLabel(p.additional, ctx)}>`;
    case 'json': return 'any';
    default: return p.kind;
  }
}

// Whether the rows of a nested object belong in this table: not for a common type (linked)
// or a recursive use.
const expands = (p) => p.kind === 'object' && !p.common && (p.properties.length || p.extends);

// The constraints the generated parser enforces, as one sentence each, so the table says what a
// call is rejected for without the description having to repeat it.
function constraintText(p, lang) {
  const l = L[lang];
  const out = [];
  if (p.minLength === 1) out.push(l.notEmpty);
  else if (p.minLength !== undefined) out.push(l.minLength(p.minLength));
  const range = rangeText(p, l);
  if (range) out.push(range);
  if (p.minItems === 1) out.push(l.notEmpty);
  else if (p.minItems !== undefined) out.push(l.minItems(p.minItems));
  // Number bounds on an array's items apply to every element.
  const items = p.kind === 'array' ? rangeText(p.items, l) : undefined;
  if (items) out.push(l.eachItem(items));
  return out;
}

function rangeText(p, l) {
  if (p.minimum !== undefined && p.maximum !== undefined) return l.between(p.minimum, p.maximum);
  if (p.minimum !== undefined) return l.atLeast(p.minimum);
  if (p.maximum !== undefined) return l.atMost(p.maximum);
  return undefined;
}

// The rows an object contributes: local bases are listed in full ahead of the object's own
// members; where the chain reaches a base from common.ts, that base is one row pointing at
// its page.
function baseRows(obj, lang, ctx, prefix, row) {
  const rows = [];
  const bases = [];
  for (let cur = obj; cur.extends; ) {
    if (cur.extendsCommon) {
      rows.push(row(`${prefix}…`, ctx.link(cur.extends), L[lang].inherits(ctx.link(cur.extends))));
      break;
    }
    const base = ctx.resolve(cur.extends, false);
    if (!base) break;
    bases.unshift(base);
    cur = base;
  }
  return { rows, properties: [...bases.flatMap((b) => b.properties), ...obj.properties] };
}

function paramRows(obj, lang, ctx, prefix = '') {
  const row = (name, type, text) => `| \`${name}\` | ${type.startsWith('[') ? type : `\`${cell(type)}\``} | | ${cell(text)} |`;
  const { rows, properties } = baseRows(obj, lang, ctx, prefix, row);
  for (const p of properties) {
    const name = `${prefix}${p.key}`;
    // Chinese sentences end in full-width punctuation and take no space before the next one.
    const text = [describe(p, lang), ...constraintText(p, lang), p.default !== undefined ? L[lang].def(JSON.stringify(p.default)) : ''].filter(Boolean).join(lang === 'zh' ? '' : ' ');
    const type = typeLabel(p, ctx);
    rows.push(`| \`${name}\` | ${type.startsWith('[') ? type : `\`${cell(type)}\``} | ${p.required ? L[lang].yes : L[lang].no} | ${cell(text)} |`);
    if (expands(p)) rows.push(...paramRows(p, lang, ctx, `${name}.`));
    if (p.kind === 'array' && expands(p.items)) rows.push(...paramRows(p.items, lang, ctx, `${name}[].`));
  }
  return rows;
}

function resultRows(obj, lang, ctx, prefix = '') {
  const row = (name, type, text) => `| \`${name}\` | ${type.startsWith('[') ? type : `\`${cell(type)}\``} | ${cell(text)} |`;
  const { rows, properties } = baseRows(obj, lang, ctx, prefix, row);
  for (const p of properties) {
    const name = `${prefix}${p.key}`;
    rows.push(row(name, typeLabel(p, ctx), describe(p, lang)));
    if (expands(p)) rows.push(...resultRows(p, lang, ctx, `${name}.`));
    if (p.kind === 'array' && expands(p.items)) rows.push(...resultRows(p.items, lang, ctx, `${name}[].`));
  }
  if (obj.additional) {
    rows.push(row(`${prefix}[${L[lang].each}]`, typeLabel(obj.additional, ctx), describe(obj.additional, lang)));
  }
  return rows;
}

export function renderRegion(method, lang, ctx = NO_CTX) {
  const t = L[lang];
  const out = [];
  if (method.experimental) out.push(lang === 'zh' ? '实验性 API，后续版本可能发生变化。' : 'Experimental API; it may change in future releases.', '');
  if (method.deprecated) out.push(lang === 'zh' ? `已弃用：${method.deprecatedZh}` : `Deprecated: ${method.deprecated}`, '');
  out.push(describe(method, lang), '');
  if (method.params.properties.length) {
    out.push(`| ${t.param} | ${t.type} | ${t.required} | ${t.desc} |`, '| --- | --- | --- | --- |', ...paramRows(method.params, lang, ctx));
  } else {
    out.push(t.none);
  }
  out.push('', t.returns, '');
  // skippedPaths is the bridge's own field on the success response of a method that drops
  // the paths failing the security check; it belongs in the table like a declared field.
  const skipped = method.params.properties.some((p) => p.skipInvalid) ? [`| \`skippedPaths\` | \`integer\` | ${t.skipped} |`] : [];
  if (method.result || skipped.length) {
    out.push(`| ${t.field} | ${t.type} | ${t.desc} |`, '| --- | --- | --- |', ...(method.result ? resultRows(method.result, lang, ctx) : []), ...skipped, '');
  }
  out.push(t.envelope(ctx.errors));
  return out.join('\n');
}

// A declared event: its description, who receives it, and the payload table.
export function renderEventRegion(event, lang, ctx = NO_CTX) {
  const t = L[lang];
  const receives = [t.delivery[event.delivery], event.customName ? t.customName : ''].filter(Boolean).join(lang === 'zh' ? '' : ' ');
  const out = [describe(event, lang), '', receives, '', t.payload, ''];
  // A shared type is documented once, on the types page; the region links to it.
  if (event.sharedPayload) {
    out.push(t.sharedPayload(ctx.link(event.sharedPayload)));
  } else if (event.payload.properties.length || event.payload.additional) {
    out.push(`| ${t.field} | ${t.type} | ${t.desc} |`, '| --- | --- | --- |', ...resultRows(event.payload, lang, ctx));
  } else {
    out.push(t.noPayload);
  }
  return out.join('\n');
}

// A common type on its own: its description and a field table. The Partial of a type is one
// sentence pointing at that type.
export function renderTypeRegion(obj, lang, ctx = NO_CTX) {
  const t = L[lang];
  const out = [];
  const description = describeType(obj, lang);
  if (description) out.push(description, '');
  if (obj.partialOf) {
    out.push(t.partial(ctx.link(obj.partialOf)));
    return out.join('\n');
  }
  out.push(`| ${t.field} | ${t.type} | ${t.desc} |`, '| --- | --- | --- |', ...resultRows(obj, lang, ctx));
  return out.join('\n');
}

// Every path-security spec the declarations give, as { level, api, param, array, nestedKey }:
// x-security on a string or an array of strings, x-path-key on an array of objects or Json
// (one row per member when the members differ in level), at any depth of the params.
export function securitySpecs(namespaces) {
  const specs = [];
  const walk = (api, props, prefix) => {
    for (const p of props ?? []) {
      const param = `${prefix}${p.key}`;
      if (p.security) specs.push({ level: p.security, api, param, array: p.kind === 'array', nestedKey: p.pathKey ?? null });
      for (const k of p.pathKeys ?? []) specs.push({ level: k.level, api, param, array: true, nestedKey: k.key });
      if (p.kind === 'object') walk(api, p.properties, `${param}.`);
      if (p.kind === 'array' && p.items.kind === 'object' && !p.pathKey && !p.pathKeys) walk(api, p.items.properties, `${param}[].`);
    }
  };
  for (const ns of namespaces) for (const m of ns.methods) walk(m.api, m.params.properties, '');
  const key = (s) => [s.api, s.param, s.nestedKey ?? ''].join('\u0000');
  return specs.sort((a, b) => (key(a) < key(b) ? -1 : key(a) > key(b) ? 1 : 0));
}

export function renderPermissionsRegion(which, specs, lang) {
  const t = L[lang].perm;
  if (which === 'counts') {
    const rows = SECURITY_LEVELS.map((level) => `| \`${level}\` | ${specs.filter((s) => s.level === level).length} | ${t.meaning[level]} |`);
    return [t.countHeader, '| --- | ---: | --- |', ...rows, t.total(specs.length, new Set(specs.map((s) => s.api)).size)].join('\n');
  }
  const mine = specs.filter((s) => s.level === which);
  const rows = mine.map((s) => `| \`${s.api}\` | \`${s.param}\` | ${s.array ? t.yes : '—'} | ${s.nestedKey ? `\`${s.nestedKey}\`` : '—'} |`);
  return [t.heading(which, t.label[which], mine.length), '', t.tableHeader, '| --- | --- | --- | --- |', ...rows].join('\n');
}

// Per locale: namespace -> Map(page, title) of the pages holding its method regions, in path
// order. The title is the page's first `# ` heading, or its file name without one.
function methodHomes(pages, methods) {
  const homes = { en: new Map(), zh: new Map() };
  for (const { rel, lang, src } of pages) {
    const title = /^# +(.+?)\s*$/m.exec(src)?.[1] ?? path.posix.basename(rel, '.md');
    for (const m of src.matchAll(REGION_RE)) {
      const found = methods.get(m[1]);
      if (!found) continue;
      const byNs = homes[lang];
      if (!byNs.has(found.ns.namespace)) byNs.set(found.ns.namespace, new Map());
      byNs.get(found.ns.namespace).set(rel, title);
    }
  }
  for (const byNs of Object.values(homes)) {
    for (const [ns, byPage] of byNs) byNs.set(ns, new Map([...byPage].sort(([a], [b]) => (a < b ? -1 : a > b ? 1 : 0))));
  }
  return homes;
}

// The `index:namespaces` region: every namespace that declares methods, its method count and
// the pages of this locale that document them. Links are relative to the page holding the region.
export function renderNamespaceIndex(namespaces, homes, lang, pageRel) {
  const t = L[lang].index;
  const localeRoot = lang === 'zh' ? `${DOCS_ROOT}/zh/` : `${DOCS_ROOT}/`;
  const declared = namespaces.filter((ns) => ns.methods.length).sort((a, b) => (a.namespace < b.namespace ? -1 : 1));
  const total = declared.reduce((n, ns) => n + ns.methods.length, 0);
  const rows = declared.map((ns) => {
    const pagesOf = [...(homes.get(ns.namespace) ?? new Map())];
    const links = pagesOf.map(([rel, title]) => `[${cell(title)}](${pageLinkFor(pageRel, rel.slice(localeRoot.length))})`);
    return `| \`${ns.namespace}\` | ${ns.methods.length} | ${links.join(', ') || '—'} |`;
  });
  return [t.total(total, declared.length), '', t.header, '| --- | ---: | --- |', ...rows].join('\n');
}

// Absolute paths of the pages under DOCS_ROOT, skipping what VitePress does not build.
export function listPages(repoRoot) {
  const root = path.join(repoRoot, DOCS_ROOT);
  const out = [];
  (function walk(dir) {
    for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
      if (e.name.startsWith('.') || ['node_modules', 'dist', 'public'].includes(e.name)) continue;
      const p = path.join(dir, e.name);
      if (e.isDirectory()) walk(p);
      else if (e.name.endsWith('.md')) out.push(p);
    }
  })(root);
  return out;
}

// Returns { changes: [{ file, next }], problems: [string] } without writing anything.
export function planDocs(repoRoot, namespaces) {
  const methods = new Map();
  for (const ns of namespaces) for (const m of ns.methods) methods.set(m.api, { method: m, ns });
  const events = new Map();
  for (const ns of namespaces) for (const e of ns.events ?? []) events.set(`event:${e.name}`, { event: e, ns });
  const common = namespaces.find((ns) => ns.namespace === COMMON_NAMESPACE);
  const types = common ? common.types : new Map();
  const specs = securitySpecs(namespaces);
  const permissionIds = ['permissions:counts', ...SECURITY_LEVELS.map((level) => `permissions:${level}`)];
  const seen = { en: new Set(), zh: new Set() };
  const changes = [];
  const problems = [];
  const pages = listPages(repoRoot).map((abs) => {
    const rel = path.relative(repoRoot, abs).split(path.sep).join('/');
    return { rel, lang: rel.startsWith(`${DOCS_ROOT}/zh/`) ? 'zh' : 'en', src: fs.readFileSync(abs, 'utf8') };
  });
  const homes = methodHomes(pages, methods);
  for (const { rel, lang, src } of pages) {
    if (!src.includes('<!-- api-schema:begin ')) continue;
    const next = src.replace(REGION_RE, (whole, id) => {
      let body;
      if (id.startsWith('index:')) {
        if (id !== NAMESPACE_INDEX) {
          problems.push(`${rel}: region ${id}; the only index region is ${NAMESPACE_INDEX}`);
          return whole;
        }
        body = renderNamespaceIndex(namespaces, homes[lang], lang, rel);
      } else if (id.startsWith('permissions:')) {
        if (!permissionIds.includes(id)) {
          problems.push(`${rel}: region ${id}; permissions regions are ${permissionIds.join(', ')}`);
          return whole;
        }
        body = renderPermissionsRegion(id.slice('permissions:'.length), specs, lang);
      } else if (id.startsWith('type:')) {
        const name = id.slice('type:'.length);
        const obj = types.get(name);
        if (!obj) {
          problems.push(`${rel}: region for type ${name}, which common.ts does not declare`);
          return whole;
        }
        body = renderTypeRegion(obj, lang, contextFor(common, namespaces, rel));
      } else if (id.startsWith('event:')) {
        const found = events.get(id);
        if (!found) {
          problems.push(`${rel}: region for ${id.slice('event:'.length)}, which no schema declares`);
          return whole;
        }
        body = renderEventRegion(found.event, lang, contextFor(found.ns, namespaces, rel));
      } else {
        const found = methods.get(id);
        if (!found) {
          problems.push(`${rel}: region for ${id}, which no schema declares`);
          return whole;
        }
        body = renderRegion(found.method, lang, contextFor(found.ns, namespaces, rel));
      }
      if (seen[lang].has(id)) problems.push(`${rel}: a second ${lang} region for ${id}`);
      seen[lang].add(id);
      return `<!-- api-schema:begin ${id} -->\n${body}\n<!-- api-schema:end -->`;
    });
    if (next !== src) changes.push({ file: rel, next });
  }
  for (const api of methods.keys()) {
    for (const lang of ['en', 'zh']) if (!seen[lang].has(api)) problems.push(`no ${lang} docs region for ${api}`);
  }
  for (const id of events.keys()) {
    for (const lang of ['en', 'zh']) if (!seen[lang].has(id)) problems.push(`no ${lang} docs region for ${id}`);
  }
  for (const name of types.keys()) {
    for (const lang of ['en', 'zh']) if (!seen[lang].has(`type:${name}`)) problems.push(`no ${lang} docs region for type ${name} (${lang === 'zh' ? `${DOCS_ROOT}/zh/` : `${DOCS_ROOT}/`}${TYPES_PAGE})`);
  }
  // The permissions page is required once any declaration carries a path-security spec.
  for (const id of specs.length ? permissionIds : []) {
    for (const lang of ['en', 'zh']) if (!seen[lang].has(id)) problems.push(`no ${lang} docs region for ${id} (${lang === 'zh' ? `${DOCS_ROOT}/zh/` : `${DOCS_ROOT}/`}${PERMISSIONS_PAGE})`);
  }
  return { changes, problems };
}

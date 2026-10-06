// Generates the MCP server's bridge tools from the declarations under src/api/schema and the
// tool table mcp/tool-table.json:
//
//   mcp/src/generated/bridge-tools.ts                   the tool definitions
//   mcp/README.md, mcp/README.zh-CN.md                  the tool tables between the mcp-tools markers
//   docs/vitepress/mcp/tools.md, zh/mcp/tools.md        the same tables, linking each action to its API page
//
// A tool bundles several host methods. The client picks one by passing its name as `action`,
// with that method's parameters beside it: `{ "action": "playlist.getTracks", "count": 50 }`.
// Tools are grouped by namespace and by what their methods do, so each tool's annotations
// (read-only, destructive, idempotent) hold for every action in it; they come from the
// methods' @effect, @idempotent and @openWorld tags.
//
// The table is keyed by tool name. A tool entry holds:
//
//   description  what the tool is for; it goes into the AI client's context on every session
//   zh           the same in Chinese, for the docs only
//   params       { param: description } for the tool's merged schema, where a parameter that
//                several actions share needs one description, or a shorter one than the
//                declaration's
//   actions      { "ns.method": action entry }, in the order the tool lists them
//   why          the reason for anything unusual, for the next reader; not emitted
//
// An action entry holds its one-line description and, where the action is narrower than its
// method, how:
//
//   params    the parameters the action exposes, in this order; omitted, every declared one
//   required  parameters the action requires although the method does not
//   bounds    { param: { minimum, maximum } }, within the declared range; on an array of
//             numbers they apply to every element
//   why       the reason for the difference; not emitted
//
// and, where the result carries a picture as a `data:<mime>;base64,` URL:
//
//   image     the top-level string field of the result that holds it. The server sends the
//             picture as an MCP image rather than as text, and the other fields as JSON; the
//             tool description says so for the action
//
// Everything else comes from the declaration: parameter types, constraints, defaults and
// descriptions. Each action keeps its exact schema, which the server checks before calling
// the host, so an action cannot send a key its method does not accept, leave out a required
// one, or allow a value its method rejects. The merged schema the client sees accepts the
// union of the actions' parameters: a parameter two actions declare differently gets the
// wider of the two declarations, or both as alternatives, and no defaults.
import fs from 'node:fs';
import path from 'node:path';

import { allProperties, findType, SchemaError } from './schema.mjs';
import { DOCS_ROOT, listPages, pageLinkFor } from './emit-docs.mjs';

export const MCP_TABLE = 'mcp/tool-table.json';
export const MCP_TOOLS_TS = 'mcp/src/generated/bridge-tools.ts';
export const MCP_READMES = { en: 'mcp/README.md', zh: 'mcp/README.zh-CN.md' };
export const MCP_DOCS = { en: 'docs/vitepress/mcp/tools.md', zh: 'docs/vitepress/zh/mcp/tools.md' };
const REGION_RE = /<!-- mcp-tools:begin -->\n[\s\S]*?<!-- mcp-tools:end -->/;
const TOOL_KEYS = ['description', 'zh', 'params', 'actions', 'why'];
const ACTION_KEYS = ['description', 'params', 'required', 'bounds', 'image', 'why'];
// The MCP spec's tool-name characters, with this server's `fb2k_` prefix and snake case.
const TOOL_NAME_RE = /^fb2k_[a-z]+(?:_[a-z]+)*$/;
const ACTION_DESCRIPTION = 'The host method to call. The tool description lists each one with the parameters it takes.';
const IMAGE_NOTE = 'the picture comes back as an image, the other fields as JSON';

function fail(msg) {
  throw new SchemaError(`${MCP_TABLE}: ${msg}`);
}

export function readToolTable(repoRoot) {
  const abs = path.join(repoRoot, MCP_TABLE);
  if (!fs.existsSync(abs)) return null;
  let doc;
  try {
    doc = JSON.parse(fs.readFileSync(abs, 'utf8'));
  } catch (e) {
    fail(e.message);
  }
  if (!doc || typeof doc.tools !== 'object' || Array.isArray(doc.tools)) fail('"tools" must be an object keyed by tool name');
  return doc.tools;
}

const isInt = (v) => Number.isInteger(v);
const isObject = (v) => v !== null && typeof v === 'object' && !Array.isArray(v);
const nonEmpty = (v) => typeof v === 'string' && v.trim().length > 0;

// One declared parameter as an MCP property schema. Keys come in a fixed order so the
// generated file reads the same way for every tool.
function toMcp(p, ctx, where, { required = p.required } = {}) {
  const out = {};
  const constraints = {};
  switch (p.kind) {
    case 'string':
      out.type = 'string';
      if (p.enum) constraints.enum = p.enum;
      if (p.minLength !== undefined && !p.enum) constraints.minLength = p.minLength;
      break;
    case 'integer':
    case 'number':
      out.type = p.kind;
      if (p.minimum !== undefined) constraints.minimum = p.minimum;
      if (p.maximum !== undefined) constraints.maximum = p.maximum;
      break;
    case 'boolean':
      out.type = 'boolean';
      break;
    case 'json':
      out.type = 'json';
      break;
    case 'array':
      out.type = 'array';
      constraints.items = toMcp(p.items, ctx, `${where}[]`, { required: true });
      if (p.minItems !== undefined) constraints.minItems = p.minItems;
      break;
    case 'object': {
      out.type = 'object';
      const members = allProperties(p, ctx.resolve, where);
      const properties = {};
      const req = [];
      for (const m of members) {
        properties[m.key] = toMcp(m, ctx, `${where}.${m.key}`);
        if (m.required) req.push(m.key);
      }
      constraints.properties = properties;
      if (req.length) constraints.required = req;
      constraints.additionalProperties = p.additional ? toMcp(p.additional, ctx, `${where}.*`, { required: true }) : false;
      break;
    }
    default:
      throw new SchemaError(`${where}: a ${p.kind} parameter has no MCP schema; the generator supports string, number, integer, boolean, json, array and object`);
  }
  if (p.description) out.description = p.description;
  Object.assign(out, constraints);
  if (p.default !== undefined && !required) out.default = p.default;
  return out;
}

// A copy of a schema without the given keywords, at every depth. Property names inside
// `properties` are data, not keywords, so they are kept whatever they are called.
function without(schema, keywords) {
  if (Array.isArray(schema)) return schema.map((s) => without(s, keywords));
  if (!isObject(schema)) return schema;
  const out = {};
  for (const [k, v] of Object.entries(schema)) {
    if (keywords.includes(k)) continue;
    out[k] = k === 'properties' ? Object.fromEntries(Object.entries(v).map(([name, s]) => [name, without(s, keywords)])) : without(v, keywords);
  }
  return out;
}
const sameShape = (a, b) => JSON.stringify(without(a, ['description'])) === JSON.stringify(without(b, ['description']));
const isNumber = (t) => t === 'integer' || t === 'number';
const lower = (a, b, pick) => (a !== undefined && b !== undefined ? pick(a, b) : undefined);

// A schema that accepts everything `a` and `b` accept, or null when there is no single one:
// numbers take the wider range, strings the union of their enums, arrays the wider element.
function widen(a, b) {
  if (sameShape(a, b)) return a;
  let out = null;
  if (isNumber(a.type) && isNumber(b.type)) {
    out = { type: a.type === 'integer' && b.type === 'integer' ? 'integer' : 'number', minimum: lower(a.minimum, b.minimum, Math.min), maximum: lower(a.maximum, b.maximum, Math.max) };
  } else if (a.type === 'string' && b.type === 'string') {
    out = { type: 'string', enum: a.enum && b.enum ? [...new Set([...a.enum, ...b.enum])] : undefined, minLength: lower(a.minLength, b.minLength, Math.min) };
  } else if (a.type === 'array' && b.type === 'array') {
    const items = widen(a.items, b.items);
    if (items) out = { type: 'array', items, minItems: lower(a.minItems, b.minItems, Math.min) };
  }
  if (!out) return null;
  for (const k of Object.keys(out)) if (out[k] === undefined) delete out[k];
  return out;
}

// One property schema accepting what every variant accepts: widened where the variants allow
// it, alternatives otherwise. The variants' own top-level descriptions are dropped; the caller
// gives the merged property its description.
function mergeVariants(variants) {
  const alternatives = [];
  for (const v of variants.map((s) => without(s, ['default']))) {
    const { description: _, ...shape } = v;
    const at = alternatives.findIndex((alt) => widen(alt, shape));
    if (at < 0) alternatives.push(shape);
    else alternatives[at] = widen(alternatives[at], shape);
  }
  return alternatives.length === 1 ? alternatives[0] : { type: 'union', anyOf: alternatives };
}

function withDescription(schema, description) {
  const { type, ...rest } = schema;
  return description ? { type, description, ...rest } : schema;
}

function names(entry, key, where, declared, api) {
  const v = entry[key];
  if (v === undefined) return undefined;
  if (!Array.isArray(v) || !v.every((s) => typeof s === 'string')) fail(`${where}.${key} must be an array of parameter names`);
  if (new Set(v).size !== v.length) fail(`${where}.${key} names a parameter twice`);
  for (const n of v) if (!declared.has(n)) fail(`${where}.${key}: ${api} declares no parameter "${n}"`);
  return v;
}

function checkAction(api, entry, method, where) {
  if (!isObject(entry)) fail(`${where} must be an object`);
  for (const k of Object.keys(entry)) if (!ACTION_KEYS.includes(k)) fail(`${where}: unknown key "${k}"; allowed: ${ACTION_KEYS.join(', ')}`);
  if (!nonEmpty(entry.description)) fail(`${where}.description must be a non-empty string`);
  if (!method.effect) fail(`${where}: ${api} declares no @effect, so the tool's annotations cannot be worked out; tag the method in its declaration`);
  const declared = new Map(method.params.properties.map((p) => [p.key, p]));
  const exposed = names(entry, 'params', where, declared, api) ?? [...declared.keys()];
  for (const [key, p] of declared) {
    if (p.required && !exposed.includes(key)) fail(`${where}.params leaves out "${key}", which ${api} requires`);
  }
  const required = names(entry, 'required', where, declared, api) ?? [];
  for (const n of required) {
    if (!exposed.includes(n)) fail(`${where}.required names "${n}", which the action does not expose`);
    if (declared.get(n).required) fail(`${where}.required names "${n}", which ${api} already requires`);
  }
  const bounds = entry.bounds ?? {};
  if (!isObject(bounds)) fail(`${where}.bounds must be an object`);
  for (const [n, b] of Object.entries(bounds)) {
    const param = declared.get(n);
    if (!param || !exposed.includes(n)) fail(`${where}.bounds names "${n}", which the action does not expose`);
    // On an array of numbers the bounds apply to every element.
    const p = param.kind === 'array' ? param.items : param;
    if (p.kind !== 'integer' && p.kind !== 'number') fail(`${where}.bounds.${n}: "${n}" is not a number or an array of numbers`);
    if (!isObject(b) || !Object.keys(b).length || Object.keys(b).some((k) => k !== 'minimum' && k !== 'maximum')) {
      fail(`${where}.bounds.${n} must set minimum, maximum or both`);
    }
    for (const k of ['minimum', 'maximum']) {
      if (b[k] === undefined) continue;
      if (typeof b[k] !== 'number' || (p.kind === 'integer' && !isInt(b[k]))) fail(`${where}.bounds.${n}.${k} must be ${p.kind === 'integer' ? 'an integer' : 'a number'}`);
    }
    const min = b.minimum ?? p.minimum;
    const max = b.maximum ?? p.maximum;
    if (b.minimum !== undefined && p.minimum !== undefined && b.minimum < p.minimum) fail(`${where}.bounds.${n}.minimum ${b.minimum} is below the declared ${p.minimum}`);
    if (b.maximum !== undefined && p.maximum !== undefined && b.maximum > p.maximum) fail(`${where}.bounds.${n}.maximum ${b.maximum} is above the declared ${p.maximum}`);
    if (min !== undefined && max !== undefined && min > max) fail(`${where}.bounds.${n}: minimum ${min} exceeds maximum ${max}`);
    if (p.default !== undefined && !required.includes(n) && ((min !== undefined && p.default < min) || (max !== undefined && p.default > max))) {
      fail(`${where}.bounds.${n}: the declared default ${p.default} falls outside the action's range`);
    }
  }
  if (entry.why !== undefined && !nonEmpty(entry.why)) fail(`${where}.why must be a non-empty string`);
  return { declared, exposed, required, bounds };
}

// The result field an action's `image` names, checked against the declared result. Only a
// top-level string qualifies: the server lifts that one field out of the result.
function imageField(api, entry, method, ctx, where) {
  if (entry.image === undefined) return undefined;
  if (!nonEmpty(entry.image)) fail(`${where}.image must name a field of the result`);
  const members = method.result ? allProperties(method.result, ctx.resolve, `${api}.result`) : [];
  const field = members.find((m) => m.key === entry.image);
  if (!field) fail(`${where}.image: ${api} declares no top-level result field "${entry.image}"`);
  if (field.kind !== 'string') fail(`${where}.image: result field "${entry.image}" of ${api} is ${field.kind}, not a string holding a data URL`);
  return entry.image;
}

// The exact input schema of one action, with the declared defaults, and its parameter list.
function planAction(api, entry, found, namespaces, where) {
  const { method, ns } = found;
  const { declared, exposed, required, bounds } = checkAction(api, entry, method, where);
  const ctx = { resolve: (name, common) => findType(name, ns, namespaces, common) };
  const image = imageField(api, entry, method, ctx, where);
  const properties = {};
  const req = [];
  for (const key of exposed) {
    const param = declared.get(key);
    const p = !bounds[key] ? param : param.kind === 'array' ? { ...param, items: { ...param.items, ...bounds[key] } } : { ...param, ...bounds[key] };
    const isRequired = p.required || required.includes(key);
    properties[key] = toMcp(p, ctx, `${api}.${key}`, { required: isRequired });
    if (isRequired) req.push(key);
  }
  const inputSchema = { type: 'object', properties };
  if (req.length) inputSchema.required = req;
  return { api, description: withLifecycle(entry.description, method, 'en'), method, inputSchema, image, exposed: exposed.map((k) => ({ key: k, required: req.includes(k) })) };
}

function withLifecycle(description, method, lang) {
  const parts = [description];
  if (method.experimental) parts.push(lang === 'zh' ? '实验性 API。' : 'Experimental.');
  if (method.deprecated) parts.push(lang === 'zh' ? `已弃用：${method.deprecatedZh}` : `Deprecated: ${method.deprecated}`);
  return parts.join(' ');
}

// What the tool's actions do, as MCP tool annotations. A read-only tool needs no more; the
// others say whether any action destroys data and whether every action is idempotent.
function annotationsOf(actions) {
  const methods = actions.map((a) => a.method);
  const openWorldHint = methods.some((m) => m.openWorld);
  if (methods.every((m) => m.effect === 'read')) return { readOnlyHint: true, openWorldHint };
  const changing = methods.filter((m) => m.effect !== 'read');
  return {
    readOnlyHint: false,
    destructiveHint: changing.some((m) => m.effect === 'destructive'),
    idempotentHint: changing.every((m) => m.idempotent),
    openWorldHint,
  };
}

function signature(action) {
  const params = action.exposed.map((p) => `${p.key}${p.required ? '' : '?'}`).join(', ');
  return params ? `${action.api}(${params})` : action.api;
}

// The schema the client sees: `action` plus every parameter of every action.
function mergedSchema(name, entry, actions) {
  const where = `tools["${name}"]`;
  const overrides = entry.params ?? {};
  if (!isObject(overrides)) fail(`${where}.params must be an object of parameter descriptions`);
  const keys = [];
  for (const a of actions) for (const k of Object.keys(a.inputSchema.properties)) if (!keys.includes(k)) keys.push(k);
  if (keys.includes('action')) fail(`${where}: an action declares a parameter named "action", which the tool uses to pick the method`);
  for (const [k, d] of Object.entries(overrides)) {
    if (!keys.includes(k)) fail(`${where}.params names "${k}", which no action exposes`);
    if (!nonEmpty(d)) fail(`${where}.params.${k} must be a non-empty string`);
  }
  const properties = { action: { type: 'string', description: ACTION_DESCRIPTION, enum: actions.map((a) => a.api) } };
  for (const key of keys) {
    const users = actions.filter((a) => key in a.inputSchema.properties);
    const variants = users.map((a) => a.inputSchema.properties[key]);
    const texts = [...new Set(variants.map((v) => v.description ?? ''))];
    if (overrides[key] === undefined && texts.length > 1) {
      fail(`${where}: "${key}" is described differently by ${users.map((a) => a.api).join(', ')}; give it one description under "params":\n${texts.map((t) => `  - ${t}`).join('\n')}`);
    }
    properties[key] = withDescription(mergeVariants(variants), overrides[key] ?? texts[0]);
  }
  return { type: 'object', properties, required: ['action'] };
}

// The tools in table order: { name, description, zh, annotations, inputSchema, actions }.
export function planTools(table, namespaces) {
  const methods = new Map();
  for (const ns of namespaces) for (const m of ns.methods) methods.set(m.api, { method: m, ns });
  const owner = new Map();
  const tools = [];
  for (const [name, entry] of Object.entries(table)) {
    const where = `tools["${name}"]`;
    if (!TOOL_NAME_RE.test(name) || name.length > 128) fail(`${where}: a tool name is fb2k_ followed by lowercase words joined by underscores, at most 128 characters`);
    if (!isObject(entry)) fail(`${where} must be an object`);
    for (const k of Object.keys(entry)) if (!TOOL_KEYS.includes(k)) fail(`${where}: unknown key "${k}"; allowed: ${TOOL_KEYS.join(', ')}`);
    if (!nonEmpty(entry.description)) fail(`${where}.description must be a non-empty string`);
    if (!nonEmpty(entry.zh)) fail(`${where}.zh must be a non-empty string`);
    if (entry.why !== undefined && !nonEmpty(entry.why)) fail(`${where}.why must be a non-empty string`);
    if (!isObject(entry.actions) || !Object.keys(entry.actions).length) fail(`${where}.actions must name at least one host method`);
    const actions = [];
    for (const [api, actionEntry] of Object.entries(entry.actions)) {
      const aw = `${where}.actions["${api}"]`;
      const found = methods.get(api);
      if (!found) fail(`${aw}: no declaration under src/api/schema declares ${api}`);
      if (owner.has(api)) fail(`${aw}: ${api} is already an action of ${owner.get(api)}`);
      owner.set(api, name);
      actions.push(planAction(api, actionEntry, found, namespaces, aw));
    }
    const list = actions.map((a) => `- ${signature(a)}: ${a.description}${a.image ? `; ${IMAGE_NOTE}` : ''}`).join('\n');
    tools.push({
      name,
      description: `${entry.description}\n\nPass one of these as \`action\`, with the parameters it lists; \`?\` marks an optional one.\n${list}`,
      summary: entry.description,
      zh: entry.zh,
      annotations: annotationsOf(actions),
      inputSchema: mergedSchema(name, entry, actions),
      actions,
    });
  }
  return tools;
}

export function emitToolsTs(tools) {
  // Each action's schema is only checked, never shown, so it carries no descriptions.
  const defs = tools.map((t) => ({
    name: t.name,
    description: t.description,
    annotations: t.annotations,
    inputSchema: t.inputSchema,
    actions: Object.fromEntries(t.actions.map((a) => [a.api, {
      inputSchema: without(a.inputSchema, ['description']),
      ...(a.image ? { image: a.image } : {}),
    }])),
  }));
  return [
    '// Generated by scripts/api-schema/generate.mjs from the declarations under src/api/schema and',
    '// mcp/tool-table.json. Do not edit: change the declaration or the table and run',
    '// `node scripts/api-schema/generate.mjs --write`.',
    'import type { ToolDefinition } from "../types.js";',
    '',
    '/** The bridge tools in the order of mcp/tool-table.json, each with the host methods it calls. */',
    `export const bridgeTools: readonly ToolDefinition[] = ${JSON.stringify(defs, null, 4)};`,
    '',
  ].join('\n');
}

// The first sentence of a description: up to the first sentence end outside backticks. An
// English sentence ends at `.`, `!` or `?` followed by white space, except after an
// abbreviation such as "e.g.".
function firstSentence(text, lang) {
  let code = false;
  for (let i = 0; i < text.length; i++) {
    const c = text[i];
    if (c === '`') code = !code;
    if (code) continue;
    if (lang === 'zh' && c === '。') return text.slice(0, i + 1);
    if (lang === 'en' && (c === '.' || c === '!' || c === '?') && /\s/.test(text[i + 1] ?? ' ')) {
      if (c === '.' && /\b(e\.g|i\.e|etc|vs|cf)\.$/i.test(text.slice(0, i + 1))) continue;
      return text.slice(0, i + 1);
    }
  }
  return text;
}

const cell = (s) => String(s).replace(/\|/g, '\\|').replace(/\n/g, ' ');

const REGION_TEXT = {
  en: {
    intro: (tools, methods) => `**${tools} bridge tools** covering ${methods} host methods, grouped by namespace and by what they change; the page tools follow below. Each tool takes an \`action\`, the host method to call, and that method's parameters beside it. A parameter marked \`?\` is optional; types, ranges and defaults are in each tool's input schema.`,
    kind: (a) => (a.readOnlyHint ? 'read-only' : a.destructiveHint ? 'destructive' : a.idempotentHint ? 'changes state, idempotent' : 'changes state'),
    actions: (n) => `${n} action${n === 1 ? '' : 's'}`,
    header: '| Action | Params | Description |',
    summary: (t) => t.summary,
  },
  zh: {
    intro: (tools, methods) => `**${tools} 个 Bridge 工具**，覆盖 ${methods} 个宿主方法，按命名空间与是否改动东西分组；页面工具见下文。每个工具接收 \`action\`（要调用的宿主方法），该方法的参数与它并列传入。带 \`?\` 的参数可以省略；参数的类型、取值范围与默认值见各工具的输入 schema。`,
    kind: (a) => (a.readOnlyHint ? '只读' : a.destructiveHint ? '破坏性' : a.idempotentHint ? '改状态，可重复调用' : '改状态'),
    actions: (n) => `${n} 个 action`,
    header: '| Action | 参数 | 说明 |',
    summary: (t) => t.zh,
  },
};

// The tables of the README (plain action names) and of the docs page (`links` maps an action
// to its API reference page, relative to the docs page).
export function renderRegion(tools, lang, links = null) {
  const t = REGION_TEXT[lang];
  const methods = tools.reduce((n, tool) => n + tool.actions.length, 0);
  const out = [t.intro(tools.length, methods)];
  for (const tool of tools) {
    out.push('', `### \`${tool.name}\``, '', `${t.kind(tool.annotations)} · ${t.actions(tool.actions.length)}`, '', t.summary(tool), '');
    out.push(t.header, '|--------|--------|-------------|');
    for (const a of tool.actions) {
      const params = a.exposed.length ? a.exposed.map((p) => `\`${p.key}${p.required ? '' : '?'}\``).join(', ') : '—';
      const desc = (lang === 'zh' && a.method.descriptionZh) || a.method.description;
      const href = links?.get(a.api);
      const name = href ? `[\`${a.api}\`](${href})` : `\`${a.api}\``;
      out.push(`| ${name} | ${params} | ${cell(withLifecycle(firstSentence(desc, lang), a.method, lang))} |`);
    }
  }
  return out.join('\n');
}

// ns.method -> link from the MCP tools page of a locale to the method's section, on whichever
// page of that locale holds the method's docs region. The anchor is the slug VitePress gives
// the `### ns.method` heading above the region.
export function apiLinks(repoRoot, lang) {
  const from = MCP_DOCS[lang];
  const zhRoot = `${DOCS_ROOT}/zh/`;
  const links = new Map();
  const pages = listPages(repoRoot).map((abs) => path.relative(repoRoot, abs).split(path.sep).join('/')).sort();
  for (const rel of pages) {
    if (rel.startsWith(zhRoot) !== (lang === 'zh')) continue;
    const target = rel.slice((lang === 'zh' ? zhRoot : `${DOCS_ROOT}/`).length);
    const text = fs.readFileSync(path.join(repoRoot, rel), 'utf8');
    for (const m of text.matchAll(/<!-- api-schema:begin ([A-Za-z]+\.[A-Za-z]+) -->/g)) {
      if (!links.has(m[1])) links.set(m[1], `${pageLinkFor(from, target)}#${m[1].toLowerCase().replace('.', '-')}`);
    }
  }
  return links;
}

function upsertRegion(repoRoot, rel, body, files, problems) {
  const abs = path.join(repoRoot, rel);
  if (!fs.existsSync(abs)) {
    problems.push(`${rel} is missing`);
    return;
  }
  const src = fs.readFileSync(abs, 'utf8');
  if (!REGION_RE.test(src)) {
    problems.push(`${rel}: no <!-- mcp-tools:begin --> ... <!-- mcp-tools:end --> region`);
    return;
  }
  const next = src.replace(REGION_RE, () => `<!-- mcp-tools:begin -->\n${body}\n<!-- mcp-tools:end -->`);
  if (next !== src) files.set(rel, next);
}

// Returns { files: Map(rel -> content), problems: [string] }; nothing when the table is absent.
export function planMcp(repoRoot, namespaces) {
  const table = readToolTable(repoRoot);
  const files = new Map();
  const problems = [];
  if (!table) return { files, problems };
  const tools = planTools(table, namespaces);
  files.set(MCP_TOOLS_TS, emitToolsTs(tools));
  for (const [lang, rel] of Object.entries(MCP_READMES)) upsertRegion(repoRoot, rel, renderRegion(tools, lang), files, problems);
  for (const [lang, rel] of Object.entries(MCP_DOCS)) upsertRegion(repoRoot, rel, renderRegion(tools, lang, apiLinks(repoRoot, lang)), files, problems);
  return { files, problems };
}

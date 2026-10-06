// Emits src/api/generated/<Namespace>Schema.h: for every method a parameter struct with
// the method name, the path parameters that need a security check, and a FromJson that
// enforces required / optional / defaults / constraints and rejects unknown keys; for
// every method with a result a result struct with a ToJson that writes the declared
// fields. Nested objects declared through a named interface share one struct. A struct
// gets FromJson when any method's params reach it and ToJson when any result does; a role
// reaches every struct nested below the one it lands on, so a shape that params reach
// anywhere is either parsed everywhere or rejected.
//
// Types from common.ts live once in CommonSchema.h under api::common, with ToJson only:
// shared types are results, a params interface is declared in its namespace file. A
// namespace header includes it and names them as api::common::Name. An interface that
// extends another becomes a struct deriving from the base struct, with FromJson and ToJson
// covering the base members too. A recursive interface holds a vector of itself; every
// struct and function is declared before the definitions, so a struct may name one that
// is defined later. Every result and common struct lists its keys in kFields, so a
// hand-written writer can be checked against the declaration.
import { allProperties, COMMON_NAMESPACE, findType, pascal, SchemaError } from './schema.mjs';

// `json` is any JSON value, held and passed on unchanged.
const SCALAR = { string: 'std::string', integer: 'std::int64_t', number: 'double', boolean: 'bool', json: 'json' };

// Names C++ cannot give a member: the language's keywords, `json` (the alias every generated
// header declares), and the names a generated struct already uses. Such a key keeps its
// JSON name everywhere else and gets a trailing underscore as the member.
const CPP_RESERVED = new Set([
  'alignas', 'alignof', 'and', 'and_eq', 'asm', 'auto', 'bitand', 'bitor', 'bool', 'break', 'case', 'catch',
  'char', 'char8_t', 'char16_t', 'char32_t', 'class', 'compl', 'concept', 'const', 'consteval', 'constexpr',
  'constinit', 'const_cast', 'continue', 'co_await', 'co_return', 'co_yield', 'decltype', 'default', 'delete',
  'do', 'double', 'dynamic_cast', 'else', 'enum', 'explicit', 'export', 'extern', 'false', 'float', 'for',
  'friend', 'goto', 'if', 'inline', 'int', 'long', 'mutable', 'namespace', 'new', 'noexcept', 'not', 'not_eq',
  'nullptr', 'operator', 'or', 'or_eq', 'private', 'protected', 'public', 'register', 'reinterpret_cast',
  'requires', 'return', 'short', 'signed', 'sizeof', 'static', 'static_assert', 'static_cast', 'struct',
  'switch', 'template', 'this', 'thread_local', 'throw', 'true', 'try', 'typedef', 'typeid', 'typename',
  'union', 'unsigned', 'using', 'virtual', 'void', 'volatile', 'wchar_t', 'while', 'xor', 'xor_eq',
  'json', 'kMethod', 'kPathParams', 'kFields', 'Result', 'additional',
]);
const member = (key) => (CPP_RESERVED.has(key) ? `${key}_` : key);

function cppString(s) {
  return `"${s.replace(/\\/g, '\\\\').replace(/"/g, '\\"').replace(/\n/g, '\\n')}"`;
}

function cppDefault(prop) {
  const v = prop.default;
  if (prop.kind === 'string') return cppString(v);
  if (prop.kind === 'boolean') return v ? 'true' : 'false';
  if (prop.kind === 'number') return Number.isInteger(v) ? `${v}.0` : String(v);
  return String(v);
}

// A common type is named through its namespace from any other header.
function qualify(name, common, ctx) {
  if (common && !ctx.isCommon) {
    ctx.usesCommon = true;
    return `api::common::${name}`;
  }
  return name;
}

// Returns the C++ type for a value; nested structs are registered in ctx in dependency
// order (children before parents). A nested object that came from a named TypeScript
// interface keeps that name, so every use of the interface shares one struct.
//
// Results may hold null array items and map values (std::optional); the parser rejects
// those on params, so params never see them here. Params never reach a common.ts type
// either: parseNamespace rejects that (checkParamsReach), shared types being results only.
function typeOf(prop, nameHint, ctx, where, role) {
  if (SCALAR[prop.kind]) {
    const constrained = prop.minLength !== undefined || prop.enum || prop.minimum !== undefined || prop.maximum !== undefined;
    // The one item constraint the parser checks: number bounds on the items of an array that is
    // itself a member (ItemsRange in renderFromJson).
    const bounded = nameHint.boundedItem && prop.minLength === undefined && !prop.enum;
    if (role === 'params' && constrained && nameHint.nested && !bounded) {
      throw new SchemaError(`${where}: constraints on array items or map values are not supported yet`);
    }
    return SCALAR[prop.kind];
  }
  if (prop.kind === 'array') {
    const boundedItem = !nameHint.nested && (prop.items.kind === 'integer' || prop.items.kind === 'number');
    const item = typeOf(prop.items, { name: `${nameHint.name}Item`, nested: true, boundedItem }, ctx, `${where}.items`, role);
    return `std::vector<${prop.items.nullable ? `std::optional<${item}>` : item}>`;
  }
  if (prop.kind === 'ref') {
    if (!ctx.resolve(prop.name, prop.common)) throw new SchemaError(`${where}: ${prop.name} is not a declared type`);
    // A reference to a local type must end up as a struct of this header, and it hands the
    // role on to that struct; both happen at the end, once every struct exists.
    if (!prop.common) ctx.refs.push({ name: prop.name, where, role });
    return qualify(prop.name, prop.common, ctx);
  }
  // object
  if (prop.properties.length && prop.additional && role === 'params') {
    throw new SchemaError(`${where}: params objects cannot mix named properties with additionalProperties`);
  }
  if (!prop.properties.length && !prop.extends) {
    const value = typeOf(prop.additional, { name: `${nameHint.name}Value`, nested: true }, ctx, `${where}.additionalProperties`, role);
    return `std::map<std::string, ${prop.additional.nullable ? `std::optional<${value}>` : value}>`;
  }
  if (prop.common && !ctx.isCommon) {
    if (!findType(prop.name, null, ctx.namespaces, true)) throw new SchemaError(`${where}: ${prop.name} is not declared in common.ts`);
    return qualify(prop.name, true, ctx);
  }
  return ensureStruct(prop.name ?? nameHint.name, prop, ctx, where, role, null);
}

// Hands a role to everything below an object: its members' structs, its map values and its
// base. Every visit re-runs the role's checks on that shape.
function reach(name, obj, ctx, where, role) {
  for (const p of allProperties(obj, ctx.resolve, where)) {
    typeOf(p, { name: `${name}${pascal(p.key)}`, nested: false }, ctx, `${where}.${p.key}`, role);
  }
  if (obj.additional) typeOf(obj.additional, { name: `${name}Value`, nested: true }, ctx, `${where}.additionalProperties`, role);
  if (obj.extends && !(obj.extendsCommon && !ctx.isCommon)) {
    ensureStruct(obj.extends, ctx.resolve(obj.extends, obj.extendsCommon), ctx, `${where}<${obj.extends}>`, role, null);
  }
}

function fieldOf(p, name, ctx, where, role) {
  const w = `${where}.${p.key}`;
  const base = typeOf(p, { name: `${name}${pascal(p.key)}`, nested: false }, ctx, w, role);
  const optional = !p.required && p.default === undefined;
  // Params read a null value as absent, so only an optional key needs std::optional there;
  // a nullable result field needs it to write null.
  const wrapped = optional || (role === 'result' && p.nullable);
  return { prop: p, type: wrapped ? `std::optional<${base}>` : base, optional };
}

// Registers the struct for an object once, records which roles reach it, and returns its
// name. A role that arrives later is handed down to everything below the struct as well.
function ensureStruct(name, obj, ctx, where, role, method) {
  const shape = JSON.stringify([obj.properties, obj.additional, obj.extends ?? null, obj.partialOf ?? null]);
  const entry = ctx.byName.get(name);
  if (entry) {
    if (entry.shape !== shape) {
      const hint = obj.additional || entry.obj.additional ? `; give the version with extra keys its own interface (interface ${name}Columns extends ${name} {}) and intersect that` : '';
      throw new SchemaError(`${where}: another object in this namespace already produces struct ${name}${hint}`);
    }
    if (!entry.roles.has(role)) {
      entry.roles.add(role);
      reach(name, obj, ctx, where, role);
    }
    return name;
  }
  if (method === null && ctx.reserved.has(name)) {
    throw new SchemaError(`${where}: another object in this namespace already produces struct ${name}`);
  }
  // The base struct comes first, so it is defined before the derived one names it.
  let base = null;
  if (obj.extends) {
    const baseObj = ctx.resolve(obj.extends, obj.extendsCommon);
    if (!baseObj) throw new SchemaError(`${where}: ${name} extends ${obj.extends}, which no schema declares`);
    base = obj.extendsCommon && !ctx.isCommon ? qualify(obj.extends, true, ctx) : ensureStruct(obj.extends, baseObj, ctx, `${where}<${obj.extends}>`, role, null);
  }
  const all = allProperties(obj, ctx.resolve, where);
  const fields = [];
  for (const p of all) {
    if ((p.security || p.pathKeys) && method === null) throw new SchemaError(`${where}.${p.key}: "x-security" and "x-path-key" are only supported on top-level params`);
    fields.push(fieldOf(p, name, ctx, where, role));
  }
  const own = fields.slice(all.length - obj.properties.length);
  const additional = obj.additional
    ? (() => {
      const value = typeOf(obj.additional, { name: `${name}Value`, nested: true }, ctx, `${where}.additionalProperties`, role);
      return `std::map<std::string, ${obj.additional.nullable ? `std::optional<${value}>` : value}>`;
    })()
    : null;
  const created = { name, obj, shape, roles: new Set([role]), method, base, baseCommon: Boolean(obj.extendsCommon && !ctx.isCommon), fields, own, additional };
  ctx.byName.set(name, created);
  ctx.entries.push(created);
  return name;
}

// Whether the struct lists kFields: results and shared types do, and a struct deriving from
// one does too, so that it never inherits a base's list that lacks its own keys.
function listsFields(e, ctx) {
  if (e.roles.has('result') || ctx.isCommon) return true;
  if (!e.base) return false;
  return e.baseCommon || listsFields(ctx.byName.get(e.base), ctx);
}

function renderStruct(e, resultName, ctx) {
  const lines = [`struct ${e.name}${e.base ? ` : ${e.base}` : ''} {`];
  if (e.method && e.roles.has('params')) {
    lines.push(`    static constexpr const char* kMethod = ${cppString(e.method.api)};`);
    // One entry per checked path; an object array with per-member levels expands to one
    // entry per member, in declaration order, which is the order the bridge checks them.
    const pathParams = e.obj.properties.filter((p) => p.security || p.pathKeys);
    const entries = pathParams.flatMap((p) => (p.pathKeys
      ? p.pathKeys.map(({ key, level }) => `{${cppString(p.key)}, api::params::PathAccess::${level}, true, false, ${cppString(key)}}`)
      : [`{${cppString(p.key)}, api::params::PathAccess::${p.security}, ${p.kind === 'array'}, ${Boolean(p.skipInvalid)}, ${p.pathKey ? cppString(p.pathKey) : 'nullptr'}${p.stringElements ? ', true' : ''}}`]));
    lines.push(`    static constexpr std::array<api::params::PathParam, ${entries.length}> kPathParams{${entries.length ? `{${entries.join(', ')}}` : ''}};`);
    // The result type this method's handler returns; api::RegisterApi checks the two agree.
    lines.push(`    using Result = ${resultName ?? 'void'};`);
    if (e.own.length || e.additional) lines.push('');
  }
  // The declared keys, base members first: the checklist for a writer that fills the JSON by hand.
  if (listsFields(e, ctx)) {
    const keys = e.fields.map((f) => cppString(f.prop.key));
    lines.push(`    static constexpr std::array<std::string_view, ${keys.length}> kFields{${keys.length ? `{${keys.join(', ')}}` : ''}};`);
    if (e.own.length || e.additional) lines.push('');
  }
  for (const f of e.own) {
    lines.push(`    ${f.type} ${member(f.prop.key)}${f.prop.default !== undefined ? ` = ${cppDefault(f.prop)}` : '{}'};`);
  }
  if (e.additional) lines.push(`    ${e.additional} additional{};`);
  lines.push('};');
  return lines.join('\n');
}

// A range limit as the argument Reader::Range takes: absent is std::nullopt.
function bound(p, x) {
  if (x === undefined) return 'std::nullopt';
  return `std::optional<${SCALAR[p.kind]}>(${p.kind === 'number' && Number.isInteger(x) ? `${x}.0` : x})`;
}

function renderFromJson(e) {
  const reads = [];
  for (const { prop: p } of e.fields) {
    const k = cppString(p.key);
    const m = member(p.key);
    const checks = [p.required ? `r.Required(${k}, out.${m})` : `r.Optional(${k}, out.${m})`];
    if (p.minLength !== undefined) checks.push(`r.MinLength(${k}, out.${m}, ${p.minLength})`);
    if (p.enum) checks.push(`r.OneOf(${k}, out.${m}, {${p.enum.map(cppString).join(', ')}})`);
    if (p.minimum !== undefined || p.maximum !== undefined) {
      checks.push(`r.Range(${k}, out.${m}, ${bound(p, p.minimum)}, ${bound(p, p.maximum)})`);
    }
    if (p.kind === 'array' && (p.items.minimum !== undefined || p.items.maximum !== undefined)) {
      checks.push(`r.ItemsRange(${k}, out.${m}, ${bound(p.items, p.items.minimum)}, ${bound(p.items, p.items.maximum)})`);
    }
    if (p.minItems !== undefined) checks.push(`r.MinItems(${k}, out.${m}, ${p.minItems})`);
    reads.push(`        && ${checks.join('\n        && ')}`);
  }
  const known = e.fields.map(({ prop }) => cppString(prop.key)).join(', ');
  const lines = [];
  // With no fields `out` is never read; leaving it unnamed keeps MSVC C4100 quiet. The
  // default for `where` is on the declaration above the definitions.
  lines.push(`inline bool FromJson(const json& j, ${e.name}&${e.fields.length ? ' out' : ''}, std::string& error, const std::string& where) {`);
  lines.push('    api::params::Reader r(j, error, where);');
  lines.push('    return r.IsObject()');
  lines.push(`        && r.OnlyKeys({${known}})${reads.length ? '' : ';'}`);
  if (reads.length) {
    reads[reads.length - 1] += ';';
    lines.push(...reads);
  }
  lines.push('}');
  return lines.join('\n');
}

function renderToJson(e) {
  const lines = [`inline json ToJson(const ${e.name}&${e.fields.length || e.additional ? ' v' : ''}) {`, '    json j = json::object();'];
  // Extra keys go first so a named field with the same key wins.
  if (e.additional) lines.push('    api::results::PutAll(j, v.additional);');
  for (const { prop: p } of e.fields) {
    const put = p.nullable && p.required ? 'PutNullable' : 'Put';
    lines.push(`    api::results::${put}(j, ${cppString(p.key)}, v.${member(p.key)});`);
  }
  lines.push('    return j;', '}');
  return lines.join('\n');
}

// Inside `namespace api`, a nested namespace named like one the generated code and the
// helpers use would hide it (`api::results::Put` in a header of namespace api::api would look
// inside api::api), so such a schema namespace gets a trailing underscore, the way a member
// named after a keyword does.
const TAKEN_NAMESPACES = new Set(['api', 'params', 'results', 'emit', 'events']);
export function cppNamespace(ns) {
  const name = typeof ns === 'string' ? ns : ns.namespace;
  return TAKEN_NAMESPACES.has(name) ? `${name}_` : name;
}

export function cppHeaderPath(ns) {
  return `src/api/generated/${pascal(ns.namespace)}Schema.h`;
}

// Whether a namespace produces a header at all: common.ts does only once it declares a type.
export function hasCppHeader(ns) {
  return ns.namespace !== COMMON_NAMESPACE || ns.types.size > 0;
}

export const EVENT_REGISTRY_PATH = 'src/api/generated/EventRegistry.h';

// Every declared event descriptor in one tuple, so a test can walk all of them without a list
// of its own to keep up. Null while no namespace declares an event.
export function emitEventRegistry(namespaces) {
  const withEvents = namespaces.filter((ns) => (ns.events ?? []).length);
  if (!withEvents.length) return null;
  const descriptors = withEvents.flatMap((ns) => ns.events.map((e) => `api::${cppNamespace(ns)}::events::${pascal(e.key)}`));
  return [
    '// Generated by scripts/api-schema/generate.mjs from src/api/schema/. Do not edit.',
    '#pragma once',
    ...withEvents.map((ns) => `#include "${cppHeaderPath(ns).replace(/^src\//, '')}"`),
    '',
    '#include <tuple>',
    '',
    'namespace api::events {',
    '',
    `using All = std::tuple<\n    ${descriptors.join(',\n    ')}>;`,
    '',
    '}  // namespace api::events',
    '',
  ].join('\n');
}

// `namespaces` supplies common.ts for the types a namespace shares; a namespace on its own
// (as the tests build them) resolves only its own types.
export function emitCppHeader(ns, namespaces = [ns]) {
  // Top-level names are reserved up front so a nested interface cannot reuse one.
  const ctx = {
    entries: [],
    byName: new Map(),
    reserved: new Set(),
    refs: [],
    namespaces,
    isCommon: ns.namespace === COMMON_NAMESPACE,
    usesCommon: false,
    resolve: (name, common) => findType(name, ns, namespaces, common),
  };
  const events = ns.events ?? [];
  for (const m of ns.methods) {
    ctx.reserved.add(`${pascal(m.name)}Params`);
    if (m.result) ctx.reserved.add(`${pascal(m.name)}Result`);
  }
  for (const e of events) if (!e.sharedPayload) ctx.reserved.add(`${pascal(e.key)}Payload`);
  const resultNames = new Map();
  for (const m of ns.methods) {
    // The result struct comes first: the params struct names it in `using Result`.
    if (m.result) resultNames.set(m, ensureStruct(`${pascal(m.name)}Result`, m.result, ctx, `${ns.file}#${m.name}.result`, 'result', m));
    ensureStruct(`${pascal(m.name)}Params`, m.params, ctx, `${ns.file}#${m.name}.params`, 'params', m);
  }
  // A payload is written, never parsed: ToJson and kFields, like a result. A shared type of
  // common.ts is its own payload; CommonSchema.h already defines it.
  const payloadType = new Map();
  for (const e of events) {
    const where = `${ns.file}#events.${e.key}.payload`;
    if (e.sharedPayload) {
      if (!ctx.resolve(e.sharedPayload, true)) throw new SchemaError(`${where}: ${e.sharedPayload} is not a type of common.ts`);
      payloadType.set(e, qualify(e.sharedPayload, true, ctx));
    } else {
      payloadType.set(e, ensureStruct(`${pascal(e.key)}Payload`, e.payload, ctx, where, 'result', e));
    }
  }
  if (ctx.isCommon) {
    // Shared types are results: ToJson only.
    for (const [name, obj] of ns.types) ensureStruct(name, obj, ctx, `${ns.file}#${name}`, 'result', null);
  }
  // A reference points at the struct of a named interface that some value uses. A method's
  // own params or result interface has no struct under its interface name, and a type used
  // through references alone never gets one. The reference also hands its role to that
  // struct, which hands it on below itself; that walk may record further references, so
  // the loop runs over the list as it grows.
  for (let i = 0; i < ctx.refs.length; i += 1) {
    const ref = ctx.refs[i];
    const entry = ctx.byName.get(ref.name);
    if (!entry) {
      throw new SchemaError(`${ref.where}: ${ref.name} is only used through a reference here; a recursive use points at a named interface that a value also uses as an object, not at a method's own params or result interface`);
    }
    ensureStruct(ref.name, entry.obj, ctx, ref.where, ref.role, null);
  }
  // Every struct and function is declared first: a struct may hold a vector of one defined
  // later (mutual recursion, or recursion through a nested object), and a FromJson may call
  // the FromJson of a struct defined later.
  const declarations = ctx.entries.map((e) => `struct ${e.name};`);
  for (const e of ctx.entries) {
    if (e.roles.has('params')) declarations.push(`inline bool FromJson(const json& j, ${e.name}& out, std::string& error, const std::string& where = {});`);
    if (e.roles.has('result')) declarations.push(`inline json ToJson(const ${e.name}& v);`);
  }
  const blocks = [declarations.join('\n')];
  for (const e of ctx.entries) {
    blocks.push(renderStruct(e, e.method && e.roles.has('params') ? resultNames.get(e.method) ?? null : null, ctx));
    if (e.roles.has('params')) blocks.push(renderFromJson(e));
    if (e.roles.has('result')) blocks.push(renderToJson(e));
  }
  // One descriptor per event: its name and payload type, for the typed emit helpers in
  // src/api/EventEmit.h. kCustomName marks an event a subscriber may deliver under its own name.
  if (events.length) {
    const descriptors = events.map((e) => [
      `struct ${pascal(e.key)} {`,
      `    static constexpr const char* kName = ${cppString(e.name)};`,
      `    static constexpr bool kCustomName = ${e.customName};`,
      `    using Payload = ${payloadType.get(e)};`,
      '};',
    ].join('\n'));
    blocks.push(['namespace events {', '', descriptors.join('\n\n'), '', '}  // namespace events'].join('\n'));
  }
  return [
    `// Generated by scripts/api-schema/generate.mjs from ${ns.file}. Do not edit.`,
    '#pragma once',
    '#include "api/ApiParams.h"',
    '#include "api/ApiResult.h"',
    ...(ctx.usesCommon ? ['#include "api/generated/CommonSchema.h"'] : []),
    '',
    '#include <array>',
    '#include <cstdint>',
    '#include <map>',
    '#include <optional>',
    '#include <string>',
    '#include <string_view>',
    '#include <vector>',
    '',
    `namespace api::${cppNamespace(ns)} {`,
    '',
    'using json = nlohmann::json;',
    '',
    blocks.join('\n\n'),
    '',
    `}  // namespace api::${cppNamespace(ns)}`,
    '',
  ].join('\n');
}

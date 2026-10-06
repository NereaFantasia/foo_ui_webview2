// Loads src/api/schema/<namespace>.ts (through load-ts.mjs) or <namespace>.json and checks
// the resulting JSON Schema document against the subset the generators support. Anything
// outside that subset is an error rather than being ignored, so a schema can never promise
// something the generated code does not do.
import fs from 'node:fs';
import path from 'node:path';

import { COMMON_FILE, COMMON_NAMESPACE, readTsSchemas } from './load-ts.mjs';

export { COMMON_NAMESPACE };
export const SCHEMA_DIR = 'src/api/schema';
const KEY_RE = /^[A-Za-z][A-Za-z0-9]*$/;
const IDENT_RE = /^[A-Za-z_][A-Za-z0-9_]*$/;
const TYPE_NAME_RE = /^[A-Z][A-Za-z0-9]*$/;
const SECURITY = new Set(['Read', 'Write', 'MediaRead', 'MediaWrite', 'FileWrite']);
// nullable: the value may also be null. On params the bridge reads a null key as absent.
const COMMON = ['description', 'x-description-zh', 'nullable'];
// x-name: the TypeScript interface a nested object came from, reused as its C++ struct and
// TypeScript interface name. x-common marks a type from common.ts, shared by every namespace.
// x-extends names the interface whose members come first; x-partial-of the interface this is
// the all-optional copy of. x-type-description(-zh) is the interface's own JSDoc, shown where
// the type is documented on its own.
const NAMED = ['x-name', 'x-common', 'x-extends', 'x-extends-common', 'x-partial-of', 'x-type-description', 'x-type-description-zh'];
const ALLOWED = {
  string: ['type', 'minLength', 'enum', 'default', 'x-security', ...COMMON],
  integer: ['type', 'minimum', 'maximum', 'default', ...COMMON],
  number: ['type', 'minimum', 'maximum', 'default', ...COMMON],
  boolean: ['type', 'default', ...COMMON],
  // Any JSON value, passed through unchanged. null is already one of its values, so it takes
  // no "nullable"; it takes no default or constraint either.
  json: ['type', 'description', 'x-description-zh'],
  array: ['type', 'items', 'minItems', 'x-security', 'x-skip-invalid', 'x-path-key', ...COMMON],
  object: ['type', 'properties', 'required', 'additionalProperties', ...NAMED, ...COMMON],
  // A use of a named interface inside its own declaration (a recursive type): only the name.
  ref: ['type', 'x-ref', 'x-common', 'description', 'x-description-zh'],
};

export class SchemaError extends Error {}

function fail(where, msg) {
  throw new SchemaError(`${where}: ${msg}`);
}

function checkKeys(node, allowed, where) {
  for (const k of Object.keys(node)) if (!allowed.includes(k)) fail(where, `unsupported keyword "${k}"`);
}

function text(node, where, key, required) {
  const v = node[key];
  if (v === undefined) {
    if (required) fail(where, `"${key}" is required`);
    return undefined;
  }
  if (typeof v !== 'string' || !v.trim()) fail(where, `"${key}" must be a non-empty string`);
  return v;
}

// role: 'params' | 'result'; describe: whether named properties must carry a description;
// nested: inside an array or a map, the only places a recursive reference may appear (a C++
// struct can hold a vector of itself, not a member of itself).
function parseProp(node, where, { role, describe, nested = false }) {
  if (!node || typeof node !== 'object' || Array.isArray(node)) fail(where, 'must be an object');
  if (node['x-ref'] !== undefined) {
    if (node.type !== 'object') fail(where, '"x-ref" needs "type": "object"');
    checkKeys(node, ALLOWED.ref, where);
    if (typeof node['x-ref'] !== 'string' || !TYPE_NAME_RE.test(node['x-ref'])) fail(where, '"x-ref" must be a PascalCase identifier');
    if (!nested) fail(where, `a recursive use of ${node['x-ref']} must be inside an array`);
    return {
      kind: 'ref',
      name: node['x-ref'],
      common: node['x-common'] === true,
      description: text(node, where, 'description', describe),
      descriptionZh: text(node, where, 'x-description-zh', false),
    };
  }
  const kind = node.type;
  if (!ALLOWED[kind] || kind === 'ref') fail(where, `"type" must be one of ${Object.keys(ALLOWED).filter((k) => k !== 'ref').join(', ')}`);
  checkKeys(node, ALLOWED[kind], where);
  const prop = {
    kind,
    description: text(node, where, 'description', describe),
    descriptionZh: text(node, where, 'x-description-zh', false),
  };
  if (node.nullable !== undefined) {
    if (node.nullable !== true) fail(where, '"nullable" can only be true');
    prop.nullable = true;
  }
  if (node['x-security'] !== undefined) {
    if (role !== 'params') fail(where, '"x-security" is only allowed on params');
    if (!SECURITY.has(node['x-security'])) fail(where, `"x-security" must be one of ${[...SECURITY].join(', ')}`);
    prop.security = node['x-security'];
  }
  // A path array that drops the paths failing the security check instead of refusing the
  // call; the bridge then reports the number dropped as `skippedPaths` on success.
  if (node['x-skip-invalid'] !== undefined) {
    if (node['x-skip-invalid'] !== true) fail(where, '"x-skip-invalid" can only be true');
    if (kind !== 'array' || node['x-security'] === undefined) fail(where, '"x-skip-invalid" needs "x-security" on an array of paths');
    prop.skipInvalid = true;
  }
  // The member holding the path in an array of objects (items[].path); the bridge checks
  // that member of every element before the handler runs. Either one member, checked at the
  // array's "x-security" level, or `{ member: level, ... }` when the members differ in level:
  // the bridge then checks them in this order and the array carries no "x-security".
  // On an array of Json the single-member form means an element is either the path string
  // itself or an object holding it in that member (handles[] taking "path" or { path, subsong }).
  if (node['x-path-key'] !== undefined) {
    const key = node['x-path-key'];
    if (node['x-skip-invalid'] !== undefined) fail(where, '"x-skip-invalid" is not supported with "x-path-key"');
    if (typeof key === 'string') {
      if (!IDENT_RE.test(key)) fail(where, '"x-path-key" must be an identifier');
      if (kind !== 'array' || node['x-security'] === undefined) fail(where, '"x-path-key" needs "x-security" on an array of objects or Json');
      prop.pathKey = key;
    } else if (key && typeof key === 'object' && !Array.isArray(key)) {
      const entries = Object.entries(key);
      if (!entries.length) fail(where, '"x-path-key" needs at least one member');
      for (const [member, level] of entries) {
        if (!IDENT_RE.test(member)) fail(where, `"x-path-key" member "${member}" must be an identifier`);
        if (!SECURITY.has(level)) fail(where, `"x-path-key" level for "${member}" must be one of ${[...SECURITY].join(', ')}`);
      }
      if (role !== 'params') fail(where, '"x-path-key" is only allowed on params');
      if (kind !== 'array') fail(where, '"x-path-key" needs an array of objects');
      if (node['x-security'] !== undefined) fail(where, '"x-path-key" with levels replaces "x-security" on the array; drop one of them');
      prop.pathKeys = entries.map(([member, level]) => ({ key: member, level }));
    } else {
      fail(where, '"x-path-key" must be an identifier or an object of member: level');
    }
  }
  if (kind === 'string') {
    if (node.minLength !== undefined) {
      if (!Number.isInteger(node.minLength) || node.minLength < 0) fail(where, '"minLength" must be a non-negative integer');
      prop.minLength = node.minLength;
    }
    if (node.enum !== undefined) {
      if (!Array.isArray(node.enum) || !node.enum.length || !node.enum.every((e) => typeof e === 'string')) {
        fail(where, '"enum" must be a non-empty array of strings');
      }
      prop.enum = node.enum;
    }
    if (node.default !== undefined && typeof node.default !== 'string') fail(where, '"default" must be a string');
  }
  if (kind === 'integer' || kind === 'number') {
    for (const k of ['minimum', 'maximum']) {
      if (node[k] === undefined) continue;
      if (typeof node[k] !== 'number' || (kind === 'integer' && !Number.isInteger(node[k]))) fail(where, `"${k}" must be ${kind === 'integer' ? 'an integer' : 'a number'}`);
      prop[k] = node[k];
    }
    if (node.default !== undefined && (typeof node.default !== 'number' || (kind === 'integer' && !Number.isInteger(node.default)))) {
      fail(where, `"default" must be ${kind === 'integer' ? 'an integer' : 'a number'}`);
    }
  }
  if (kind === 'boolean' && node.default !== undefined && typeof node.default !== 'boolean') fail(where, '"default" must be a boolean');
  if (node.default !== undefined) prop.default = node.default;
  if (kind === 'array') {
    if (!node.items) fail(where, '"items" is required');
    prop.items = parseProp(node.items, `${where}.items`, { role, describe: false, nested: true });
    if (role === 'params' && prop.items.nullable) fail(`${where}.items`, 'array items in params cannot be null; the generated parser rejects them');
    if (prop.items.security) fail(`${where}.items`, 'put "x-security" on the array, not on its items');
    if (prop.pathKeys) {
      if (prop.items.kind !== 'object') fail(where, '"x-path-key" needs an array of objects');
      for (const { key } of prop.pathKeys) {
        const member = (prop.items.properties ?? []).find((p) => p.key === key);
        if (!member || member.kind !== 'string' || !member.required) fail(where, `"x-path-key" must name a required string member of the items, got "${key}"`);
      }
    } else if (prop.security && prop.items.kind === 'object') {
      if (!prop.pathKey) fail(where, '"x-security" on an array of objects needs "x-path-key"');
      const member = (prop.items.properties ?? []).find((p) => p.key === prop.pathKey);
      if (!member || member.kind !== 'string' || !member.required) fail(where, `"x-path-key" must name a required string member of the items, got "${prop.pathKey}"`);
    } else if (prop.security && prop.items.kind === 'json') {
      if (!prop.pathKey) fail(where, '"x-security" on an array of Json needs "x-path-key": string elements are paths, objects hold the path in that member');
      prop.stringElements = true;
    } else if (prop.security && prop.items.kind !== 'string') {
      fail(where, '"x-security" arrays must hold strings, objects or Json');
    } else if (prop.pathKey) {
      fail(where, '"x-path-key" needs an array of objects');
    }
    if (node.minItems !== undefined) {
      if (!Number.isInteger(node.minItems) || node.minItems < 0) fail(where, '"minItems" must be a non-negative integer');
      prop.minItems = node.minItems;
    }
  }
  if (prop.security && kind !== 'string' && kind !== 'array') fail(where, '"x-security" is only allowed on strings and arrays of strings');
  if (kind === 'object') {
    Object.assign(prop, parseObjectBody(node, where, { role }));
    for (const key of ['x-name', 'x-extends', 'x-partial-of']) {
      if (node[key] !== undefined && (typeof node[key] !== 'string' || !TYPE_NAME_RE.test(node[key]))) fail(where, `"${key}" must be a PascalCase identifier`);
    }
    for (const key of ['x-common', 'x-extends-common']) {
      if (node[key] !== undefined && node[key] !== true) fail(where, `"${key}" can only be true`);
    }
    if (node['x-name'] !== undefined) prop.name = node['x-name'];
    if (node['x-common']) {
      if (!prop.name) fail(where, '"x-common" needs "x-name"');
      prop.common = true;
    }
    if (node['x-extends'] !== undefined) {
      if (!prop.name) fail(where, '"x-extends" needs "x-name"');
      if (node['x-extends'] === prop.name) fail(where, `${prop.name} cannot extend itself`);
      prop.extends = node['x-extends'];
      prop.extendsCommon = node['x-extends-common'] === true;
    } else if (node['x-extends-common']) {
      fail(where, '"x-extends-common" needs "x-extends"');
    }
    if (node['x-partial-of'] !== undefined) {
      if (!prop.name) fail(where, '"x-partial-of" needs "x-name"');
      if (prop.extends) fail(where, 'a Partial cannot also extend an interface');
      if (prop.properties.some((p) => p.required)) fail(where, 'every member of a Partial is optional');
      prop.partialOf = node['x-partial-of'];
    }
    prop.typeDescription = text(node, where, 'x-type-description', false);
    prop.typeDescriptionZh = text(node, where, 'x-type-description-zh', false);
  }
  return prop;
}

// The members of an object with its extended interfaces' members first. `resolve(name)` gives
// the registered type of that name (see findType); a missing base is an error.
export function allProperties(obj, resolve, where = '') {
  if (!obj.extends) return obj.properties;
  const base = resolve(obj.extends, obj.extendsCommon);
  if (!base) fail(where || obj.name, `${obj.name} extends ${obj.extends}, which no schema declares`);
  return [...allProperties(base, resolve, where), ...obj.properties];
}

// Looks a named type up in a namespace's own types, then in common.ts. `common` narrows the
// search to common.ts for a name known to come from there.
export function findType(name, ns, namespaces, common = false) {
  if (!common) {
    const own = ns?.types?.get(name);
    if (own) return own;
  }
  return namespaces.find((n) => n.namespace === COMMON_NAMESPACE)?.types?.get(name) ?? null;
}

function parseObjectBody(node, where, { role }) {
  const out = { properties: [], additional: false };
  if (node.properties !== undefined) {
    if (!node.properties || typeof node.properties !== 'object' || Array.isArray(node.properties)) fail(where, '"properties" must be an object');
    const required = node.required ?? [];
    if (!Array.isArray(required) || !required.every((r) => typeof r === 'string')) fail(where, '"required" must be an array of strings');
    for (const r of required) if (!(r in node.properties)) fail(where, `"required" names "${r}", which is not in "properties"`);
    for (const [key, child] of Object.entries(node.properties)) {
      if (!KEY_RE.test(key)) fail(where, `property name "${key}" must be camelCase letters and digits (a leading underscore is reserved for the bridge)`);
      const prop = parseProp(child, `${where}.properties.${key}`, { role, describe: true });
      prop.key = key;
      prop.required = required.includes(key);
      if (prop.required && prop.default !== undefined) fail(`${where}.properties.${key}`, 'a required property cannot have a default');
      if (prop.required && prop.nullable && role === 'params') fail(`${where}.properties.${key}`, 'a required parameter cannot be null; the bridge reads null as absent');
      // Both at once would need a three-state value on the C++ side (absent / null / value).
      if (!prop.required && prop.nullable && role === 'result') fail(`${where}.properties.${key}`, 'a result field cannot be both optional and nullable; pick one');
      out.properties.push(prop);
    }
  } else if (node.required !== undefined) {
    fail(where, '"required" needs "properties"');
  }
  if (node.additionalProperties === undefined) {
    fail(where, '"additionalProperties" must be stated: false, or a schema for the extra values');
  }
  if (node.additionalProperties !== false) {
    out.additional = parseProp(node.additionalProperties, `${where}.additionalProperties`, { role, describe: false, nested: true });
    if (out.additional.kind === 'ref') fail(`${where}.additionalProperties`, 'a recursive use must be inside an array, not a map');
    if (out.additional.security) fail(`${where}.additionalProperties`, '"x-security" is not allowed here');
    if (role === 'params' && out.additional.nullable) fail(`${where}.additionalProperties`, 'map values in params cannot be null; the generated parser rejects them');
  }
  // An interface that only extends another has no members of its own.
  if (!out.properties.length && out.additional === false && node['x-extends'] === undefined) fail(where, 'an object needs "properties" or "additionalProperties"');
  return out;
}

// The params rules over everything a params object reaches. Inline members were checked
// as they were parsed; members inherited through x-extends were parsed under the "type"
// rules, and a type from common.ts is never parsed here. A shared type is results only:
// its path fields would otherwise skip the security check that kPathParams gives top-level
// parameters.
function checkParamsReach(obj, where, types) {
  const resolve = (name) => types.get(name) ?? null;
  const resultsOnly = (name) => `${name} comes from common.ts; shared types are results only, declare a params interface in the namespace file`;
  // A Partial<T> carries T's members, so it is as much a use of T's base as T itself is.
  const commonBase = (o) => {
    for (let t = o; t; t = t.extends ? resolve(t.extends) : null) {
      if (t.extendsCommon) return t.extends;
    }
    return null;
  };
  const visited = new Set();
  const value = (p, w) => {
    if (p.kind === 'array') {
      if (p.items.nullable) fail(`${w}.items`, 'array items in params cannot be null; the generated parser rejects them');
      value(p.items, `${w}.items`);
    } else if (p.kind === 'ref') {
      if (p.common) fail(w, resultsOnly(p.name));
      // The rules follow a reference into the interface it names, once per interface.
      if (!visited.has(p.name)) {
        visited.add(p.name);
        const target = resolve(p.name);
        if (target) object(target, `${w}<${p.name}>`);
      }
    } else if (p.kind === 'object') {
      object(p, w);
    }
  };
  const object = (o, w) => {
    if (o.common) fail(w, resultsOnly(o.name));
    if (o.extendsCommon) fail(w, `${o.name} extends ${o.extends}; ${resultsOnly(o.extends)}`);
    const base = commonBase(o) ?? (o.partialOf ? commonBase(resolve(o.partialOf)) : null);
    if (base) fail(w, `${o.name} carries the members of ${base}; ${resultsOnly(base)}`);
    for (const p of allProperties(o, resolve, w)) {
      const pw = `${w}.properties.${p.key}`;
      if (p.required && p.nullable) fail(pw, 'a required parameter cannot be null; the bridge reads null as absent');
      value(p, pw);
    }
    if (o.additional) {
      if (o.additional.nullable) fail(`${w}.additionalProperties`, 'map values in params cannot be null; the generated parser rejects them');
      value(o.additional, `${w}.additionalProperties`);
    }
  };
  object(obj, where);
}

// Keys the bridge writes on every response: `success` on both shapes, `error` and `code` on
// the failure envelope, `skippedPaths` when a skipInvalid path check dropped entries. A
// result declares only the method's own fields; nested objects are not envelopes and may
// use these names.
const ENVELOPE_KEYS = ['success', 'error', 'code', 'skippedPaths'];

// An event payload follows the result rules but carries no envelope, so it may use the
// envelope's key names.
function parseTopObject(node, where, role) {
  if (!node || node.type !== 'object') fail(where, '"type" must be "object"');
  checkKeys(node, ['type', 'properties', 'required', 'additionalProperties'], where);
  const body = parseObjectBody(node, where, { role: role === 'event' ? 'result' : role });
  if (role === 'params' && body.additional !== false) fail(where, 'params must set "additionalProperties": false');
  if (role === 'result') {
    for (const p of body.properties) {
      if (ENVELOPE_KEYS.includes(p.key)) fail(`${where}.properties.${p.key}`, `"${p.key}" is written by the bridge envelope; declare only the method's own fields`);
    }
  }
  return body;
}

// The document's named types: every nested interface a namespace uses, and every interface
// of common.ts. Parsed with the result rules, the wider of the two (a type used in params is
// checked again, inline, under the params rules).
function parseTypes(json, where) {
  const types = new Map();
  if (json.types === undefined) return types;
  if (!json.types || typeof json.types !== 'object' || Array.isArray(json.types)) fail(where, '"types" must be an object');
  for (const [name, node] of Object.entries(json.types)) {
    const w = `${where}#types.${name}`;
    if (!TYPE_NAME_RE.test(name)) fail(w, 'type names are PascalCase identifiers');
    // The type itself is described by x-type-description; its members need descriptions.
    // Role 'type' applies neither side's rules: each use is parsed again, inline, under the
    // rules of the side it appears on.
    const prop = parseProp(node, w, { role: 'type', describe: false });
    if (prop.kind !== 'object' || prop.name !== name) fail(w, `"types" entry ${name} must be an object with "x-name": "${name}"`);
    types.set(name, prop);
  }
  for (const t of types.values()) {
    if (t.extends && !t.extendsCommon && !types.has(t.extends)) fail(`${where}#types.${t.name}`, `${t.name} extends ${t.extends}, which "types" does not hold`);
    if (t.partialOf && !t.common && !types.has(t.partialOf)) fail(`${where}#types.${t.name}`, `${t.name} is the Partial of ${t.partialOf}, which "types" does not hold`);
  }
  return types;
}

// Who receives an event: every window, the page that made the call, the page that owns the
// subscription or task, the window the caller named, the page of the window or panel the
// event is about, or the main window's page whatever page caused it.
const DELIVERY = new Set(['broadcast', 'caller', 'owner', 'target', 'window', 'main']);

// An event's payload is either its own object or, with x-payload-type, a shared type of
// common.ts as a whole; `payload` is null then and `sharedPayload` names the type.
function parseEvents(json, ns, where) {
  if (json.events === undefined) return [];
  if (!json.events || typeof json.events !== 'object' || Array.isArray(json.events)) fail(where, '"events" must be an object');
  const events = [];
  for (const [key, e] of Object.entries(json.events)) {
    const ew = `${where}#events.${key}`;
    if (!KEY_RE.test(key)) fail(ew, 'event name must be a camelCase identifier');
    checkKeys(e, ['description', 'x-description-zh', 'x-delivery', 'x-custom-name', 'x-payload-type', 'payload'], ew);
    if (!DELIVERY.has(e['x-delivery'])) fail(ew, `"x-delivery" must be one of ${[...DELIVERY].join(', ')}`);
    if (e['x-custom-name'] !== undefined && e['x-custom-name'] !== true) fail(ew, '"x-custom-name" must be true when present');
    const shared = e['x-payload-type'];
    if (shared !== undefined) {
      if (typeof shared !== 'string' || !TYPE_NAME_RE.test(shared)) fail(ew, '"x-payload-type" must name a type of common.ts');
      if (e.payload !== undefined) fail(ew, '"x-payload-type" replaces "payload"; give one of the two');
    }
    events.push({
      key,
      name: `${ns}:${key}`,
      description: text(e, ew, 'description', true),
      descriptionZh: text(e, ew, 'x-description-zh', false),
      delivery: e['x-delivery'],
      customName: e['x-custom-name'] === true,
      sharedPayload: shared ?? null,
      payload: shared !== undefined ? null : e.payload ? parseTopObject(e.payload, `${ew}.payload`, 'event') : { properties: [], additional: false },
    });
  }
  return events;
}

// How a method affects its environment, for the MCP tool annotations. `read` changes nothing;
// `write` changes state or adds data without losing any (playback, volume, selection, a new
// playlist); `destructive` removes or overwrites data the user keeps (playlists and their
// rows, the queue, tags, artwork, files). `idempotent` says a repeated call with the same
// arguments has no further effect, which only a changing method can claim; `openWorld` says
// the method reaches beyond the foobar2000 instance. A method without `x-effect` is not
// described; the MCP generator refuses to expose one.
const EFFECTS = new Set(['read', 'write', 'destructive']);

function parseBehavior(m, where) {
  const effect = m['x-effect'];
  if (effect !== undefined && !EFFECTS.has(effect)) fail(where, `"x-effect" must be one of ${[...EFFECTS].join(', ')}`);
  for (const key of ['x-idempotent', 'x-open-world']) {
    if (m[key] !== undefined && m[key] !== true) fail(where, `"${key}" can only be true`);
  }
  if (m['x-idempotent'] && (effect === undefined || effect === 'read')) fail(where, '"x-idempotent" needs "x-effect" write or destructive; a read changes nothing to repeat');
  if (m['x-open-world'] && effect === undefined) fail(where, '"x-open-world" needs "x-effect"');
  return { effect: effect ?? null, idempotent: m['x-idempotent'] === true, openWorld: m['x-open-world'] === true };
}

const LIFECYCLE_KEYS = ['x-experimental', 'x-deprecated', 'x-deprecated-zh'];

function parseLifecycle(m, where) {
  if (m['x-experimental'] !== undefined && m['x-experimental'] !== true) fail(where, '"x-experimental" can only be true');
  const deprecated = text(m, where, 'x-deprecated', false);
  const deprecatedZh = text(m, where, 'x-deprecated-zh', false);
  if ((deprecated === undefined) !== (deprecatedZh === undefined)) fail(where, '"x-deprecated" and "x-deprecated-zh" must be supplied together');
  return { experimental: m['x-experimental'] === true, deprecated: deprecated ?? null, deprecatedZh: deprecatedZh ?? null };
}

export function parseNamespace(json, file) {
  const where = file;
  checkKeys(json, ['namespace', 'methods', 'types', 'events'], where);
  const ns = json.namespace;
  if (typeof ns !== 'string' || !KEY_RE.test(ns)) fail(where, '"namespace" must be a camelCase identifier');
  if (path.basename(file).replace(/\.(json|ts)$/, '') !== ns) fail(where, `file name must be ${ns}${path.extname(file)}`);
  const types = parseTypes(json, where);
  if (ns === COMMON_NAMESPACE) {
    if (json.methods && Object.keys(json.methods).length) fail(where, 'common.ts declares shared types only, not methods');
    if (json.events && Object.keys(json.events).length) fail(where, 'common.ts declares shared types only, not events');
    return { namespace: ns, file, methods: [], events: [], types };
  }
  const events = parseEvents(json, ns, where);
  if (json.methods !== undefined && (!json.methods || typeof json.methods !== 'object' || Array.isArray(json.methods))) fail(where, '"methods" must be an object');
  if (!Object.keys(json.methods ?? {}).length && !events.length) fail(where, '"methods" must name at least one method, or "events" at least one event');
  const methods = [];
  for (const [name, m] of Object.entries(json.methods ?? {})) {
    const mw = `${where}#${name}`;
    if (!KEY_RE.test(name)) fail(mw, 'method name must be a camelCase identifier');
    checkKeys(m, ['description', 'x-description-zh', 'params', 'result', 'x-effect', 'x-idempotent', 'x-open-world', ...LIFECYCLE_KEYS], mw);
    methods.push({
      name,
      api: `${ns}.${name}`,
      description: text(m, mw, 'description', true),
      descriptionZh: text(m, mw, 'x-description-zh', false),
      ...parseBehavior(m, mw),
      ...parseLifecycle(m, mw),
      params: m.params ? parseTopObject(m.params, `${mw}.params`, 'params') : { properties: [], additional: false },
      result: m.result ? parseTopObject(m.result, `${mw}.result`, 'result') : null,
    });
  }
  for (const m of methods) checkParamsReach(m.params, `${where}#${m.name}.params`, types);
  return { namespace: ns, file, methods, events, types };
}

// A namespace is declared either in <ns>.json or in <ns>.ts (see load-ts.mjs), never both.
// common.ts holds the shared types (read as the methodless namespace `common`) and
// tsconfig.json the compiler options.
export function loadNamespaces(repoRoot) {
  const dir = path.join(repoRoot, SCHEMA_DIR);
  if (!fs.existsSync(dir)) return [];
  const names = fs.readdirSync(dir).sort();
  const jsonFiles = names.filter((f) => f.endsWith('.json') && f !== 'tsconfig.json');
  const tsFiles = names.filter((f) => f.endsWith('.ts'));
  const docs = jsonFiles.map((f) => {
    const file = `${SCHEMA_DIR}/${f}`;
    try {
      return { file, json: JSON.parse(fs.readFileSync(path.join(dir, f), 'utf8')) };
    } catch (e) {
      throw new SchemaError(`${file}: ${e.message}`);
    }
  });
  docs.push(...readTsSchemas(repoRoot, tsFiles.map((f) => `${SCHEMA_DIR}/${f}`)));
  const seen = new Map();
  // A common.ts without interfaces declares no shared types, so there is no common namespace.
  const namespaces = docs.map(({ file, json }) => parseNamespace(json, file)).filter((ns) => ns.namespace !== COMMON_NAMESPACE || ns.types.size);
  for (const ns of namespaces) {
    if (seen.has(ns.namespace)) throw new SchemaError(`${ns.file}: namespace ${ns.namespace} is also declared in ${seen.get(ns.namespace)}`);
    seen.set(ns.namespace, ns.file);
  }
  return namespaces.sort((a, b) => a.namespace.localeCompare(b.namespace));
}

export const pascal = (s) => s.charAt(0).toUpperCase() + s.slice(1);

// Emits the TypeScript Params / Response declarations for a declared method, and the named
// interfaces the declarations share. The SDK type generator (scripts/gen_sdk_types.mjs)
// writes them into sdk/src/types/generated/.
//
// A Response is a union: the success shape (`success: true` plus the declared result with
// its declared required-ness) or the failure envelope `ApiFailure` ({ success: false, error,
// code }), which carries none of the declared fields. A caller reads the result only after
// checking `success`.
//
// A nested object that came from a named interface is written as that interface's name;
// emitTsNamedTypes renders every such interface once, common.ts ones first, so the SDK
// exports one `Track` or one `FileFilter` rather than a copy per method.
import { allProperties, COMMON_NAMESPACE, findType, SchemaError } from './schema.mjs';

const INDENT = '    ';

// Member docs collapse to one line when they fit; declaration heads always use the
// block form, like the rest of the generated files.
function jsdoc(lines, indent, { block = false } = {}) {
  const text = lines.filter(Boolean).map((line) => line.replaceAll('*/', '*&#47;'));
  if (!text.length) return [];
  if (!block && text.length === 1 && !text[0].includes('\n')) return [`${indent}/** ${text[0]} */`];
  return [`${indent}/**`, ...text.flatMap((t) => t.split('\n')).map((t) => `${indent} * ${t}`), `${indent} */`];
}

function lifecycleTags(method) {
  return [method.experimental ? '@experimental' : null, method.deprecated ? `@deprecated ${method.deprecated}` : null];
}

/** Lifecycle tags for a method-name mapping member; unmarked methods add no text. */
export function emitTsMethodLifecycle(method, indent = '') {
  return jsdoc(lifecycleTags(method), indent);
}

// `used` collects the named interfaces a block refers to, for the file's import line.
function tsType(prop, indent, used) {
  const t = baseType(prop, indent, used);
  return prop.nullable ? `${t} | null` : t;
}

function baseType(prop, indent, used) {
  switch (prop.kind) {
    case 'string':
      return prop.enum ? prop.enum.map((e) => JSON.stringify(e)).join(' | ') : 'string';
    case 'integer':
    case 'number':
      return 'number';
    case 'boolean':
      return 'boolean';
    case 'json':
      return 'JsonValue';
    case 'array': {
      const inner = tsType(prop.items, indent, used);
      return /^[\w.]+$/.test(inner) ? `${inner}[]` : `Array<${inner}>`;
    }
    case 'ref':
      used?.add(prop.name);
      return prop.name;
    case 'object':
      if (prop.name) {
        used?.add(prop.name);
        // `Name & Record<string, T>`: the named interface plus extra keys, as it was declared.
        return prop.additional ? `${prop.name} & Record<string, ${tsType(prop.additional, indent, used)}>` : prop.name;
      }
      if (!prop.properties.length) return `Record<string, ${tsType(prop.additional, indent, used)}>`;
      return objectLiteral(prop, indent, { forceOptional: false }, used);
    default:
      throw new Error(`unknown kind ${prop.kind}`);
  }
}

function members(obj, indent, { forceOptional }, used, properties = obj.properties) {
  const out = [];
  const types = new Set();
  for (const p of properties) {
    const t = tsType(p, indent, used);
    const optional = forceOptional || !p.required;
    const doc = [p.description, p.default !== undefined ? `@default ${JSON.stringify(p.default)}` : null];
    out.push(...jsdoc(doc, indent), `${indent}${p.key}${optional ? '?' : ''}: ${t};`);
    types.add(t);
  }
  if (obj.additional) {
    const t = tsType(obj.additional, indent, used);
    // An index signature must admit every named member's type as well.
    const union = [...new Set([...types, t, ...(forceOptional || properties.some((p) => !p.required) ? ['undefined'] : [])])];
    out.push(...jsdoc([obj.additional.description], indent), `${indent}[key: string]: ${union.join(' | ')};`);
  }
  return out;
}

function objectLiteral(obj, indent, opts, used) {
  return `{\n${members(obj, indent + INDENT, opts, used).join('\n')}\n${indent}}`;
}

export function emitTsParams(method, interfaceName, used) {
  const head = jsdoc([`Parameters for \`${method.api}\`.`, method.description, ...lifecycleTags(method)], '', { block: true });
  if (!method.params.properties.length) {
    return [...head, `export type ${interfaceName} = Record<string, never>;`].join('\n');
  }
  return [...head, `export interface ${interfaceName} {`, ...members(method.params, INDENT, { forceOptional: false }, used), '}'].join('\n');
}

const SKIPPED_PATHS = { key: 'skippedPaths', type: 'number', description: 'Number of paths the security check dropped before the call ran; present only when at least one was dropped.' };

// `WindowCreatePopupResponse` -> `WindowCreatePopupSuccess`.
const pascal = (name, separator) =>
  name.split(separator).map((seg) => seg.charAt(0).toUpperCase() + seg.slice(1)).join('');

// `playlist.addPaths` → `PlaylistAddPathsParams` / `PlaylistAddPathsResponse`.
export function apiTypeName(api, role) {
  return `${pascal(api, '.')}${role}`;
}

// `playback:volumeChanged` → `PlaybackVolumeChangedPayload`.
export function eventTypeName(name) {
  return `${pascal(name, ':')}Payload`;
}

export function successTypeName(responseName) {
  return responseName.replace(/Response$/, 'Success');
}

export function emitTsResponse(method, interfaceName, used) {
  const successName = successTypeName(interfaceName);
  const result = method.result ?? { properties: [], additional: false };
  const lines = [...jsdoc(['Always `true` here; a failed call resolves with `ApiFailure` instead.'], INDENT), `${INDENT}success: true;`];
  const types = ['true'];
  let optional = false;
  // The bridge adds this after the handler returns, on the success response only.
  if (method.params.properties.some((p) => p.skipInvalid)) {
    lines.push(...jsdoc([SKIPPED_PATHS.description], INDENT), `${INDENT}${SKIPPED_PATHS.key}?: ${SKIPPED_PATHS.type};`);
    types.push(SKIPPED_PATHS.type);
    optional = true;
  }
  for (const p of result.properties) {
    const t = tsType(p, INDENT, used);
    const doc = [p.description, p.default !== undefined ? `@default ${JSON.stringify(p.default)}` : null];
    lines.push(...jsdoc(doc, INDENT), `${INDENT}${p.key}${p.required ? '' : '?'}: ${t};`);
    types.push(t);
    if (!p.required) optional = true;
  }
  if (result.additional) {
    // An index signature must admit every named member, `undefined` too when one is optional.
    const union = [...new Set([...types, tsType(result.additional, INDENT, used), ...(optional ? ['undefined'] : [])])];
    lines.push(...jsdoc([result.additional.description], INDENT), `${INDENT}[key: string]: ${union.join(' | ')};`);
  }
  return [
    ...jsdoc([`Successful response from \`${method.api}\`.`, ...lifecycleTags(method)], '', { block: true }),
    `export interface ${successName} {`, ...lines, '}',
    '',
    ...jsdoc([`Response from \`${method.api}\`: \`${successName}\`, or \`ApiFailure\` when the call failed.`, ...lifecycleTags(method)], '', { block: true }),
    `export type ${interfaceName} = ${successName} | ApiFailure;`,
  ].join('\n');
}

// The payload of a declared event, as its declared fields: an event carries no envelope. A
// payload that is a shared type as a whole is an alias of that type.
export function emitTsEventPayload(event, interfaceName, used) {
  const head = jsdoc([`Payload of the \`${event.name}\` event.`, event.description], '', { block: true });
  if (event.sharedPayload) {
    if (used) used.add(event.sharedPayload);
    return [...head, `export type ${interfaceName} = ${event.sharedPayload};`].join('\n');
  }
  if (!event.payload.properties.length && !event.payload.additional) {
    return [...head, `export type ${interfaceName} = Record<string, never>;`].join('\n');
  }
  return [...head, `export interface ${interfaceName} {`, ...members(event.payload, INDENT, { forceOptional: false }, used), '}'].join('\n');
}

// One named interface. A derived interface extends its base rather than repeating the
// members; an interface with extra keys carries the index signature over all its members.
function emitTsNamedType(obj, resolve) {
  const head = jsdoc([obj.typeDescription], '', { block: true });
  const extend = obj.extends ? ` extends ${obj.extends}` : '';
  const body = obj.additional
    ? members({ ...obj, properties: allProperties(obj, resolve) }, INDENT, { forceOptional: false }, null)
    : members(obj, INDENT, { forceOptional: false }, null);
  if (obj.additional && obj.extends) {
    // The index signature has to admit the inherited members, which the literal above lists.
    return [...head, `export interface ${obj.name} {`, ...body, '}'].join('\n');
  }
  return [...head, `export interface ${obj.name}${extend} {`, ...body, '}'].join('\n');
}

// Every named interface of every namespace, common.ts first, each exactly once. Two
// namespaces declaring different shapes under one name is an error: the SDK exports the
// names flat, so they have to be distinct.
export function emitTsNamedTypes(namespaces) {
  const common = namespaces.find((ns) => ns.namespace === COMMON_NAMESPACE);
  const others = namespaces.filter((ns) => ns !== common).sort((a, b) => a.namespace.localeCompare(b.namespace));
  const blocks = [];
  const owners = new Map();
  const seen = new Map();
  const add = (ns, obj) => {
    const shape = JSON.stringify([obj.properties, obj.additional, obj.extends ?? null, obj.partialOf ?? null]);
    if (seen.has(obj.name)) {
      if (seen.get(obj.name) !== shape) {
        throw new SchemaError(`${ns.file}: type ${obj.name} is also declared, with another shape, in ${owners.get(obj.name)}; the SDK exports type names flat, so rename one`);
      }
      return;
    }
    seen.set(obj.name, shape);
    owners.set(obj.name, ns.file);
    blocks.push(emitTsNamedType(obj, (name, isCommon) => findType(name, ns, namespaces, isCommon)));
  };
  if (common) for (const obj of common.types.values()) add(common, obj);
  for (const ns of others) {
    for (const obj of ns.types.values()) {
      if (obj.common) continue;
      add(ns, obj);
    }
  }
  return blocks;
}

// The names emitTsNamedTypes exports, so a file can import the ones its blocks use.
export function namedTypeNames(namespaces) {
  const names = new Set();
  for (const ns of namespaces) for (const obj of ns.types.values()) names.add(obj.name);
  return names;
}

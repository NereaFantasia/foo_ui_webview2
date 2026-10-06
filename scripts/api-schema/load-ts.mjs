// Reads API declarations written as TypeScript (src/api/schema/<namespace>.ts) and turns
// each file into the JSON Schema document a <namespace>.json would hold, so parseNamespace
// applies one set of rules to both. The TypeScript compiler resolves imports and type-checks
// the files first; descriptions and constraints come from JSDoc on the declarations:
//
//   /**
//    * English description.
//    * @zh 中文说明
//    * @minLength 1  @minimum 0  @maximum 100  @minItems 1   (@minimum/@maximum on an array
//      of numbers bound every element)
//    * @default "question"
//    * @security MediaRead
//    */
//
// A method's JSDoc may also say how the method affects its environment, which the MCP tools
// report as their annotations: `@effect read | write | destructive`, `@idempotent` (a repeated
// call with the same arguments has no further effect) and `@openWorld` (it reaches beyond the
// foobar2000 instance).
//
// A namespace file declares its methods as signatures on an exported `interface Api` and its
// events as properties on an exported `interface Events` (the property type is the payload,
// `void` when there is none, or an interface of common.ts when the event carries that shared
// type as a whole; `@delivery` says who receives it and `@customName` marks an event a
// subscriber may rename), either or both, and the namespace is the file name. `Int` from
// common.ts marks an integer, `Json` from common.ts any JSON value passed through unchanged,
// a union of string
// literals is an enum, `Record<string, T>` is a map, `A & Record<string, T>` is an object
// with named fields plus extra keys of type T, and `T | null` is a T that may also be null.
// A documented type alias lends its JSDoc to the places that use it. Anything else is an
// error rather than a silent approximation.
//
// Named interfaces are types in their own right: the document's `types` map holds every
// interface a namespace uses as a nested object (and, for common.ts, every interface it
// declares), so the emitters can share one struct or one TypeScript interface per name.
// `interface D extends B` keeps only D's own members under `properties` and names B in
// `x-extends`; `keyof B` is the enum of B's keys; `Partial<B>` is B with every member
// optional under the name `BPartial`; an interface that refers to itself inside an array
// is a recursive type, and the inner use becomes an `x-ref` node. Interfaces from common.ts
// carry `x-common: true` wherever a namespace uses them, so the emitters refer to the shared
// definition instead of making a copy.
import path from 'node:path';
import { createRequire } from 'node:module';

import { SchemaError } from './schema.mjs';

export const COMMON_FILE = 'common.ts';
export const COMMON_NAMESPACE = 'common';
const TAG_KEYS = {
  zh: 'x-description-zh',
  minLength: 'minLength',
  minimum: 'minimum',
  maximum: 'maximum',
  minItems: 'minItems',
  default: 'default',
  security: 'x-security',
  skipInvalid: 'x-skip-invalid',
  pathKey: 'x-path-key',
  delivery: 'x-delivery',
  customName: 'x-custom-name',
  effect: 'x-effect',
  idempotent: 'x-idempotent',
  openWorld: 'x-open-world',
  experimental: 'x-experimental',
  deprecated: 'x-deprecated',
  deprecatedZh: 'x-deprecated-zh',
};
// Tags that describe a method as a whole; they apply to nothing else.
const LIFECYCLE_TAGS = ['experimental', 'deprecated', 'deprecatedZh'];
const METHOD_TAGS = ['effect', 'idempotent', 'openWorld', ...LIFECYCLE_TAGS];

// The loader uses the compiler the SDK installs; it needs the classic JavaScript compiler
// API (TypeScript 5 / 6).
function loadTypeScript() {
  const require = createRequire(new URL('../../sdk/package.json', import.meta.url));
  try {
    return require('typescript');
  } catch {
    throw new SchemaError('reading src/api/schema/*.ts needs typescript from sdk/; run npm ci in sdk');
  }
}

function fail(where, msg) {
  throw new SchemaError(`${where}: ${msg}`);
}

// JSDoc wraps long lines. A wrap between two CJK characters (or full-width punctuation) joins
// without a space, as Chinese text is written; any other wrap becomes one space.
const CJK = /[　-〿㐀-鿿＀-￯]/;
const oneLine = (s) =>
  s
    .replace(/\s*\n\s*/g, (m, at, all) => (CJK.test(all[at - 1] ?? '') && CJK.test(all[at + m.length] ?? '') ? '' : ' '))
    .trim();

function readDoc(ts, node, where) {
  const docs = ts.getJSDocCommentsAndTags(node).filter((d) => ts.isJSDoc(d));
  const text = oneLine(docs.map((d) => ts.getTextOfJSDocComment(d.comment) ?? '').join(' '));
  const tags = {};
  for (const d of docs) {
    for (const tag of d.tags ?? []) {
      const name = tag.tagName.text;
      if (!(name in TAG_KEYS)) fail(where, `unsupported JSDoc tag @${name}`);
      if (name in tags) fail(where, `@${name} appears twice`);
      tags[name] = oneLine(ts.getTextOfJSDocComment(tag.comment) ?? '');
    }
  }
  return { text, tags };
}

// Unused declarations and containers never reach type conversion, but their lifecycle
// tags must still be rejected. Inspect each node's own docs, not inherited JSDoc.
function checkLifecyclePositions(ts, sourceFile, rel) {
  const checked = new Set();
  const checkDoc = (doc, method, docSource = sourceFile, offset = 0) => {
    for (const tag of doc.tags ?? []) {
      if (LIFECYCLE_TAGS.includes(tag.tagName.text) && !method) {
        const { line } = sourceFile.getLineAndCharacterOfPosition(offset + tag.getStart(docSource));
        fail(`${rel}:${line + 1}`, `@${tag.tagName.text} applies to an exported Api method only`);
      }
    }
  };
  const visit = (node, parent) => {
    const method = ts.isMethodSignature(node) && parent && ts.isInterfaceDeclaration(parent)
      && parent.name.text === 'Api' && parent.modifiers?.some((m) => m.kind === ts.SyntaxKind.ExportKeyword)
      && parent.parent === sourceFile && path.basename(rel) !== COMMON_FILE;
    for (const doc of node.jsDoc ?? []) {
      checked.add(doc.pos);
      checkDoc(doc, method);
    }
    ts.forEachChild(node, (child) => visit(child, node));
  };
  visit(sourceFile, null);

  // Type-position comments may have no jsDoc owner. Token trivia ranges include them
  // without mistaking strings, regular expressions or template text for comments.
  const visitTrivia = (node) => {
    if (ts.isJSDoc(node)) return;
    if (!ts.isToken(node)) {
      for (const child of node.getChildren(sourceFile)) visitTrivia(child);
      return;
    }
    const ranges = [
      ...(ts.getLeadingCommentRanges(sourceFile.text, node.pos) ?? []),
      ...(ts.getTrailingCommentRanges(sourceFile.text, node.pos) ?? []),
    ];
    for (const range of ranges) {
      if (checked.has(range.pos) || range.kind !== ts.SyntaxKind.MultiLineCommentTrivia) continue;
      checked.add(range.pos);
      const comment = sourceFile.text.slice(range.pos, range.end);
      if (!comment.startsWith('/**')) continue;
      const parsed = ts.createSourceFile(rel, `${comment}\ninterface Doc {}`, ts.ScriptTarget.Latest, true);
      for (const doc of parsed.statements[0].jsDoc ?? []) checkDoc(doc, false, parsed, range.pos);
    }
  };
  visitTrivia(sourceFile);
}

// Copies a declaration's JSDoc onto a schema node, converting tag values to JSON types.
function applyDoc(schema, doc, where) {
  if (doc.text) schema.description = doc.text;
  for (const [name, raw] of Object.entries(doc.tags)) {
    if (METHOD_TAGS.includes(name)) fail(where, `@${name} applies to a method only`);
    let value = raw;
    if (['minLength', 'minimum', 'maximum', 'minItems'].includes(name)) {
      value = Number(raw);
      if (raw === '' || !Number.isFinite(value)) fail(where, `@${name} needs a number`);
    } else if (name === 'default') {
      try {
        value = JSON.parse(raw);
      } catch {
        fail(where, `@default needs a JSON value (strings in double quotes), got ${raw}`);
      }
    } else if (name === 'skipInvalid') {
      if (raw) fail(where, '@skipInvalid takes no value');
      value = true;
    } else if (name === 'pathKey' && raw.includes('=')) {
      // `member=Level` pairs: each named member of the array's objects is checked at its own
      // level, in this order, and the array then carries no @security of its own.
      value = {};
      for (const pair of raw.split(/\s+/)) {
        const m = /^([A-Za-z_][A-Za-z0-9_]*)=([A-Za-z]+)$/.exec(pair);
        if (!m) fail(where, `@pathKey takes one member or "member=Level" pairs separated by spaces, got "${pair}"`);
        if (m[1] in value) fail(where, `@pathKey names "${m[1]}" twice`);
        value[m[1]] = m[2];
      }
    } else if (!raw) {
      fail(where, `@${name} needs a value`);
    }
    schema[TAG_KEYS[name]] = value;
  }
  // On an array of numbers, @minimum and @maximum bound every element.
  if (schema.type === 'array' && (schema.minimum !== undefined || schema.maximum !== undefined)) {
    const kind = schema.items?.type;
    if (kind !== 'integer' && kind !== 'number') fail(where, '@minimum and @maximum on an array need number elements');
    schema.items = { ...schema.items };
    for (const k of ['minimum', 'maximum']) {
      if (schema[k] === undefined) continue;
      schema.items[k] = schema[k];
      delete schema[k];
    }
  }
  return schema;
}

const clone = (v) => JSON.parse(JSON.stringify(v));

function createConverter(ts, program, repoRoot) {
  const checker = program.getTypeChecker();
  const commonAbs = path.join(repoRoot, 'src/api/schema', COMMON_FILE);
  const isCommonDecl = (decl) => path.resolve(decl.getSourceFile().fileName) === commonAbs;

  // Per file: the named types it uses, and the interfaces currently being converted (for
  // recursion). Reset by convertFile.
  let types = {};
  let converting = new Set();

  function declarationOf(typeName, where) {
    let symbol = checker.getSymbolAtLocation(typeName);
    if (symbol && symbol.flags & ts.SymbolFlags.Alias) symbol = checker.getAliasedSymbol(symbol);
    const decl = symbol?.declarations?.[0];
    if (!decl) fail(where, `cannot resolve ${typeName.getText()}`);
    return decl;
  }

  // `Int` and `Json` count only when they are the aliases from common.ts, not a same-named local.
  function isCommonAlias(decl, name) {
    return ts.isTypeAliasDeclaration(decl) && decl.name.text === name && isCommonDecl(decl);
  }

  function objectFromMembers(members, where) {
    const out = { type: 'object', properties: {}, required: [], additionalProperties: false };
    for (const m of members) {
      if (ts.isPropertySignature(m)) {
        const key = m.name.getText();
        const w = `${where}.${key}`;
        if (!m.type) fail(w, 'needs a type');
        out.properties[key] = applyDoc(typeToSchema(m.type, w), readDoc(ts, m, w), w);
        if (!m.questionToken) out.required.push(key);
      } else {
        fail(where, `only property signatures are supported in object types (${ts.SyntaxKind[m.kind]})`);
      }
    }
    if (!out.required.length) delete out.required;
    if (!Object.keys(out.properties).length) delete out.properties;
    return out;
  }

  function recordValue(node, where) {
    const [keyType, valueType] = node.typeArguments ?? [];
    if (!keyType || keyType.kind !== ts.SyntaxKind.StringKeyword || !valueType) fail(where, 'only Record<string, T> is supported');
    return typeToSchema(valueType, `${where}[value]`);
  }

  // The interface a heritage clause or a type argument names, with the plain-interface rules.
  // A heritage clause holds an expression (`extends Base`), a type argument a type reference.
  function interfaceNamed(node, where, what) {
    const nameNode = ts.isExpressionWithTypeArguments(node) ? node.expression : ts.isTypeReferenceNode(node) ? node.typeName : null;
    if (!nameNode || node.typeArguments?.length) fail(where, `${what} must name an interface`);
    const decl = declarationOf(nameNode, where);
    if (!ts.isInterfaceDeclaration(decl)) fail(where, `${what} must name an interface`);
    if (decl.typeParameters?.length) fail(where, `interface ${decl.name.text} cannot take type parameters`);
    return decl;
  }

  // The keys of an interface, base interfaces first, as `keyof` and `Partial` see them.
  function keysOf(decl, where) {
    const base = baseOf(decl, where);
    const own = decl.members.map((m) => (ts.isPropertySignature(m) ? m.name.getText() : fail(where, 'only property signatures are supported')));
    return [...(base ? keysOf(base, where) : []), ...own];
  }

  function baseOf(decl, where) {
    const clauses = decl.heritageClauses ?? [];
    if (!clauses.length) return null;
    if (clauses.length > 1 || clauses[0].types.length !== 1) fail(where, `interface ${decl.name.text} can extend one interface only`);
    return interfaceNamed(clauses[0].types[0], `${where}<${decl.name.text}>`, 'the extended type');
  }

  // Converts a named interface once per file and records it under `types`. A use inside its
  // own conversion (through an array) becomes a reference node instead of an endless copy.
  function namedInterface(decl, where) {
    const name = decl.name.text;
    const common = isCommonDecl(decl);
    if (converting.has(name)) return { type: 'object', 'x-ref': name, ...(common ? { 'x-common': true } : {}) };
    if (!types[name]) {
      converting.add(name);
      const w = `${where}<${name}>`;
      const base = baseOf(decl, where);
      const schema = objectFromMembers(decl.members, w);
      if (base) {
        const baseSchema = namedInterface(base, where);
        if (baseSchema['x-ref']) fail(w, `interface ${name} cannot extend ${base.name.text} while ${base.name.text} is being declared`);
        if (baseSchema.additionalProperties !== false) fail(w, `interface ${name} cannot extend ${base.name.text}, which has extra keys`);
        for (const key of Object.keys(schema.properties ?? {})) {
          if (key in (baseSchema.properties ?? {})) fail(w, `${key} is already declared by ${base.name.text}`);
        }
        schema['x-extends'] = base.name.text;
        if (isCommonDecl(base)) schema['x-extends-common'] = true;
      }
      const doc = readDoc(ts, decl, w);
      for (const tag of Object.keys(doc.tags)) if (tag !== 'zh') fail(w, `@${tag} does not apply to an interface`);
      if (doc.text) schema['x-type-description'] = doc.text;
      if (doc.tags.zh) schema['x-type-description-zh'] = doc.tags.zh;
      schema['x-name'] = name;
      if (common) schema['x-common'] = true;
      converting.delete(name);
      types[name] = schema;
    }
    return clone(types[name]);
  }

  // `Partial<B>`: B's members, base members included, all optional, as the named type BPartial.
  // The Partial of a common.ts interface is itself a common type, so common.ts has to declare
  // it (`export type BPartial = Partial<B>;`) for the shared definition to exist.
  function partialOf(node, where) {
    const [arg] = node.typeArguments ?? [];
    if (!arg || node.typeArguments.length !== 1) fail(where, 'Partial takes one interface');
    const decl = interfaceNamed(arg, where, 'Partial');
    const name = `${decl.name.text}Partial`;
    if (!types[name]) {
      const full = namedInterface(decl, where);
      if (full['x-ref']) fail(where, `Partial<${decl.name.text}> cannot be used while ${decl.name.text} is being declared`);
      if (full['x-common'] && !commonDeclaresAlias(name)) {
        fail(where, `Partial<${decl.name.text}> of a common.ts interface needs \`export type ${name} = Partial<${decl.name.text}>;\` in common.ts`);
      }
      const properties = {};
      for (const d of chainOf(full)) Object.assign(properties, d.properties ?? {});
      const schema = { type: 'object', properties, additionalProperties: false, 'x-name': name, 'x-partial-of': decl.name.text };
      if (full['x-type-description']) schema['x-type-description'] = full['x-type-description'];
      if (full['x-type-description-zh']) schema['x-type-description-zh'] = full['x-type-description-zh'];
      if (full['x-common']) schema['x-common'] = true;
      types[name] = schema;
    }
    return clone(types[name]);
  }

  function commonDeclaresAlias(name) {
    const common = program.getSourceFile(commonAbs) ?? program.getSourceFile(commonAbs.split(path.sep).join('/'));
    return Boolean(common?.statements.some((s) => ts.isTypeAliasDeclaration(s) && s.name.text === name));
  }

  const isPartialAlias = (decl) => ts.isTypeAliasDeclaration(decl) && ts.isTypeReferenceNode(decl.type) && decl.type.typeName.getText() === 'Partial';

  // `type BPartial = Partial<B>`: the alias has to carry the name the Partial gets. Its own
  // JSDoc, when it has one, describes the Partial instead of B's description.
  function partialAlias(decl, where) {
    const [arg] = decl.type.typeArguments ?? [];
    if (!arg || decl.type.typeArguments.length !== 1) fail(where, 'Partial takes one interface');
    const target = interfaceNamed(arg, where, 'Partial');
    if (decl.name.text !== `${target.name.text}Partial`) fail(where, `the Partial of ${target.name.text} must be named ${target.name.text}Partial`);
    const schema = partialOf(decl.type, where);
    const doc = readDoc(ts, decl, where);
    for (const tag of Object.keys(doc.tags)) if (tag !== 'zh') fail(where, `@${tag} does not apply to a type alias of Partial`);
    if (doc.text) types[schema['x-name']]['x-type-description'] = doc.text;
    if (doc.tags.zh) types[schema['x-name']]['x-type-description-zh'] = doc.tags.zh;
    return clone(types[schema['x-name']]);
  }

  // The interface and its bases, base first, all of which `types` holds by then.
  function chainOf(schema) {
    const chain = [schema];
    for (let s = schema; s['x-extends']; s = types[s['x-extends']]) chain.unshift(types[s['x-extends']]);
    return chain;
  }

  function typeToSchema(node, where) {
    const K = ts.SyntaxKind;
    switch (node.kind) {
      case K.StringKeyword:
        return { type: 'string' };
      case K.NumberKeyword:
        return { type: 'number' };
      case K.BooleanKeyword:
        return { type: 'boolean' };
      case K.ParenthesizedType:
        return typeToSchema(node.type, where);
      case K.LiteralType:
        if (node.literal.kind === K.TrueKeyword || node.literal.kind === K.FalseKeyword) return { type: 'boolean' };
        if (ts.isStringLiteral(node.literal)) return { type: 'string', enum: [node.literal.text] };
        return fail(where, `unsupported literal type ${node.getText()}`);
      case K.TypeOperator: {
        if (node.operator !== K.KeyOfKeyword) fail(where, `unsupported type ${node.getText()}`);
        const decl = interfaceNamed(node.type, where, 'keyof');
        return { type: 'string', enum: keysOf(decl, where) };
      }
      case K.UnionType: {
        // `T | null` marks a value that may be null; what remains is converted on its own.
        const isNull = (t) => t.kind === K.NullKeyword || (ts.isLiteralTypeNode(t) && t.literal.kind === K.NullKeyword);
        const rest = node.types.filter((t) => !isNull(t));
        if (rest.length < node.types.length) {
          if (rest.length !== 1 && !rest.every((t) => ts.isLiteralTypeNode(t) && ts.isStringLiteral(t.literal))) {
            fail(where, `only T | null or a union of string literals | null is supported (${node.getText()})`);
          }
          const inner = rest.length === 1 ? typeToSchema(rest[0], where) : { type: 'string', enum: rest.map((t) => t.literal.text) };
          return { ...inner, nullable: true };
        }
        const values = node.types.map((t) => (ts.isLiteralTypeNode(t) && ts.isStringLiteral(t.literal) ? t.literal.text : null));
        if (values.includes(null)) fail(where, `only unions of string literals are supported (${node.getText()})`);
        return { type: 'string', enum: values };
      }
      case K.ArrayType:
        return { type: 'array', items: typeToSchema(node.elementType, `${where}[]`) };
      case K.TypeLiteral:
        return objectFromMembers(node.members, where);
      case K.IntersectionType: {
        // Named fields plus extra keys: exactly one object part and one Record<string, T>.
        const records = node.types.filter((t) => ts.isTypeReferenceNode(t) && t.typeName.getText() === 'Record');
        const others = node.types.filter((t) => !records.includes(t));
        if (records.length !== 1 || others.length !== 1) fail(where, 'an intersection must be one object type & Record<string, T>');
        const obj = typeToSchema(others[0], where);
        if (obj.type !== 'object' || obj['x-ref'] || obj.additionalProperties !== false) fail(where, 'the first part of the intersection must be a plain object type');
        // The shared definition has no extra keys; a namespace that wants them declares its
        // own interface extending the common one and intersects that.
        if (obj['x-common']) fail(where, `${obj['x-name']} from common.ts cannot take extra keys here; extend it in a namespace interface first`);
        obj.additionalProperties = recordValue(records[0], where);
        return obj;
      }
      case K.TypeReference: {
        const name = node.typeName.getText();
        if (name === 'Record') return { type: 'object', additionalProperties: recordValue(node, where) };
        if (name === 'Array') {
          const [item] = node.typeArguments ?? [];
          if (!item) fail(where, 'Array needs an element type');
          return { type: 'array', items: typeToSchema(item, `${where}[]`) };
        }
        if (name === 'Partial') return partialOf(node, where);
        if (node.typeArguments?.length) fail(where, `generic type ${name} is not supported`);
        const decl = declarationOf(node.typeName, where);
        if (isCommonAlias(decl, 'Int')) return { type: 'integer' };
        if (isCommonAlias(decl, 'Json')) return { type: 'json' };
        if (ts.isInterfaceDeclaration(decl)) {
          if (decl.typeParameters?.length) fail(where, `interface ${name} cannot take type parameters`);
          return namedInterface(decl, where);
        }
        if (ts.isTypeAliasDeclaration(decl)) {
          if (decl.typeParameters?.length) fail(where, `generic alias ${name} is not supported`);
          // `type BPartial = Partial<B>` is the declaration of the named type BPartial itself.
          if (isPartialAlias(decl)) return partialAlias(decl, `${where}<${name}>`);
          const schema = typeToSchema(decl.type, `${where}<${name}>`);
          delete schema['x-name'];
          delete schema['x-common'];
          delete schema['x-partial-of'];
          // A documented alias describes whatever uses it; the use site can still override.
          return applyDoc(schema, readDoc(ts, decl, `${where}<${name}>`), where);
        }
        return fail(where, `${name} must be an interface or a type alias`);
      }
      default:
        return fail(where, `unsupported type ${node.getText()}`);
    }
  }

  // Top-level params / result: an object type whose declaration name is not kept.
  function topObject(node, where) {
    const schema = typeToSchema(node, where);
    if (schema.type !== 'object' || schema['x-ref']) fail(where, 'must be an object type');
    if (schema['x-extends'] || schema['x-partial-of']) fail(where, 'a params or result type cannot extend another interface or be a Partial; nest the shared type in a field instead');
    if (schema['x-common']) fail(where, 'a params or result type must be declared in the namespace file, not in common.ts');
    const name = schema['x-name'];
    delete schema['x-name'];
    delete schema['x-common'];
    delete schema.description;
    delete schema['x-type-description'];
    delete schema['x-type-description-zh'];
    return { schema, name };
  }

  // An event payload that is an interface of common.ts as a whole: the event carries the shared
  // type itself, so its name is kept rather than its members copied into the event.
  function sharedPayload(node, where) {
    if (!ts.isTypeReferenceNode(node) || node.typeArguments?.length) return null;
    const decl = declarationOf(node.typeName, where);
    if (!ts.isInterfaceDeclaration(decl) || !isCommonDecl(decl)) return null;
    namedInterface(decl, where);
    return decl.name.text;
  }

  // common.ts declares no methods: every interface in it is a shared type, and an alias of the
  // form `type BPartial = Partial<B>` declares the shared Partial. Other aliases (Int, Json,
  // enums) are read where they are used.
  function convertCommon(sourceFile) {
    types = {};
    converting = new Set();
    for (const s of sourceFile.statements) {
      if (ts.isInterfaceDeclaration(s)) namedInterface(s, `${COMMON_FILE}#${s.name.text}`);
      if (isPartialAlias(s)) partialAlias(s, `${COMMON_FILE}#${s.name.text}`);
    }
    return { namespace: COMMON_NAMESPACE, methods: {}, types };
  }

  return function convertFile(sourceFile, rel) {
    checkLifecyclePositions(ts, sourceFile, rel);
    if (path.basename(rel) === COMMON_FILE) return convertCommon(sourceFile);
    types = {};
    converting = new Set();
    const namespace = path.basename(rel, '.ts');
    const find = (name) => {
      const declarations = sourceFile.statements.filter((s) => ts.isInterfaceDeclaration(s) && s.name.text === name);
      if (declarations.length > 1) fail(rel, `interface ${name} is declared twice; combine its members into one declaration`);
      return declarations[0];
    };
    const exported = (d) => d.modifiers?.some((m) => m.kind === ts.SyntaxKind.ExportKeyword);
    const api = find('Api');
    const eventsDecl = find('Events');
    if (!api && !eventsDecl) fail(rel, 'must export `interface Api` listing the methods, `interface Events` listing the events, or both');
    if (api && !exported(api)) fail(rel, 'must export `interface Api`; it is declared without export');
    if (eventsDecl && !exported(eventsDecl)) fail(rel, 'must export `interface Events`; it is declared without export');
    const methods = {};
    const topLevel = new Set();
    for (const m of api?.members ?? []) {
      if (!ts.isMethodSignature(m)) fail(`${rel}#Api`, 'members must be method signatures, e.g. eval(params: EvalParams): EvalResult');
      const name = m.name.getText();
      const where = `${rel}#${name}`;
      const doc = readDoc(ts, m, where);
      for (const tag of Object.keys(doc.tags)) if (tag !== 'zh' && !METHOD_TAGS.includes(tag)) fail(where, `@${tag} does not apply to a method`);
      const method = {};
      if (doc.text) method.description = doc.text;
      if (doc.tags.zh) method['x-description-zh'] = doc.tags.zh;
      if (doc.tags.effect !== undefined) {
        if (!doc.tags.effect) fail(where, '@effect needs a value: read, write or destructive');
        method['x-effect'] = doc.tags.effect;
      }
      for (const flag of ['idempotent', 'openWorld', 'experimental']) {
        if (doc.tags[flag] === undefined) continue;
        if (doc.tags[flag]) fail(where, `@${flag} takes no value`);
        method[TAG_KEYS[flag]] = true;
      }
      for (const tag of ['deprecated', 'deprecatedZh']) {
        if (doc.tags[tag] === undefined) continue;
        if (!doc.tags[tag]) fail(where, `@${tag} needs a value`);
        method[TAG_KEYS[tag]] = doc.tags[tag];
      }
      if (m.parameters.length > 1) fail(where, 'takes at most one parameter object');
      if (m.parameters.length === 1) {
        const p = m.parameters[0];
        if (!p.type || p.questionToken) fail(where, 'the parameter object must be typed and not optional; omit it when the method takes none');
        const top = topObject(p.type, `${where}.params`);
        method.params = top.schema;
        if (top.name) topLevel.add(top.name);
      }
      if (!m.type) fail(where, 'needs a return type (void when it returns nothing but success)');
      if (m.type.kind !== ts.SyntaxKind.VoidKeyword) {
        const top = topObject(m.type, `${where}.result`);
        method.result = top.schema;
        if (top.name) topLevel.add(top.name);
      }
      if (name in methods) fail(where, 'declared twice');
      methods[name] = method;
    }
    // An event is a property whose type is its payload; the event name is `<namespace>:<key>`.
    const events = {};
    for (const e of eventsDecl?.members ?? []) {
      if (!ts.isPropertySignature(e)) fail(`${rel}#Events`, 'members must be properties, e.g. trackChanged: TrackChangedPayload');
      const key = e.name.getText();
      const where = `${rel}#Events.${key}`;
      const doc = readDoc(ts, e, where);
      for (const tag of Object.keys(doc.tags)) if (!['zh', 'delivery', 'customName'].includes(tag)) fail(where, `@${tag} does not apply to an event`);
      const event = {};
      if (doc.text) event.description = doc.text;
      if (doc.tags.zh) event['x-description-zh'] = doc.tags.zh;
      if (doc.tags.delivery !== undefined) event['x-delivery'] = doc.tags.delivery;
      if (doc.tags.customName !== undefined) {
        if (doc.tags.customName) fail(where, '@customName takes no value');
        event['x-custom-name'] = true;
      }
      if (e.questionToken) fail(where, 'an event cannot be optional');
      if (!e.type) fail(where, 'needs a payload type (void when the event carries no fields)');
      if (e.type.kind !== ts.SyntaxKind.VoidKeyword) {
        const shared = sharedPayload(e.type, `${where}.payload`);
        if (shared) {
          event['x-payload-type'] = shared;
        } else {
          const top = topObject(e.type, `${where}.payload`);
          event.payload = top.schema;
          if (top.name) topLevel.add(top.name);
        }
      }
      if (key in events) fail(where, 'declared twice');
      events[key] = event;
    }
    // A params, result or payload interface is its method's or event's own shape, not a
    // shared type. The C++ struct carries the method's or event's name, so a recursive use of
    // the interface has no struct to point at; used as a nested type, a base or a Partial
    // elsewhere it stays registered and gets a second struct under its own name.
    const roots = [...Object.values(methods).flatMap((m) => [m.params, m.result]), ...Object.values(events).map((e) => e.payload)];
    for (const name of topLevel) {
      if (usedNested(roots, name, ['x-ref'])) fail(`${rel}#${name}`, `a method's params or result interface cannot be used recursively, nor an event's payload; move the recursive part into its own interface`);
      if (types[name] && !usedNested(roots, name, ['x-name', 'x-extends', 'x-partial-of'])) delete types[name];
    }
    return { namespace, methods, types, ...(eventsDecl ? { events } : {}) };
  };

  // Whether any nested node below the roots names the interface through one of the keys.
  function usedNested(roots, name, keys) {
    const stack = [...roots];
    while (stack.length) {
      const n = stack.pop();
      if (!n || typeof n !== 'object') continue;
      for (const child of [...Object.values(n.properties ?? {}), n.items, n.additionalProperties]) {
        if (!child || typeof child !== 'object') continue;
        if (keys.some((k) => child[k] === name)) return true;
        stack.push(child);
      }
    }
    return false;
  }
}

// Returns [{ file, json }] for the given repo-relative .ts files, in the same order.
export function readTsSchemas(repoRoot, relFiles) {
  if (!relFiles.length) return [];
  const ts = loadTypeScript();
  const configPath = path.join(repoRoot, 'src/api/schema/tsconfig.json');
  const config = ts.readConfigFile(configPath, ts.sys.readFile);
  if (config.error) throw new SchemaError(ts.flattenDiagnosticMessageText(config.error.messageText, '\n'));
  const parsed = ts.parseJsonConfigFileContent(config.config, ts.sys, path.dirname(configPath));
  const program = ts.createProgram(parsed.fileNames, { ...parsed.options, noEmit: true });
  const diagnostics = ts.getPreEmitDiagnostics(program);
  if (diagnostics.length) {
    const text = ts.formatDiagnostics(diagnostics, {
      getCanonicalFileName: (f) => f,
      getCurrentDirectory: () => repoRoot,
      getNewLine: () => '\n',
    });
    throw new SchemaError(`src/api/schema does not type-check:\n${text}`);
  }
  const convert = createConverter(ts, program, repoRoot);
  return relFiles.map((rel) => {
    const sf = program.getSourceFile(path.join(repoRoot, rel)) ?? program.getSourceFile(path.join(repoRoot, rel).split(path.sep).join('/'));
    if (!sf) throw new SchemaError(`${rel}: not part of src/api/schema/tsconfig.json`);
    return { file: rel, json: convert(sf, rel) };
  });
}

// scripts/__tests__/api_schema.test.mjs
//
// The schema generators (scripts/api-schema/) promise that a schema either produces
// code that enforces it or is rejected. These tests pin the rejections, the generated
// C++ / TS shapes for each kind of property, the docs-region bookkeeping, and the
// checks that tie a declared method to its registration in the code index, and the
// TypeScript front end that turns src/api/schema/*.ts into the same schema documents.

import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

import { allProperties, findType, loadNamespaces, parseNamespace, SchemaError } from '../api-schema/schema.mjs';
import { cppNamespace, emitCppHeader, emitEventRegistry, hasCppHeader } from '../api-schema/emit-cpp.mjs';
import { emitTsEventPayload, emitTsNamedTypes, emitTsParams, emitTsResponse } from '../api-schema/emit-ts.mjs';
import { planDocs, renderEventRegion, renderRegion } from '../api-schema/emit-docs.mjs';
import { checkRegistrations } from '../api-schema/registrations.mjs';
import { checkEmits } from '../api-schema/emits.mjs';

const FILE = 'src/api/schema/demo.json';

function ns(methods) {
  return parseNamespace({ namespace: 'demo', methods }, FILE);
}

function method(params, result) {
  return { description: 'Demo method.', params, ...(result ? { result } : {}) };
}

const params = (properties, required = []) => ({ type: 'object', additionalProperties: false, required, properties });
// An array of { source, destination } objects, the shape whose members take different levels.
const PAIR_ITEMS = { type: 'array', items: { type: 'object', additionalProperties: false, required: ['source', 'destination'], properties: { source: { type: 'string', description: 'x' }, destination: { type: 'string', description: 'x' } } }, description: 'x' };

describe('api-schema · JSON method lifecycle', () => {
  const tags = { 'x-experimental': true, 'x-deprecated': 'Use stable.', 'x-deprecated-zh': '请改用 stable。' };
  test('experimental and deprecated methods can be marked independently', () => {
    const [experimental, deprecated] = ns({
      preview: { ...method(), 'x-experimental': true },
      old: { ...method(), 'x-deprecated': 'Use stable.', 'x-deprecated-zh': '请改用 stable。' },
    }).methods;
    assert.deepEqual([experimental.experimental, experimental.deprecated, experimental.deprecatedZh], [true, null, null]);
    assert.deepEqual([deprecated.experimental, deprecated.deprecated, deprecated.deprecatedZh], [false, 'Use stable.', '请改用 stable。']);
    assert.match(renderRegion(experimental, 'en'), /^Experimental API; it may change in future releases\.\n\nDemo method\./);
    assert.match(renderRegion(deprecated, 'zh'), /^已弃用：请改用 stable。\n\nDemo method\./);
    assert.doesNotMatch(emitTsParams(experimental, 'DemoPreviewParams'), /@deprecated/);
    assert.doesNotMatch(emitTsResponse(deprecated, 'DemoOldResponse'), /@experimental/);
  });
  test('normalizes lifecycle fields and defaults without adding blank lines to plain docs', () => {
    const [marked, plain] = ns({ marked: { ...method(), ...tags }, plain: method() }).methods;
    assert.deepEqual([marked.experimental, marked.deprecated, marked.deprecatedZh], [true, 'Use stable.', '请改用 stable。']);
    assert.deepEqual([plain.experimental, plain.deprecated, plain.deprecatedZh], [false, null, null]);
    assert.equal(renderRegion(plain, 'en'), 'Demo method.\n\nThis method takes no parameters.\n\n**Returns**\n\n`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.');
    assert.equal(emitTsParams(plain, 'DemoPlainParams'), '/**\n * Parameters for `demo.plain`.\n * Demo method.\n */\nexport type DemoPlainParams = Record<string, never>;');
  });
  for (const invalid of [
    { 'x-experimental': false }, { 'x-experimental': 'true' }, { 'x-experimental': null },
    { 'x-deprecated': '' }, { 'x-deprecated': ' ' }, { 'x-deprecated': true },
    { 'x-deprecated': null }, { 'x-deprecated': 'Use stable.' },
    { 'x-deprecated-zh': '请改用 stable。' },
    { ...tags, 'x-deprecated-zh': '' }, { ...tags, 'x-deprecated-zh': ' ' },
    { ...tags, 'x-deprecated-zh': false }, { ...tags, 'x-deprecated-zh': null },
  ]) {
    test(`rejects invalid lifecycle values ${JSON.stringify(invalid)}`, () => {
      assert.throws(() => ns({ run: { ...method(), ...invalid } }), SchemaError);
    });
  }
  for (const [key, value] of Object.entries(tags)) {
    const marker = { [key]: value };
    const object = params({ value: { type: 'string', description: 'Value.' } });
    for (const [place, document] of Object.entries({
      namespace: { namespace: 'demo', methods: { run: method() }, ...marker },
      type: { namespace: 'demo', methods: { run: method() }, types: { Row: { ...object, ...marker } } },
      event: { namespace: 'demo', events: { changed: { description: 'Changed.', 'x-delivery': 'caller', ...marker } } },
      params: { namespace: 'demo', methods: { run: method({ ...object, ...marker }) } },
      result: { namespace: 'demo', methods: { run: method(undefined, { ...object, ...marker }) } },
      property: { namespace: 'demo', methods: { run: method(params({ value: { type: 'string', description: 'Value.', ...marker } })) } },
    })) {
      test(`rejects ${key} on JSON ${place}`, () => assert.throws(() => parseNamespace(document, FILE), (e) => e instanceof SchemaError && e.message.includes(`unsupported keyword "${key}"`)));
    }
  }
});

describe('api-schema · rejects what the generators cannot enforce', () => {
  const rejects = (label, methods, pattern) =>
    test(label, () => assert.throws(() => ns(methods), (e) => e instanceof SchemaError && pattern.test(e.message)));

  rejects('an unsupported keyword', { run: method(params({ id: { type: 'string', pattern: '^a', description: 'x' } })) }, /unsupported keyword "pattern"/);
  rejects('params without additionalProperties: false', { run: method({ type: 'object', additionalProperties: { type: 'string' }, properties: { a: { type: 'string', description: 'x' } } }) }, /additionalProperties/);
  rejects('a required key that is not a property', { run: method(params({ a: { type: 'string', description: 'x' } }, ['b'])) }, /names "b"/);
  rejects('x-security on a result field', { run: method(undefined, { type: 'object', additionalProperties: false, properties: { p: { type: 'string', 'x-security': 'Read', description: 'x' } } }) }, /only allowed on params/);
  rejects('x-security on a number', { run: method(params({ n: { type: 'integer', 'x-security': 'Read', description: 'x' } })) }, /unsupported keyword "x-security"/);
  rejects('x-skip-invalid without x-security', { run: method(params({ paths: { type: 'array', items: { type: 'string' }, 'x-skip-invalid': true, description: 'x' } })) }, /needs "x-security" on an array of paths/);
  rejects('x-skip-invalid on a string', { run: method(params({ path: { type: 'string', 'x-security': 'Read', 'x-skip-invalid': true, description: 'x' } })) }, /unsupported keyword "x-skip-invalid"/);
  rejects('x-security on an array of objects without x-path-key', { run: method(params({ items: { type: 'array', items: { type: 'object', additionalProperties: false, required: ['path'], properties: { path: { type: 'string', description: 'x' } } }, 'x-security': 'Read', description: 'x' } })) }, /needs "x-path-key"/);
  rejects('x-path-key naming a member that is not a required string', { run: method(params({ items: { type: 'array', items: { type: 'object', additionalProperties: false, required: [], properties: { path: { type: 'string', description: 'x' } } }, 'x-security': 'Read', 'x-path-key': 'path', description: 'x' } })) }, /must name a required string member/);
  rejects('x-path-key on an array of strings', { run: method(params({ paths: { type: 'array', items: { type: 'string' }, 'x-security': 'Read', 'x-path-key': 'path', description: 'x' } })) }, /needs an array of objects/);
  rejects('x-security on an array of Json without x-path-key', { run: method(params({ handles: { type: 'array', items: { type: 'json' }, 'x-security': 'Read', description: 'x' } })) }, /array of Json needs "x-path-key"/);
  rejects('x-path-key levels on an array of Json', { run: method(params({ handles: { type: 'array', items: { type: 'json' }, 'x-path-key': { path: 'Read' }, description: 'x' } })) }, /needs an array of objects/);
  rejects('x-path-key without x-security', { run: method(params({ items: { type: 'array', items: { type: 'object', additionalProperties: false, required: ['path'], properties: { path: { type: 'string', description: 'x' } } }, 'x-path-key': 'path', description: 'x' } })) }, /needs "x-security" on an array of objects/);
  rejects('x-skip-invalid together with x-path-key', { run: method(params({ items: { type: 'array', items: { type: 'object', additionalProperties: false, required: ['path'], properties: { path: { type: 'string', description: 'x' } } }, 'x-security': 'Read', 'x-path-key': 'path', 'x-skip-invalid': true, description: 'x' } })) }, /not supported with "x-path-key"/);
  rejects('x-path-key levels together with x-security', { run: method(params({ items: { ...PAIR_ITEMS, 'x-security': 'Read', 'x-path-key': { source: 'Read', destination: 'FileWrite' } } })) }, /replaces "x-security"/);
  rejects('x-path-key with an unknown level', { run: method(params({ items: { ...PAIR_ITEMS, 'x-path-key': { source: 'Root' } } })) }, /must be one of/);
  rejects('x-path-key levels naming an optional member', { run: method(params({ items: { ...PAIR_ITEMS, items: { ...PAIR_ITEMS.items, required: ['source'] }, 'x-path-key': { source: 'Read', destination: 'FileWrite' } } })) }, /must name a required string member of the items, got "destination"/);
  rejects('x-path-key levels on an array of strings', { run: method(params({ paths: { type: 'array', items: { type: 'string' }, 'x-path-key': { path: 'Read' }, description: 'x' } })) }, /needs an array of objects/);
  rejects('x-path-key levels on a result field', { run: method(undefined, { type: 'object', additionalProperties: false, properties: { items: { ...PAIR_ITEMS, 'x-path-key': { source: 'Read' } } } }) }, /only allowed on params/);
  // Nesting is caught where the path entries are emitted, not by the parser.
  test('x-path-key levels below the top level', () => assert.throws(
    () => emitCppHeader(ns({ run: method(params({ batch: { type: 'object', additionalProperties: false, required: ['items'], properties: { items: { ...PAIR_ITEMS, 'x-path-key': { source: 'Read' } } }, description: 'x' } })) })),
    (e) => e instanceof SchemaError && /only supported on top-level params/.test(e.message),
  ));
  rejects('a property name with a leading underscore', { run: method(params({ _hwnd: { type: 'integer', description: 'x' } })) }, /reserved for the bridge/);
  rejects('a required property with a default', { run: method(params({ a: { type: 'string', default: 'x', description: 'x' } }, ['a'])) }, /cannot have a default/);
  rejects('a named property without a description', { run: method(params({ a: { type: 'string' } })) }, /"description" is required/);
  rejects('a required parameter that may be null', { run: method(params({ a: { type: 'string', nullable: true, description: 'x' } }, ['a'])) }, /required parameter cannot be null/);
  rejects('nullable array items in params', { run: method(params({ a: { type: 'array', items: { type: 'string', nullable: true }, description: 'x' } })) }, /array items in params cannot be null/);
  rejects('nullable: false', { run: method(params({ a: { type: 'string', nullable: false, description: 'x' } })) }, /"nullable" can only be true/);
  rejects('a nullable Json value', { run: method(params({ a: { type: 'json', nullable: true, description: 'x' } })) }, /unsupported keyword "nullable"/);
  rejects('a Json value with a default', { run: method(params({ a: { type: 'json', default: 1, description: 'x' } })) }, /unsupported keyword "default"/);
});

describe('api-schema · Json values', () => {
  const namespace = ns({
    log: method(
      params({ message: { type: 'json', description: 'What to log.' }, args: { type: 'array', items: { type: 'json' }, description: 'More values.' } }, ['message']),
      { type: 'object', additionalProperties: false, required: ['echo'], properties: { echo: { type: 'json', description: 'Echoed value.' } } },
    ),
  });
  const [log] = namespace.methods;

  test('C++ holds them as json, optional when not required', () => {
    const header = emitCppHeader(namespace);
    assert.match(header, /\n    json message\{\};/);
    assert.match(header, /\n    std::optional<std::vector<json>> args\{\};/);
  });

  test('TypeScript types them as JsonValue', () => {
    assert.match(emitTsParams(log, 'DemoLogParams'), /\n    message: JsonValue;/);
    assert.match(emitTsParams(log, 'DemoLogParams'), /\n    args\?: JsonValue\[\];/);
    assert.match(emitTsResponse(log, 'DemoLogResponse'), /\n    echo: JsonValue;/);
  });
});

describe('api-schema · C++ parameter structs', () => {
  const header = emitCppHeader(ns({
    run: method(params({
      path: { type: 'string', minLength: 1, 'x-security': 'MediaRead', description: 'x' },
      paths: { type: 'array', items: { type: 'string' }, 'x-security': 'Read', 'x-skip-invalid': true, description: 'x' },
      items: { type: 'array', items: { type: 'object', additionalProperties: false, required: ['path'], properties: { path: { type: 'string', description: 'x' }, type: { type: 'string', description: 'x' } } }, 'x-security': 'MediaRead', 'x-path-key': 'path', description: 'x' },
      limit: { type: 'integer', minimum: 1, maximum: 500, default: 100, description: 'x' },
      mode: { type: 'string', enum: ['a', 'b'], description: 'x' },
      ratio: { type: 'number', default: 1, description: 'x' },
    }, ['path'])),
    ping: method(undefined),
  }));

  test('an array of Json with a path member checks string elements too', () => {
    const handles = emitCppHeader(ns({ run: method(params({ handles: { type: 'array', items: { type: 'json' }, 'x-security': 'MediaRead', 'x-path-key': 'path', description: 'x' } }, ['handles'])) }));
    assert.match(handles, /kPathParams\{\{\{"handles", api::params::PathAccess::MediaRead, true, false, "path", true\}\}\}/);
  });

  test('members with their own levels expand to one path entry each, in declaration order', () => {
    const moves = emitCppHeader(ns({ run: method(params({ moves: { ...PAIR_ITEMS, 'x-path-key': { source: 'FileWrite', destination: 'Read' } } }, ['moves'])) }));
    assert.match(moves, /kPathParams\{\{\{"moves", api::params::PathAccess::FileWrite, true, false, "source"\}, \{"moves", api::params::PathAccess::Read, true, false, "destination"\}\}\}/);
  });

  test('required, defaulted and optional fields get the right C++ types', () => {
    assert.match(header, /std::string path\{\};/);
    assert.match(header, /std::optional<std::vector<std::string>> paths\{\};/);
    assert.match(header, /std::int64_t limit = 100;/);
    assert.match(header, /std::optional<std::string> mode\{\};/);
    // An integral default on a number stays a double literal.
    assert.match(header, /double ratio = 1\.0;/);
  });

  test('constraints become Reader calls in declaration order', () => {
    assert.match(header, /r\.Required\("path", out\.path\)\s+&& r\.MinLength\("path", out\.path, 1\)/);
    assert.match(header, /r\.Range\("limit", out\.limit, std::optional<std::int64_t>\(1\), std::optional<std::int64_t>\(500\)\)/);
    assert.match(header, /r\.OneOf\("mode", out\.mode, \{"a", "b"\}\)/);
    assert.match(header, /r\.OnlyKeys\(\{"path", "paths", "items", "limit", "mode", "ratio"\}\)/);
  });

  test('path security comes from x-security, arrays flagged, skipping flagged, the member of an object array named', () => {
    assert.match(header, /kPathParams\{\{\{"path", api::params::PathAccess::MediaRead, false, false, nullptr\}, \{"paths", api::params::PathAccess::Read, true, true, nullptr\}, \{"items", api::params::PathAccess::MediaRead, true, false, "path"\}\}\}/);
    // The elements themselves parse as ordinary objects; the bridge checks their path member.
    assert.match(header, /std::optional<std::vector<RunParamsItemsItem>> items\{\};/);
  });

  test('a key C++ cannot name keeps its JSON name and gets a trailing underscore as the member', () => {
    const h = emitCppHeader(ns({
      get: method(
        params({ default: { type: 'string', description: 'Fallback.' }, json: int('Json.') }, ['json']),
        result({ json: str('Json.'), kFields: int('K.') }, ['json', 'kFields']),
      ),
    }));
    assert.match(h, /std::optional<std::string> default_\{\};\n    std::int64_t json_\{\};/);
    assert.match(h, /r\.Optional\("default", out\.default_\)/);
    assert.match(h, /r\.Required\("json", out\.json_\)/);
    assert.match(h, /kFields\{\{"json", "kFields"\}\};\n\n    std::string json_\{\};\n    std::int64_t kFields_\{\};/);
    assert.match(h, /api::results::Put\(j, "json", v\.json_\);\n    api::results::Put\(j, "kFields", v\.kFields_\);/);
  });

  test('a method without params still rejects unknown keys and names no unused out', () => {
    assert.match(header, /struct PingParams \{\n    static constexpr const char\* kMethod = "demo\.ping";\n    static constexpr std::array<api::params::PathParam, 0> kPathParams\{\};\n    using Result = void;\n\};/);
    assert.match(header, /inline bool FromJson\(const json& j, PingParams&, std::string& error/);
    assert.match(header, /r\.OnlyKeys\(\{\}\);/);
  });

  test('the header is deterministic', () => {
    const again = emitCppHeader(ns({ ping: method(undefined) }));
    assert.equal(again, emitCppHeader(ns({ ping: method(undefined) })));
  });
});

describe('api-schema · C++ result structs', () => {
  const row = { type: 'object', 'x-name': 'Row', additionalProperties: false, required: ['id', 'note'], properties: { id: { type: 'integer', description: 'x' }, note: { type: 'string', nullable: true, description: 'x' } } };
  const header = emitCppHeader(ns({
    list: method(undefined, {
      type: 'object',
      additionalProperties: false,
      required: ['total', 'rows', 'cover'],
      properties: {
        total: { type: 'integer', description: 'x' },
        rows: { type: 'array', items: row, description: 'x' },
        cover: { type: 'string', nullable: true, description: 'x' },
        cursor: { type: 'string', description: 'x' },
      },
    }),
    fields: method(undefined, { type: 'object', additionalProperties: { type: 'string' }, required: ['path'], properties: { path: { type: 'string', description: 'x' } } }),
    ping: method(undefined),
  }));

  test('a result struct mirrors the declared fields and the params struct names it', () => {
    assert.match(header, /struct ListResult \{\n    static constexpr std::array<std::string_view, 4> kFields\{\{"total", "rows", "cover", "cursor"\}\};\n\n    std::int64_t total\{\};\n    std::vector<Row> rows\{\};\n    std::optional<std::string> cover\{\};\n    std::optional<std::string> cursor\{\};\n\};/);
    assert.match(header, /struct ListParams \{[\s\S]*?using Result = ListResult;/);
    assert.match(header, /struct PingParams \{[\s\S]*?using Result = void;/);
    assert.doesNotMatch(header, /struct PingResult/);
  });

  test('ToJson omits absent optional fields, writes null for nullable ones and nests through Value', () => {
    assert.match(header, /inline json ToJson\(const ListResult& v\) \{\n    json j = json::object\(\);\n    api::results::Put\(j, "total", v\.total\);\n    api::results::Put\(j, "rows", v\.rows\);\n    api::results::PutNullable\(j, "cover", v\.cover\);\n    api::results::Put\(j, "cursor", v\.cursor\);\n    return j;\n\}/);
    // The shared nested struct is a result-only shape: it gets ToJson but no FromJson.
    assert.match(header, /inline json ToJson\(const Row& v\)/);
    assert.doesNotMatch(header, /FromJson\(const json& j, Row&/);
    // The result struct comes before the params struct that names it.
    assert.ok(header.indexOf('struct ListResult') < header.indexOf('struct ListParams'));
  });

  test('named fields plus extra keys become a struct with an additional map written first', () => {
    assert.match(header, /struct FieldsResult \{\n    static constexpr std::array<std::string_view, 1> kFields\{\{"path"\}\};\n\n    std::string path\{\};\n    std::map<std::string, std::string> additional\{\};\n\};/);
    assert.match(header, /api::results::PutAll\(j, v\.additional\);\n    api::results::Put\(j, "path", v\.path\);/);
  });

  test('a nested interface used by both params and a result gets both functions', () => {
    const filter = { type: 'object', 'x-name': 'Filter', additionalProperties: false, properties: { ext: { type: 'string', description: 'x' } } };
    const both = emitCppHeader(ns({
      open: method(params({ filters: { type: 'array', items: filter, description: 'x' } }), { type: 'object', additionalProperties: false, properties: { used: { ...filter, description: 'x' } } }),
    }));
    assert.match(both, /inline bool FromJson\(const json& j, Filter& out/);
    assert.match(both, /inline json ToJson\(const Filter& v\)/);
    assert.equal(both.match(/struct Filter \{/g).length, 1);
  });

  test('envelope keys and optional-plus-nullable fields are rejected on results', () => {
    const rejects = (result, pattern) => assert.throws(() => ns({ run: method(undefined, result) }), (e) => e instanceof SchemaError && pattern.test(e.message), `expected ${pattern}`);
    for (const key of ['success', 'error', 'code', 'skippedPaths']) {
      rejects({ type: 'object', additionalProperties: false, properties: { [key]: { type: 'string', description: 'x' } } }, new RegExp(`"${key}" is written by the bridge envelope`));
    }
    rejects({ type: 'object', additionalProperties: false, properties: { cover: { type: 'string', nullable: true, description: 'x' } } }, /cannot be both optional and nullable/);
    // Nested objects are not envelopes.
    assert.ok(ns({ run: method(undefined, { type: 'object', additionalProperties: false, properties: { rows: { type: 'array', description: 'x', items: { type: 'object', additionalProperties: false, properties: { success: { type: 'boolean', description: 'x' } } } } } }) }));
  });
});

describe('api-schema · TypeScript declarations', () => {
  const [run, ping] = ns({
    run: method(
      params({ path: { type: 'string', description: 'Track path.' }, limit: { type: 'integer', default: 10, description: 'Max rows.' } }, ['path']),
      {
        type: 'object',
        additionalProperties: false,
        required: ['rows'],
        properties: {
          rows: { type: 'array', description: 'Rows.', items: { type: 'object', additionalProperties: false, required: ['id'], properties: { id: { type: 'integer', description: 'Row id.' } } } },
        },
      },
    ),
    ping: method(undefined),
  }).methods;

  test('a method that may drop invalid paths gets skippedPaths on its response', () => {
    const [add] = ns({ add: method(params({ paths: { type: 'array', items: { type: 'string' }, 'x-security': 'MediaRead', 'x-skip-invalid': true, description: 'Paths.' } }, ['paths'])) }).methods;
    assert.match(emitTsResponse(add, 'DemoAddResponse'), /\n    skippedPaths\?: number;/);
    assert.doesNotMatch(emitTsResponse(run, 'DemoRunResponse'), /skippedPaths/);
  });

  test('params keep required-ness and document defaults', () => {
    const ts = emitTsParams(run, 'DemoRunParams');
    assert.match(ts, /export interface DemoRunParams \{/);
    assert.match(ts, /\n    path: string;/);
    assert.match(ts, /\/\*\*\n     \* Max rows\.\n     \* @default 10\n     \*\/\n    limit\?: number;/);
  });

  test('nullable values admit null in params, results and array items', () => {
    const [pick] = ns({
      pick: method(
        params({ icon: { type: 'string', nullable: true, description: 'Icon.' } }),
        {
          type: 'object',
          additionalProperties: false,
          required: ['cover', 'names'],
          properties: {
            cover: { type: 'string', nullable: true, description: 'Cover.' },
            names: { type: 'array', items: { type: 'string', nullable: true }, description: 'Names.' },
          },
        },
      ),
    }).methods;
    assert.match(emitTsParams(pick, 'DemoPickParams'), /\n    icon\?: string \| null;/);
    const res = emitTsResponse(pick, 'DemoPickResponse');
    assert.match(res, /\n    cover: string \| null;/);
    assert.match(res, /\n    names: Array<string \| null>;/);
    // The C++ side is unchanged: Reader::Optional already treats a null key as absent.
    assert.match(emitCppHeader(ns({ pick: method(params({ icon: { type: 'string', nullable: true, description: 'x' } })) })), /std::optional<std::string> icon\{\};/);
  });

  test('a method without params is Record<string, never>', () => {
    assert.match(emitTsParams(ping, 'DemoPingParams'), /export type DemoPingParams = Record<string, never>;/);
  });

  test('responses are the success shape or the failure envelope, and keep declared required-ness', () => {
    const ts = emitTsResponse(run, 'DemoRunResponse');
    assert.match(ts, /export interface DemoRunSuccess \{\n    \/\*\* [^\n]* \*\/\n    success: true;\n/);
    assert.match(ts, /\nexport type DemoRunResponse = DemoRunSuccess \| ApiFailure;$/);
    assert.doesNotMatch(ts, /\n    (error|code)\??:/);
    assert.match(ts, /\n    rows: Array<\{\n        \/\*\* Row id\. \*\/\n        id: number;\n    \}>;/);
    assert.equal(ts.match(/\n    success/g).length, 1);
  });

  test('a void method answers with success alone, or the failure envelope', () => {
    const ts = emitTsResponse(ping, 'DemoPingResponse');
    assert.match(ts, /export interface DemoPingSuccess \{\n[\s\S]*?\n    success: true;\n\}/);
    assert.match(ts, /\nexport type DemoPingResponse = DemoPingSuccess \| ApiFailure;$/);
  });
});

describe('api-schema · docs regions', () => {
  function tmpRepo(pages) {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'api-schema-'));
    for (const [rel, text] of Object.entries(pages)) {
      const abs = path.join(root, 'docs/vitepress', rel);
      fs.mkdirSync(path.dirname(abs), { recursive: true });
      fs.writeFileSync(abs, text);
    }
    return root;
  }
  const namespaces = [ns({ ping: { description: 'Ping.', 'x-description-zh': '探测。' } })];
  const region = (api, body = 'stale') => `<!-- api-schema:begin ${api} -->\n${body}\n<!-- api-schema:end -->`;

  test('a method that may drop invalid paths lists skippedPaths in its return table', () => {
    const [add] = ns({ add: method(params({ paths: { type: 'array', items: { type: 'string' }, 'x-security': 'MediaRead', 'x-skip-invalid': true, description: 'Paths.' } }, ['paths'])) }).methods;
    const en = renderRegion(add, 'en');
    assert.match(en, /\*\*Returns\*\*\n\n\| Field \| Type \| Description \|\n\| --- \| --- \| --- \|\n\| `skippedPaths` \| `integer` \| Number of paths the security check dropped before the call ran; present only when at least one was dropped\. \|\n\n`success` is `true`/);
    assert.match(renderRegion(add, 'zh'), /\| `skippedPaths` \| `integer` \| 调用前被路径安全检查丢弃的路径数；只在至少丢弃一条时出现。 \|/);
  });

  test('regions are rewritten per locale and the prose around them is kept', () => {
    const root = tmpRepo({ 'api/demo.md': `# Demo\n\n${region('demo.ping')}\n\nKept.\n`, 'zh/api/demo.md': `${region('demo.ping')}\n` });
    const { changes, problems } = planDocs(root, namespaces);
    assert.deepEqual(problems, []);
    const en = changes.find((c) => c.file === 'docs/vitepress/api/demo.md').next;
    const zh = changes.find((c) => c.file === 'docs/vitepress/zh/api/demo.md').next;
    assert.match(en, /^# Demo\n\n<!-- api-schema:begin demo\.ping -->\nPing\.\n\nThis method takes no parameters\.\n\n\*\*Returns\*\*\n\n`success` is `true` on success\. On failure the response is `\{ success: false, error, code \}`; see \[Error codes\]\(\.\.\/reference\/errors\.md\) for `code`\.\n<!-- api-schema:end -->\n\nKept\.\n$/);
    assert.match(zh, /探测。\n\n无参数。\n\n\*\*返回值\*\*\n\n成功时 `success` 为 `true`；失败时返回/);
  });

  test('the namespace index counts methods and links the pages of its locale that document them', () => {
    const two = [ns({ ping: { description: 'Ping.' }, pong: { description: 'Pong.' } })];
    const root = tmpRepo({
      'api/overview.md': `# Overview\n\n${region('index:namespaces')}\n`,
      'api/demo.md': `# Demo API\n\n${region('demo.ping')}\n`,
      'api/more/extra.md': `# Extra\n\n${region('demo.pong')}\n`,
      'zh/api/overview.md': `${region('index:namespaces')}\n`,
      'zh/api/demo.md': `# 演示\n\n${region('demo.ping')}\n${region('demo.pong')}\n`,
    });
    const { changes, problems } = planDocs(root, two);
    assert.deepEqual(problems, []);
    const en = changes.find((c) => c.file === 'docs/vitepress/api/overview.md').next;
    const zh = changes.find((c) => c.file === 'docs/vitepress/zh/api/overview.md').next;
    assert.match(en, /2 methods in 1 namespace\.\n\n\| Namespace \| Methods \| Documented on \|\n\| --- \| ---: \| --- \|\n\| `demo` \| 2 \| \[Demo API\]\(\.\/demo\.md\), \[Extra\]\(\.\/more\/extra\.md\) \|\n/);
    assert.match(zh, /共 2 个方法，分属 1 个命名空间。[\s\S]*\| `demo` \| 2 \| \[演示\]\(\.\/demo\.md\) \|/);
  });

  test('an unknown index region is reported', () => {
    const root = tmpRepo({ 'api/demo.md': `${region('demo.ping')}\n${region('index:events')}\n`, 'zh/api/demo.md': `${region('demo.ping')}\n` });
    const { problems } = planDocs(root, namespaces);
    assert.ok(problems.some((p) => /region index:events; the only index region is index:namespaces/.test(p)), problems.join('\n'));
  });

  test('the error codes link is relative to the page, at any depth', () => {
    const root = tmpRepo({ 'reference/sdk/demo.md': `${region('demo.ping')}\n`, 'zh/demo.md': `${region('demo.ping')}\n` });
    const { changes, problems } = planDocs(root, namespaces);
    assert.deepEqual(problems, []);
    const en = changes.find((c) => c.file === 'docs/vitepress/reference/sdk/demo.md').next;
    const zh = changes.find((c) => c.file === 'docs/vitepress/zh/demo.md').next;
    assert.match(en, /see \[Error codes\]\(\.\.\/errors\.md\) for `code`/);
    assert.match(zh, /`code` 见\[错误码\]\(\.\/reference\/errors\.md\)。/);
  });

  test('the parameter table states the constraints the parser enforces', () => {
    const constrained = [ns({
      pick: method(params({
        path: { type: 'string', minLength: 1, description: 'Where.', 'x-description-zh': '位置。' },
        name: { type: 'string', minLength: 3, description: 'Name.' },
        limit: { type: 'integer', minimum: 1, maximum: 50, default: 10, description: 'Limit.', 'x-description-zh': '上限。' },
        offset: { type: 'integer', minimum: 0, description: 'Offset.' },
        ratio: { type: 'number', maximum: 1, description: 'Ratio.' },
        ids: { type: 'array', items: { type: 'string' }, minItems: 2, description: 'Ids.' },
      }, ['path'])),
    })];
    const root = tmpRepo({ 'api/demo.md': `${region('demo.pick')}\n`, 'zh/api/demo.md': `${region('demo.pick')}\n` });
    const { changes } = planDocs(root, constrained);
    const en = changes.find((c) => c.file === 'docs/vitepress/api/demo.md').next;
    const zh = changes.find((c) => c.file === 'docs/vitepress/zh/api/demo.md').next;
    assert.match(en, /\| `path` \| `string` \| Yes \| Where\. Must not be empty\. \|/);
    assert.match(en, /\| `name` \| `string` \| No \| Name\. At least 3 characters\. \|/);
    assert.match(en, /\| `limit` \| `integer` \| No \| Limit\. Between `1` and `50` inclusive\. Default: `10`\. \|/);
    assert.match(en, /\| `offset` \| `integer` \| No \| Offset\. At least `0`\. \|/);
    assert.match(en, /\| `ratio` \| `number` \| No \| Ratio\. At most `1`\. \|/);
    assert.match(en, /\| `ids` \| `string\[\]` \| No \| Ids\. At least 2 items\. \|/);
    assert.match(zh, /\| `path` \| `string` \| 是 \| 位置。不能为空。 \|/);
    assert.match(zh, /\| `limit` \| `integer` \| 否 \| 上限。取值 `1` 到 `50`（含端点）。默认 `10`。 \|/);
  });

  test('a missing locale region and a region for an undeclared method are both reported', () => {
    const root = tmpRepo({ 'api/demo.md': `${region('demo.ping')}\n${region('demo.gone')}\n` });
    const { problems } = planDocs(root, namespaces);
    assert.ok(problems.some((p) => /no zh docs region for demo\.ping/.test(p)), problems.join('\n'));
    assert.ok(problems.some((p) => /region for demo\.gone, which no schema declares/.test(p)), problems.join('\n'));
  });
});

describe('api-schema · registrations in the sources', () => {
  const namespaces = [ns({ run: method(undefined), stop: method(undefined) })];
  const src = (text) => [{ file: 'src/api/DemoApi.cpp', text }];

  test('a typed registration of every declared method passes', () => {
    assert.deepEqual(checkRegistrations(namespaces, src('api::RegisterApi("demo.run", Run);\napi::RegisterApi("demo.stop", Stop);\n'), new Map()), []);
  });

  test('a typed deferred registration counts as registered', () => {
    assert.deepEqual(checkRegistrations(namespaces, src('api::RegisterApi("demo.run", Run);\napi::RegisterApiDeferred("demo.stop", Stop);\n'), new Map()), []);
  });

  test('unregistered, raw-json and deferred registrations are each reported with file and line', () => {
    const problems = checkRegistrations(namespaces, src('// intro\nBridgeCore::GetInstance().RegisterApi("demo.run", RunRaw);\nRegisterApiDeferred("demo.stop", StopDeferred);\n'), new Map());
    assert.equal(problems.length, 4, problems.join('\n'));
    assert.match(problems[0], /^demo\.run is declared in src\/api\/schema\/demo\.json but src\/api\/DemoApi\.cpp:2 registers it through the raw-json RegisterApi/);
    assert.match(problems[1], /^demo\.run is declared .* but no src\/\*\*\/\*\.cpp registers it with api::RegisterApi\("demo\.run"/);
    assert.match(problems[2], /^demo\.stop is declared .* src\/api\/DemoApi\.cpp:3 registers it through RegisterApiDeferred/);
    assert.match(problems[3], /^demo\.stop is declared .* but no src/);
  });

  test('the literal has to be the whole method name', () => {
    const problems = checkRegistrations(namespaces, src('api::RegisterApi("demo.runner", Runner);\napi::RegisterApi("demo.stop", Stop);\n'), new Map());
    assert.deepEqual(problems.map((p) => p.split(' ')[0]), ['demo.run']);
  });

  test('an untyped registration of an undeclared name is reported unless it is listed', () => {
    const text = 'api::RegisterApi("demo.run", Run);\napi::RegisterApi("demo.stop", Stop);\nbridge.RegisterUndeclaredApi("demo.extra", Extra);\nbridge.RegisterApiDeferred("demo.later", Later);\nbridge.RegisterUndeclaredApi("demo.__private", Private);\n';
    const problems = checkRegistrations(namespaces, src(text), new Map([['demo.__private', 'listed']]));
    assert.equal(problems.length, 2, problems.join('\n'));
    assert.match(problems[0], /^demo\.extra is registered at src\/api\/DemoApi\.cpp:3 without a declaration/);
    assert.match(problems[1], /^demo\.later is registered at src\/api\/DemoApi\.cpp:4 without a declaration/);
  });

  test('a declared method registered through RegisterUndeclaredApi is reported', () => {
    const problems = checkRegistrations(namespaces, src('api::RegisterApi("demo.run", Run);\nbridge.RegisterUndeclaredApi("demo.stop", Stop);\n'), new Map());
    assert.equal(problems.length, 2, problems.join('\n'));
    assert.match(problems[0], /^demo\.stop is declared .* src\/api\/DemoApi\.cpp:2 registers it through RegisterUndeclaredApi/);
    assert.match(problems[1], /^demo\.stop is declared .* but no src/);
  });

  test('a listed undeclared name that nothing registers is reported', () => {
    const problems = checkRegistrations(namespaces, src('api::RegisterApi("demo.run", Run);\napi::RegisterApi("demo.stop", Stop);\n'), new Map([['demo.__gone', 'listed']]));
    assert.deepEqual(problems, ['demo.__gone is listed in UNDECLARED_RAW but no src/**/*.cpp registers it with RegisterUndeclaredApi("demo.__gone"']);
  });

  test('a registration whose name is not a string literal is not seen', () => {
    assert.deepEqual(checkRegistrations(namespaces, src('api::RegisterApi("demo.run", Run);\napi::RegisterApi("demo.stop", Stop);\nbridge.RegisterApi(fullName, Handler);\n'), new Map()), []);
  });
});

describe('api-schema · named nested structs', () => {
  const filter = { type: 'object', 'x-name': 'Filter', additionalProperties: false, properties: { ext: { type: 'string', description: 'x' } } };

  test('every use of a named object shares one struct', () => {
    const header = emitCppHeader(ns({
      open: method(params({ filters: { type: 'array', items: filter, description: 'x' } })),
      save: method(params({ filters: { type: 'array', items: filter, description: 'y' } })),
    }));
    assert.equal(header.match(/struct Filter \{/g).length, 1);
    assert.match(header, /std::optional<std::vector<Filter>> filters\{\};[\s\S]*std::optional<std::vector<Filter>> filters\{\};/);
  });

  test('two different shapes under one name are an error, and so is reusing a params struct name', () => {
    const other = { ...filter, properties: { name: { type: 'string', description: 'x' } } };
    assert.throws(
      () => emitCppHeader(ns({ open: method(params({ a: { ...filter, description: 'x' }, b: { ...other, description: 'x' } })) })),
      /already produces struct Filter/,
    );
    assert.throws(
      () => emitCppHeader(ns({ open: method(params({ a: { ...filter, 'x-name': 'OpenParams', description: 'x' } })) })),
      /already produces struct OpenParams/,
    );
  });
});

describe('api-schema · TypeScript declarations', () => {
  const TSCONFIG = { compilerOptions: { strict: true, noEmit: true, target: 'ES2022', module: 'ESNext', moduleResolution: 'Bundler', lib: ['ES2022'], types: [], skipLibCheck: true }, include: ['*.ts'] };
  const COMMON = 'export type Int = number;\nexport type Json = unknown;\n';

  function tsRepo(demo) {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'api-schema-ts-'));
    const dir = path.join(root, 'src/api/schema');
    fs.mkdirSync(dir, { recursive: true });
    fs.writeFileSync(path.join(dir, 'tsconfig.json'), JSON.stringify(TSCONFIG));
    fs.writeFileSync(path.join(dir, 'common.ts'), COMMON);
    fs.writeFileSync(path.join(dir, 'demo.ts'), demo);
    return root;
  }
  const load = (demo) => loadNamespaces(tsRepo(demo)).find((ns) => ns.namespace === 'demo');

  test('method lifecycle tags survive the TS loader without changing descriptions or shared types', () => {
    const source = `export interface Api {
  /**
   * Read a row.
   * @zh 读取一行。
   * @experimental
   * @deprecated Use stable instead.
   * @deprecatedZh 请改用 stable。
   */
  preview(params: Input): Output;
  /** Read a row. */
  stable(params: Input): Output;
}
interface Input {
  /** Row to read. */
  row: Row;
}
interface Output {
  /** Row read. */
  row: Row;
}
/** A shared row. */
interface Row {
  /** Row label. */
  label: string;
}
`;
    const demo = load(source);
    const [preview, stable] = demo.methods;
    assert.deepEqual([preview.experimental, preview.deprecated, preview.deprecatedZh], [true, 'Use stable instead.', '请改用 stable。']);
    assert.deepEqual([stable.experimental, stable.deprecated, stable.deprecatedZh], [false, null, null]);
    assert.equal(preview.description, 'Read a row.');
    assert.equal(preview.descriptionZh, '读取一行。');
    for (const rendered of [emitTsParams(preview, 'DemoPreviewParams'), emitTsResponse(preview, 'DemoPreviewResponse')]) {
      assert.match(rendered, / \* @experimental\n \* @deprecated Use stable instead\./);
      assert.doesNotMatch(rendered, /deprecatedZh|请改用/);
    }
    assert.equal((emitTsResponse(preview, 'DemoPreviewResponse').match(/@deprecated/g) ?? []).length, 2);
    assert.doesNotMatch(emitTsNamedTypes([demo]).join('\n'), /@experimental|@deprecated/);
    assert.deepEqual(preview.params, stable.params);
    assert.deepEqual(preview.result, stable.result);
    assert.match(renderRegion(preview, 'en'), /^Experimental API; it may change in future releases\.\n\nDeprecated: Use stable instead\.\n\nRead a row\./);
    assert.match(renderRegion(preview, 'zh'), /^实验性 API，后续版本可能发生变化。\n\n已弃用：请改用 stable。\n\n读取一行。/);
    const unmarked = load(source.replace(/   \* @(experimental|deprecated(?:Zh)?)[^\n]*\n/g, ''));
    assert.equal(emitCppHeader(demo), emitCppHeader(unmarked));
    assert.equal(emitTsNamedTypes([demo]).join('\n'), emitTsNamedTypes([unmarked]).join('\n'));
  });

  const lifecycleDoc = (tags) => `/**\n * Read.\n${tags.map((tag) => ` * ${tag}\n`).join('')} */`;
  const lifecycleApi = (tags) => `export interface Api {\n${lifecycleDoc(tags)}\nread(): void;\n}`;
  for (const [label, tags] of [
    ['experimental', ['@experimental']],
    ['invalid experimental', ['@experimental invalid']],
    ['one-sided deprecation', ['@deprecated Use stable.']],
    ['unmarked', []],
  ]) {
    test(`method lifecycle rejects a second Api declaration with ${label}`, () => {
      const source = `${lifecycleApi([])}\n${lifecycleApi(tags)}`;
      assert.throws(() => load(source), (e) => e instanceof SchemaError && /interface Api.*declared twice/.test(e.message));
    });
  }
  test('rejects a second Events declaration instead of dropping its members', () => {
    const source = `${lifecycleApi([])}
export interface Events {
  /**
   * Changed.
   * @delivery caller
   */
  changed: void;
}
export interface Events {
  /**
   * Reset.
   * @delivery caller
   */
  reset: void;
}
`;
    assert.throws(() => load(source), (e) => e instanceof SchemaError && /interface Events.*declared twice/.test(e.message));
  });
  test('method lifecycle reads standalone experimental and paired deprecation tags', () => {
    assert.equal(load(lifecycleApi(['@experimental'])).methods[0].experimental, true);
    const deprecated = load(lifecycleApi(['@deprecated Use stable.', '@deprecatedZh 请改用 stable。'])).methods[0];
    assert.deepEqual([deprecated.experimental, deprecated.deprecated, deprecated.deprecatedZh], [false, 'Use stable.', '请改用 stable。']);
  });
  for (const [label, tags, pattern] of [
    ['experimental value', ['@experimental true'], /@experimental takes no value/],
    ['empty English migration', ['@deprecated', '@deprecatedZh 请改用 read。'], /x-deprecated.*non-empty|@deprecated needs a value/],
    ['empty Chinese migration', ['@deprecated Use read.', '@deprecatedZh'], /x-deprecated-zh.*non-empty|@deprecatedZh needs a value/],
    ['English migration alone', ['@deprecated Use read.'], /x-deprecated.*x-deprecated-zh|@deprecated.*@deprecatedZh/],
    ['Chinese migration alone', ['@deprecatedZh 请改用 read。'], /x-deprecated.*x-deprecated-zh|@deprecated.*@deprecatedZh/],
    ...['experimental', 'deprecated', 'deprecatedZh'].map((tag) => [`duplicate ${tag}`, [`@${tag}`, `@${tag}`], new RegExp(`@${tag} appears twice`)]),
  ]) {
    test(`method lifecycle rejects ${label}`, () => {
      assert.throws(() => load(lifecycleApi(tags)), (e) => e instanceof SchemaError && pattern.test(e.message));
    });
  }

  for (const tag of ['@experimental', '@deprecated Use stable.', '@deprecatedZh 请改用 stable。']) {
    const doc = lifecycleDoc([tag]);
    const plain = lifecycleApi([]);
    const placements = {
      'Api container': `${doc}\n${plain}`,
      'Events container': `${plain}\n${doc}\nexport interface Events {}`,
      'Events member': `${plain}\nexport interface Events {\n${doc}\nchanged: void;\n}`,
      'unused interface': `${plain}\n${doc}\ninterface Unused {}`,
      'unused alias': `${plain}\n${doc}\ntype Unused = string;`,
      'unused member': `${plain}\ninterface Unused {\n${doc}\nvalue: string;\n}`,
      'parameter': `export interface Api {\n/** Read. */\nread(\n${doc}\nparams: {}): void;\n}`,
      'parameter property': `export interface Api {\n/** Read. */\nread(params: {\n${doc}\nvalue: string;\n}): void;\n}`,
      'return property': `export interface Api {\n/** Read. */\nread(): {\n${doc}\nvalue: string;\n};\n}`,
      'return type trivia': `export interface Api {\n/** Read. */\nread(): ${doc} void;\n}`,
      'single-line return type trivia': `export interface Api {\n/** Read. */\nread(): /** ${tag} */ void;\n}`,
      'type argument trivia': `${plain}\ntype Unused = Array<${doc} string>;`,
      'tuple element trivia': `${plain}\ntype Unused = [${doc} string];`,
      'union member trivia': `${plain}\ntype Unused = number | ${doc} string;`,
      'parenthesized type trivia': `${plain}\ntype Unused = (${doc} string);`,
      'end of file trivia': `${plain}\n${doc}`,
    };
    for (const [place, source] of Object.entries(placements)) {
      test(`method lifecycle rejects ${tag.split(' ')[0]} on ${place}`, () => {
        const line = source.slice(0, source.indexOf(tag)).split('\n').length;
        assert.throws(() => load(source), (e) => e instanceof SchemaError
          && e.message.includes(`src/api/schema/demo.ts:${line}:`)
          && /applies to.*method|does not apply/.test(e.message));
      });
    }
  }

  for (const [place, source] of Object.entries({
    'string literal': 'const text = "/** @experimental */";',
    'regex literal': 'const pattern = /[/** @experimental */]/;',
    'template literal': 'const text = `/** @experimental */`;',
    'template with substitution': 'const text = `/** @experimental */${"/** @deprecated */"}/** @deprecatedZh */`;',
    'line comment': '// /** @experimental */',
    'block comment': '/* @experimental */',
  })) {
    test(`method lifecycle ignores tag-like text in ${place}`, () => {
      const [read] = load(`${lifecycleApi(['@experimental'])}\n${source}`).methods;
      assert.equal(read.experimental, true);
    });
  }

  for (const comment of ['/** Text mentions @experimental here. */', '/**\n * ```ts\n * @experimental\n * ```\n */']) {
    test(`method lifecycle follows TypeScript tag parsing in unattached ${JSON.stringify(comment)}`, () => {
      assert.throws(() => load(`${lifecycleApi([])}\ntype Unused = Array<${comment} string>;`),
        (e) => e instanceof SchemaError && /@experimental applies to an exported Api method only/.test(e.message));
    });
  }

  test('signatures, JSDoc tags, enums, integers, maps and documented aliases become the IR', () => {
    const demo = load(`import type { Int } from './common';
export interface Api {
  /**
   * Pick one.
   * @zh 选一个。
   */
  pick(params: PickParams): PickResult;
  /** Ping. */
  ping(): void;
}
interface Item {
  /** Id. */
  id: Int;
}
interface PickParams {
  /**
   * Where.
   * @minLength 1
   * @security MediaRead
   */
  path: string;
  /**
   * Mode.
   * @default "fast"
   */
  mode?: 'fast' | 'slow';
  /**
   * Limit.
   * @minimum 1
   * @maximum 50
   * @default 10
   */
  limit?: Int;
  /** Tags. */
  tags?: Record<string, string>;
  /** Items. */
  items?: Item[];
  /**
   * More.
   * @security Read
   * @skipInvalid
   */
  more?: string[];
  /**
   * Entries.
   * @security MediaWrite
   * @pathKey path
   */
  entries?: Entry[];
  /**
   * Moves.
   * @pathKey source=Read destination=FileWrite
   */
  moves?: Move[];
}
interface Entry {
  /** Path. */
  path: string;
}
interface Move {
  /** Source. */
  source: string;
  /** Destination. */
  destination: string;
}
/** Extra text. */
type Extra = string;
type PickResult = {
  /** Where. */
  path: string;
} & Record<string, Extra>;
`);
    assert.equal(demo.file, 'src/api/schema/demo.ts');
    const [pick, ping] = demo.methods;
    assert.equal(pick.description, 'Pick one.');
    assert.equal(pick.descriptionZh, '选一个。');
    const [p, mode, limit, tags, items, more, entries, moves] = pick.params.properties;
    assert.deepEqual([p.kind, p.required, p.minLength, p.security], ['string', true, 1, 'MediaRead']);
    assert.deepEqual([more.kind, more.security, more.skipInvalid], ['array', 'Read', true]);
    assert.deepEqual([entries.kind, entries.security, entries.pathKey, entries.items.name], ['array', 'MediaWrite', 'path', 'Entry']);
    assert.deepEqual([moves.security, moves.pathKey, moves.pathKeys, moves.items.name], [undefined, undefined, [{ key: 'source', level: 'Read' }, { key: 'destination', level: 'FileWrite' }], 'Move']);
    assert.deepEqual([mode.enum, mode.default, mode.required], [['fast', 'slow'], 'fast', false]);
    assert.deepEqual([limit.kind, limit.minimum, limit.maximum, limit.default], ['integer', 1, 50, 10]);
    assert.equal(tags.additional.kind, 'string');
    assert.deepEqual([items.items.name, items.items.properties[0].kind], ['Item', 'integer']);
    assert.deepEqual([pick.result.properties[0].key, pick.result.additional.description], ['path', 'Extra text.']);
    assert.deepEqual([ping.params.properties.length, ping.result], [0, null]);
  });

  test('T | null and string literals | null are nullable', () => {
    const demo = load(`export interface Api {
  /** Run. */
  run(params: P): void;
}
interface P {
  /** Icon. */
  icon?: string | null;
  /** Mode. */
  mode?: 'a' | 'b' | null;
}
`);
    const [icon, mode] = demo.methods[0].params.properties;
    assert.deepEqual([icon.kind, icon.nullable, icon.enum], ['string', true, undefined]);
    assert.deepEqual([mode.enum, mode.nullable], [['a', 'b'], true]);
  });

  test('a wrapped JSDoc line joins Chinese without a space and anything else with one', () => {
    const demo = load(`export interface Api {
  /**
   * Run the
   * thing.
   * @zh 运行这个
   * 东西；然后
   * \`停下\`。
   */
  run(): void;
}
`);
    const [run] = demo.methods;
    assert.equal(run.description, 'Run the thing.');
    assert.equal(run.descriptionZh, '运行这个东西；然后 `停下`。');
  });

  test('Json from common.ts becomes the pass-through kind', () => {
    const demo = load(`import type { Json } from './common';
export interface Api {
  /** Log. */
  log(params: LogParams): void;
}
interface LogParams {
  /** What to log. */
  message: Json;
  /** More values. */
  args?: Json[];
}
`);
    const [message, args] = demo.methods[0].params.properties;
    assert.deepEqual([message.kind, message.required], ['json', true]);
    assert.deepEqual([args.kind, args.items.kind], ['array', 'json']);
  });

  test('@effect, @idempotent and @openWorld describe a method; without them it is not described', () => {
    const demo = load(`export interface Api {
  /**
   * Look.
   * @effect read
   */
  look(): void;
  /**
   * Set.
   * @zh 设置。
   * @effect write
   * @idempotent
   */
  set(): void;
  /**
   * Fetch.
   * @effect destructive
   * @openWorld
   */
  fetch(): void;
  /** Plain. */
  plain(): void;
}
`);
    const behavior = (m) => [m.effect, m.idempotent, m.openWorld];
    assert.deepEqual(demo.methods.map(behavior), [
      ['read', false, false],
      ['write', true, false],
      ['destructive', false, true],
      [null, false, false],
    ]);
    assert.equal(demo.methods[1].descriptionZh, '设置。');
  });

  const rejects = (label, body, pattern) =>
    test(label, () => assert.throws(() => load(body), (e) => e instanceof SchemaError && pattern.test(e.message), `expected ${pattern}`));

  const method = (tags) => `export interface Api {\n  /**\n   * Run.\n${tags.map((t) => `   * ${t}\n`).join('')}   */\n  run(): void;\n}\n`;
  rejects('an unknown @effect', method(['@effect maybe']), /"x-effect" must be one of read, write, destructive/);
  rejects('@effect without a value', method(['@effect']), /@effect needs a value/);
  rejects('@idempotent with a value', method(['@effect write', '@idempotent yes']), /@idempotent takes no value/);
  rejects('@idempotent on a read', method(['@effect read', '@idempotent']), /"x-idempotent" needs "x-effect" write or destructive/);
  rejects('@openWorld without @effect', method(['@openWorld']), /"x-open-world" needs "x-effect"/);

  // TypeScript attaches a JSDoc comment only when it starts on its own line.
  const api = (signature, member) =>
    `export interface Api {\n  /** Run. */\n  ${signature}\n}\ninterface P {\n  ${member.split('\n').join('\n  ')}\n}\n`;

  rejects('a local alias named Json', 'type Json = unknown;\n' + api('run(params: P): void;', '/** x */\na: Json;'), /unsupported type unknown/);

  rejects('an unknown JSDoc tag', api('run(params: P): void;', '/**\n * x\n * @pattern ^a\n */\na: string;'), /unsupported JSDoc tag @pattern/);
  rejects('a union that is not string literals', api('run(params: P): void;', '/** x */\na: string | number;'), /only unions of string literals/);
  rejects('a union of two types and null', api('run(params: P): void;', '/** x */\na?: string | number | null;'), /only T \| null/);
  rejects('an optional parameter object', api('run(params?: P): void;', '/** x */\na: string;'), /must be typed and not optional/);
  rejects('a file without an exported Api', 'interface Api {\n  /** Run. */\n  run(): void;\n}\n', /must export `interface Api`/);
  rejects('a file with neither Api nor Events', 'interface Other {\n  /** x */\n  a: string;\n}\n', /must export `interface Api` listing the methods, `interface Events` listing the events, or both/);
  rejects('an event declared as a method', 'export interface Events {\n  /**\n   * x\n   * @delivery caller\n   */\n  reset(): void;\n}\n', /members must be properties/);
  rejects('@customName with a value', 'export interface Events {\n  /**\n   * x\n   * @delivery owner\n   * @customName yes\n   */\n  frame: void;\n}\n', /@customName takes no value/);
  rejects('a parameter tag on an event', 'export interface Events {\n  /**\n   * x\n   * @delivery caller\n   * @minimum 1\n   */\n  reset: void;\n}\n', /@minimum does not apply to an event/);
  rejects('an optional event', 'export interface Events {\n  /**\n   * x\n   * @delivery caller\n   */\n  reset?: void;\n}\n', /an event cannot be optional/);
  rejects('@minimum on an array of strings', api('run(params: P): void;', '/**\n * x\n * @minimum 0\n */\na: string[];'), /@minimum and @maximum on an array need number elements/);
  rejects('@effect on a parameter', api('run(params: P): void;', '/**\n * x\n * @effect read\n */\na: string;'), /@effect applies to a method only/);
  rejects('@idempotent on an event', 'export interface Events {\n  /**\n   * x\n   * @delivery caller\n   * @idempotent\n   */\n  reset: void;\n}\n', /@idempotent does not apply to an event/);

  test('@minimum and @maximum on an array of numbers bound every item, and the C++ parser checks each one', () => {
    const src = `import type { Int } from './common';
export interface Api {
  /** Run. */
  run(params: P): void;
}
interface P {
  /**
   * Rows.
   * @minimum 0
   */
  rows: Int[];
  /**
   * Gains.
   * @minimum -1
   * @maximum 1
   */
  gains?: number[];
}
`;
    const namespaces = loadNamespaces(tsRepo(src));
    const demo = namespaces.find((n) => n.namespace === 'demo');
    const [rows, gains] = demo.methods[0].params.properties;
    assert.deepEqual([rows.minimum, rows.items.kind, rows.items.minimum, rows.items.maximum], [undefined, 'integer', 0, undefined]);
    assert.deepEqual([gains.minimum, gains.maximum, gains.items.minimum, gains.items.maximum], [undefined, undefined, -1, 1]);
    const header = emitCppHeader(demo, namespaces);
    assert.match(header, /r\.ItemsRange\("rows", out\.rows, std::optional<std::int64_t>\(0\), std::nullopt\)/);
    assert.match(header, /r\.ItemsRange\("gains", out\.gains, std::optional<double>\(-1\.0\), std::optional<double>\(1\.0\)\)/);
    assert.doesNotMatch(header, /r\.Range\("(rows|gains)"/);
  });

  test('Events declares each event as a property whose type is its payload, void for none', () => {
    const demo = load(`export interface Events {
  /**
   * The thing changed.
   * @zh 东西变了。
   * @delivery broadcast
   */
  changed: ChangedPayload;
  /**
   * A frame.
   * @delivery owner
   * @customName
   */
  frame: void;
}
interface ChangedPayload {
  /** How many. */
  count: number;
}
`);
    assert.deepEqual(demo.methods, []);
    const [changed, frame] = demo.events;
    assert.deepEqual([changed.name, changed.delivery, changed.descriptionZh, changed.payload.properties.map((p) => p.key)], ['demo:changed', 'broadcast', '东西变了。', ['count']]);
    assert.deepEqual([frame.customName, frame.payload.properties], [true, []]);
    // The payload interface is the event's own shape, not a shared type.
    assert.equal(demo.types.has('ChangedPayload'), false);
  });
  rejects('a type error', api('run(params: Missing): void;', '/** x */\na: string;'), /does not type-check[\s\S]*Missing/);
  rejects('a non-JSON default', api('run(params: P): void;', '/**\n * x\n * @default fast\n */\na?: string;'), /@default needs a JSON value/);
  rejects('@skipInvalid with a value', api('run(params: P): void;', '/**\n * x\n * @security Read\n * @skipInvalid yes\n */\na?: string[];'), /@skipInvalid takes no value/);
  rejects('@pathKey without a value', api('run(params: P): void;', '/**\n * x\n * @security Read\n * @pathKey\n */\na?: string[];'), /@pathKey needs a value/);
  rejects('@pathKey with a malformed pair', api('run(params: P): void;', '/**\n * x\n * @pathKey source=Read destination\n */\na?: string[];'), /@pathKey takes one member or "member=Level" pairs/);
  rejects('@pathKey naming a member twice', api('run(params: P): void;', '/**\n * x\n * @pathKey source=Read source=FileWrite\n */\na?: string[];'), /@pathKey names "source" twice/);
});

// ── Shared types: the `types` registry, extends, Partial, recursion and common.ts ──────────

const COMMON_FILE = 'src/api/schema/common.ts';
const str = (description) => ({ type: 'string', description });
const int = (description) => ({ type: 'integer', description });
const named = (name, properties, required, extra = {}) => ({ type: 'object', 'x-name': name, additionalProperties: false, required, properties, ...extra });
const TRACK = named('Track', { path: str('Path.'), title: str('Title.') }, ['path', 'title'], { 'x-common': true, 'x-type-description': 'A track.', 'x-type-description-zh': '曲目。' });
const TRACK_PARTIAL = named('TrackPartial', { path: str('Path.'), title: str('Title.') }, [], { 'x-common': true, 'x-partial-of': 'Track', 'x-type-description': 'A track.', 'x-type-description-zh': '曲目。' });
const NULLABLE = named('Nullable', { note: { type: 'string', nullable: true, description: 'Note.' } }, ['note'], { 'x-common': true });
const MENU_ITEM = named('MenuItem', { label: str('Label.'), submenu: { type: 'array', items: { type: 'object', 'x-ref': 'MenuItem', 'x-common': true }, description: 'Submenu.' } }, ['label'], { 'x-common': true });
const PLAYLIST_ROW = named('PlaylistRow', { index: int('Row number.') }, ['index'], { 'x-extends': 'Track', 'x-extends-common': true });
// The row plus extra keys is its own interface, so the bare row keeps its struct.
const PLAYLIST_ROW_COLUMNS = { type: 'object', 'x-name': 'PlaylistRowColumns', 'x-extends': 'PlaylistRow', additionalProperties: false };
// A params row is a namespace's own interface: shared types are results only.
const LOCAL_ROW = named('LocalRow', { index: int('Row number.') }, ['index']);
const result = (properties, required) => ({ type: 'object', additionalProperties: false, required, properties });
const ref = (name, common = false) => ({ type: 'object', 'x-ref': name, ...(common ? { 'x-common': true } : {}) });
const arr = (items, description) => ({ type: 'array', items, description });

function sharedFixture() {
  const common = parseNamespace({ namespace: 'common', methods: {}, types: { Track: TRACK, TrackPartial: TRACK_PARTIAL, Nullable: NULLABLE, MenuItem: MENU_ITEM } }, COMMON_FILE);
  const demo = parseNamespace({
    namespace: 'demo',
    methods: {
      list: method(undefined, result({
        rows: { type: 'array', items: PLAYLIST_ROW, description: 'Rows.' },
        tracks: { type: 'array', items: { ...TRACK, description: 'Tracks.' }, description: 'Tracks.' },
        partial: { type: 'array', items: TRACK_PARTIAL, description: 'Partial rows.' },
        menu: { type: 'array', items: MENU_ITEM, description: 'Menu.' },
        extra: { type: 'array', items: { ...PLAYLIST_ROW_COLUMNS, additionalProperties: { type: 'string' } }, description: 'Rows with columns.' },
      }, ['rows', 'tracks', 'partial', 'menu', 'extra'])),
      add: method(params({ rows: { type: 'array', items: LOCAL_ROW, description: 'Rows.' } }, ['rows'])),
    },
    types: { PlaylistRow: PLAYLIST_ROW, PlaylistRowColumns: PLAYLIST_ROW_COLUMNS, LocalRow: LOCAL_ROW, Track: TRACK, TrackPartial: TRACK_PARTIAL, MenuItem: MENU_ITEM },
  }, FILE);
  return { common, demo, namespaces: [common, demo] };
}

describe('api-schema · shared types in the IR', () => {
  const base = named('Base', { id: int('Id.') }, ['id']);
  const row = named('Row', { note: str('Note.') }, ['note'], { 'x-extends': 'Base' });
  const doc = (methods, types) => parseNamespace({ namespace: 'demo', methods, types }, FILE);
  const rejects = (label, fn, pattern) => test(label, () => assert.throws(fn, (e) => e instanceof SchemaError && pattern.test(e.message), `expected ${pattern}`));

  test('a namespace lists its named types, and allProperties puts the extended members first', () => {
    const namespace = doc({ list: method(undefined, result({ rows: { type: 'array', items: row, description: 'Rows.' } }, ['rows'])) }, { Base: base, Row: row });
    assert.deepEqual([...namespace.types.keys()], ['Base', 'Row']);
    const resolve = (name, common) => findType(name, namespace, [namespace], common);
    assert.deepEqual(allProperties(namespace.types.get('Row'), resolve).map((p) => p.key), ['id', 'note']);
    assert.equal(findType('Track', namespace, sharedFixture().namespaces).name, 'Track');
  });

  test('common.ts is the namespace of shared types and declares no methods', () => {
    const { common } = sharedFixture();
    assert.deepEqual([common.methods, [...common.types.keys()]], [[], ['Track', 'TrackPartial', 'Nullable', 'MenuItem']]);
    assert.equal(common.types.get('Track').typeDescriptionZh, '曲目。');
    assert.throws(() => parseNamespace({ namespace: 'common', methods: { ping: method() }, types: {} }, COMMON_FILE), /shared types only/);
  });

  test('a recursive use is accepted inside an array only', () => {
    const item = named('Item', { label: str('Label.'), children: { type: 'array', items: { type: 'object', 'x-ref': 'Item' }, description: 'Children.' } }, ['label']);
    const tree = doc({ get: method(undefined, result({ items: { type: 'array', items: item, description: 'Items.' } }, ['items'])) }, { Item: item });
    assert.deepEqual([tree.types.get('Item').properties[1].items.kind, tree.types.get('Item').properties[1].items.name], ['ref', 'Item']);
    assert.throws(() => doc({ get: method(undefined, result({ parent: { type: 'object', 'x-ref': 'Item', description: 'x' } })) }), /must be inside an array/);
    assert.throws(() => doc({ get: method(undefined, result({ byId: { type: 'object', additionalProperties: { type: 'object', 'x-ref': 'Item' }, description: 'x' } })) }), /not a map/);
  });

  rejects('x-extends without x-name', () => doc({ run: method(undefined, result({ r: { type: 'object', 'x-extends': 'Base', additionalProperties: false, properties: { a: str('x') }, description: 'x' } })) }), /"x-extends" needs "x-name"/);
  rejects('an interface extending itself', () => doc({ run: method(undefined, result({ r: { ...row, 'x-extends': 'Row', description: 'x' } })) }), /cannot extend itself/);
  rejects('an extended interface the namespace does not hold', () => doc({ run: method(undefined, result({ r: { ...row, description: 'x' } })) }, { Row: row }), /which "types" does not hold/);
  rejects('a Partial with a required member', () => doc({ run: method(undefined, result({ r: { ...named('BasePartial', { id: int('x') }, ['id'], { 'x-partial-of': 'Base' }), description: 'x' } })) }), /every member of a Partial is optional/);
  rejects('x-common without x-name', () => doc({ run: method(undefined, result({ r: { type: 'object', 'x-common': true, additionalProperties: false, properties: { a: str('x') }, description: 'x' } })) }), /"x-common" needs "x-name"/);
  rejects('a types entry under another name', () => doc({ run: method() }, { Other: base }), /must be an object with "x-name": "Other"/);

  // Shared types are results only: a path field inside one would skip the security check.
  rejects('a common type in params', () => doc({ run: method(params({ t: { ...TRACK, description: 'x' } }, ['t'])) }), /Track comes from common\.ts; shared types are results only/);
  rejects('a common type as array items in params', () => doc({ run: method(params({ t: arr(TRACK, 'x') }, ['t'])) }), /shared types are results only/);
  rejects('a common type referenced from params', () => doc({ run: method(params({ t: arr(ref('MenuItem', true), 'x') }, ['t'])) }), /MenuItem comes from common\.ts; shared types are results only/);
  rejects('an interface extending a common type in params', () => doc({ run: method(params({ rows: arr(PLAYLIST_ROW, 'x') }, ['rows'])) }, { PlaylistRow: PLAYLIST_ROW }), /PlaylistRow extends Track; Track comes from common\.ts; shared types are results only/);
  // A Partial of such an interface carries the same members, and so does an interface
  // deriving from it.
  rejects('a Partial of an interface extending a common type in params', () => doc({ run: method(params({ rows: arr(named('PlaylistRowPartial', { index: int('x') }, [], { 'x-partial-of': 'PlaylistRow' }), 'x') }, ['rows'])) }, { PlaylistRow: PLAYLIST_ROW }), /PlaylistRowPartial carries the members of Track; Track comes from common\.ts/);
  rejects('an interface deriving from one that extends a common type in params', () => doc({ run: method(params({ rows: arr(named('Wide', { note: str('x') }, ['note'], { 'x-extends': 'PlaylistRow' }), 'x') }, ['rows'])) }, { PlaylistRow: PLAYLIST_ROW }), /Wide carries the members of Track; Track comes from common\.ts/);
  // The rules follow a reference into the interface it names.
  rejects('a required nullable member reached through a reference', () => {
    const d = named('D', { cs: arr(ref('C'), 'x') }, ['cs']);
    const c = named('C', { ds: arr(d, 'x'), note: { type: 'string', nullable: true, description: 'x' } }, ['ds', 'note']);
    return doc({ run: method(params({ d: arr(d, 'x') }, ['d'])) }, { C: c, D: d });
  }, /run\.params\.properties\.d\.items\.properties\.cs\.items<C>\.properties\.note: a required parameter cannot be null/);

  // The params rules reach members inherited from a local base, which "types" parsed under
  // the looser rules.
  const nullableBase = named('NullableBase', { note: { type: 'string', nullable: true, description: 'Note.' } }, ['note']);
  const derived = named('Derived', { id: int('Id.') }, ['id'], { 'x-extends': 'NullableBase' });
  rejects('an inherited required member that may be null, in params', () => doc({ run: method(params({ rows: arr(derived, 'x') }, ['rows'])) }, { NullableBase: nullableBase, Derived: derived }), /rows\.items\.properties\.note: a required parameter cannot be null/);
  test('the same base is fine on the result side', () => {
    const namespace = doc({ run: method(undefined, result({ rows: arr(derived, 'x') }, ['rows'])) }, { NullableBase: nullableBase, Derived: derived });
    assert.deepEqual([...namespace.types.keys()], ['NullableBase', 'Derived']);
  });
});

describe('api-schema · C++ shared structs', () => {
  const { common, demo, namespaces } = sharedFixture();
  const header = emitCppHeader(demo, namespaces);
  const commonHeader = emitCppHeader(common, namespaces);

  test('a namespace names common types through api::common and includes the common header', () => {
    assert.match(header, /#include "api\/generated\/CommonSchema\.h"/);
    assert.match(header, /std::vector<api::common::Track> tracks\{\};/);
    assert.match(header, /std::vector<api::common::TrackPartial> partial\{\};/);
    assert.match(header, /std::vector<api::common::MenuItem> menu\{\};/);
    assert.doesNotMatch(header, /struct Track \{/);
    assert.doesNotMatch(commonHeader, /CommonSchema\.h/);
  });

  test('an interface extending a common one derives from its struct and writes every member', () => {
    assert.match(header, /struct PlaylistRow : api::common::Track \{\n    static constexpr std::array<std::string_view, 3> kFields\{\{"path", "title", "index"\}\};\n\n    std::int64_t index\{\};\n\};/);
    // Results only: no parser for it, nor for anything else that derives from a shared type.
    assert.doesNotMatch(header, /FromJson\(const json& j, PlaylistRow/);
    assert.match(header, /inline json ToJson\(const PlaylistRow& v\) \{\n    json j = json::object\(\);\n    api::results::Put\(j, "path", v\.path\);\n    api::results::Put\(j, "title", v\.title\);\n    api::results::Put\(j, "index", v\.index\);/);
    // Extra keys on top of the derived row: its own struct, deriving from the row, with the map.
    assert.match(header, /struct PlaylistRowColumns : PlaylistRow \{\n    static constexpr std::array<std::string_view, 3> kFields\{\{"path", "title", "index"\}\};\n\n    std::map<std::string, std::string> additional\{\};\n\};/);
    assert.match(header, /inline json ToJson\(const PlaylistRowColumns& v\) \{\n    json j = json::object\(\);\n    api::results::PutAll\(j, v\.additional\);\n    api::results::Put\(j, "path", v\.path\);/);
    assert.ok(header.indexOf('struct PlaylistRow :') < header.indexOf('struct PlaylistRowColumns :'));
  });

  test('a bare interface and the same interface with extra keys cannot share one struct', () => {
    const row = named('Row', { a: str('x') }, ['a']);
    const clash = parseNamespace({
      namespace: 'demo',
      methods: { list: method(undefined, result({ a: { type: 'array', items: row, description: 'x' }, b: { type: 'array', items: { ...row, additionalProperties: { type: 'string' } }, description: 'x' } }, ['a', 'b'])) },
      types: { Row: row },
    }, FILE);
    assert.throws(() => emitCppHeader(clash), /already produces struct Row; give the version with extra keys its own interface \(interface RowColumns extends Row \{\}\)/);
  });

  // A definition, as opposed to the declaration that precedes every definition.
  const fromJsonDef = (name) => new RegExp(`inline bool FromJson\\(const json& j, ${name}& out, std::string& error, const std::string& where\\) \\{\\n([\\s\\S]*?)\\n\\}`);

  test('a local base struct is defined before the struct that extends it, and both sides cover the base members', () => {
    const base = named('Base', { id: int('Id.') }, ['id']);
    const row = named('Row', { note: str('Note.') }, ['note'], { 'x-extends': 'Base' });
    const local = parseNamespace({
      namespace: 'demo',
      methods: { list: method(params({ rows: arr(row, 'Rows.') }, ['rows']), result({ rows: arr(row, 'Rows.') }, ['rows'])) },
      types: { Base: base, Row: row },
    }, FILE);
    const h = emitCppHeader(local);
    const baseAt = h.indexOf('struct Base {');
    const rowAt = h.indexOf('struct Row : Base {');
    assert.ok(baseAt >= 0 && rowAt >= 0 && baseAt < rowAt, h);
    assert.match(h, /struct Row : Base \{\n    static constexpr std::array<std::string_view, 2> kFields\{\{"id", "note"\}\};\n\n    std::string note\{\};\n\};/);
    assert.match(h, fromJsonDef('Row'));
    assert.match(h.match(fromJsonDef('Row'))[1], /r\.OnlyKeys\(\{"id", "note"\}\)\n        && r\.Required\("id", out\.id\)\n        && r\.Required\("note", out\.note\);/);
    assert.match(h, /inline json ToJson\(const Row& v\) \{\n    json j = json::object\(\);\n    api::results::Put\(j, "id", v\.id\);\n    api::results::Put\(j, "note", v\.note\);/);
  });

  test('the common header defines every shared type once, with ToJson only', () => {
    assert.match(commonHeader, /namespace api::common \{/);
    assert.match(commonHeader, /struct Track \{\n    static constexpr std::array<std::string_view, 2> kFields\{\{"path", "title"\}\};/);
    assert.match(commonHeader, /inline json ToJson\(const Track& v\)/);
    assert.doesNotMatch(commonHeader, /FromJson/);
    assert.match(commonHeader, /inline json ToJson\(const Nullable& v\)[\s\S]*?PutNullable\(j, "note", v\.note\)/);
    assert.match(commonHeader, /struct TrackPartial \{[\s\S]*?std::optional<std::string> path\{\};\n    std::optional<std::string> title\{\};\n\};/);
    assert.equal(commonHeader.match(/struct Track \{/g).length, 1);
  });

  test('a recursive interface holds a vector of itself, on either side', () => {
    assert.match(commonHeader, /struct MenuItem \{[\s\S]*?std::string label\{\};\n    std::optional<std::vector<MenuItem>> submenu\{\};\n\};/);
    assert.match(commonHeader, /api::results::Put\(j, "submenu", v\.submenu\)/);
    const item = named('Item', { label: str('Label.'), children: arr(ref('Item'), 'Children.') }, ['label']);
    const h = emitCppHeader(parseNamespace({ namespace: 'demo', methods: { set: method(params({ items: arr(item, 'Items.') }, ['items'])) }, types: { Item: item } }, FILE));
    assert.match(h, /struct Item \{\n    std::string label\{\};\n    std::optional<std::vector<Item>> children\{\};\n\};/);
    assert.match(h.match(fromJsonDef('Item'))[1], /r\.Optional\("children", out\.children\)/);
  });

  test('every struct and function is declared before the definitions, so definition order cannot matter', () => {
    // B is defined before A (children first), yet holds a vector of A; Node recurses
    // through its anonymous child object.
    const b = named('B', { as: arr(ref('A'), 'As.') }, ['as']);
    const a = named('A', { bs: arr(b, 'Bs.') }, ['bs']);
    const node = named('Node', { child: { type: 'object', additionalProperties: false, required: ['items'], properties: { items: arr(ref('Node'), 'Items.') }, description: 'Child.' } }, []);
    const h = emitCppHeader(parseNamespace({
      namespace: 'demo',
      methods: { get: method(params({ pair: arr(a, 'Pair.'), tree: arr(node, 'Tree.') }, ['pair', 'tree']), result({ pair: arr(a, 'Pair.') }, ['pair'])) },
      types: { A: a, B: b, Node: node },
    }, FILE));
    // Children come first, so B (nested in A) is declared and defined before A.
    const decl = h.indexOf('struct B;\nstruct A;');
    assert.ok(decl >= 0 && decl < h.indexOf('struct B {') && h.indexOf('struct B {') < h.indexOf('struct A {'), h);
    assert.match(h, /std::vector<A> as\{\};/);
    assert.match(h, /struct NodeChild \{\n    std::vector<Node> items\{\};\n\};/);
    assert.match(h, /inline bool FromJson\(const json& j, B& out, std::string& error, const std::string& where = \{\}\);\ninline json ToJson\(const B& v\);/);
    assert.match(h, /inline bool FromJson\(const json& j, GetParams& out, std::string& error, const std::string& where = \{\}\);/);
    // The definitions carry no default: it lives on the declaration.
    assert.doesNotMatch(h, /where = \{\}\) \{/);
  });

  test('a role reaches the structs nested below the one it lands on', () => {
    // Item is first reached from a result, then from params: its anonymous meta object
    // needs a parser too, or the parser of Item would not compile.
    const item = named('Item', { meta: { type: 'object', additionalProperties: false, required: ['n'], properties: { n: int('N.') }, description: 'Meta.' } }, ['meta']);
    const h = emitCppHeader(ns({
      list: method(undefined, result({ items: arr(item, 'Items.') }, ['items'])),
      add: method(params({ items: arr(item, 'Items.') }, ['items'])),
    }));
    assert.match(h, fromJsonDef('ItemMeta'));
    assert.match(h, fromJsonDef('Item'));
    assert.match(h, /inline json ToJson\(const ItemMeta& v\)/);
    // And the params checks apply on that second visit: a constrained array item is not
    // something the parser handles, wherever the shape was first seen.
    const constrained = named('Constrained', { tags: arr({ type: 'string', minLength: 1 }, 'Tags.') }, ['tags']);
    assert.throws(() => emitCppHeader(ns({
      list: method(undefined, result({ items: arr(constrained, 'Items.') }, ['items'])),
      add: method(params({ items: arr(constrained, 'Items.') }, ['items'])),
    })), /add\.params\.items\.items\.tags\.items: constraints on array items/);
  });

  test('a struct deriving from one that lists kFields lists its own', () => {
    const base = named('Base', { id: int('Id.') }, ['id']);
    const derived = named('Derived', { extra: str('Extra.') }, ['extra'], { 'x-extends': 'Base' });
    const h = emitCppHeader(parseNamespace({
      namespace: 'demo',
      methods: { list: method(params({ rows: arr(derived, 'Rows.') }, ['rows']), result({ rows: arr(base, 'Rows.') }, ['rows'])) },
      types: { Base: base, Derived: derived },
    }, FILE));
    assert.match(h, /struct Derived : Base \{\n    static constexpr std::array<std::string_view, 2> kFields\{\{"id", "extra"\}\};\n\n    std::string extra\{\};\n\};/);
    assert.doesNotMatch(h, /ToJson\(const Derived&/);
  });

  test('a type only ever referenced has no struct to point at', () => {
    const loop = named('Loop', { items: arr(ref('Loop'), 'Items.') }, ['items']);
    assert.throws(() => emitCppHeader(parseNamespace({
      namespace: 'demo',
      methods: { get: method(undefined, result({ items: arr(ref('Loop'), 'Items.') }, ['items'])) },
      types: { Loop: loop },
    }, FILE)), /get\.result\.items\.items: Loop is only used through a reference/);
  });

  test('a role reaches the struct a reference points at', () => {
    // C holds D, and D refers back to C. A result lands on C, so both are written; params
    // land on D only, and the reference is the sole way params reach C: it must get a parser
    // too, or D's parser would not compile.
    const d = named('D', { cs: arr(ref('C'), 'Cs.') }, ['cs']);
    const c = named('C', { ds: arr(d, 'Ds.') }, ['ds']);
    const h = emitCppHeader(parseNamespace({
      namespace: 'demo',
      methods: {
        get: method(undefined, result({ c: arr(c, 'C.') }, ['c'])),
        set: method(params({ d: arr(d, 'D.') }, ['d'])),
      },
      types: { C: c, D: d },
    }, FILE));
    assert.match(h, fromJsonDef('C'));
    assert.match(h, fromJsonDef('D'));
    assert.match(h, /inline json ToJson\(const C& v\)/);
    assert.match(h, /inline json ToJson\(const D& v\)/);
    // The params checks run on that visit as well, wherever the reference sits.
    const loose = named('Loose', { cs: arr(ref('Constrained'), 'Cs.') }, ['cs']);
    const constrained = named('Constrained', { tags: arr({ type: 'string', minLength: 1 }, 'Tags.'), back: arr(loose, 'Back.') }, ['tags']);
    assert.throws(() => emitCppHeader(parseNamespace({
      namespace: 'demo',
      methods: {
        get: method(undefined, result({ c: arr(constrained, 'C.') }, ['c'])),
        set: method(params({ l: arr(loose, 'L.') }, ['l'])),
      },
      types: { Constrained: constrained, Loose: loose },
    }, FILE)), /set\.params\.l\.items\.cs\.items\.tags\.items: constraints on array items/);
  });

  test('common.ts without types produces no header', () => {
    assert.equal(hasCppHeader(parseNamespace({ namespace: 'common', methods: {}, types: {} }, COMMON_FILE)), false);
    assert.equal(hasCppHeader(common), true);
    assert.equal(hasCppHeader(demo), true);
  });

  test('a params-only struct lists no kFields', () => {
    const h = emitCppHeader(ns({ run: method(params({ a: str('x') })) }));
    assert.doesNotMatch(h, /kFields/);
  });
});

describe('api-schema · TypeScript shared interfaces', () => {
  const { common, demo, namespaces } = sharedFixture();
  const [list, add] = demo.methods;

  test('declared blocks name the interfaces and report which ones they use', () => {
    const used = new Set();
    const response = emitTsResponse(list, 'DemoListResponse', used);
    assert.match(response, /\n    rows: PlaylistRow\[\];\n/);
    assert.match(response, /\n    tracks: Track\[\];\n/);
    assert.match(response, /\n    menu: MenuItem\[\];\n/);
    assert.match(response, /\n    extra: Array<PlaylistRowColumns & Record<string, string>>;\n/);
    assert.deepEqual([...used].sort(), ['MenuItem', 'PlaylistRow', 'PlaylistRowColumns', 'Track', 'TrackPartial']);
    const usedByParams = new Set();
    assert.match(emitTsParams(add, 'DemoAddParams', usedByParams), /\n    rows: LocalRow\[\];\n/);
    assert.deepEqual([...usedByParams], ['LocalRow']);
  });

  test('every named interface is emitted once, common.ts first, extends kept and Partial members optional', () => {
    const blocks = emitTsNamedTypes(namespaces);
    assert.match(blocks[0], /^\/\*\*\n \* A track\.\n \*\/\nexport interface Track \{\n    \/\*\* Path\. \*\/\n    path: string;\n    \/\*\* Title\. \*\/\n    title: string;\n\}$/);
    const text = blocks.join('\n\n');
    assert.match(text, /export interface TrackPartial \{\n    \/\*\* Path\. \*\/\n    path\?: string;\n    \/\*\* Title\. \*\/\n    title\?: string;\n\}/);
    assert.match(text, /export interface MenuItem \{[\s\S]*?submenu\?: MenuItem\[\];\n\}/);
    assert.match(text, /export interface PlaylistRow extends Track \{\n    \/\*\* Row number\. \*\/\n    index: number;\n\}/);
    assert.equal(text.match(/export interface Track \{/g).length, 1);
    assert.match(text, /export interface PlaylistRowColumns extends PlaylistRow \{\n\}/);
    assert.match(text, /export interface LocalRow \{\n    \/\*\* Row number\. \*\/\n    index: number;\n\}/);
    assert.equal(blocks.length, 7);
  });

  test('two namespaces may share a name only with the same shape', () => {
    const rowA = named('Row', { a: str('x') }, ['a']);
    const rowB = named('Row', { b: str('x') }, ['b']);
    const withRow = (nsName, row) => parseNamespace({ namespace: nsName, methods: { list: method(undefined, result({ rows: { type: 'array', items: row, description: 'x' } }, ['rows'])) }, types: { Row: row } }, `src/api/schema/${nsName}.json`);
    assert.equal(emitTsNamedTypes([withRow('one', rowA), withRow('two', rowA)]).length, 1);
    assert.throws(() => emitTsNamedTypes([withRow('one', rowA), withRow('two', rowB)]), /type Row is also declared, with another shape, in src\/api\/schema\/one\.json; the SDK exports type names flat, so rename one/);
  });
});

describe('api-schema · docs for shared types', () => {
  function tmpRepo(pages) {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'api-schema-types-'));
    for (const [rel, text] of Object.entries(pages)) {
      const abs = path.join(root, 'docs/vitepress', rel);
      fs.mkdirSync(path.dirname(abs), { recursive: true });
      fs.writeFileSync(abs, text);
    }
    return root;
  }
  const region = (id) => `<!-- api-schema:begin ${id} -->\nstale\n<!-- api-schema:end -->`;
  const typesPage = ['type:Track', 'type:TrackPartial', 'type:Nullable', 'type:MenuItem'].map(region).join('\n\n');
  const { namespaces } = sharedFixture();

  test('tables link common types to the types page and list an extended base as one row', () => {
    const root = tmpRepo({ 'api/demo.md': `${region('demo.list')}\n${region('demo.add')}\n`, 'zh/api/demo.md': `${region('demo.list')}\n${region('demo.add')}\n`, 'reference/types.md': `${typesPage}\n`, 'zh/reference/types.md': `${typesPage}\n` });
    const { changes, problems } = planDocs(root, namespaces);
    assert.deepEqual(problems, []);
    const en = changes.find((c) => c.file === 'docs/vitepress/api/demo.md').next;
    const zh = changes.find((c) => c.file === 'docs/vitepress/zh/api/demo.md').next;
    assert.match(en, /\| `rows` \| `PlaylistRow\[\]` \| Rows\. \|\n\| `rows\[\]\.…` \| \[Track\]\(\.\.\/reference\/types\.md#track\) \| Every field of \[Track\]\(\.\.\/reference\/types\.md#track\)\. \|\n\| `rows\[\]\.index` \| `integer` \| Row number\. \|/);
    assert.match(en, /\| `tracks` \| \[Track\[\]\]\(\.\.\/reference\/types\.md#track\) \| Tracks\. \|/);
    assert.match(en, /\| `menu` \| \[MenuItem\[\]\]\(\.\.\/reference\/types\.md#menuitem\) \| Menu\. \|/);
    // A local interface extending a local one lists every member; the common base is one row.
    assert.match(en, /\| `extra` \| `\(PlaylistRowColumns & Record<string, string>\)\[\]` \| Rows with columns\. \|\n\| `extra\[\]\.…` \| \[Track\]\(\.\.\/reference\/types\.md#track\) \| Every field of \[Track\]\(\.\.\/reference\/types\.md#track\)\. \|\n\| `extra\[\]\.index` \| `integer` \| Row number\. \|\n\| `extra\[\]\.\[each key\]` \| `string` \|  \|/);
    assert.match(en, /\| `rows` \| `LocalRow\[\]` \| Yes \| Rows\. \|\n\| `rows\[\]\.index` \| `integer` \| Yes \| Row number\. \|/);
    assert.match(zh, /\| `rows\[\]\.…` \| \[Track\]\(\.\.\/reference\/types\.md#track\) \| 包含 \[Track\]\(\.\.\/reference\/types\.md#track\) 的全部字段。 \|/);
    const types = changes.find((c) => c.file === 'docs/vitepress/reference/types.md').next;
    assert.match(types, /<!-- api-schema:begin type:Track -->\nA track\.\n\n\| Field \| Type \| Description \|\n\| --- \| --- \| --- \|\n\| `path` \| `string` \| Path\. \|\n\| `title` \| `string` \| Title\. \|\n<!-- api-schema:end -->/);
    assert.match(types, /type:TrackPartial -->\nA track\.\n\nEvery field of \[Track\]\(\.\/types\.md#track\), each optional\.\n<!-- api-schema:end -->/);
    assert.match(types, /\| `submenu` \| \[MenuItem\[\]\]\(\.\/types\.md#menuitem\) \| Submenu\. \|/);
    assert.match(types, /<!-- api-schema:begin type:Nullable -->\n\| Field \| Type \| Description \|\n\| --- \| --- \| --- \|\n\| `note` \| `string \\\| null` \| Note\. \|/);
    const zhTypes = changes.find((c) => c.file === 'docs/vitepress/zh/reference/types.md').next;
    assert.match(zhTypes, /type:Track -->\n曲目。\n\n\| 字段 \| 类型 \| 说明 \|/);
  });

  test('a missing type region and a region for an unknown type are reported', () => {
    const root = tmpRepo({ 'api/demo.md': `${region('demo.list')}\n${region('demo.add')}\n`, 'zh/api/demo.md': `${region('demo.list')}\n${region('demo.add')}\n`, 'reference/types.md': `${region('type:Track')}\n${region('type:Ghost')}\n` });
    const { problems } = planDocs(root, namespaces);
    assert.ok(problems.some((p) => /no zh docs region for type Track \(docs\/vitepress\/zh\/reference\/types\.md\)/.test(p)), problems.join('\n'));
    assert.ok(problems.some((p) => /no en docs region for type MenuItem/.test(p)), problems.join('\n'));
    assert.ok(problems.some((p) => /region for type Ghost, which common\.ts does not declare/.test(p)), problems.join('\n'));
  });
});

describe('api-schema · TypeScript declarations of shared types', () => {
  const TSCONFIG = { compilerOptions: { strict: true, noEmit: true, target: 'ES2022', module: 'ESNext', moduleResolution: 'Bundler', lib: ['ES2022'], types: [], skipLibCheck: true }, include: ['*.ts'] };
  const COMMON = `export type Int = number;
export type Json = unknown;
/**
 * A track.
 * @zh 曲目。
 */
export interface Track {
  /** Path. */
  path: string;
  /** Title. */
  title: string;
}
/**
 * Some of a track.
 * @zh 部分曲目。
 */
export type TrackPartial = Partial<Track>;
export type TrackField = keyof Track;
/** A menu entry. */
export interface MenuItem {
  /** Label. */
  label: string;
  /** Submenu. */
  submenu?: MenuItem[];
}
`;
  function repo(demo, common = COMMON) {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'api-schema-ts-shared-'));
    const dir = path.join(root, 'src/api/schema');
    fs.mkdirSync(dir, { recursive: true });
    fs.writeFileSync(path.join(dir, 'tsconfig.json'), JSON.stringify(TSCONFIG));
    fs.writeFileSync(path.join(dir, 'common.ts'), common);
    fs.writeFileSync(path.join(dir, 'demo.ts'), demo);
    return loadNamespaces(root);
  }
  const DEMO = `import type { Int, Track, TrackPartial, TrackField, MenuItem } from './common';
export interface Api {
  /** List. */
  list(params: ListParams): ListResult;
}
interface ListParams {
  /** Sort key. */
  sortBy?: TrackField;
}
/** A row. */
interface Row extends Track {
  /** Index. */
  index: Int;
}
interface RowColumns extends Row {}
interface ListResult {
  /** Rows. */
  rows: Row[];
  /** Partial rows. */
  partial: TrackPartial[];
  /** Menu. */
  menu: MenuItem[];
  /** Rows with columns. */
  extra: (RowColumns & Record<string, string>)[];
  /** Local partial. */
  sparse: Partial<Row>[];
}
`;

  test('extends, keyof, Partial, recursion and common.ts interfaces become the IR', () => {
    const namespaces = repo(DEMO);
    const common = namespaces.find((n) => n.namespace === 'common');
    const demo = namespaces.find((n) => n.namespace === 'demo');
    assert.deepEqual([...common.types.keys()], ['Track', 'TrackPartial', 'MenuItem']);
    assert.deepEqual([common.types.get('Track').common, common.types.get('Track').typeDescription, common.types.get('Track').typeDescriptionZh], [true, 'A track.', '曲目。']);
    assert.deepEqual([common.types.get('TrackPartial').partialOf, common.types.get('TrackPartial').properties.map((p) => [p.key, p.required])], ['Track', [['path', false], ['title', false]]]);
    // The alias's own JSDoc describes the Partial; without one it would inherit Track's.
    assert.deepEqual([common.types.get('TrackPartial').typeDescription, common.types.get('TrackPartial').typeDescriptionZh], ['Some of a track.', '部分曲目。']);
    assert.deepEqual([common.types.get('MenuItem').properties[1].items.kind, common.types.get('MenuItem').properties[1].items.name], ['ref', 'MenuItem']);
    // The namespace registers the shared types it uses and its own; not its Params/Result interfaces.
    assert.deepEqual([...demo.types.keys()].sort(), ['MenuItem', 'Row', 'RowColumns', 'RowPartial', 'Track', 'TrackPartial']);
    const row = demo.types.get('Row');
    assert.deepEqual([row.extends, row.extendsCommon, row.common ?? false, row.typeDescription, row.properties.map((p) => p.key)], ['Track', true, false, 'A row.', ['index']]);
    const [list] = demo.methods;
    assert.deepEqual(list.params.properties[0].enum, ['path', 'title']);
    // The same declarations go through the C++ emitter: what the IR accepts, it generates.
    const header = emitCppHeader(demo, namespaces);
    assert.match(header, /r\.OneOf\("sortBy", out\.sortBy, \{"path", "title"\}\)/);
    assert.match(header, /struct Row : api::common::Track \{/);
    assert.match(header, /struct RowPartial \{/);
    const [rows, partial, menu, extra, sparse] = list.result.properties;
    assert.deepEqual([rows.items.name, rows.items.extends], ['Row', 'Track']);
    assert.deepEqual([partial.items.name, partial.items.common, partial.items.partialOf], ['TrackPartial', true, 'Track']);
    assert.deepEqual([menu.items.name, menu.items.common, menu.items.properties[1].items.common], ['MenuItem', true, true]);
    assert.deepEqual([extra.items.name, extra.items.extends, extra.items.additional.kind], ['RowColumns', 'Row', 'string']);
    // Partial of a local interface flattens the base: RowPartial holds path, title and index.
    assert.deepEqual([sparse.items.name, sparse.items.common ?? false, sparse.items.properties.map((p) => p.key)], ['RowPartial', false, ['path', 'title', 'index']]);
  });

  const rejects = (label, demo, pattern, common) =>
    test(label, () => assert.throws(() => repo(demo, common), (e) => e instanceof SchemaError && pattern.test(e.message), `expected ${pattern}`));
  const withResult = (body) => `import type { Track } from './common';\nexport interface Api {\n  /** List. */\n  list(): R;\n}\n${body}\n`;

  rejects('redeclaring a member of the extended interface', withResult('interface Row extends Track {\n  /** x */\n  path: string;\n}\ninterface R {\n  /** x */\n  rows: Row[];\n}'), /path is already declared by Track/);
  rejects('extra keys on a common interface', withResult('interface R {\n  /** x */\n  rows: (Track & Record<string, string>)[];\n}'), /Track from common\.ts cannot take extra keys here/);
  rejects('a recursive member outside an array', withResult('interface Node {\n  /** x */\n  parent?: Node;\n}\ninterface R {\n  /** x */\n  nodes: Node[];\n}'), /must be inside an array/);
  rejects('a Partial of a common interface that common.ts does not declare', withResult('interface R {\n  /** x */\n  rows: Partial<Track>[];\n}'), /needs `export type TrackPartial = Partial<Track>;` in common\.ts/, 'export type Int = number;\nexport type Json = unknown;\nexport interface Track {\n  /** x */\n  path: string;\n}\n');
  rejects('a Partial alias under the wrong name', withResult('interface R {\n  /** x */\n  rows: Track[];\n}'), /the Partial of Track must be named TrackPartial/, 'export type Int = number;\nexport type Json = unknown;\nexport interface Track {\n  /** x */\n  path: string;\n}\nexport type Sparse = Partial<Track>;\n');
  rejects('keyof of something that is not an interface', withResult('type K = keyof string;\ninterface R {\n  /** x */\n  k: K;\n}'), /keyof must name an interface/);
  rejects('a params interface that extends another', `import type { Track } from './common';\nexport interface Api {\n  /** List. */\n  list(params: P): void;\n}\ninterface P extends Track {\n  /** x */\n  a: string;\n}\n`, /cannot extend another interface or be a Partial/);
  rejects('a result interface that recurses into itself', withResult('interface R {\n  /** x */\n  items: R[];\n}'), /R: a method's params or result interface cannot be used recursively/);

  test('a keyof array in params reaches the C++ emitter, which cannot constrain array items yet', () => {
    const namespaces = repo(`import type { TrackField } from './common';
export interface Api {
  /** List. */
  list(params: P): void;
}
interface P {
  /** Fields. */
  fields?: TrackField[];
}
`);
    const demo = namespaces.find((n) => n.namespace === 'demo');
    assert.deepEqual(demo.methods[0].params.properties[0].items.enum, ['path', 'title']);
    assert.throws(() => emitCppHeader(demo, namespaces), /list\.params\.fields\.items: constraints on array items/);
  });

  test('a params interface reused only through Partial stays a named type', () => {
    const namespaces = repo(`export interface Api {
  /** List. */
  list(params: P): R;
}
interface P {
  /** x */
  a: string;
}
interface R {
  /** x */
  sparse: Partial<P>[];
}
`);
    const demo = namespaces.find((n) => n.namespace === 'demo');
    assert.deepEqual([...demo.types.keys()].sort(), ['P', 'PPartial']);
    const header = emitCppHeader(demo, namespaces);
    assert.match(header, /struct ListParams \{/);
    assert.match(header, /struct PPartial \{\n    static constexpr std::array<std::string_view, 1> kFields\{\{"a"\}\};\n\n    std::optional<std::string> a\{\};\n\};/);
  });

  test('an event whose payload is a common.ts interface carries that type by name', () => {
    const namespaces = repo(`import type { Track } from './common';
export interface Events {
  /**
   * Playback moved on.
   * @delivery broadcast
   */
  trackChanged: Track;
}
`);
    const demo = namespaces.find((n) => n.namespace === 'demo');
    const [trackChanged] = demo.events;
    assert.deepEqual([trackChanged.sharedPayload, trackChanged.payload], ['Track', null]);
    const header = emitCppHeader(demo, namespaces);
    assert.match(header, /#include "api\/generated\/CommonSchema\.h"/);
    assert.match(header, /struct TrackChanged \{\n    static constexpr const char\* kName = "demo:trackChanged";\n    static constexpr bool kCustomName = false;\n    using Payload = api::common::Track;\n\};/);
    assert.doesNotMatch(header, /TrackChangedPayload/);
    const used = new Set();
    assert.match(emitTsEventPayload(trackChanged, 'DemoTrackChangedPayload', used), /export type DemoTrackChangedPayload = Track;$/);
    assert.deepEqual([...used], ['Track']);
    const common = namespaces.find((n) => n.namespace === 'common');
    const ctx = { resolve: (name, isCommon) => findType(name, demo, namespaces, isCommon), link: (name) => `[${name}](../reference/types.md#${name.toLowerCase()})` };
    assert.match(renderEventRegion(trackChanged, 'en', ctx), /\*\*Payload\*\*\n\nThe payload is a \[Track\]\(\.\.\/reference\/types\.md#track\)\.$/);
    assert.match(renderEventRegion(trackChanged, 'zh', ctx), /载荷是一个 \[Track\]\(\.\.\/reference\/types\.md#track\)。$/);
    assert.ok(common.types.has('Track'));
  });
});

describe('api-schema · events', () => {
  const FILE = 'src/api/schema/demo.json';
  const top = (properties, required = []) => ({ type: 'object', additionalProperties: false, required, properties });
  const demo = parseNamespace({
    namespace: 'demo',
    methods: { ping: { description: 'Ping.' } },
    events: {
      changed: { description: 'The demo changed.', 'x-description-zh': '演示变了。', 'x-delivery': 'broadcast', payload: top({ count: { type: 'integer', description: 'Count.' }, note: { type: 'string', nullable: true, description: 'Note.' } }, ['count', 'note']) },
      reset: { description: 'The demo was reset.', 'x-delivery': 'caller' },
      frame: { description: 'A frame.', 'x-delivery': 'owner', 'x-custom-name': true, payload: top({ success: { type: 'boolean', description: 'Whether it worked.' } }) },
    },
  }, FILE);

  test('an event gets its full name, delivery, custom-name flag and payload', () => {
    const [changed, reset, frame] = demo.events;
    assert.deepEqual([changed.name, changed.delivery, changed.customName, changed.descriptionZh], ['demo:changed', 'broadcast', false, '演示变了。']);
    assert.deepEqual(changed.payload.properties.map((p) => p.key), ['count', 'note']);
    assert.deepEqual([reset.payload.properties, frame.customName], [[], true]);
    // A payload carries no envelope, so it may use the envelope's key names.
    assert.equal(frame.payload.properties[0].key, 'success');
  });

  test('a namespace may declare events only, but not neither', () => {
    const only = parseNamespace({ namespace: 'demo', events: { reset: { description: 'x', 'x-delivery': 'caller' } } }, FILE);
    assert.deepEqual([only.methods, only.events.map((e) => e.name)], [[], ['demo:reset']]);
    assert.throws(() => parseNamespace({ namespace: 'demo' }, FILE), /"methods" must name at least one method, or "events" at least one event/);
  });

  const rejects = (label, events, pattern) =>
    test(label, () => assert.throws(() => parseNamespace({ namespace: 'demo', events }, FILE), (e) => e instanceof SchemaError && pattern.test(e.message)));
  rejects('an unknown delivery', { reset: { description: 'x', 'x-delivery': 'everyone' } }, /"x-delivery" must be one of broadcast, caller, owner, target/);
  rejects('a missing delivery', { reset: { description: 'x' } }, /"x-delivery" must be one of/);
  rejects('a custom-name flag that is not true', { reset: { description: 'x', 'x-delivery': 'caller', 'x-custom-name': false } }, /"x-custom-name" must be true when present/);
  rejects('an event name that is not camelCase', { 'bad-name': { description: 'x', 'x-delivery': 'caller' } }, /event name must be a camelCase identifier/);
  rejects('an event without a description', { reset: { 'x-delivery': 'caller' } }, /"description" is required/);
  test('common.ts declares no events', () => {
    assert.throws(() => parseNamespace({ namespace: 'common', events: { reset: { description: 'x', 'x-delivery': 'caller' } } }, 'src/api/schema/common.ts'), /declares shared types only, not events/);
  });

  test('the header holds a payload struct with ToJson and kFields per event, and a descriptor naming it', () => {
    const header = emitCppHeader(demo);
    assert.match(header, /struct ChangedPayload \{\n    static constexpr std::array<std::string_view, 2> kFields\{\{"count", "note"\}\};/);
    assert.match(header, /inline json ToJson\(const ChangedPayload& v\)/);
    assert.match(header, /struct ResetPayload \{\n    static constexpr std::array<std::string_view, 0> kFields\{\};\n\};/);
    assert.doesNotMatch(header, /FromJson\(const json& j, ChangedPayload&/);
    assert.match(header, /namespace events \{\n\nstruct Changed \{\n    static constexpr const char\* kName = "demo:changed";\n    static constexpr bool kCustomName = false;\n    using Payload = ChangedPayload;\n\};/);
    assert.match(header, /struct Frame \{\n    static constexpr const char\* kName = "demo:frame";\n    static constexpr bool kCustomName = true;/);
  });

  test('the registry lists every declared event and includes only the headers that declare one', () => {
    const quiet = parseNamespace({ namespace: 'quiet', methods: { ping: { description: 'Ping.' } } }, 'src/api/schema/quiet.json');
    const other = parseNamespace({ namespace: 'jitQueue', events: { drained: { description: 'x', 'x-delivery': 'broadcast' } } }, 'src/api/schema/jitQueue.json');
    const registry = emitEventRegistry([demo, quiet, other]);
    assert.match(registry, /#include "api\/generated\/DemoSchema\.h"\n#include "api\/generated\/JitQueueSchema\.h"\n\n/);
    assert.doesNotMatch(registry, /QuietSchema/);
    assert.match(registry, /using All = std::tuple<\n    api::demo::events::Changed,\n    api::demo::events::Reset,\n    api::demo::events::Frame,\n    api::jitQueue::events::Drained>;/);
    assert.equal(emitEventRegistry([quiet]), null);
  });

  test('a payload interface lists its fields, and an empty one is Record<string, never>', () => {
    const [changed, reset] = demo.events;
    const ts = emitTsEventPayload(changed, 'DemoChangedPayload');
    assert.match(ts, /\/\*\*\n \* Payload of the `demo:changed` event\.\n \* The demo changed\.\n \*\/\nexport interface DemoChangedPayload \{/);
    assert.match(ts, /\n    count: number;/);
    assert.match(ts, /\n    note: string \| null;/);
    assert.match(emitTsEventPayload(reset, 'DemoResetPayload'), /export type DemoResetPayload = Record<string, never>;/);
  });

  test('an event region states who receives it and lists the payload', () => {
    const [changed, reset, frame] = demo.events;
    const en = renderEventRegion(changed, 'en');
    assert.match(en, /^The demo changed\.\n\nSent to every window\.\n\n\*\*Payload\*\*\n\n\| Field \| Type \| Description \|/);
    assert.match(en, /\| `note` \| `string \\\| null` \| Note\. \|/);
    assert.match(renderEventRegion(changed, 'zh'), /^演示变了。\n\n发给所有窗口。\n\n\*\*载荷\*\*/);
    assert.match(renderEventRegion(reset, 'en'), /Sent to the page that made the call\.\n\n\*\*Payload\*\*\n\nThe event carries no fields\.$/);
    assert.match(renderEventRegion(frame, 'en'), /Sent to the page that owns the subscription or task\. A subscriber can have it delivered under a name of its own/);
  });

  test('an event about one window or panel goes to that page', () => {
    const [focus] = parseNamespace({ namespace: 'demo', events: { focus: { description: 'x', 'x-delivery': 'window' } } }, FILE).events;
    assert.match(renderEventRegion(focus, 'en'), /^x\n\nSent to the page of the window or panel it concerns\./);
    assert.match(renderEventRegion(focus, 'zh'), /发给它所涉及的窗口或面板里的页面。/);
  });

  test('a shared payload names a type of common.ts instead of declaring fields', () => {
    const shared = { description: 'x', 'x-delivery': 'broadcast', 'x-payload-type': 'Track' };
    const [changed] = parseNamespace({ namespace: 'demo', events: { changed: shared } }, FILE).events;
    assert.deepEqual([changed.sharedPayload, changed.payload], ['Track', null]);
    const reject = (e, pattern) => assert.throws(() => parseNamespace({ namespace: 'demo', events: { changed: e } }, FILE), pattern);
    reject({ ...shared, 'x-payload-type': 'track' }, /"x-payload-type" must name a type of common\.ts/);
    reject({ ...shared, payload: top({}) }, /"x-payload-type" replaces "payload"; give one of the two/);
    // Only the emitter sees every namespace, so it is the one to report a type common.ts lacks.
    assert.throws(() => emitCppHeader(parseNamespace({ namespace: 'demo', events: { changed: shared } }, FILE)), /demo\.json#events\.changed\.payload: Track is not a type of common\.ts/);
  });

  test('a namespace named like one the generated code uses takes a trailing underscore in C++', () => {
    const apiNs = parseNamespace({ namespace: 'api', events: { registered: { description: 'x', 'x-delivery': 'broadcast' } } }, 'src/api/schema/api.json');
    const header = emitCppHeader(apiNs);
    assert.match(header, /\nnamespace api::api_ \{\n/);
    assert.match(header, /\n\}  \/\/ namespace api::api_\n$/);
    assert.match(emitEventRegistry([apiNs]), /api::api_::events::Registered>;/);
    assert.equal(cppNamespace('cursor'), 'cursor');
  });
});

describe('api-schema · emissions in the sources', () => {
  const FILE = 'src/api/schema/demo.json';
  const namespaces = [parseNamespace({ namespace: 'demo', events: { changed: { description: 'x', 'x-delivery': 'broadcast' } } }, FILE)];
  const src = (text, file = 'src/api/DemoApi.cpp') => [{ file, text }];
  const none = { internal: new Map() };

  test('a declared event emitted through its descriptor passes', () => {
    assert.deepEqual(checkEmits(namespaces, src('api::emit::Broadcast<api::demo::events::Changed>(payload);\n'), none), []);
  });

  test('the literal name of a declared event is reported wherever it appears, with file and line', () => {
    const problems = checkEmits(namespaces, src('api::emit::Broadcast<demo::events::Changed>(p);\nctx.BroadcastEvent("demo:changed", data);\nif (name == "demo:changed") {}\n'), none);
    assert.equal(problems.length, 2, problems.join('\n'));
    assert.match(problems[0], /^demo:changed is declared in src\/api\/schema\/demo\.json but its name appears as a literal at src\/api\/DemoApi\.cpp:2/);
    assert.match(problems[1], /DemoApi\.cpp:3/);
  });

  test('a declared event nothing emits through its descriptor is reported', () => {
    const problems = checkEmits(namespaces, src('// no emitter here\n'), none);
    assert.deepEqual(problems, ['demo:changed is declared in src/api/schema/demo.json but no source under src/ emits it through demo::events::Changed']);
  });

  test('a literal emission of an undeclared event is reported unless it is internal', () => {
    const text = 'api::emit::Broadcast<demo::events::Changed>(p);\nbridge.EmitEvent("demo:extra", {});\nctx.SendEventTo(windowId, "demo:later", data);\nhost.PostEventMessage("demo:posted", json);\nbridge.EmitEvent("demo:__private", {});\n';
    const problems = checkEmits(namespaces, src(text), { internal: new Map([['demo:__private', 'overlay']]) });
    assert.deepEqual(problems.map((p) => p.split(' ')[0]), ['demo:extra', 'demo:posted', 'demo:later']);
    assert.match(problems[2], /emitted at src\/api\/DemoApi\.cpp:3 without a declaration/);
  });

  test('the descriptor counts only under its own namespace or an alias the file declares for it', () => {
    const two = [
      parseNamespace({ namespace: 'plugin', events: { registered: { description: 'x', 'x-delivery': 'broadcast' } } }, 'src/api/schema/plugin.json'),
      parseNamespace({ namespace: 'api', events: { registered: { description: 'x', 'x-delivery': 'broadcast' } } }, 'src/api/schema/api.json'),
    ];
    // Emitting plugin:registered does not count for api:registered, which has the same key.
    assert.deepEqual(checkEmits(two, src('api::emit::Broadcast<api::plugin::events::Registered>(p);\n'), none), [
      'api:registered is declared in src/api/schema/api.json but no source under src/ emits it through api_::events::Registered',
    ]);
    const aliased = 'namespace pl = api::plugin;\nnamespace apis = api::api_;\nemit::Broadcast<pl::events::Registered>(p);\nemit::Broadcast<apis::events::Registered>(q);\n';
    assert.deepEqual(checkEmits(two, src(aliased), none), []);
    // An alias for another namespace does not stand in for this one.
    assert.equal(checkEmits(two, src('namespace pl = api::other;\nemit::Broadcast<pl::events::Registered>(p);\nemit::Broadcast<api::api_::events::Registered>(q);\n'), none).length, 1);
  });
});

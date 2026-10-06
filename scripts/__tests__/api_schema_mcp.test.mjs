// scripts/__tests__/api_schema_mcp.test.mjs
//
// The MCP bridge tools and the permissions tables are generated from the declarations
// (scripts/api-schema/emit-mcp.mjs, emit-docs.mjs). These tests pin how declarations and a
// tool-table entry become a tool: its merged schema, each action's exact schema, the
// annotations, which table entries are refused, the tool tables, and the path-security specs
// the permissions page lists.

import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

import { parseNamespace, SchemaError } from '../api-schema/schema.mjs';
import { apiLinks, emitToolsTs, planTools, renderRegion } from '../api-schema/emit-mcp.mjs';
import { renderPermissionsRegion, securitySpecs } from '../api-schema/emit-docs.mjs';

const FILE = 'src/api/schema/demo.json';
const params = (properties, required = []) => ({ type: 'object', additionalProperties: false, required, properties });
const method = (p, effect = 'read', extra = {}) => ({
  description: 'Do the thing. Then more words.',
  'x-description-zh': '做这件事。然后更多。',
  'x-effect': effect,
  params: p,
  ...extra,
});

const demo = parseNamespace({
  namespace: 'demo',
  methods: {
    listRows: method(params({
      playlist: { type: 'integer', minimum: 0, description: 'Playlist index.' },
      limit: { type: 'integer', minimum: 0, default: 100, description: 'Most rows.' },
      query: { type: 'string', default: '', description: 'Filter.' },
    })),
    countRows: method(params({
      playlist: { type: 'integer', minimum: 0, description: 'Playlist index.' },
      limit: { type: 'integer', minimum: 1, maximum: 2000, default: 1000, description: 'Most rows counted.' },
    })),
    removeRows: method(params({
      playlist: { type: 'integer', minimum: 0, description: 'Playlist to change.' },
      rows: { type: 'array', items: { type: 'integer' }, description: 'Row indices.' },
    }, ['rows']), 'destructive', { 'x-idempotent': true }),
    writeTags: method(params({
      path: { type: 'string', minLength: 1, 'x-security': 'MediaWrite', description: 'Track path.' },
      tags: { type: 'object', additionalProperties: { type: 'json' }, description: 'Tag values.' },
      items: {
        type: 'array',
        minItems: 1,
        'x-security': 'MediaRead',
        'x-path-key': 'path',
        items: { type: 'object', additionalProperties: false, required: ['path'], properties: { path: { type: 'string', description: 'Item path.' }, cue: { type: 'integer', default: -1, description: 'Sub-track.' } } },
        description: 'Batch.',
      },
    }, ['path']), 'write', { 'x-idempotent': true }),
    setRows: method(params({
      rows: { type: 'array', items: { type: 'string' }, description: 'Row names.' },
    }, ['rows']), 'write'),
    copy: method(params({
      items: {
        type: 'array',
        'x-path-key': { source: 'Read', destination: 'FileWrite' },
        items: { type: 'object', additionalProperties: false, required: ['source', 'destination'], properties: { source: { type: 'string', description: 'From.' }, destination: { type: 'string', description: 'To.' } } },
        description: 'Pairs.',
      },
    }, ['items']), 'write', { 'x-open-world': true }),
    ping: method(undefined),
    readCover: method(undefined, 'read', {
      result: {
        type: 'object',
        additionalProperties: false,
        required: ['available'],
        properties: {
          available: { type: 'boolean', description: 'Whether a picture was found.' },
          size: { type: 'integer', description: 'Picture size in bytes.' },
          dataUrl: { type: 'string', description: 'The picture as a data URL.' },
        },
      },
    }),
    untagged: { description: 'No effect declared.' },
  },
}, FILE);
const namespaces = [demo];
const plan = (table) => planTools(table, namespaces);
const tool = (actions, extra = {}) => ({ fb2k_demo_tool: { description: 'Demo tool.', zh: '演示工具。', actions, ...extra } });
const only = (actions, extra) => plan(tool(actions, extra))[0];
const actionOf = (t, api) => t.actions.find((a) => a.api === api);

describe('api-schema · MCP method lifecycle', () => {
  test('action notices survive sentence truncation in both locales without changing execution or annotations', () => {
    const plain = parseNamespace({ namespace: 'demo', methods: { run: method() } }, FILE);
    const marked = parseNamespace({ namespace: 'demo', methods: { run: method(undefined, 'read', {
      'x-experimental': true, 'x-deprecated': 'Use stable. Keep the same arguments.', 'x-deprecated-zh': '请改用 stable。参数保持不变。',
    }) } }, FILE);
    const table = tool({ 'demo.run': { description: 'Run the action.' } });
    const before = planTools(table, [plain]);
    const after = planTools(table, [marked]);
    assert.equal(after[0].actions[0].description, 'Run the action. Experimental. Deprecated: Use stable. Keep the same arguments.');
    assert.match(after[0].description, /demo\.run: Run the action\. Experimental\. Deprecated: Use stable\. Keep the same arguments\./);
    assert.deepEqual(after[0].annotations, before[0].annotations);
    assert.deepEqual(after[0].inputSchema, before[0].inputSchema);
    assert.deepEqual(after[0].actions[0].inputSchema, before[0].actions[0].inputSchema);
    assert.equal(after[0].actions[0].image, before[0].actions[0].image);
    for (const links of [null, new Map([['demo.run', './demo.md#demo-run']])]) {
      assert.match(renderRegion(after, 'en', links), /\| Do the thing\. Experimental\. Deprecated: Use stable\. Keep the same arguments\. \|/);
      assert.match(renderRegion(after, 'zh', links), /\| 做这件事。 实验性 API。 已弃用：请改用 stable。参数保持不变。 \|/);
      assert.doesNotMatch(renderRegion(after, 'en', links), /Then more/);
      assert.doesNotMatch(renderRegion(after, 'zh', links), /然后更多/);
    }
    assert.match(emitToolsTs(after), /Experimental\. Deprecated: Use stable\./);
  });
});

describe('api-schema · MCP tool schemas', () => {
  test('an action takes every declared parameter with its type, constraints, default and description', () => {
    const t = only({ 'demo.listRows': { description: 'List rows.' } });
    assert.deepEqual(actionOf(t, 'demo.listRows').inputSchema, {
      type: 'object',
      properties: {
        playlist: { type: 'integer', description: 'Playlist index.', minimum: 0 },
        limit: { type: 'integer', description: 'Most rows.', minimum: 0, default: 100 },
        query: { type: 'string', description: 'Filter.', default: '' },
      },
    });
  });

  test('the merged schema has the action enum first and every parameter without defaults', () => {
    const t = only({ 'demo.listRows': { description: 'List.' }, 'demo.ping': { description: 'Ping.' } });
    assert.equal(t.name, 'fb2k_demo_tool');
    assert.deepEqual(t.inputSchema.required, ['action']);
    assert.deepEqual(Object.keys(t.inputSchema.properties), ['action', 'playlist', 'limit', 'query']);
    assert.deepEqual(t.inputSchema.properties.action.enum, ['demo.listRows', 'demo.ping']);
    assert.deepEqual(t.inputSchema.properties.limit, { type: 'integer', description: 'Most rows.', minimum: 0 });
    assert.equal(t.inputSchema.properties.query.default, undefined);
  });

  test('the description lists each action with its parameters, optional ones marked', () => {
    const t = only({ 'demo.removeRows': { description: 'Remove rows.' }, 'demo.ping': { description: 'Ping.' } });
    assert.equal(t.description, 'Demo tool.\n\nPass one of these as `action`, with the parameters it lists; `?` marks an optional one.\n- demo.removeRows(playlist?, rows): Remove rows.\n- demo.ping: Ping.');
  });

  test('objects, maps, Json and arrays of objects keep their declared shape', () => {
    const s = actionOf(only({ 'demo.writeTags': { description: 'Write.' } }), 'demo.writeTags').inputSchema;
    assert.deepEqual(s.required, ['path']);
    assert.deepEqual(s.properties.path, { type: 'string', description: 'Track path.', minLength: 1 });
    assert.deepEqual(s.properties.tags, { type: 'object', description: 'Tag values.', properties: {}, additionalProperties: { type: 'json' } });
    assert.deepEqual(s.properties.items, {
      type: 'array',
      description: 'Batch.',
      items: {
        type: 'object',
        properties: { path: { type: 'string', description: 'Item path.' }, cue: { type: 'integer', description: 'Sub-track.', default: -1 } },
        required: ['path'],
        additionalProperties: false,
      },
      minItems: 1,
    });
  });

  test('params, required and bounds narrow an action, and bounds on an array apply to its elements', () => {
    const t = only({
      'demo.listRows': { description: 'List.', params: ['limit', 'query'], required: ['query'], bounds: { limit: { minimum: 1, maximum: 500 } } },
      'demo.removeRows': { description: 'Remove.', bounds: { rows: { minimum: 0 } } },
    }, { params: { playlist: 'Playlist index.' } });
    const list = actionOf(t, 'demo.listRows').inputSchema;
    assert.deepEqual(Object.keys(list.properties), ['limit', 'query']);
    assert.deepEqual(list.required, ['query']);
    assert.deepEqual(list.properties.limit, { type: 'integer', description: 'Most rows.', minimum: 1, maximum: 500, default: 100 });
    // A parameter the action requires takes no default.
    assert.equal(list.properties.query.default, undefined);
    assert.deepEqual(actionOf(t, 'demo.removeRows').inputSchema.properties.rows.items, { type: 'integer', minimum: 0 });
  });

  test('a shared parameter declared differently gets the wider range, or both shapes as alternatives', () => {
    const t = only({
      'demo.listRows': { description: 'List.' },
      'demo.countRows': { description: 'Count.' },
      'demo.removeRows': { description: 'Remove.' },
      'demo.setRows': { description: 'Set.' },
    }, { params: { playlist: 'Playlist index.', limit: 'Most rows.', rows: 'Rows.' } });
    // listRows sets no maximum, so the merged limit has none either.
    assert.deepEqual(t.inputSchema.properties.limit, { type: 'integer', description: 'Most rows.', minimum: 0 });
    assert.deepEqual(t.inputSchema.properties.rows, {
      type: 'union',
      description: 'Rows.',
      anyOf: [
        { type: 'array', items: { type: 'integer' } },
        { type: 'array', items: { type: 'string' } },
      ],
    });
  });

  test('a parameter shared with one description keeps it; params overrides it', () => {
    const table = { 'demo.listRows': { description: 'List.' }, 'demo.countRows': { description: 'Count.' } };
    assert.equal(only(table, { params: { limit: 'Most.' } }).inputSchema.properties.playlist.description, 'Playlist index.');
    assert.equal(only(table, { params: { limit: 'Most.', playlist: 'Which one.' } }).inputSchema.properties.playlist.description, 'Which one.');
  });

  test('the generated file carries each action schema without descriptions', () => {
    const ts = emitToolsTs(plan(tool({ 'demo.listRows': { description: 'List.' } })));
    const defs = JSON.parse(ts.slice(ts.indexOf('= [') + 2, ts.lastIndexOf('];') + 1));
    assert.deepEqual(defs[0].actions['demo.listRows'].inputSchema, {
      type: 'object',
      properties: { playlist: { type: 'integer', minimum: 0 }, limit: { type: 'integer', minimum: 0, default: 100 }, query: { type: 'string', default: '' } },
    });
    assert.deepEqual(defs[0].annotations, { readOnlyHint: true, openWorldHint: false });
  });

  test('image names the result field holding the picture, and the description says it comes back as an image', () => {
    const t = only({ 'demo.readCover': { description: 'Read the cover', image: 'dataUrl' }, 'demo.ping': { description: 'Ping' } });
    assert.equal(actionOf(t, 'demo.readCover').image, 'dataUrl');
    assert.match(t.description, /\n- demo\.readCover: Read the cover; the picture comes back as an image, the other fields as JSON\n- demo\.ping: Ping$/);
    const ts = emitToolsTs([t]);
    const defs = JSON.parse(ts.slice(ts.indexOf('= [') + 2, ts.lastIndexOf('];') + 1));
    assert.equal(defs[0].actions['demo.readCover'].image, 'dataUrl');
    assert.equal('image' in defs[0].actions['demo.ping'], false);
  });

  const refuses = (label, table, pattern) =>
    test(label, () => assert.throws(() => plan(table), (e) => e instanceof SchemaError && pattern.test(e.message)));
  refuses('a tool name outside the fb2k_ snake case', { 'fb2k-Demo': { description: 'x', zh: 'x', actions: { 'demo.ping': { description: 'x' } } } }, /a tool name is fb2k_ followed by lowercase words/);
  refuses('a tool without a Chinese description', { fb2k_demo: { description: 'x', actions: { 'demo.ping': { description: 'x' } } } }, /\.zh must be a non-empty string/);
  refuses('a tool without actions', tool({}), /actions must name at least one host method/);
  refuses('an unknown tool key', tool({ 'demo.ping': { description: 'x' } }, { title: 'x' }), /unknown key "title"/);
  refuses('an undeclared method', tool({ 'demo.nope': { description: 'x' } }), /no declaration .* declares demo\.nope/);
  refuses('a method without @effect', tool({ 'demo.untagged': { description: 'x' } }), /demo\.untagged declares no @effect/);
  refuses('a method in two tools', { ...tool({ 'demo.ping': { description: 'x' } }), fb2k_demo_other: { description: 'x', zh: 'x', actions: { 'demo.ping': { description: 'x' } } } }, /already an action of fb2k_demo_tool/);
  refuses('an action without a description', tool({ 'demo.ping': {} }), /description must be a non-empty string/);
  refuses('an unknown action key', tool({ 'demo.ping': { description: 'x', rename: 'y' } }), /unknown key "rename"/);
  refuses('params naming an undeclared parameter', tool({ 'demo.listRows': { description: 'x', params: ['limit', 'offset'] } }), /declares no parameter "offset"/);
  refuses('params leaving out a required parameter', tool({ 'demo.writeTags': { description: 'x', params: ['tags'] } }), /leaves out "path"/);
  refuses('required naming a parameter the method already requires', tool({ 'demo.writeTags': { description: 'x', required: ['path'] } }), /already requires/);
  refuses('bounds wider than the declaration', tool({ 'demo.listRows': { description: 'x', bounds: { limit: { minimum: -1 } } } }), /below the declared 0/);
  refuses('bounds on a string', tool({ 'demo.listRows': { description: 'x', bounds: { query: { minimum: 1 } } } }), /not a number or an array of numbers/);
  refuses('bounds that exclude the default', tool({ 'demo.listRows': { description: 'x', bounds: { limit: { maximum: 50 } } } }), /default 100 falls outside/);
  refuses('a shared parameter described differently without params', tool({ 'demo.listRows': { description: 'x' }, 'demo.countRows': { description: 'y' } }), /"limit" is described differently by demo\.listRows, demo\.countRows; give it one description under "params"/);
  refuses('params describing a parameter no action exposes', tool({ 'demo.ping': { description: 'x' } }, { params: { limit: 'x' } }), /params names "limit", which no action exposes/);
  refuses('image on a method without a result', tool({ 'demo.ping': { description: 'x', image: 'dataUrl' } }), /demo\.ping declares no top-level result field "dataUrl"/);
  refuses('image naming a field the result lacks', tool({ 'demo.readCover': { description: 'x', image: 'cover' } }), /declares no top-level result field "cover"/);
  refuses('image naming a nested path', tool({ 'demo.readCover': { description: 'x', image: 'rows.dataUrl' } }), /declares no top-level result field "rows\.dataUrl"/);
  refuses('image naming a field that is not a string', tool({ 'demo.readCover': { description: 'x', image: 'size' } }), /"size" of demo\.readCover is integer, not a string holding a data URL/);
  refuses('an empty image', tool({ 'demo.readCover': { description: 'x', image: '' } }), /\.image must name a field of the result/);
});

describe('api-schema · MCP annotations', () => {
  const annotations = (actions, extra) => only(actions, extra).annotations;

  test('a tool of reads is read-only and says nothing more', () => {
    assert.deepEqual(annotations({ 'demo.ping': { description: 'x' }, 'demo.listRows': { description: 'x' } }), { readOnlyHint: true, openWorldHint: false });
  });

  test('one destructive action makes the tool destructive; idempotent only when every change is', () => {
    assert.deepEqual(annotations({ 'demo.writeTags': { description: 'x' }, 'demo.removeRows': { description: 'x' } }), {
      readOnlyHint: false,
      destructiveHint: true,
      idempotentHint: true,
      openWorldHint: false,
    });
    assert.deepEqual(annotations({ 'demo.writeTags': { description: 'x' }, 'demo.setRows': { description: 'x' } }), {
      readOnlyHint: false,
      destructiveHint: false,
      idempotentHint: false,
      openWorldHint: false,
    });
  });

  test('a read beside a change leaves idempotency to the changes; one open-world action opens the tool', () => {
    assert.deepEqual(annotations({ 'demo.ping': { description: 'x' }, 'demo.writeTags': { description: 'x' } }).idempotentHint, true);
    assert.equal(annotations({ 'demo.copy': { description: 'x' } }).openWorldHint, true);
  });
});

describe('api-schema · MCP tool tables', () => {
  const tools = plan(tool({ 'demo.writeTags': { description: 'Write.' }, 'demo.ping': { description: 'Ping.' } }));

  test('one section per tool with its kind, summary and one row per action', () => {
    const en = renderRegion(tools, 'en');
    assert.match(en, /^\*\*1 bridge tools\*\* covering 2 host methods/);
    assert.match(en, /### `fb2k_demo_tool`\n\nchanges state, idempotent · 2 actions\n\nDemo tool\.\n/);
    assert.match(en, /\| `demo\.writeTags` \| `path`, `tags\?`, `items\?` \| Do the thing\. \|/);
    assert.match(en, /\| `demo\.ping` \| — \| Do the thing\. \|/);
  });

  test('the Chinese table takes the Chinese summary and descriptions', () => {
    const zh = renderRegion(tools, 'zh');
    assert.match(zh, /改状态，可重复调用 · 2 个 action\n\n演示工具。/);
    assert.match(zh, /\| `demo\.ping` \| — \| 做这件事。 \|/);
  });

  test('the docs table links each action to its API page', () => {
    const links = new Map([['demo.ping', '../api/demo.md#demo-ping']]);
    assert.match(renderRegion(tools, 'en', links), /\| \[`demo\.ping`\]\(\.\.\/api\/demo\.md#demo-ping\) \| — \|/);
  });

  test('actions link to the page of their locale that holds the region, wherever it sits', () => {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'api-schema-mcp-'));
    const pages = {
      'api/demo.md': '<!-- api-schema:begin demo.ping -->\n<!-- api-schema:end -->\n',
      'reference/bridge/rows.md': '<!-- api-schema:begin demo.listRows -->\n<!-- api-schema:end -->\n',
      'zh/api/demo.md': '<!-- api-schema:begin demo.ping -->\n<!-- api-schema:end -->\n',
    };
    for (const [rel, text] of Object.entries(pages)) {
      const abs = path.join(root, 'docs/vitepress', rel);
      fs.mkdirSync(path.dirname(abs), { recursive: true });
      fs.writeFileSync(abs, text);
    }
    assert.deepEqual([...apiLinks(root, 'en')], [
      ['demo.ping', '../api/demo.md#demo-ping'],
      ['demo.listRows', '../reference/bridge/rows.md#demo-listrows'],
    ]);
    assert.deepEqual([...apiLinks(root, 'zh')], [['demo.ping', '../api/demo.md#demo-ping']]);
  });
});

describe('api-schema · permissions tables', () => {
  const specs = securitySpecs(namespaces);

  test('specs come from x-security and x-path-key, one row per member level', () => {
    assert.deepEqual(specs, [
      { level: 'Read', api: 'demo.copy', param: 'items', array: true, nestedKey: 'source' },
      { level: 'FileWrite', api: 'demo.copy', param: 'items', array: true, nestedKey: 'destination' },
      { level: 'MediaRead', api: 'demo.writeTags', param: 'items', array: true, nestedKey: 'path' },
      { level: 'MediaWrite', api: 'demo.writeTags', param: 'path', array: false, nestedKey: null },
    ].sort((a, b) => [a.api, a.param, a.nestedKey ?? ''].join('\u0000').localeCompare([b.api, b.param, b.nestedKey ?? ''].join('\u0000'))));
  });

  test('the count table totals specs and unique methods', () => {
    const counts = renderPermissionsRegion('counts', specs, 'en');
    assert.match(counts, /\| `Read` \| 1 \|/);
    assert.match(counts, /\| `Write` \| 0 \|/);
    assert.match(counts, /\| \*\*Total\*\* \| \*\*4\*\* \| \*\*2 unique APIs\*\* \|/);
  });

  test('a level table has its heading with the count and one row per spec', () => {
    assert.equal(renderPermissionsRegion('FileWrite', specs, 'en'), [
      '### FileWrite — general file writes (1 spec)',
      '',
      '| API | Parameter | Array | Nested key |',
      '| --- | --- | --- | --- |',
      '| `demo.copy` | `items` | yes | `destination` |',
    ].join('\n'));
    assert.match(renderPermissionsRegion('MediaWrite', specs, 'zh'), /^### MediaWrite — 修改媒体文件（1 条）/);
  });
});

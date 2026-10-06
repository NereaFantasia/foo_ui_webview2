// scripts/__tests__/gen_sdk_types.test.mjs
//
// Tests for scripts/gen_sdk_types.mjs, which renders sdk/src/types/generated/ from the
// declarations under src/api/schema. They run against the repository's own declarations;
// loading them type-checks the schema files, so the render happens once.

import { test, describe, before, after } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { fileURLToPath } from 'node:url';
import { createRequire } from 'node:module';

import { renderAll, run } from '../gen_sdk_types.mjs';

const REPO_ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const GENERATED_DIR = path.join(REPO_ROOT, 'sdk', 'src', 'types', 'generated');
const ts = createRequire(new URL('../../sdk/package.json', import.meta.url))('typescript');

const { decl, files } = renderAll();
const file = (name) => files.find((f) => f.name === name).content;

describe('gen_sdk_types · method lifecycle', () => {
  for (const migration of ['Use scanFiles with `**/*.mp4` instead.', 'Use scanFiles instead.\nPass `**/*.mp4` as the filter.']) {
    test(`keeps comment terminators inside valid generated JSDoc: ${JSON.stringify(migration)}`, (t) => {
      const root = fs.mkdtempSync(path.join(os.tmpdir(), 'gen-sdk-jsdoc-'));
      t.after(() => fs.rmSync(root, { recursive: true, force: true }));
      const dir = path.join(root, 'src/api/schema');
      fs.mkdirSync(dir, { recursive: true });
      const description = 'Matches `**/*.mp4` files.';
      const field = { type: 'string', description, default: '**/*.mp4' };
      const object = { type: 'object', additionalProperties: false, properties: { filter: field } };
      fs.writeFileSync(path.join(dir, 'demo.json'), JSON.stringify({
        namespace: 'demo',
        methods: { read: { description, params: object, result: object, 'x-deprecated': migration, 'x-deprecated-zh': '请改用 scanFiles，传入 `**/*.mp4`。' } },
        types: { Filter: { ...object, 'x-name': 'Filter', 'x-type-description': description } },
        events: { changed: { description, 'x-delivery': 'caller', payload: object } },
      }));
      const { decl, files: generated } = renderAll(root);
      assert.equal(decl.methods.get('demo.read').deprecated, migration);
      assert.equal(decl.methods.get('demo.read').deprecatedZh, '请改用 scanFiles，传入 `**/*.mp4`。');
      const marked = [];
      for (const { name, content } of generated) {
        const source = ts.createSourceFile(name, content, ts.ScriptTarget.Latest, true);
        assert.deepEqual(source.parseDiagnostics.map((d) => ts.flattenDiagnosticMessageText(d.messageText, '\n')), [], name);
        const visit = (node) => {
          for (const doc of node.jsDoc ?? []) {
            for (const tag of doc.tags ?? []) {
              if (tag.tagName.text !== 'deprecated') continue;
              assert.equal(ts.getTextOfJSDocComment(tag.comment).replaceAll('&#47;', '/'), migration);
              marked.push(node.name.getText(source));
            }
          }
          ts.forEachChild(node, visit);
        };
        visit(source);
        assert.doesNotMatch(content, /请改用|deprecatedZh/);
      }
      assert.deepEqual(marked.sort(), ['DemoReadParams', 'DemoReadResponse', 'DemoReadSuccess', '"demo.read"', '"demo.read"', '"demo.read"'].sort());
      for (const name of ['params.ts', 'responses.ts', 'schema-types.ts', 'events.ts']) {
        const content = generated.find((f) => f.name === name).content.replaceAll('&#47;', '/');
        assert.ok(content.includes(description), `${name}: description preserved`);
        assert.ok(content.includes('@default "**/*.mp4"'), `${name}: default preserved`);
      }
    });
  }

  test('puts lifecycle JSDoc on method types and all three maps without marking shared members', (t) => {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'gen-sdk-lifecycle-'));
    t.after(() => fs.rmSync(root, { recursive: true, force: true }));
    const dir = path.join(root, 'src/api/schema');
    fs.mkdirSync(dir, { recursive: true });
    fs.writeFileSync(path.join(dir, 'tsconfig.json'), JSON.stringify({ compilerOptions: { strict: true, noEmit: true, types: [], skipLibCheck: true }, include: ['*.ts'] }));
    fs.writeFileSync(path.join(dir, 'demo.ts'), `export interface Api {
  /**
   * Read preview.
   * @experimental
   * @deprecated Use stable.
   * @deprecatedZh 请改用 stable。
   */
  preview(): void;
  /** Read stable. */
  stable(): void;
}
`);
    const generated = renderAll(root).files;
    for (const name of ['params.ts', 'responses.ts', 'index.ts']) {
      const content = generated.find((f) => f.name === name).content;
      assert.match(content, /    \/\*\*\n     \* @experimental\n     \* @deprecated Use stable\.\n     \*\/\n    "demo\.preview":/);
      assert.match(content, /;\n    "demo\.stable":/);
      assert.doesNotMatch(content, /请改用|deprecatedZh/);
    }
    for (const [name, count] of [['params.ts', 2], ['responses.ts', 3], ['index.ts', 1]]) {
      assert.equal((generated.find((f) => f.name === name).content.match(/@deprecated/g) ?? []).length, count);
    }
    for (const name of ['schema-types.ts', 'events.ts', 'param-shapes.ts']) {
      assert.doesNotMatch(generated.find((f) => f.name === name).content, /@experimental|@deprecated/);
    }
  });
});

describe('gen_sdk_types · rendering', () => {
  test('renders the six generated files, each with the generated-file header', () => {
    assert.deepEqual(files.map((f) => f.name), ['schema-types.ts', 'params.ts', 'responses.ts', 'events.ts', 'index.ts', 'param-shapes.ts']);
    for (const f of files) {
      assert.match(f.content, /^\/\/ ─+\n\/\/ GENERATED FILE — DO NOT EDIT\n/);
      assert.match(f.content, /\/\/ Source: src\/api\/schema\/\*\.ts\n/);
      assert.ok(f.content.endsWith('\n') && !f.content.includes('\n\n\n'));
    }
  });

  test('param-shapes.ts has a shape for every declared method and names recursive types', () => {
    const shapes = file('param-shapes.ts');
    const body = shapes.slice(shapes.indexOf('export const API_PARAM_SHAPES'));
    const keys = [...body.matchAll(/^ {4}"([^"]+)": \{/gm)].map((m) => m[1]);
    assert.deepEqual(keys, [...decl.methods.keys()]);
    assert.match(shapes, /"submenu": "TrayMenuItem"/);
    assert.match(shapes, /export const API_PARAM_SHAPES: \{ readonly \[M in keyof ApiParamsMap\]: ParamShape \}/);
  });

  test('ApiMethodMap lists every declared method and nothing undeclared', () => {
    const index = file('index.ts');
    const keys = [...index.matchAll(/^ {4}"([^"]+)": \[/gm)].map((m) => m[1]);
    assert.deepEqual(keys, [...decl.methods.keys()]);
    assert.ok(keys.includes('playback.play'));
    assert.ok(!keys.includes('test.echo') && !keys.includes('test.ping'), 'test.echo / test.ping are undeclared');
    assert.ok(!keys.some((k) => k.includes('.__')), 'internal endpoints stay out');
    assert.match(index, /"playback\.play": \[PlaybackPlayParams, PlaybackPlayResponse\];/);
  });

  test('a response is its success shape or ApiFailure, and the file imports ApiFailure', () => {
    const responses = file('responses.ts');
    assert.match(responses, /^import type \{ ApiFailure \} from '\.\.\/responses\.js';$/m);
    assert.match(responses, /^export type PlaybackPlayResponse = PlaybackPlaySuccess \| ApiFailure;$/m);
    assert.match(responses, /^export interface ApiResponseMap \{$/m);
  });

  test('a method without params gets Record<string, never>', () => {
    assert.match(file('params.ts'), /^export type PlaybackPlayParams = Record<string, never>;$/m);
  });

  test('FBEventName and FBEventPayloadMap list every declared event', () => {
    const events = file('events.ts');
    const union = events.slice(events.indexOf('export type FBEventName ='));
    for (const name of decl.events.keys()) assert.ok(union.includes(`| ${JSON.stringify(name)}`), name);
    assert.match(events, /^ {4}\| "[^"]+";$/m, 'the union closes with a semicolon on its last member');
    assert.match(events, /"playback:volumeChanged": PlaybackVolumeChangedPayload;/);
  });
});

describe('gen_sdk_types · CLI', () => {
  let tmp;
  before(() => {
    tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'gen-sdk-'));
  });
  after(() => fs.rmSync(tmp, { recursive: true, force: true }));

  test('the committed generated files are up to date', () => {
    assert.equal(run(['--all', '--diff', '--out-dir', GENERATED_DIR]), 0);
  });

  test('--diff exits 3 when a file differs, 0 once it is rewritten', () => {
    const out = path.join(tmp, 'diff');
    assert.equal(run(['--all', '--out-dir', out]), 0);
    assert.equal(run(['--all', '--diff', '--out-dir', out]), 0);
    fs.appendFileSync(path.join(out, 'params.ts'), '// edited\n');
    assert.equal(run(['--all', '--diff', '--out-dir', out]), 3);
  });

  test('--dry-run writes nothing', () => {
    const out = path.join(tmp, 'dry');
    assert.equal(run(['--all', '--dry-run', '--out-dir', out]), 0);
    assert.ok(!fs.existsSync(out));
  });

  test('an unknown argument exits 1', () => {
    assert.equal(run(['--index', 'code-index.json']), 1);
  });
});

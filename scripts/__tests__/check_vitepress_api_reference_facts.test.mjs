import { test, describe, after } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const SCRIPT = path.join(ROOT, 'scripts', 'check_vitepress_api_reference_facts.mjs');
const FIX = fs.mkdtempSync(path.join(os.tmpdir(), 'foo-ui-webview2-api-reference-facts-'));

after(() => fs.rmSync(FIX, { recursive: true, force: true }));

function run(args) {
  return spawnSync(process.execPath, [SCRIPT, ...args], {
    cwd: ROOT,
    encoding: 'utf8',
    env: { ...process.env, NODE_NO_WARNINGS: '1' },
  });
}

// A declaration file with one path parameter per method, the way src/api/schema declares
// path security: { 'read': 'Read' } declares <namespace>.read with `path` at level Read.
function declaration(namespace, levels) {
  const methods = Object.fromEntries(Object.entries(levels).map(([name, level]) => [name, {
    description: 'x',
    params: { type: 'object', additionalProperties: false, required: ['path'], properties: { path: { type: 'string', 'x-security': level, description: 'x' } } },
  }]));
  return JSON.stringify({ namespace, methods });
}

function writeCase(name, files) {
  const base = path.join(FIX, name);
  fs.rmSync(base, { recursive: true, force: true });
  for (const [rel, content] of Object.entries(files)) {
    const abs = path.join(base, rel);
    fs.mkdirSync(path.dirname(abs), { recursive: true });
    fs.writeFileSync(abs, content);
  }
  return base;
}

describe('check_vitepress_api_reference_facts', () => {
  test('forbids stopAfter value literal', () => {
    const rules = {
      facts: [
        {
          id: 'P3-CS01-stopAfterCurrent-no-value',
          op: 'literal-forbids',
          files: ['api/playback.md'],
          locales: ['root', 'zh'],
          literals: ['{value}'],
          findingKind: 'stopAfterCurrent-value-payload',
        },
      ],
    };
    const base = writeCase('neg-value', {
      'rules.json': JSON.stringify(rules),
      'docs/api/playback.md': 'payload {value}\n',
      'locale/api/playback.md': 'payload {enabled}\n',
      'src/api/ErrorEnvelope.h': 'namespace ApiErrorCode { constexpr const char* X = "INVALID_PARAMS"; }\n',
    });
    const r = run([
      '--repo-root', base,
      '--docs-root', path.join(base, 'docs'),
      '--locale-root', path.join(base, 'locale'),
      '--authority-root', base,
      '--rules', path.join(base, 'rules.json'),
      '--strict', '--json',
    ]);
    assert.equal(r.status, 1);
    const j = JSON.parse(r.stdout);
    assert.ok(j.findings.some((f) => f.findingKind === 'stopAfterCurrent-value-payload'));
  });

  test('playlist payload keys equal', () => {
    const rules = {
      facts: [
        {
          id: 'P3-CS02',
          op: 'payload-keys-equal',
          files: ['api/events.md'],
          locales: ['root'],
          events: [{ event: 'playlist:itemsAdded', keys: ['playlist', 'start', 'count'] }],
          findingKind: 'playlist-payload-mismatch',
        },
      ],
    };
    const base = writeCase('neg-playlist', {
      'rules.json': JSON.stringify(rules),
      'docs/api/events.md': '| playlist:itemsAdded | x | `{ playlist, count }` |\n',
      'locale/api/events.md': '| playlist:itemsAdded | x | `{ playlist, start, count }` |\n',
      'src/api/ErrorEnvelope.h': 'namespace ApiErrorCode { constexpr const char* X = "INVALID_PARAMS"; }\n',
    });
    const r = run([
      '--repo-root', base,
      '--docs-root', path.join(base, 'docs'),
      '--locale-root', path.join(base, 'locale'),
      '--authority-root', base,
      '--rules', path.join(base, 'rules.json'),
      '--strict', '--json',
    ]);
    assert.equal(r.status, 1);
    const j = JSON.parse(r.stdout);
    assert.ok(j.findings.some((f) => f.findingKind === 'playlist-payload-mismatch'));
  });

  test('security-level count drift fails', () => {
    const rules = {
      facts: [{
        id: 'security-counts', op: 'security-level-counts-match-authority', files: ['reference/permissions.md'],
        locales: ['root'], expectedCounts: { Read: 2 },
      }],
    };
    const base = writeCase('neg-security-count', {
      'rules.json': JSON.stringify(rules),
      'docs/reference/permissions.md': '| SecurityLevel | Count |\n| --- | --- |\n| Read | 2 |\n',
      'src/api/schema/file.json': declaration('file', { read: 'Read' }),
    });
    const r = run([
      '--repo-root', base,
      '--docs-root', path.join(base, 'docs'),
      '--locale-root', path.join(base, 'locale'),
      '--authority-root', base,
      '--rules', path.join(base, 'rules.json'),
      '--strict', '--json',
    ]);
    assert.equal(r.status, 1);
    const j = JSON.parse(r.stdout);
    assert.ok(j.findings.some((f) => f.findingKind === 'security-level-count-mismatch'));
  });

  test('permissions summaries and level tables must match the declared security levels', () => {
    const rules = { facts: [{
      id: 'permissions-contract', op: 'permissions-documentation-match-authority',
      files: ['reference/permissions.md'], locales: ['root'],
    }] };
    const base = writeCase('neg-permissions-document', {
      'rules.json': JSON.stringify(rules),
      'src/api/schema/a.json': declaration('a', { read: 'Read', write: 'Write' }),
      'src/api/schema/b.json': declaration('b', { read: 'Read' }),
      'docs/reference/permissions.md': [
        '| Level | Spec count | Meaning |', '| --- | ---: | --- |', '| `Read` | 2 | x |', '| `Write` | 1 | x |', '| **Total** | **3** | x |',
        '### Read — filesystem read (2 specs)', '| API | Parameter |', '| --- | --- |', '| `a.read` | `path` |',
        '### Write — destinations (1 specs)', '| API | Parameter |', '| --- | --- |', '| `a.write` | `path` |',
        '## Counts summary', '| Level | Specs | Unique APIs in this level table |', '| --- | ---: | ---: |', '| Read | 2 | 2 |', '| Write | 1 | 1 |', '| **Total** | **3** | **2** |',
      ].join('\n'),
    });
    const r = run([
      '--repo-root', base, '--docs-root', path.join(base, 'docs'), '--locale-root', path.join(base, 'locale'),
      '--authority-root', base, '--rules', path.join(base, 'rules.json'), '--strict', '--json',
    ]);
    assert.equal(r.status, 1);
    const j = JSON.parse(r.stdout);
    assert.ok(j.findings.some((f) => f.findingKind === 'permissions-level-entry-count-mismatch'));
  });
});

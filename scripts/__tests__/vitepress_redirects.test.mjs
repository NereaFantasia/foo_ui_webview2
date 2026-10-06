// scripts/__tests__/vitepress_redirects.test.mjs
// Resolution rules of docs/vitepress/.vitepress/redirects.mjs: where an old page and fragment
// end up, and how source paths map to published URL paths and back.
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const { locationUrl, pageFromPathname, pageUrlPath, resolveMoved, splitTarget } = await import(
  pathToFileURL(path.join(ROOT, 'docs', 'vitepress', '.vitepress', 'redirects.mjs')).href
);

const BASE = '/foo_ui_webview2/';

describe('vitepress redirects · resolveMoved', () => {
  const redirects = {
    'sdk/navigation.md': { to: 'sdk/queue.md', anchors: { 'registerhotkey': 'sdk/keyboard.md#register-a-hotkey' } },
    'sdk/old-queue.md': { to: 'sdk/navigation.md' },
    'mcp/tools-ui.md': { to: 'mcp/tools.md#fb2k-page-inspect' },
    'reference/smp-compat.md': { anchors: { 'quick-start': 'guide/smp-migration.md#quick-start' } },
  };

  test('a moved page takes an unmapped fragment along', () => {
    assert.deepEqual(resolveMoved(redirects, 'sdk/navigation.md', 'remove-index'), { page: 'sdk/queue.md', id: 'remove-index' });
    assert.deepEqual(resolveMoved(redirects, 'sdk/navigation.md', ''), { page: 'sdk/queue.md', id: '' });
  });

  test('a mapped fragment goes where its entry says', () => {
    assert.deepEqual(resolveMoved(redirects, 'sdk/navigation.md', 'registerhotkey'), { page: 'sdk/keyboard.md', id: 'register-a-hotkey' });
  });

  test('a fragment on "to" is used only when the old URL has none', () => {
    assert.deepEqual(resolveMoved(redirects, 'mcp/tools-ui.md', ''), { page: 'mcp/tools.md', id: 'fb2k-page-inspect' });
    assert.deepEqual(resolveMoved(redirects, 'mcp/tools-ui.md', 'fb2k-ui-toast'), { page: 'mcp/tools.md', id: 'fb2k-ui-toast' });
  });

  test('chains resolve to the last place, applying anchors met on the way', () => {
    assert.deepEqual(resolveMoved(redirects, 'sdk/old-queue.md', 'registerhotkey'), { page: 'sdk/keyboard.md', id: 'register-a-hotkey' });
    assert.deepEqual(resolveMoved(redirects, 'sdk/old-queue.md', ''), { page: 'sdk/queue.md', id: '' });
  });

  test('a page that stayed answers only for its mapped anchors', () => {
    assert.deepEqual(resolveMoved(redirects, 'reference/smp-compat.md', 'quick-start'), { page: 'guide/smp-migration.md', id: 'quick-start' });
    assert.equal(resolveMoved(redirects, 'reference/smp-compat.md', 'fb-object'), null);
    assert.equal(resolveMoved(redirects, 'guide/overview.md', 'x'), null);
  });

  test('a cycle is an error, not a hang', () => {
    assert.throws(() => resolveMoved({ 'a.md': { to: 'b.md' }, 'b.md': { to: 'a.md' } }, 'a.md', ''), /redirect cycle/);
  });

  test('names inherited from Object are not entries', () => {
    assert.equal(resolveMoved({}, 'constructor', ''), null);
    assert.equal(resolveMoved({ 'a.md': { anchors: {} } }, 'a.md', 'toString'), null);
  });

  test('splitTarget separates the page from the id', () => {
    assert.deepEqual(splitTarget('sdk/queue.md#remove-target'), { page: 'sdk/queue.md', id: 'remove-target' });
    assert.deepEqual(splitTarget('sdk/queue.md'), { page: 'sdk/queue.md', id: '' });
  });
});

describe('vitepress redirects · URL paths', () => {
  const cases = [
    ['index.md', ''],
    ['zh/index.md', 'zh/'],
    ['components/index.md', 'components/'],
    ['guide/overview.md', 'guide/overview.html'],
    ['zh/sdk/player.md', 'zh/sdk/player.html'],
  ];

  test('pageUrlPath publishes pages the way VitePress does without clean URLs', () => {
    for (const [page, url] of cases) assert.equal(pageUrlPath(page), url, page);
  });

  test('pageFromPathname inverts it, with or without the base', () => {
    for (const [page, url] of cases) {
      assert.equal(pageFromPathname(`${BASE}${url}`, BASE), page, url);
      assert.equal(pageFromPathname(`/${url}`, '/'), page, url);
    }
    assert.equal(pageFromPathname('/foo_ui_webview2', BASE), 'index.md');
    assert.equal(pageFromPathname('/foo_ui_webview2', '/foo_ui_webview2'), 'index.md');
  });

  test('pageFromPathname accepts the extension-less form GitHub Pages serves', () => {
    assert.equal(pageFromPathname('/foo_ui_webview2/guide/installation', BASE), 'guide/installation.md');
  });

  test('locationUrl encodes the fragment and keeps the path readable', () => {
    assert.equal(locationUrl({ page: 'guide/overview.md', id: '主要特性' }, BASE), '/foo_ui_webview2/guide/overview.html#%E4%B8%BB%E8%A6%81%E7%89%B9%E6%80%A7');
    assert.equal(locationUrl({ page: 'index.md', id: '' }, BASE), '/foo_ui_webview2/');
    assert.equal(pageFromPathname('/foo_ui_webview2/zh/%E4%B8%AD.html', BASE), 'zh/中.md');
  });
});

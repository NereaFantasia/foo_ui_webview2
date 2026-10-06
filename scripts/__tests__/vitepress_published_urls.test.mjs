// scripts/__tests__/vitepress_published_urls.test.mjs
// docs/vitepress/.vitepress/published-urls.mjs: what the published-page check refuses, the
// redirect pages the build writes (including where their script sends a visitor), and the
// check of redirect targets against the build output.
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import vm from 'node:vm';
import { fileURLToPath, pathToFileURL } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const { checkBuiltTargets, checkPublishedUrls, listSourcePages, renderRedirectPages } = await import(
  pathToFileURL(path.join(ROOT, 'docs', 'vitepress', '.vitepress', 'published-urls.mjs')).href
);

const BASE = '/foo_ui_webview2/';
const ORIGIN = 'https://example.github.io';

describe('vitepress published urls · check', () => {
  const pages = ['guide/overview.md', 'mcp/tools.md', 'sdk/queue.md'];

  test('a moved page with a redirect to an existing page passes', () => {
    const redirects = { 'mcp/tools-ui.md': { to: 'mcp/tools.md' } };
    assert.deepEqual(checkPublishedUrls({ pages, published: [...pages, 'mcp/tools-ui.md'], redirects }), []);
  });

  test('a published page that is gone without a redirect is refused', () => {
    const problems = checkPublishedUrls({ pages, published: [...pages, 'sdk/navigation.md'], redirects: {} });
    assert.deepEqual(problems, ['sdk/navigation.md was published and is gone; add its new place to redirects.json']);
  });

  test('a redirect must not shadow a page that still exists', () => {
    const problems = checkPublishedUrls({ pages, published: pages, redirects: { 'sdk/queue.md': { to: 'mcp/tools.md' } } });
    assert.match(problems.join('\n'), /sdk\/queue\.md still exists/);
  });

  test('targets must exist, after following chains', () => {
    const redirects = { 'a.md': { to: 'b.md' }, 'b.md': { to: 'gone.md' }, 'c.md': { to: 'sdk/queue.md', anchors: { x: 'nowhere.md#y' } } };
    const problems = checkPublishedUrls({ pages, published: pages, redirects }).join('\n');
    assert.match(problems, /redirect of a\.md ends at gone\.md, which does not exist/);
    assert.match(problems, /anchor c\.md#x ends at nowhere\.md, which does not exist/);
  });

  test('an anchors-only entry needs its page; an entry needs "to" or "anchors"', () => {
    const redirects = { 'old.md': { anchors: { x: 'sdk/queue.md#x' } }, 'guide/overview.md': {} };
    const problems = checkPublishedUrls({ pages, published: pages, redirects }).join('\n');
    assert.match(problems, /old\.md maps anchors but no longer exists/);
    assert.match(problems, /guide\/overview\.md has neither "to" nor "anchors"/);
  });

  test('malformed entries and cycles are reported instead of thrown', () => {
    const redirects = { 'guide': { to: 'x.md' }, 'a.md': { to: 'b.md', rename: 1 }, 'b.md': { to: 'a.md' }, 'c.md': { to: 'sdk/queue.md', anchors: { x: 'sdk/queue.md' } } };
    const problems = checkPublishedUrls({ pages, published: [...pages, 'a.md'], redirects }).join('\n');
    assert.match(problems, /"guide" is not a page path/);
    assert.match(problems, /a\.md has unknown key\(s\) rename/);
    assert.match(problems, /redirect cycle/);
    assert.match(problems, /c\.md anchor x must point at page\.md#id/);
  });

  test('a page missing from the list is refused unless the caller skips that rule', () => {
    const problems = checkPublishedUrls({ pages, published: ['guide/overview.md', 'mcp/tools.md'], redirects: {} });
    assert.equal(problems.length, 1);
    assert.match(problems[0], /sdk\/queue\.md is not in published-pages\.json; run/);
    assert.deepEqual(checkPublishedUrls({ pages, published: [], redirects: {}, requireListed: false }), []);
  });

  test('the current docs tree and its data files pass', () => {
    const docsRoot = path.join(ROOT, 'docs', 'vitepress');
    const read = (f) => JSON.parse(fs.readFileSync(path.join(docsRoot, '.vitepress', f), 'utf8'));
    assert.deepEqual(checkPublishedUrls({ pages: listSourcePages(docsRoot), published: read('published-pages.json'), redirects: read('redirects.json') }), []);
  });
});

describe('vitepress published urls · redirect pages', () => {
  const redirects = {
    'sdk/navigation.md': { to: 'sdk/queue.md', anchors: { registerhotkey: 'sdk/keyboard.md#register-a-hotkey' } },
    'zh/mcp/tools-ui.md': { to: 'zh/mcp/tools.md#fb2k-page-inspect' },
    'reference/smp-compat.md': { anchors: { 'quick-start': 'guide/smp.md#quick-start' } },
  };
  const rendered = renderRedirectPages({ redirects, base: BASE, origin: ORIGIN });
  const byFile = Object.fromEntries(rendered.map((r) => [r.file, r.html]));

  // Runs a redirect page's script with the given fragment and returns where it navigates.
  function landing(html, hash) {
    const script = /<script>([\s\S]*?)<\/script>/.exec(html)[1];
    let target = null;
    vm.runInNewContext(script, { location: { hash, replace: (url) => { target = url; } } });
    return target;
  }

  test('only pages that moved get one, at their old output path', () => {
    assert.deepEqual(Object.keys(byFile).sort(), ['sdk/navigation.html', 'zh/mcp/tools-ui.html']);
  });

  test('the page is marked for its locale, not indexed, and canonical to its new place', () => {
    const en = byFile['sdk/navigation.html'];
    assert.match(en, /<html lang="en-US">/);
    assert.match(en, /<meta name="robots" content="noindex">/);
    assert.match(en, /<link rel="canonical" href="https:\/\/example\.github\.io\/foo_ui_webview2\/sdk\/queue\.html">/);
    assert.match(en, /<noscript><meta http-equiv="refresh" content="0; url=\/foo_ui_webview2\/sdk\/queue\.html"><\/noscript>/);
    assert.match(byFile['zh/mcp/tools-ui.html'], /<html lang="zh-CN">[\s\S]*此页已移至/);
  });

  test('the script keeps an unmapped fragment and maps a moved one', () => {
    const en = byFile['sdk/navigation.html'];
    assert.equal(landing(en, ''), '/foo_ui_webview2/sdk/queue.html');
    assert.equal(landing(en, '#remove-index'), '/foo_ui_webview2/sdk/queue.html#remove-index');
    assert.equal(landing(en, '#registerhotkey'), '/foo_ui_webview2/sdk/keyboard.html#register-a-hotkey');
    assert.equal(landing(en, '#%E4%B8%BB'), '/foo_ui_webview2/sdk/queue.html#%E4%B8%BB');
  });

  test('a fragment on "to" is where a URL without one lands', () => {
    const zh = byFile['zh/mcp/tools-ui.html'];
    assert.equal(landing(zh, ''), '/foo_ui_webview2/zh/mcp/tools.html#fb2k-page-inspect');
    assert.equal(landing(zh, '#fb2k-ui-toast'), '/foo_ui_webview2/zh/mcp/tools.html#fb2k-ui-toast');
  });

  test('built targets are checked for their page and their id', () => {
    const outDir = fs.mkdtempSync(path.join(os.tmpdir(), 'published-urls-'));
    const write = (file, html) => {
      fs.mkdirSync(path.dirname(path.join(outDir, file)), { recursive: true });
      fs.writeFileSync(path.join(outDir, file), html);
    };
    write('sdk/queue.html', '<h2 id="remove-target">remove</h2>');
    write('zh/mcp/tools.html', '<h3 id="fb2k-page-inspect">x</h3>');
    write('guide/smp.html', '<h2 id="other">x</h2>');
    const problems = checkBuiltTargets({ outDir, redirects });
    assert.deepEqual(problems, [
      'anchor sdk/navigation.md#registerhotkey goes to sdk/keyboard.md, which the build did not produce',
      'anchor reference/smp-compat.md#quick-start goes to guide/smp.md#quick-start, which has no such id',
    ]);
  });
});

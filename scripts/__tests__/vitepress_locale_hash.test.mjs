// scripts/__tests__/vitepress_locale_hash.test.mjs
// Pure-function regression for theme locale-switch hash preservation.
// Loads standard ESM helper (.mjs) — Node 20 native, no strip-types flag.
import { test, describe, before } from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import { pathToFileURL } from 'node:url';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const HELPER_MJS = path.join(
  ROOT,
  'docs',
  'vitepress',
  '.vitepress',
  'theme',
  'locale-hash.mjs',
);

/** @type {any} */
let helper;

before(async () => {
  helper = await import(pathToFileURL(HELPER_MJS).href);
  assert.equal(typeof helper.resolveLocaleHashNavigation, 'function');
  assert.equal(typeof helper.applyLocaleHashClick, 'function');
  assert.equal(typeof helper.rewriteHrefWithPendingHash, 'function');
  assert.equal(typeof helper.isCorrespondingLocalePath, 'function');
  assert.equal(typeof helper.localeSeoUrls, 'function');
});

describe('vitepress locale hash preservation', () => {
  test('root->zh appends missing hash for Simplified Chinese label', () => {
    const d = helper.resolveLocaleHashNavigation({
      hrefAttr: '/foo_ui_webview2/zh/guide/overview.html',
      linkText: '\u7b80\u4f53\u4e2d\u6587',
      inLangMenu: true,
      currentHash: '#\u4e3b\u8981\u7279\u6027',
      origin: 'https://127.0.0.1:64127',
    });
    assert.deepEqual(d, {
      action: 'navigate',
      href: '/foo_ui_webview2/zh/guide/overview.html#\u4e3b\u8981\u7279\u6027',
    });
  });

  test('zh->root appends missing hash for transitional root label', () => {
    const d = helper.resolveLocaleHashNavigation({
      hrefAttr: '/foo_ui_webview2/guide/overview.html',
      linkText: '\u4e2d\u6587\uff08\u6839\u8def\u7531\u8fc7\u6e21\uff09',
      inLangMenu: true,
      currentHash: '#\u4e3b\u8981\u7279\u6027',
      origin: 'https://127.0.0.1:64127',
    });
    assert.deepEqual(d, {
      action: 'navigate',
      href: '/foo_ui_webview2/guide/overview.html#\u4e3b\u8981\u7279\u6027',
    });
  });

  test('preserves hash already present on locale link (useLangs path)', () => {
    const d = helper.resolveLocaleHashNavigation({
      hrefAttr: '/foo_ui_webview2/zh/reference/smp-compat.html#smp-events',
      linkText: '\u7b80\u4f53\u4e2d\u6587',
      inLangMenu: true,
      currentHash: '#smp-events',
      origin: 'https://127.0.0.1:64127',
    });
    assert.deepEqual(d, {
      action: 'navigate',
      href: '/foo_ui_webview2/zh/reference/smp-compat.html#smp-events',
    });
  });

  test('keeps the hash on a base-less locale link into every top-level section', () => {
    for (const section of ['tutorials', 'how-to', 'concepts', 'guide', 'api', 'sdk', 'mcp', 'reference', 'components']) {
      const d = helper.resolveLocaleHashNavigation({
        hrefAttr: `/${section}/page.html`,
        linkText: 'English',
        inLangMenu: true,
        currentHash: '#anchor',
        origin: 'https://127.0.0.1:64127',
      });
      assert.deepEqual(d, { action: 'navigate', href: `/${section}/page.html#anchor` }, section);
    }
  });

  test('ignores ordinary in-site links outside language menu', () => {
    const d = helper.resolveLocaleHashNavigation({
      hrefAttr: '/foo_ui_webview2/guide/overview.html',
      linkText: '\u5feb\u901f\u5f00\u59cb',
      inLangMenu: false,
      currentHash: '#\u4e3b\u8981\u7279\u6027',
      origin: 'https://127.0.0.1:64127',
    });
    assert.equal(d.action, 'ignore');
  });

  test('ignores external links even inside menu', () => {
    const d = helper.resolveLocaleHashNavigation({
      hrefAttr: 'https://github.com/NereaFantasia/foo_ui_webview2',
      linkText: 'GitHub',
      inLangMenu: true,
      currentHash: '#\u4e3b\u8981\u7279\u6027',
      origin: 'https://127.0.0.1:64127',
    });
    assert.equal(d.action, 'ignore');
  });

  test('ignores locale switch when there is no hash to preserve', () => {
    const d = helper.resolveLocaleHashNavigation({
      hrefAttr: '/foo_ui_webview2/zh/guide/overview.html',
      linkText: '\u7b80\u4f53\u4e2d\u6587',
      inLangMenu: true,
      currentHash: '',
      origin: 'https://127.0.0.1:64127',
    });
    assert.equal(d.action, 'ignore');
  });

  test('rewriteHrefWithPendingHash appends missing fragment', () => {
    assert.equal(
      helper.rewriteHrefWithPendingHash(
        '/foo_ui_webview2/zh/guide/overview.html',
        '#\u4e3b\u8981\u7279\u6027',
      ),
      '/foo_ui_webview2/zh/guide/overview.html#\u4e3b\u8981\u7279\u6027',
    );
    assert.equal(
      helper.rewriteHrefWithPendingHash(
        '/foo_ui_webview2/zh/guide/overview.html#already',
        '#\u4e3b\u8981\u7279\u6027',
      ),
      null,
    );
    assert.equal(
      helper.rewriteHrefWithPendingHash(
        '/foo_ui_webview2/zh/guide/overview.html',
        '',
      ),
      null,
    );
  });

  test('isCorrespondingLocalePath root<->zh only', () => {
    assert.equal(
      helper.isCorrespondingLocalePath(
        '/foo_ui_webview2/guide/overview.html',
        '/foo_ui_webview2/zh/guide/overview.html',
      ),
      true,
    );
    assert.equal(
      helper.isCorrespondingLocalePath(
        '/foo_ui_webview2/zh/guide/overview.html',
        '/foo_ui_webview2/guide/overview.html',
      ),
      true,
    );
    assert.equal(
      helper.isCorrespondingLocalePath(
        '/foo_ui_webview2/guide/overview.html',
        '/foo_ui_webview2/guide/overview.html',
      ),
      false,
    );
    assert.equal(
      helper.isCorrespondingLocalePath(
        '/foo_ui_webview2/guide/overview.html',
        '/foo_ui_webview2/api/overview.html',
      ),
      false,
    );
    assert.equal(
      helper.isCorrespondingLocalePath(
        '/foo_ui_webview2/reference/smp-compat.html',
        '/foo_ui_webview2/zh/reference/smp-compat.html',
      ),
      true,
    );
  });

  test('applyLocaleHashClick stops later SPA capture handler from overriding', () => {
    const decision = helper.resolveLocaleHashNavigation({
      hrefAttr: '/foo_ui_webview2/zh/guide/overview.html',
      linkText: '\u7b80\u4f53\u4e2d\u6587',
      inLangMenu: true,
      currentHash: '#\u4e3b\u8981\u7279\u6027',
      origin: 'https://127.0.0.1:64127',
    });

    const calls = [];
    let stopped = false;
    const event = {
      defaultPrevented: false,
      preventDefault() {
        this.defaultPrevented = true;
        calls.push('preventDefault');
      },
      stopImmediatePropagation() {
        stopped = true;
        calls.push('stopImmediatePropagation');
      },
      stopPropagation() {
        calls.push('stopPropagation');
      },
    };
    const locationLike = {
      assign(href) {
        calls.push(['assign', href]);
      },
    };

    const forced = helper.applyLocaleHashClick(event, decision, locationLike);
    assert.equal(forced, true);
    assert.equal(stopped, true);

    // Simulated later SPA capture handler: must not run when stopped.
    const spaWouldNavigate = () => {
      if (event.defaultPrevented || stopped) return;
      locationLike.assign('/foo_ui_webview2/zh/guide/overview.html');
      calls.push('spa-go');
    };
    spaWouldNavigate();

    assert.deepEqual(calls, [
      'preventDefault',
      'stopImmediatePropagation',
      ['assign', '/foo_ui_webview2/zh/guide/overview.html#\u4e3b\u8981\u7279\u6027'],
    ]);
    assert.ok(!calls.includes('spa-go'));
  });

  test('onBeforeRouteChange rewrite model: corresponding + missing hash', () => {
    // Models theme installLocaleHashPreservation decision path.
    const fromPath = '/foo_ui_webview2/guide/overview.html';
    const spaTarget = '/foo_ui_webview2/zh/guide/overview.html';
    const liveHash = '#\u4e3b\u8981\u7279\u6027';
    const rewritten = helper.rewriteHrefWithPendingHash(spaTarget, liveHash);
    assert.equal(
      rewritten,
      '/foo_ui_webview2/zh/guide/overview.html#\u4e3b\u8981\u7279\u6027',
    );
    assert.equal(helper.isCorrespondingLocalePath(fromPath, rewritten), true);

    // Same-locale sidebar click must not rewrite.
    const sameLocale = helper.rewriteHrefWithPendingHash(
      '/foo_ui_webview2/api/overview.html',
      liveHash,
    );
    assert.equal(sameLocale, '/foo_ui_webview2/api/overview.html#\u4e3b\u8981\u7279\u6027');
    assert.equal(
      helper.isCorrespondingLocalePath(fromPath, sameLocale),
      false,
    );
  });

  test('localeSeoUrls updates canonical and hreflang targets after SPA locale switches', () => {
    assert.deepEqual(
      helper.localeSeoUrls('/foo_ui_webview2/reference/smp-compat.html'),
      {
        canonical: 'https://nereafantasia.github.io/foo_ui_webview2/reference/smp-compat.html',
        en: 'https://nereafantasia.github.io/foo_ui_webview2/reference/smp-compat.html',
        zh: 'https://nereafantasia.github.io/foo_ui_webview2/zh/reference/smp-compat.html',
        xDefault: 'https://nereafantasia.github.io/foo_ui_webview2/reference/smp-compat.html',
      },
    );
    assert.deepEqual(
      helper.localeSeoUrls('/foo_ui_webview2/zh/reference/smp-compat.html'),
      {
        canonical: 'https://nereafantasia.github.io/foo_ui_webview2/zh/reference/smp-compat.html',
        en: 'https://nereafantasia.github.io/foo_ui_webview2/reference/smp-compat.html',
        zh: 'https://nereafantasia.github.io/foo_ui_webview2/zh/reference/smp-compat.html',
        xDefault: 'https://nereafantasia.github.io/foo_ui_webview2/reference/smp-compat.html',
      },
    );
    assert.equal(
      helper.localeSeoUrls('/foo_ui_webview2/zh/').canonical,
      'https://nereafantasia.github.io/foo_ui_webview2/zh/',
    );
  });

  test('regression oracle: preventDefault alone cannot stop earlier SPA handler', () => {
    const decision = helper.resolveLocaleHashNavigation({
      hrefAttr: '/foo_ui_webview2/zh/guide/overview.html',
      linkText: '\u7b80\u4f53\u4e2d\u6587',
      inLangMenu: true,
      currentHash: '#\u4e3b\u8981\u7279\u6027',
      origin: 'https://127.0.0.1:64127',
    });
    const assigned = [];
    // SPA runs first (window capture registered earlier)
    assigned.push('/foo_ui_webview2/zh/guide/overview.html');
    // Legacy theme: only preventDefault + assign (too late to cancel SPA)
    const brokenEvent = {
      defaultPrevented: false,
      preventDefault() {
        this.defaultPrevented = true;
      },
    };
    brokenEvent.preventDefault();
    assigned.push(decision.href);
    // Final URL determined by SPA pushState (first) — hash lost in real race.
    // This oracle documents why onBeforeRouteChange rewrite is required.
    assert.equal(assigned[0], '/foo_ui_webview2/zh/guide/overview.html');
    assert.notEqual(assigned[0], decision.href);
  });
});
